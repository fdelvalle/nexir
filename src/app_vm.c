#include "app.h"
#include "app_binary.h"
#include <stdio.h>
#include <string.h>

#define MAX_STEPS 256

char inputs[256][32];
char status[160] = "Ready";
uint8_t focused;

static int64_t parse_input(uint8_t id,int*ok){const char*s=inputs[id];if(!*s){*ok=0;return 0;}int64_t v=0;for(;*s;s++){if(*s<'0'||*s>'9'||v>(INT64_MAX-9)/10){*ok=0;return 0;}v=v*10+(*s-'0');}return v;}
int app_execute(Program*p,const Action*a){
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
