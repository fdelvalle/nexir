#ifndef NX_APP_H
#define NX_APP_H
#include <stdint.h>

#define MAX_WIDGETS 64
#define MAX_ACTIONS 64
#define MAX_CODE 4096
#define MAX_TEXT 120
typedef struct { uint8_t type,id; int16_t x,y,w,h; char text[MAX_TEXT+1]; } Widget;
typedef struct {uint8_t id;uint16_t length;uint8_t code[MAX_CODE];} Action;
typedef struct {char title[121];int width,height;int64_t vars[16];Widget widgets[MAX_WIDGETS];int nw;Action actions[MAX_ACTIONS];int na;} Program;
extern char inputs[256][32];
extern char status[160];
extern uint8_t focused;

int app_load(const char *path, Program *program);
int app_execute(Program *program, const Action *action);
int app_cli(int argc, char **argv);
int app_window(Program *program);
int app_workspace(int argc, char **argv);
#endif
