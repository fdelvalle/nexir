#include "game_vm.h"
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
int main(void){
 assert(game_load("river.nxg"));assert(gp.nt==6&&gp.nd==4);game_reset();assert(game.lives==3);
 game_fire();assert(game.shots==1);
 for(int i=0;i<100;i++)game_tick(0,0);
 assert(game.frame>0);printf("PASS: game bytecode loaded, tick=%" PRIu32 " score=%d fuel=%.1f\n",game.frame,game.score,game.fuel);
 assert(game_load("river_fast.nxg"));assert(gp.speed==7);printf("PASS: alternate program loaded\n");
 assert(game_load("river_training.nxg"));game_reset();for(int i=0;i<60;i++)game_tick(0,0);assert(game.fuel==100);puts("PASS: training bytecode disables fuel consumption");
 FILE *bad = fopen("invalid.nxg", "wb");
 assert(bad);assert(fwrite("NXG6",1,4,bad)==4);assert(fclose(bad)==0);
 assert(!game_load("invalid.nxg"));assert(gp.speed==2);
 assert(!game_load("missing.nxg"));puts("PASS: missing program rejected");
 return 0;
}
