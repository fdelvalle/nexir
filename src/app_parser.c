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
static int parse_records(const uint8_t *bytes, size_t n, Program *p)
{
    if (n < 13 || memcmp(bytes, "NXIR", 4) || bytes[4] != 3)
        return fail("invalid format/version");
    uint16_t width = u16(bytes + 5), height = u16(bytes + 7);
    uint16_t title_length = u16(bytes + 9);
    if (width < 240 || width > 1600 || height < 180 || height > 1000 ||
        !title_length || title_length > MAX_TEXT || 13u + title_length > n)
        return fail("invalid window");
    p->width = width;
    p->height = height;
    memcpy(p->title, bytes + 11, title_length);
    uint16_t count = u16(bytes + 11 + title_length);
    if (count > MAX_WIDGETS + MAX_ACTIONS + 16) return fail("too many records");

    size_t pos = 13 + title_length;
    for (uint16_t i = 0; i < count; ++i) {
        if (n - pos < 3) return fail("truncated record");
        uint8_t kind = bytes[pos];
        uint16_t length = u16(bytes + pos + 1);
        pos += 3;
        if (n - pos < length) return fail("truncated payload");
        const uint8_t *data = bytes + pos;
        pos += length;
        switch (kind) {
        case 1: {
            if (length < 10 || p->nw >= MAX_WIDGETS) return fail("invalid widget");
            Widget *widget = &p->widgets[p->nw++];
            widget->type = data[0];
            widget->id = data[1];
            widget->x = (int16_t)u16(data + 2);
            widget->y = (int16_t)u16(data + 4);
            widget->w = (int16_t)u16(data + 6);
            widget->h = (int16_t)u16(data + 8);
            size_t text_length = length - 10;
            if (text_length > MAX_TEXT || widget->type < 1 || widget->type > 5 ||
                (widget->type == 4 && widget->id >= 16) ||
                widget->w < 1 || widget->h < 1) return fail("invalid widget data");
            memcpy(widget->text, data + 10, text_length);
            break;
        }
        case 2: {
            if (length < 4 || p->na >= MAX_ACTIONS) return fail("invalid action");
            Action *action = &p->actions[p->na++];
            action->id = data[0];
            action->length = u16(data + 1);
            if (action->length > MAX_CODE || action->length != length - 3)
                return fail("invalid action length");
            memcpy(action->code, data + 3, action->length);
            if (!validate_action(action)) return 0;
            break;
        }
        case 3:
            if (length != 9 || data[0] >= 16) return fail("invalid variable");
            p->vars[data[0]] = i64(data + 1);
            break;
        default:
            return fail("unknown record");
        }
    }
    return pos == n ? 1 : fail("trailing bytes");
}

int app_load(const char *path, Program *program)
{
    Program candidate = {0};
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return 0;
    }
    uint8_t bytes[MAX_FILE + 1];
    size_t count = fread(bytes, 1, sizeof bytes, file);
    int read_error = ferror(file);
    fclose(file);
    if (read_error) return fail("cannot read file");
    if (count > MAX_FILE) return fail("file too large");
    if (!parse_records(bytes, count, &candidate)) return 0;
    *program = candidate;
    return 1;
}
