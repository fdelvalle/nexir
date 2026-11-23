#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_WIDGETS 64
#define MAX_ACTIONS 64
#define MAX_CODE 4096
#define MAX_TEXT 120
#define MAX_FILE 65536
#define MAX_STEPS 256
#define OP_LOAD 1
#define OP_INPUT 2
#define OP_ADD 3
#define OP_SUB 4
#define OP_CHECK_POS 5
#define OP_CHECK_LE 6
#define OP_STORE 7
#define OP_STATUS 8
#define OP_CLEAR 9
#define OP_PUSH 10
#define OP_EQ 11
#define OP_JZ 12
#define OP_JMP 13
#define OP_HALT 255

typedef struct { uint8_t type,id; int16_t x,y,w,h; char text[MAX_TEXT+1]; } Widget;
typedef struct {uint8_t id;uint16_t length;uint8_t code[MAX_CODE];} Action;
typedef struct {char title[121];int width,height;int64_t vars[16];Widget widgets[MAX_WIDGETS];int nw;Action actions[MAX_ACTIONS];int na;} Program;
static char inputs[256][32];static char status[160]="Ready";static uint8_t focused=0;
static uint16_t u16(const uint8_t*p){return (uint16_t)(p[0]|(p[1]<<8));}
static int64_t i64(const uint8_t*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return (int64_t)v;}
static int fail(const char*s){fprintf(stderr,"NEXIR error: %s\n",s);return 0;}
static int validate_action(const Action*a){
    int depth[MAX_CODE+1], boundary[MAX_CODE+1]={0}, queue[MAX_CODE+1];
    size_t pc=0; int n=0, front=0, back=0, reached_halt=0;
    for(int i=0;i<=MAX_CODE;i++)depth[i]=-1;
    while(pc<a->length){
        boundary[pc]=1;uint8_t op=a->code[pc++];int size=0;
        switch(op){
          case OP_LOAD:case OP_INPUT:case OP_STORE:case OP_STATUS:case OP_CLEAR:size=1;break;
          case OP_PUSH:size=8;break;
          case OP_JZ:case OP_JMP:size=2;break;
          case OP_ADD:case OP_SUB:case OP_CHECK_LE:case OP_CHECK_POS:case OP_EQ:case OP_HALT:break;
          default:return fail("unknown opcode");
        }
        if(pc+(size_t)size>a->length)return fail("truncated opcode");
        if((op==OP_LOAD||op==OP_STORE)&&a->code[pc]>=16)return fail("variable index");
        pc+=size;n++;
    }
    boundary[pc]=1;
    if(!n)return fail("empty action");
    depth[0]=0;queue[back++]=0;
    while(front<back){
        int at=queue[front++];if(at==(int)a->length)return fail("execution falls off end");
        uint8_t op=a->code[at];int size=(op==OP_PUSH?8:(op==OP_JZ||op==OP_JMP?2:(op==OP_LOAD||op==OP_INPUT||op==OP_STORE||op==OP_STATUS||op==OP_CLEAR?1:0)));
        int next=at+1+size,d=depth[at],pop=0,push=0;
        if(op==OP_ADD||op==OP_SUB||op==OP_CHECK_LE||op==OP_EQ)pop=2;
        if(op==OP_STORE||op==OP_JZ)pop=1;
        if(op==OP_CHECK_POS)pop=1,push=1;
        if(op==OP_LOAD||op==OP_INPUT||op==OP_PUSH)push=1;
        if(op==OP_ADD||op==OP_SUB||op==OP_EQ)push=1;
        if(d<pop||d-pop+push>32)return fail("stack depth invalid");
        int nd=d-pop+push;
        if(op==OP_HALT){reached_halt=1;continue;}
        int targets[2]={next,-1};
        if(op==OP_JZ||op==OP_JMP){int offset=(int16_t)u16(a->code+at+1);int target=next+offset;
            if(target<=at||target>=(int)a->length||!boundary[target])return fail("invalid forward jump");
            targets[0]=target;if(op==OP_JZ)targets[1]=next;
        }
        for(int k=0;k<2;k++)if(targets[k]>=0){int t=targets[k];
            if(t>=(int)a->length||!boundary[t])return fail("invalid control flow");
            if(depth[t]!=-1&&depth[t]!=nd)return fail("incompatible branch stack");
            if(depth[t]==-1){depth[t]=nd;queue[back++]=t;}
        }
    }
    return reached_halt?1:fail("missing reachable halt");
}
static int load(const char*path,Program*p){FILE*f=fopen(path,"rb");if(!f){perror(path);return 0;}uint8_t b[MAX_FILE+1];size_t n=fread(b,1,sizeof(b),f);fclose(f);if(n>MAX_FILE)return fail("file too large");if(n<13||memcmp(b,"NXIR",4)||b[4]!=3)return fail("invalid format/version");size_t pos=5;uint16_t w=u16(b+pos);pos+=2;uint16_t h=u16(b+pos);pos+=2;uint16_t tl=u16(b+pos);pos+=2;if(w<240||w>1600||h<180||h>1000||tl==0||tl>120||pos+tl+2>n)return fail("invalid window");p->width=w;p->height=h;memcpy(p->title,b+pos,tl);p->title[tl]=0;pos+=tl;uint16_t count=u16(b+pos);pos+=2;if(count>MAX_WIDGETS+MAX_ACTIONS+16)return fail("too many records");for(int r=0;r<count;r++){if(pos+3>n)return fail("truncated record");uint8_t kind=b[pos++];uint16_t len=u16(b+pos);pos+=2;if(pos+len>n)return fail("truncated payload");const uint8_t*d=b+pos;pos+=len;if(kind==1){if(len<10||p->nw>=MAX_WIDGETS)return fail("invalid widget");Widget*e=&p->widgets[p->nw++];e->type=d[0];e->id=d[1];e->x=(int16_t)u16(d+2);e->y=(int16_t)u16(d+4);e->w=(int16_t)u16(d+6);e->h=(int16_t)u16(d+8);size_t t=len-10;if(t>MAX_TEXT||e->type<1||e->type>5||e->w<1||e->h<1)return fail("invalid widget data");memcpy(e->text,d+10,t);e->text[t]=0;}else if(kind==2){if(len<4||p->na>=MAX_ACTIONS)return fail("invalid action");Action*a=&p->actions[p->na++];a->id=d[0];a->length=u16(d+1);if(a->length>MAX_CODE||a->length!=(size_t)len-3)return fail("invalid action length");memcpy(a->code,d+3,a->length);if(!validate_action(a))return 0;}else if(kind==3){if(len!=9||d[0]>=16)return fail("invalid variable");p->vars[d[0]]=i64(d+1);}else return fail("unknown record");}if(pos!=n)return fail("trailing bytes");return 1;}
static int64_t parse_input(uint8_t id,int*ok){const char*s=inputs[id];if(!*s){*ok=0;return 0;}int64_t v=0;for(;*s;s++){if(*s<'0'||*s>'9'||v>(INT64_MAX-9)/10){*ok=0;return 0;}v=v*10+(*s-'0');}return v;}
static int execute(Program*p,const Action*a){
    int64_t stack[32];int sp=0;size_t pc=0;int64_t vars[16];
    char pending_status[160]="Ready";uint8_t clear_ids[256]={0};
    memcpy(vars,p->vars,sizeof(vars));
    for(int steps=0;steps<MAX_STEPS&&pc<a->length;steps++){
        uint8_t op=a->code[pc++],arg=0;int64_t x,y;
        if(op==OP_LOAD||op==OP_INPUT||op==OP_STORE||op==OP_STATUS||op==OP_CLEAR)arg=a->code[pc++];
        switch(op){
        case OP_LOAD:stack[sp++]=vars[arg];break;
        case OP_INPUT:{int ok=1;int64_t v=parse_input(arg,&ok);if(!ok){strcpy(status,"Invalid input");return 0;}stack[sp++]=v;break;}
        case OP_PUSH:stack[sp++]=i64(a->code+pc);pc+=8;break;
        case OP_ADD:y=stack[--sp];x=stack[--sp];if(__builtin_add_overflow(x,y,&stack[sp])){strcpy(status,"Arithmetic overflow");return 0;}sp++;break;
        case OP_SUB:y=stack[--sp];x=stack[--sp];if(__builtin_sub_overflow(x,y,&stack[sp])){strcpy(status,"Arithmetic overflow");return 0;}sp++;break;
        case OP_EQ:y=stack[--sp];x=stack[--sp];stack[sp++]=(x==y);break;
        case OP_JZ:{int16_t offset=(int16_t)u16(a->code+pc);pc+=2;if(!stack[--sp])pc=(size_t)((int)pc+offset);break;}
        case OP_JMP:{int16_t offset=(int16_t)u16(a->code+pc);pc=(size_t)((int)pc+2+offset);break;}
        case OP_CHECK_POS:if(stack[sp-1]<=0){strcpy(status,"Amount must be positive");return 0;}break;
        case OP_CHECK_LE:y=stack[--sp];x=stack[--sp];if(x>y){strcpy(status,"Insufficient balance / limit");return 0;}break;
        case OP_STORE:vars[arg]=stack[--sp];break;
        case OP_STATUS:snprintf(pending_status,sizeof(pending_status),"%s",arg==1?"Approved":arg==2?"Calculated":arg==3?"Enabled":arg==4?"Disabled":"Done");break;
        case OP_CLEAR:clear_ids[arg]=1;break;
        case OP_HALT:memcpy(p->vars,vars,sizeof(vars));strcpy(status,pending_status);for(int i=0;i<256;i++)if(clear_ids[i])inputs[i][0]=0;return 1;
        }
    }
    strcpy(status,"Execution budget exceeded");return 0;
}
static void draw(Display*d,Window win,GC gc,int screen,const Program*p){XSetForeground(d,gc,WhitePixel(d,screen));XFillRectangle(d,win,gc,0,0,p->width,p->height);XSetForeground(d,gc,BlackPixel(d,screen));for(int i=0;i<p->nw;i++){const Widget*e=&p->widgets[i];char buf[200];const char*s=e->text;if(e->type==4){snprintf(buf,sizeof(buf),"%s: %lld.%02lld",s,(long long)(p->vars[e->id]/100),(long long)llabs(p->vars[e->id]%100));s=buf;}if(e->type==3){XDrawRectangle(d,win,gc,e->x,e->y,e->w,e->h);s=inputs[e->id];}if(e->type==2){XDrawRectangle(d,win,gc,e->x,e->y,e->w,e->h);}if(e->type==5){s=status;}int tx=e->x+((e->type==2||e->type==3)?10:0);int ty=e->y+((e->type==2||e->type==3)?e->h/2+5:15);XDrawString(d,win,gc,tx,ty,s,(int)strlen(s));}XFlush(d);}
int main(int argc,char**argv){if(argc==5&&!strcmp(argv[1],"--run-seq")){Program p={0};if(!load(argv[2],&p))return 2;int id=atoi(argv[3]),times=atoi(argv[4]);if(times<1||times>100)return 5;for(int i=0;i<times;i++){int found=0;for(int j=0;j<p.na;j++)if(p.actions[j].id==id){found=1;if(!execute(&p,&p.actions[j]))return 4;break;}if(!found)return 5;printf("step=%d status=%s var0=%lld\n",i+1,status,(long long)p.vars[0]);}return 0;}if(argc==4&&!strcmp(argv[1],"--run")){Program p={0};if(!load(argv[2],&p))return 2;int id=atoi(argv[3]);for(int j=0;j<p.na;j++)if(p.actions[j].id==id){int ok=execute(&p,&p.actions[j]);printf("ok=%d status=%s",ok,status);for(int k=0;k<16;k++)printf(" var%d=%lld",k,(long long)p.vars[k]);puts("");return ok?0:4;}return 5;}if(argc==3&&!strcmp(argv[1],"--validate")){Program p={0};if(!load(argv[2],&p))return 2;printf("VALID: %s (%d widgets, %d actions)\n",p.title,p.nw,p.na);return 0;}if(argc!=2){fprintf(stderr,"Usage: %s [--validate|--run file.nx action_id] file.nx\n",argv[0]);return 1;}Program p={0};if(!load(argv[1],&p))return 2;Display*d=XOpenDisplay(NULL);if(!d){fprintf(stderr,"No X11 display; use --validate for headless testing\n");return 3;}int screen=DefaultScreen(d);Window win=XCreateSimpleWindow(d,RootWindow(d,screen),50,50,p.width,p.height,1,BlackPixel(d,screen),WhitePixel(d,screen));XStoreName(d,win,p.title);XSelectInput(d,win,ExposureMask|KeyPressMask|ButtonPressMask|StructureNotifyMask);Atom wm_delete=XInternAtom(d,"WM_DELETE_WINDOW",False);XSetWMProtocols(d,win,&wm_delete,1);GC gc=XCreateGC(d,win,0,NULL);XMapWindow(d,win);int running=1;while(running){XEvent ev;XNextEvent(d,&ev);if(ev.type==ClientMessage&&(Atom)ev.xclient.data.l[0]==wm_delete)break;if(ev.type==KeyPress){KeySym key;char chars[16];int n=XLookupString(&ev.xkey,chars,sizeof(chars),&key,NULL);if(key==XK_Escape){running=0;continue;}if(focused){size_t len=strlen(inputs[focused]);if(key==XK_BackSpace&&len)inputs[focused][len-1]=0;else if(n==1&&chars[0]>='0'&&chars[0]<='9'&&len<18){inputs[focused][len]=chars[0];inputs[focused][len+1]=0;}}}if(ev.type==ButtonPress){int x=ev.xbutton.x,y=ev.xbutton.y;for(int i=0;i<p.nw;i++){Widget*e=&p.widgets[i];if(x>=e->x&&x<=e->x+e->w&&y>=e->y&&y<=e->y+e->h){if(e->type==3)focused=e->id;if(e->type==2){for(int j=0;j<p.na;j++)if(p.actions[j].id==e->id)execute(&p,&p.actions[j]);}break;}}}if(ev.type==Expose||ev.type==KeyPress||ev.type==ButtonPress)draw(d,win,gc,screen,&p);}XFreeGC(d,gc);XDestroyWindow(d,win);XCloseDisplay(d);return 0;}
