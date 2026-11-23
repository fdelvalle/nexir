#include "app.h"
#include "app_binary.h"
#include <stdio.h>
#include <string.h>

#define MAX_FILE 65536

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
    for (size_t at = 0; at < a->length; ++at) {
        if (!boundary[at] || (a->code[at] != OP_JZ && a->code[at] != OP_JMP)) continue;
        int target = (int)at + 3 + (int16_t)u16(a->code + at + 1);
        if (target <= (int)at || target >= (int)a->length || !boundary[target])
            return fail("invalid forward jump");
    }
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
static int load_records(const char*path,Program*p){FILE*f=fopen(path,"rb");if(!f){perror(path);return 0;}uint8_t b[MAX_FILE+1];size_t n=fread(b,1,sizeof(b),f);int read_error=ferror(f);fclose(f);if(read_error)return fail("cannot read file");if(n>MAX_FILE)return fail("file too large");if(n<13||memcmp(b,"NXIR",4)||b[4]!=3)return fail("invalid format/version");size_t pos=5;uint16_t w=u16(b+pos);pos+=2;uint16_t h=u16(b+pos);pos+=2;uint16_t tl=u16(b+pos);pos+=2;if(w<240||w>1600||h<180||h>1000||tl==0||tl>120||pos+tl+2>n)return fail("invalid window");p->width=w;p->height=h;memcpy(p->title,b+pos,tl);p->title[tl]=0;pos+=tl;uint16_t count=u16(b+pos);pos+=2;if(count>MAX_WIDGETS+MAX_ACTIONS+16)return fail("too many records");for(int r=0;r<count;r++){if(pos+3>n)return fail("truncated record");uint8_t kind=b[pos++];uint16_t len=u16(b+pos);pos+=2;if(pos+len>n)return fail("truncated payload");const uint8_t*d=b+pos;pos+=len;if(kind==1){if(len<10||p->nw>=MAX_WIDGETS)return fail("invalid widget");Widget*e=&p->widgets[p->nw++];e->type=d[0];e->id=d[1];e->x=(int16_t)u16(d+2);e->y=(int16_t)u16(d+4);e->w=(int16_t)u16(d+6);e->h=(int16_t)u16(d+8);size_t t=len-10;if(t>MAX_TEXT||e->type<1||e->type>5||(e->type==4&&e->id>=16)||e->w<1||e->h<1)return fail("invalid widget data");memcpy(e->text,d+10,t);e->text[t]=0;}else if(kind==2){if(len<4||p->na>=MAX_ACTIONS)return fail("invalid action");Action*a=&p->actions[p->na++];a->id=d[0];a->length=u16(d+1);if(a->length>MAX_CODE||a->length!=(size_t)len-3)return fail("invalid action length");memcpy(a->code,d+3,a->length);if(!validate_action(a))return 0;}else if(kind==3){if(len!=9||d[0]>=16)return fail("invalid variable");p->vars[d[0]]=i64(d+1);}else return fail("unknown record");}if(pos!=n)return fail("trailing bytes");return 1;}

int app_load(const char *path, Program *program)
{
    Program candidate = {0};
    if (!load_records(path, &candidate)) return 0;
    *program = candidate;
    return 1;
}
