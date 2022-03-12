#include <stdint.h>
const uint8_t tiny_ctab8[768]={
/* Determined by comparing TA 8-bit to TA 16-bit: */
#define _(r,g,b) 0x##r,0x##g,0x##b,
//         x0         x1         x2         x3         x4         x5         x6         x7         x8         x9         xa         xb         xc         xd         xe         xf
/*0x*/_(00,00,00)_(45,00,00)_(ba,00,00)_(ff,00,00)_(00,22,00)_(45,20,00)_(b8,24,00)_(ff,24,00)_(00,40,00)_(45,41,00)_(ba,40,00)_(ff,40,00)_(00,60,00)_(45,60,00)_(ba,60,00)_(ff,5d,00)
/*1x*/_(00,98,00)_(45,9a,00)_(bd,9d,00)_(ff,9a,00)_(00,bc,00)_(45,b8,00)_(ba,be,00)_(ff,be,00)_(00,e0,00)_(4d,db,00)_(bf,db,00)_(ff,e3,00)_(00,ff,00)_(4d,ff,00)_(b8,ff,00)_(ff,ff,00)
/*2x*/_(00,00,20)_(45,00,24)_(ba,00,24)_(ff,00,24)_(00,24,24)_(45,20,24)_(bc,20,28)_(ff,24,24)_(00,41,24)_(41,42,24)_(ba,44,24)_(ff,41,24)_(00,62,22)_(42,62,22)_(bf,60,24)_(ff,65,1c)
/*3x*/_(00,a2,24)_(45,9a,24)_(bc,9a,24)_(ff,9a,24)_(00,be,24)_(45,be,24)_(be,b8,24)_(ff,be,24)_(00,db,24)_(4d,db,24)_(bc,dc,24)_(ff,db,24)_(00,ff,24)_(4d,ff,24)_(c0,ff,24)_(ff,ff,24)
/*4x*/_(00,00,41)_(45,00,41)_(ba,00,41)_(ff,00,41)_(00,1c,41)_(45,1c,41)_(ba,1c,41)_(ff,24,41)_(00,40,40)_(40,40,40)_(ba,41,41)_(ff,40,40)_(00,5d,41)_(45,60,40)_(b8,5e,44)_(ff,5d,41)
/*5x*/_(00,9f,41)_(40,9c,40)_(bc,9c,44)_(ff,9a,41)_(00,be,41)_(45,be,41)_(bd,bc,45)_(ff,be,41)_(00,db,49)_(3d,e3,41)_(ba,d8,40)_(ff,dc,44)_(00,ff,49)_(4d,ff,41)_(b8,ff,40)_(ff,ff,41)
/*6x*/_(00,00,65)_(40,00,65)_(ba,00,65)_(ff,00,65)_(00,24,65)_(45,1c,65)_(ba,1c,65)_(ff,24,60)_(00,41,65)_(45,41,65)_(ba,41,65)_(ff,41,65)_(00,60,60)_(40,65,60)_(bc,62,65)_(ff,60,60)
/*7x*/_(00,a2,65)_(45,9c,65)_(ba,9c,65)_(ff,9c,65)_(00,be,65)_(45,be,65)_(b8,be,65)_(ff,be,65)_(00,db,6d)_(45,db,65)_(bc,dc,65)_(ff,dc,64)_(00,ff,6d)_(45,ff,60)_(ba,ff,65)_(ff,ff,65)
/*8x*/_(00,00,9a)_(40,00,98)_(ba,00,9a)_(ff,18,98)_(00,24,9a)_(40,24,9a)_(ba,24,9a)_(ff,28,98)_(00,49,92)_(45,41,9a)_(ba,44,9a)_(ff,44,98)_(00,65,92)_(45,60,98)_(ba,65,9a)_(ff,5d,9a)
/*9x*/_(00,9a,9a)_(45,9a,9a)_(a0,9c,9c)_(ff,98,98)_(00,be,92)_(4c,b8,9a)_(ba,be,9a)_(ff,be,92)_(00,db,9a)_(4b,d8,98)_(ba,e3,9a)_(ff,db,92)_(00,ff,92)_(45,ff,a4)_(ba,ff,9a)_(ff,ff,9a)
/*ax*/_(00,1e,b6)_(40,00,be)_(ba,20,b6)_(ff,00,b6)_(00,24,b6)_(40,28,be)_(ba,24,b6)_(ff,24,b6)_(00,49,b6)_(40,44,b8)_(ba,44,b8)_(ff,44,b8)_(00,65,b6)_(45,65,b4)_(ba,6d,b6)_(ff,64,b8)
/*bx*/_(00,9a,be)_(40,98,be)_(ba,9a,be)_(ff,9a,be)_(00,be,be)_(4a,ba,bb)_(ba,be,be)_(ff,bc,bc)_(00,e3,be)_(45,e3,be)_(bc,de,ba)_(ff,dc,b8)_(00,ff,b6)_(45,ff,be)_(ba,ff,b6)_(ff,ff,be)
/*cx*/_(00,00,db)_(40,10,db)_(ba,00,db)_(ff,14,db)_(00,24,db)_(40,2c,d7)_(ba,24,db)_(ff,24,db)_(00,47,d7)_(45,41,db)_(ba,41,db)_(ff,49,db)_(00,65,d3)_(45,65,db)_(ba,65,db)_(ff,65,db)
/*dx*/_(00,9a,e3)_(45,9a,db)_(ba,a2,db)_(ff,a2,db)_(00,be,db)_(40,be,db)_(ba,be,db)_(ff,b6,db)_(00,db,db)_(44,db,db)_(ba,db,db)_(f8,de,de)_(00,ff,db)_(4d,ff,db)_(ba,ff,db)_(ff,ff,db)
/*ex*/_(00,00,ff)_(44,00,ff)_(ba,00,ff)_(ff,00,ff)_(00,24,ff)_(40,24,ff)_(ba,24,ff)_(ff,24,ff)_(00,41,ff)_(44,44,ff)_(ba,49,ff)_(ff,40,ff)_(00,65,ff)_(42,5c,ff)_(ba,65,ff)_(ff,65,ff)
/*fx*/_(00,9a,ff)_(48,9a,ff)_(ba,a0,ff)_(ff,a0,ff)_(00,b6,ff)_(4d,b6,ff)_(ba,c4,ff)_(ff,c4,ff)_(00,db,ff)_(45,db,ff)_(b8,e0,ff)_(ff,e4,ff)_(00,ff,ff)_(4d,ff,ff)_(ba,ff,ff)_(ff,ff,ff)
/**/
};
