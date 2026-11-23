"""NEXIR game binary assembler; build a game from opcode sequences."""
import struct
from pathlib import Path

OPS = dict(STEER=1, BANK_COLLISION=2, SPAWN=3, MOVE=4, COLLISIONS=5, FUEL=6,
           DRAW_RIVER=32, DRAW_OBJECTS=33, DRAW_PLAYER=34, DRAW_HUD=35)

def build(path, *, width=330, speed=4, spawn=42, fuel=1, seed=12345,
          colors=(0x173C29, 0x146AA1, 0x173C29, 0xE5EFFC, 0xF97373),
          tick=('STEER','BANK_COLLISION','SPAWN','MOVE','COLLISIONS','FUEL'),
          draw=('DRAW_RIVER','DRAW_OBJECTS','DRAW_PLAYER','DRAW_HUD')):
    if len(tick)>64 or len(draw)>64: raise ValueError('too many instructions')
    b=bytearray(b'NXG6'+bytes([1,0,0,0]))
    b+=struct.pack('<5I',width,speed,spawn,fuel,seed)
    b+=struct.pack('<5I',*colors)
    b+=bytes([len(tick),len(draw),0,0])
    for name in (*tick,*draw): b+=struct.pack('<Bi',OPS[name],0)
    Path(path).write_bytes(b)
    return len(b)

if __name__=='__main__':
    print('river.nxg:',build('river.nxg'),'bytes')
    print('river_fast.nxg:',build('river_fast.nxg',width=390,speed=7,spawn=24,fuel=2,
         colors=(0x16243A,0x116DA5,0x284A32,0xFFE39A,0xFF7D6B)),'bytes')
    print('river_training.nxg:',build('river_training.nxg',width=440,speed=2,spawn=90,
         tick=('STEER','SPAWN','MOVE','COLLISIONS'),
         colors=(0x13382A,0x1683C1,0x13382A,0xFFFFFF,0xFF7D6B)),'bytes')
