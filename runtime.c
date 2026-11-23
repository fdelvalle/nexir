#include "src/app.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int number(const char *text, int max)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    return errno || end == text || *end || value < 0 || value > max ? -1 : (int)value;
}

int app_cli(int argc,char**argv){if(argc==5&&!strcmp(argv[1],"--run-seq")){Program p={0};if(!app_load(argv[2],&p))return 2;int id=number(argv[3], 255),times=number(argv[4], 100);if(times<1||times>100)return 5;for(int i=0;i<times;i++){int found=0;for(int j=0;j<p.na;j++)if(p.actions[j].id==id){found=1;if(!app_execute(&p,&p.actions[j]))return 4;break;}if(!found)return 5;printf("step=%d status=%s var0=%lld\n",i+1,status,(long long)p.vars[0]);}return 0;}if(argc==4&&!strcmp(argv[1],"--run")){Program p={0};if(!app_load(argv[2],&p))return 2;int id=number(argv[3], 255);for(int j=0;j<p.na;j++)if(p.actions[j].id==id){int ok=app_execute(&p,&p.actions[j]);printf("ok=%d status=%s",ok,status);for(int k=0;k<16;k++)printf(" var%d=%lld",k,(long long)p.vars[k]);puts("");return ok?0:4;}return 5;}if(argc==3&&!strcmp(argv[1],"--validate")){Program p={0};if(!app_load(argv[2],&p))return 2;printf("VALID: %s (%d widgets, %d actions)\n",p.title,p.nw,p.na);return 0;}return -1;}
