#ifndef NEXIR_GAME_VM_H
#define NEXIR_GAME_VM_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

enum {GOP_CLEAR=1,GOP_RIVER=2,GOP_SPAWN=3,GOP_MOVE=4,GOP_COLLIDE=5,GOP_FUEL=6,GOP_DRAW_RIVER=32,GOP_DRAW_OBJECTS=33,GOP_DRAW_PLAYER=34,GOP_DRAW_HUD=35};
typedef struct {uint8_t op;int32_t arg;} GInstruction;
typedef struct {int river_width,speed,spawn_interval,fuel_rate,seed;uint32_t sky,water,land,player,enemy;GInstruction tick[64],draw[64];int nt,nd;} GProgram;
typedef struct {float x,y;int kind,alive;} GObject;
typedef struct {float x,y;int alive;} GShot;
typedef struct {float x,y,fuel;int lives,score,playing,paused;uint32_t frame;int shots;} GState;
static GProgram gp;static GState game;static GObject hazards[32];static GShot bullets[16];static uint32_t grng=12345;
static uint32_t grand32(void){grng^=grng<<13;grng^=grng>>17;grng^=grng<<5;return grng;}
static int grand(int n){return n?(int)(grand32()%(uint32_t)n):0;}
static uint32_t gread32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int gvalid(uint8_t op,int draw){return draw?(op>=GOP_DRAW_RIVER&&op<=GOP_DRAW_HUD):(op>=GOP_CLEAR&&op<=GOP_FUEL);}
static int game_load(const char *path){FILE*f=fopen(path,"rb");if(!f)return 0;uint8_t b[1024];size_t n=fread(b,1,sizeof b,f);int extra=fgetc(f);int read_error=ferror(f);fclose(f);if(read_error||extra!=EOF||n<48||memcmp(b,"NXG6",4)||b[4]!=1)return 0;
 GProgram candidate={0};candidate.river_width=(int)gread32(b+8);candidate.speed=(int)gread32(b+12);candidate.spawn_interval=(int)gread32(b+16);candidate.fuel_rate=(int)gread32(b+20);candidate.seed=(int)gread32(b+24);
 candidate.sky=gread32(b+28);candidate.water=gread32(b+32);candidate.land=gread32(b+36);candidate.player=gread32(b+40);candidate.enemy=gread32(b+44);
 if(candidate.river_width<180||candidate.river_width>550||candidate.speed<1||candidate.speed>12||candidate.spawn_interval<10||candidate.spawn_interval>200||candidate.fuel_rate<1||candidate.fuel_rate>10)return 0;
 if(n<52)return 0;
 candidate.nt=b[48];candidate.nd=b[49];
 if(candidate.nt>64||candidate.nd>64||n!=52+(size_t)(candidate.nt+candidate.nd)*5)return 0;
 size_t off=52;for(int i=0;i<candidate.nt;i++,off+=5){candidate.tick[i]=(GInstruction){b[off],(int32_t)gread32(b+off+1)};if(!gvalid(b[off],0))return 0;}
 for(int i=0;i<candidate.nd;i++,off+=5){candidate.draw[i]=(GInstruction){b[off],(int32_t)gread32(b+off+1)};if(!gvalid(b[off],1))return 0;}gp=candidate;return 1;}
static float river_center(float y,uint32_t frame){return 0.5f+0.11f*sinf((y+frame*0.004f)*5.0f)+0.05f*sinf((y+frame*0.002f)*12.0f);}
static void game_reset(void){memset(&game,0,sizeof game);memset(hazards,0,sizeof hazards);memset(bullets,0,sizeof bullets);game.x=.5f;game.y=.80f;game.fuel=100;game.lives=3;game.playing=1;grng=gp.seed?gp.seed:12345;}
static void game_fire(void){if(!game.playing||game.paused)return;for(int i=0;i<16;i++)if(!bullets[i].alive){bullets[i]=(GShot){game.x,game.y-.055f,1};if(game.shots<INT_MAX)game.shots++;break;}}
static void game_add_score(int points){if(game.score<=INT_MAX-points)game.score+=points;}
static void game_tick(int left,int right){if(!game.playing||game.paused)return;game.frame++;float bank=gp.river_width/900.0f;
 for(int k=0;k<gp.nt;k++){GInstruction in=gp.tick[k];switch(in.op){
 case GOP_CLEAR: if(left)game.x-=.011f;if(right)game.x+=.011f;if(game.x<.03f)game.x=.03f;if(game.x>.97f)game.x=.97f;break;
 case GOP_RIVER:{float c=river_center(game.y,game.frame);if(fabsf(game.x-c)>bank/2-.035f){game.lives--;game.x=c;game.fuel-=8;if(game.lives<=0)game.playing=0;}}break;
 case GOP_SPAWN:if(game.frame%(unsigned)gp.spawn_interval==0)for(int i=0;i<32;i++)if(!hazards[i].alive){float c=river_center(.01f,game.frame);hazards[i]=(GObject){c+((float)grand(1000)/999.f-.5f)*(bank-.10f),.01f,1+grand(5),1};break;}break;
 case GOP_MOVE:for(int i=0;i<32;i++)if(hazards[i].alive){hazards[i].y+=.003f*gp.speed;if(hazards[i].y>1.05f){hazards[i].alive=0;game_add_score(2);}}for(int i=0;i<16;i++)if(bullets[i].alive){bullets[i].y-=.020f;if(bullets[i].y<0)bullets[i].alive=0;}break;
 case GOP_COLLIDE:for(int i=0;i<32;i++)if(hazards[i].alive){GObject*h=&hazards[i];if(fabsf(h->x-game.x)<.045f&&fabsf(h->y-game.y)<.05f){h->alive=0;if(h->kind==5){game.fuel=fminf(100,game.fuel+32);game_add_score(15);}else{game.lives--;game.fuel-=10;if(game.lives<=0)game.playing=0;}}}for(int i=0;i<16;i++)if(bullets[i].alive)for(int j=0;j<32;j++)if(hazards[j].alive&&fabsf(bullets[i].x-hazards[j].x)<.045f&&fabsf(bullets[i].y-hazards[j].y)<.045f){bullets[i].alive=0;if(hazards[j].kind!=5){hazards[j].alive=0;game_add_score(25);}break;}break;
 case GOP_FUEL:if(game.frame%12==0)game.fuel-=.1f*gp.fuel_rate;if(game.fuel<=0){game.fuel=0;game.playing=0;}break;
 }}}
#endif
