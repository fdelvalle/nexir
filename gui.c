#define main nexir_headless_main
#include "runtime.c"
#undef main
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include "game_vm.h"

static Display *D; static Window W; static GC G; static XFontStruct *font;
static unsigned long color(const char *hex){ XColor c; Colormap cm=DefaultColormap(D,DefaultScreen(D)); XParseColor(D,cm,hex,&c); XAllocColor(D,cm,&c);return c.pixel; }
static unsigned long bg, panel, sidebar, fg, muted, accent, line, white, good, bad, inputbg;
static int ww=960,wh=650,page=0,dark=1; static Program p;
static void rect(int x,int y,int w,int h,unsigned long c){if(w<1||h<1)return;XSetForeground(D,G,c);XFillRectangle(D,W,G,x,y,w,h);}
static void border(int x,int y,int w,int h,unsigned long c){XSetForeground(D,G,c);XDrawRectangle(D,W,G,x,y,w,h);}
static void txt(int x,int y,const char*s,unsigned long c){XSetForeground(D,G,c);XDrawString(D,W,G,x,y,s,(int)strlen(s));}
static void palette(){bg=color(dark?"#0B1120":"#F3F6FB");panel=color(dark?"#172237":"#FFFFFF");sidebar=color(dark?"#101A2C":"#E8EEF8");fg=color(dark?"#F1F5FA":"#17243B");muted=color(dark?"#9BAAC2":"#64748B");accent=color("#527BFA");line=color(dark?"#2A3A52":"#DAE2ED");white=color("#FFFFFF");good=color("#21C994");bad=color("#F27880");inputbg=color(dark?"#0C1729":"#F6F8FC");}
static int keyleft=0,keyright=0;
static int cx(){return 224;} static int cw(){return ww-cx()-28;}
static void nav(int index,int y,const char *name){rect(15,y,193,44,page==index?accent:sidebar);txt(33,y+27,name,page==index?white:muted);}
static void heading(const char *name,const char *sub){txt(cx()+24,62,name,fg);txt(cx()+24,88,sub,muted);}
static void widget_geometry(const Widget *e,int *x,int *y,int *w,int *h){int avail=cw()-70;double scale=(double)avail/(p.width-40);if(scale>1.25)scale=1.25;if(scale<0.5)scale=0.5;*x=cx()+44+(int)((e->x-25)*scale);*y=160+(int)(e->y*scale);*w=(int)(e->w*scale);*h=(int)(e->h*scale);}
static unsigned long rgb(uint32_t c){char hex[12];snprintf(hex,sizeof hex,"#%06X",c&0xFFFFFF);return color(hex);}
static void game_render(void){
 int x=cx()+24,y=112,w=cw()-48,h=wh-160;if(w<200||h<200)return;
 rect(x,y,w,h,rgb(gp.land));
 for(int opi=0;opi<gp.nd;opi++)switch(gp.draw[opi].op){
 case GOP_DRAW_RIVER:
 for(int j=0;j<h;j+=6){float yy=(float)j/h;float c=river_center(yy,game.frame);int mid=x+(int)(c*w);int half=(int)(gp.river_width*w/900.f/2);rect(mid-half,y+j,half*2,7,rgb(gp.water));if(j%42==0){rect(mid-half-4,y+j,4,18,color("#D8C37C"));rect(mid+half,y+j,4,18,color("#D8C37C"));}}break;
 case GOP_DRAW_OBJECTS:
 for(int i=0;i<32;i++)if(hazards[i].alive){GObject*o=&hazards[i];int ox=x+(int)(o->x*w),oy=y+(int)(o->y*h);unsigned long hc=o->kind==5?color("#55E69A"):rgb(gp.enemy);rect(ox-11,oy-10,22,20,hc);rect(ox-4,oy-15,8,30,hc);}
 for(int i=0;i<16;i++)if(bullets[i].alive){int bx=x+(int)(bullets[i].x*w),by=y+(int)(bullets[i].y*h);rect(bx-2,by-9,4,14,color("#FFF19A"));}break;
 case GOP_DRAW_PLAYER:{int px=x+(int)(game.x*w),py=y+(int)(game.y*h);rect(px-7,py-24,14,45,rgb(gp.player));rect(px-24,py-1,48,10,color("#F99C55"));rect(px-12,py+14,24,10,rgb(gp.player));}break;
 case GOP_DRAW_HUD:{char buf[160];rect(x+8,y+8,w-16,34,color("#101A2C"));snprintf(buf,sizeof buf,"SCORE %06d   LIVES %d   FUEL %d%%",game.score,game.lives,(int)game.fuel);txt(x+20,y+31,buf,color("#F1F5FA"));if(!game.playing||game.paused){rect(x+35,y+h/2-40,w-70,88,color("#101A2C"));txt(x+55,y+h/2-8,game.paused?"PAUSED":"GAME OVER",color("#FFFFFF"));txt(x+55,y+h/2+20,"R: restart  |  P: pause",color("#9BAAC2"));}}break;
 }
}
static void render(){palette();rect(0,0,ww,wh,bg);rect(0,0,224,wh,sidebar);rect(223,0,1,wh,line);
 txt(28,45,"NEXIR",fg);txt(28,67,"BINARY APPLICATION ENGINE",muted);
 txt(28,115,"WORKSPACE",muted);nav(0,131,"Dashboard");nav(1,184,"Application");nav(2,237,"River Flight");nav(3,290,"About runtime");
 rect(18,wh-104,190,1,line);txt(28,wh-79,"NEXIR 0.6",fg);txt(28,wh-54,"[T] Switch theme",muted);txt(28,wh-29,"[Esc] Exit",muted);
 if(page==0){heading("Dashboard","Your native application overview");int x=cx()+24,w=cw()-48;rect(x,112,w,142,panel);txt(x+25,151,"ACTIVE PROGRAM",muted);txt(x+25,184,p.title,fg);txt(x+25,220,"Binary .nx loaded  |  X11 renderer",muted);
 int gap=14,card=(w-gap)/2;rect(x,272,card,108,panel);rect(x+card+gap,272,card,108,panel);char buf[128];snprintf(buf,sizeof(buf),"%d widgets",p.nw);txt(x+18,314,buf,fg);txt(x+18,345,"Interface elements",muted);snprintf(buf,sizeof(buf),"%d actions",p.na);txt(x+card+gap+18,314,buf,fg);txt(x+card+gap+18,345,"Binary VM programs",muted);
 rect(x,399,w,114,panel);txt(x+22,433,"QUICK START",muted);txt(x+22,466,"Open Application to interact with the program.",fg);txt(x+22,492,"Press F1-F4 for pages. F3 opens River Raid.",muted);
 }else if(page==1){heading(p.title,"Native UI generated from NEXIR binary widgets");int x=cx()+24,w=cw()-48;rect(x,111,w,wh-162,panel);rect(x,111,w,4,accent);
 for(int i=0;i<p.nw;i++){const Widget *e=&p.widgets[i];int xx,yy,ew,eh;widget_geometry(e,&xx,&yy,&ew,&eh);if(yy+eh>wh-52)continue;char buf[240];const char *s=e->text;
 if(e->type==4){if(strstr(p.title,"Toggle"))snprintf(buf,sizeof(buf),"%s: %s",s,p.vars[e->id]?"ON":"OFF");else snprintf(buf,sizeof(buf),"%s: %lld.%02lld",s,(long long)(p.vars[e->id]/100),(long long)llabs(p.vars[e->id]%100));s=buf;}
 if(e->type==3){rect(xx,yy,ew,eh,inputbg);border(xx,yy,ew,eh,focused==e->id?accent:line);s=inputs[e->id][0]?inputs[e->id]:"Type an amount...";txt(xx+12,yy+eh/2+5,s,inputs[e->id][0]?fg:muted);}
 else if(e->type==2){rect(xx,yy,ew,eh,accent);txt(xx+16,yy+eh/2+5,s,white);}
 else if(e->type==5){rect(xx,yy,ew,eh+6,inputbg);txt(xx+12,yy+eh/2+5,status,!strcmp(status,"Ready")?muted:(strstr(status,"Invalid")||strstr(status,"Insufficient")||strstr(status,"exceeded")||strstr(status,"positive")?bad:good));}
 else if(e->type==4){txt(xx,yy+22,s,fg);}else txt(xx,yy+17,s,muted);
 }
 }else if(page==2){heading("River Flight","Arrow keys / A D: steer  |  Space: fire  |  P: pause  |  R: restart");game_render();}else{heading("About NEXIR","Native runtime and binary VM");int x=cx()+24,w=cw()-48;rect(x,112,w,335,panel);txt(x+24,154,"NEXIR 0.6  |  Desktop shell",fg);txt(x+24,196,"Runtime: C11 + Xlib",muted);txt(x+24,227,"Application: .nx binary bytecode (v3 compatible)",muted);txt(x+24,258,"Navigation: Dashboard / Application / River Raid / About",muted);txt(x+24,289,"Keyboard: F1-F4, T, Esc",muted);txt(x+24,332,"VM actions run with bounds and overflow checks.",fg);txt(x+24,368,"Data is local; no network access is used.",muted);}
 rect(cx(),wh-37,ww-cx(),37,sidebar);txt(cx()+24,wh-14,"NEXIR 0.6  /  Local native runtime",muted);XFlush(D);
}
static void click(int x,int y){if(x<224){focused=0;if(y>=131&&y<175)page=0;else if(y>=184&&y<228)page=1;else if(y>=237&&y<281)page=2;else if(y>=290&&y<334)page=3;return;}if(page!=1)return;for(int i=0;i<p.nw;i++){Widget *e=&p.widgets[i];int xx,yy,ew,eh;widget_geometry(e,&xx,&yy,&ew,&eh);if(x>=xx&&x<=xx+ew&&y>=yy&&y<=yy+eh){if(e->type==3)focused=e->id;if(e->type==2){for(int j=0;j<p.na;j++)if(p.actions[j].id==e->id){execute(&p,&p.actions[j]);break;}}break;}}}
int main(int argc,char **argv){if((argc!=2&&argc!=3)||!strcmp(argv[1],"--help")){fprintf(stderr,"Usage: %s file.nx [game.nxg]\n",argv[0]);return argc==2?0:1;}if(!load(argv[1],&p))return 2; if(!game_load(argc==3?argv[2]:"river.nxg")){fprintf(stderr,"Missing or invalid game bytecode\n");return 4;}game_reset();D=XOpenDisplay(NULL);if(!D){fprintf(stderr,"Cannot open X11 display. Run under WSLg or X11.\n");return 3;}int screen=DefaultScreen(D);W=XCreateSimpleWindow(D,RootWindow(D,screen),30,30,ww,wh,0,BlackPixel(D,screen),BlackPixel(D,screen));XStoreName(D,W,"NEXIR 0.6 | Native Workspace");XSelectInput(D,W,ExposureMask|KeyPressMask|KeyReleaseMask|ButtonPressMask|StructureNotifyMask);Atom del=XInternAtom(D,"WM_DELETE_WINDOW",False);XSetWMProtocols(D,W,&del,1);G=XCreateGC(D,W,0,NULL);font=XLoadQueryFont(D,"9x15");if(font)XSetFont(D,G,font->fid);XMapWindow(D,W);int running=1;while(running){while(XPending(D)){XEvent e;XNextEvent(D,&e);if(e.type==ClientMessage&&(Atom)e.xclient.data.l[0]==del)break;if(e.type==ConfigureNotify){ww=e.xconfigure.width;wh=e.xconfigure.height;if(ww<600)ww=600;if(wh<460)wh=460;}if(e.type==ButtonPress)click(e.xbutton.x,e.xbutton.y);if(e.type==KeyPress){KeySym key;char chars[16];int n=XLookupString(&e.xkey,chars,sizeof(chars),&key,NULL);if(key==XK_Escape)running=0;else if(key==XK_F1||(key==XK_1&&(!focused||page!=1)))page=0;else if(key==XK_F2||(key==XK_2&&(!focused||page!=1)))page=1;else if(key==XK_F3)page=2;else if(key==XK_F4)page=3;else if(page==2&&(key==XK_Left||key==XK_a))keyleft=1;else if(page==2&&(key==XK_Right||key==XK_d))keyright=1;else if(page==2&&key==XK_space)game_fire();else if(page==2&&key==XK_p)game.paused=!game.paused;else if(page==2&&key==XK_r)game_reset();else if(key==XK_t&&(!focused||page!=1))dark=!dark;else if(page==1&&focused){size_t len=strlen(inputs[focused]);if(key==XK_BackSpace&&len)inputs[focused][len-1]=0;else if(n==1&&chars[0]>='0'&&chars[0]<='9'&&len<18){inputs[focused][len]=chars[0];inputs[focused][len+1]=0;}}}if(e.type==KeyRelease){KeySym k=XLookupKeysym(&e.xkey,0);if(k==XK_Left||k==XK_a)keyleft=0;if(k==XK_Right||k==XK_d)keyright=0;}if(e.type==Expose||e.type==ButtonPress||e.type==KeyPress||e.type==ConfigureNotify)render();}if(page==2){game_tick(keyleft,keyright);render();}struct timeval delay={0,16000};select(0,NULL,NULL,NULL,&delay);}if(font)XFreeFont(D,font);XFreeGC(D,G);XDestroyWindow(D,W);XCloseDisplay(D);return 0;}
