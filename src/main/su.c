#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

uint32_t micros();

/* Generator context.
 */
 
struct su_generator {
  uint16_t possible[81]; // 0x1ff for each cell, which values remain possible
  uint8_t expose[81]; // 1 if a cell should be visible to the user
  uint8_t value[81]; // The final value 1..9 for each cell, 0 if we're generating it.
};

static void su_generator_cleanup(struct su_generator *g) {
}

/* Dump generator for troubleshooting.
 */
 
static void dump_generator(const struct su_generator *g) {
  fprintf(stderr,"---- begin dump\n");
  const uint8_t *src=g->value;
  uint8_t row=0; for (;row<9;row++) {
    uint8_t col=0; for (;col<9;col++,src++) {
      fprintf(stderr," %c",(*src)?(0x30+*src):'_');
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"----- possible\n");
  const uint8_t *expose=g->expose;
  const uint16_t *possible=g->possible;
  for (src=g->value,row=0;row<9;row++) {
    uint8_t col=0; for (;col<9;col++,src++,expose++,possible++) {
      if (*expose) fprintf(stderr," ---");
      else fprintf(stderr," %03x",(*possible)&0x1ff);
    }
    fprintf(stderr,"\n");
  }
}

/* Fire your callback for each neighbor of a given cell. (row,col,zone)
 * This will report some neighbors more than once.
 * It will NOT report the focus cell.
 */
 
static void su_for_each_neighbor(
  uint8_t col,uint8_t row,
  void (*cb)(uint8_t col,uint8_t row,void *userdata),
  void *userdata
) {
  //fprintf(stderr,"neighbors of %d,%d:\n",col,row);
  uint8_t i;
  for (i=0;i<9;i++) {
    if (i!=col) cb(i,row,userdata);
    if (i!=row) cb(col,i,userdata);
  }
  uint8_t col0=(col/3)*3;
  uint8_t row0=(row/3)*3;
  uint8_t suby=0; for (;suby<3;suby++) {
    uint8_t qrow=row0+suby;
    uint8_t subx=0; for (;subx<3;subx++) {
      uint8_t qcol=col0+subx;
      if ((qcol==col)&&(qrow==row)) continue;
      cb(qcol,qrow,userdata);
    }
  }
}
 
static void su_for_each_exposed_neighbor(
  uint8_t col,uint8_t row,
  const uint8_t *expose,
  void (*cb)(uint8_t col,uint8_t row,void *userdata),
  void *userdata
) {
  //fprintf(stderr,"neighbors of %d,%d:\n",col,row);
  uint8_t i;
  for (i=0;i<9;i++) {
    if ((i!=col)&&expose[row*9+i]) cb(i,row,userdata);
    if ((i!=row)&&expose[i*9+col]) cb(col,i,userdata);
  }
  uint8_t col0=(col/3)*3;
  uint8_t row0=(row/3)*3;
  uint8_t suby=0; for (;suby<3;suby++) {
    uint8_t qrow=row0+suby;
    uint8_t subx=0; for (;subx<3;subx++) {
      uint8_t qcol=col0+subx;
      if ((qcol==col)&&(qrow==row)) continue;
      if (!expose[qrow*9+qcol]) continue;
      cb(qcol,qrow,userdata);
    }
  }
}
 
static void su_for_each_unexposed_neighbor(
  uint8_t col,uint8_t row,
  const uint8_t *expose,
  void (*cb)(uint8_t col,uint8_t row,void *userdata),
  void *userdata
) {
  //fprintf(stderr,"neighbors of %d,%d:\n",col,row);
  uint8_t i;
  for (i=0;i<9;i++) {
    if ((i!=col)&&!expose[row*9+i]) cb(i,row,userdata);
    if ((i!=row)&&!expose[i*9+col]) cb(col,i,userdata);
  }
  uint8_t col0=(col/3)*3;
  uint8_t row0=(row/3)*3;
  uint8_t suby=0; for (;suby<3;suby++) {
    uint8_t qrow=row0+suby;
    uint8_t subx=0; for (;subx<3;subx++) {
      uint8_t qcol=col0+subx;
      if ((qcol==col)&&(qrow==row)) continue;
      if (expose[qrow*9+qcol]) continue;
      cb(qcol,qrow,userdata);
    }
  }
}

/* Get the nine positions (0..80) for a given axis.
 */
 
static void su_get_column_positions(uint8_t *dst,uint8_t v) {
  if (v>=9) return;
  uint8_t i=9;
  for (;i-->0;dst++,v+=9) *dst=v;
}

static void su_get_row_positions(uint8_t *dst,uint8_t v) {
  if (v>=9) return;
  v*=9;
  uint8_t i=9;
  for (;i-->0;dst++,v++) *dst=v;
}

static void su_get_zone_positions(uint8_t *dst,uint8_t v) {
  if (v>=9) return;
  uint8_t col=v%9,row=v/9;
  uint8_t majorcol=col/3,majorrow=row/3;
  col=majorcol*3;
  row=majorrow*3;
  dst[0]=row*9+col;
  dst[1]=dst[0]+1;
  dst[2]=dst[0]+2;
  dst[3]=dst[0]+9;
  dst[4]=dst[0]+10;
  dst[5]=dst[0]+11;
  dst[6]=dst[0]+18;
  dst[7]=dst[0]+19;
  dst[8]=dst[0]+20;
}

static void su_get_axis_positions(uint8_t *dst,uint8_t v) {
  if (v>=18) su_get_zone_positions(dst,v-18);
  else if (v>=9) su_get_row_positions(dst,v-9);
  else su_get_column_positions(dst,v);
}

/* Filter a list of positions.
 */
 
// Retain those where (ref[p]==0)
static uint8_t su_filter_positions_unset(uint8_t *v,uint8_t c,const uint8_t *ref) {
  uint8_t i=c;
  while (i-->0) {
    if (ref[v[i]]) {
      c--;
      memmove(v+i,v+i+1,c-i);
    }
  }
  return c;
}

/* Fill values randomly.
 * Nonzero on success, zero if we painted ourselves into a corner.
 */
 
struct su_generator_fill_ctx {
  const struct su_generator *g;
  uint8_t *optv;
  uint8_t optc;
};
 
static void su_generator_fill_options_neighbor(uint8_t col,uint8_t row,void *userdata) {
  struct su_generator_fill_ctx *ctx=userdata;
  uint8_t digit=ctx->g->value[row*9+col];
  //fprintf(stderr,"  %d,%d (%d)\n",col,row,digit);
  if ((digit<1)||(digit>9)) return;
  ctx->optv[digit-1]=0;
}
 
static uint8_t su_generator_get_fill_options(
  uint8_t *optv, // a=9
  const struct su_generator *g,
  uint8_t col,uint8_t row // 0..8
) {
  struct su_generator_fill_ctx ctx={
    .g=g,
    .optv=optv,
    .optc=0,
  };
  while (ctx.optc<9) { ctx.optv[ctx.optc]=1+ctx.optc; ctx.optc++; }
  su_for_each_neighbor(col,row,su_generator_fill_options_neighbor,&ctx);
  uint8_t i=9; while (i-->0) {
    if (ctx.optv[i]) continue;
    ctx.optc--;
    memmove(ctx.optv+i,ctx.optv+i+1,ctx.optc-i);
  }
  return ctx.optc;
}
 
static uint8_t su_generator_fill(struct su_generator *g) {
  memset(g->value,0,81);
  static const uint8_t path[81]={
    0x33,0x43,0x53,0x34,0x44,0x54,0x35,0x45,0x55, // middle zone
    0x03,0x13,0x23,0x04,0x14,0x24,0x05,0x15,0x25, // west zone
    0x63,0x73,0x83,0x64,0x74,0x84,0x65,0x75,0x85, // east zone
    0x30,0x40,0x50,0x31,0x41,0x51,0x32,0x42,0x52, // north zone
    0x36,0x46,0x56,0x37,0x47,0x57,0x38,0x48,0x58, // south zone
    0x00,0x10,0x20,0x01,0x11,0x21,0x02,0x12,0x22, // NW
    0x60,0x70,0x80,0x61,0x71,0x81,0x62,0x72,0x82, // NE
    0x06,0x16,0x26,0x07,0x17,0x27,0x08,0x18,0x28, // SW
    0x66,0x76,0x86,0x67,0x77,0x87,0x68,0x78,0x88, // SE
  };/**/
  const uint8_t *src=path;
  uint8_t i=81;
  for (;i-->0;src++) {
    uint8_t col=(*src)>>4;
    uint8_t row=(*src)&15;
    uint8_t optv[9];
    uint8_t optc=su_generator_get_fill_options(optv,g,col,row);
    if (!optc) {
      return 0;
    } else {
      g->value[row*9+col]=optv[rand()%optc];
    }
  }
  return 1;
}

/* Pick a hidden cell at random.
 * Zero if everything is exposed.
 */
 
static uint8_t su_generator_random_hidden_cell(
  uint8_t *col,uint8_t *row,
  const struct su_generator *g
) {
  uint8_t optc=0,i=81;
  uint8_t optv[81];
  const uint8_t *v=g->expose;
  for (;i-->0;v++) if (!*v) optv[optc++]=i;
  if (!optc) return 0;
  uint8_t revp=optv[rand()%optc];
  uint8_t p=80-revp;
  *col=p%9;
  *row=p/9;
  return 1;
}

/* Mark a cell exposed and update possible for all neighbors.
 */
 
struct su_expose_ctx {
  struct su_generator *g;
  uint16_t mask;
};

static void su_generator_expose_cell_cb(uint8_t col,uint8_t row,void *userdata) {
  struct su_expose_ctx *ctx=userdata;
  uint8_t p=row*9+col;
  ctx->g->possible[p]&=ctx->mask;
}
 
static void su_generator_expose_cell(struct su_generator *g,uint8_t col,uint8_t row) {
  uint8_t p=row*9+col;
  uint16_t bit=1<<(g->value[p]-1);
  g->expose[p]=1;
  g->possible[p]=bit;
  struct su_expose_ctx ctx={
    .g=g,
    .mask=~bit,
  };
  su_for_each_unexposed_neighbor(col,row,g->expose,su_generator_expose_cell_cb,&ctx);
}

/* Given a list of positions, detect subgroups and apply their restriction.
 * Nonzero if we change something.
 */
 
static uint8_t su_detect_and_apply_subgroups(
  struct su_generator *g,const uint8_t *pv,uint8_t pc
) {
  uint8_t result=0;
  uint8_t ai=pc;
  while (ai-->0) {
    uint16_t forbitten=~g->possible[pv[ai]];
    uint8_t subc=0; // how many of (a)'s peers contain no more possibilities than (a)
    uint8_t bi=pc;
    while (bi-->0) {
      // It's ok if (ai==bi); that creates our minimum count of 1.
      if (g->possible[pv[bi]]&forbitten) continue;
      subc++;
    }
    uint8_t bc=0;
    uint16_t q=g->possible[pv[ai]]&0x1ff;
    for (;q;q>>=1) if (q&1) bc++;
    if (subc==bc) {
      // There is a subgroup among the non-forbittens. Remove (a)'s full mask from all other neighbors.
      for (bi=pc;bi-->0;) {
        if (!(g->possible[pv[bi]]&forbitten)) continue;
        if (!(g->possible[pv[bi]]&~forbitten)) continue;
        g->possible[pv[bi]]&=forbitten;
        result=1;
      }
    }
  }
  return result;
}

/* Update exposure phase.
 * Return <0 if the puzzle became unsolvable (shouldn't be possible).
 * 0 if the puzzle is ready, >0 if we changed something.
 */
 
static int8_t su_generator_update_exposure(struct su_generator *g) {
  uint8_t result=0;
  
  /* If each cell is either exposed, or has just one "possible" bit, we're solved.
   * Shouldn't be possible, but if a "possible" reduces to zero, fail.
   */
  {
    result=1;
    const uint8_t *expose=g->expose;
    uint16_t *possible=g->possible;
    uint8_t i=81;
    for (;i-->0;expose++,possible++) {
      if (*expose) continue;
      uint16_t b=(*possible)&0x1ff;
      uint8_t c=0;
      for (;b;b>>=1) if (b&1) c++;
      if (c==1) continue;
      if (!c) return -1; // shouldn't happen
      result=0;
      break;
    }
    if (result) return 0;
  }

  /* Within each of the 27 axes, identify groups with restricted membership based on (g->possible).
   * Meaning like, if there's a row with 5 open cells, and two of them have only (1,2) possible, 
   * we can remove (1,2) from the possibilities of the other cells.
   * An edge case of this: When a cell has just one possible, we remove that one from its neighbors.
   */
  uint8_t i;
  uint8_t pv[9];
  for (i=0;i<27;i++) {
    su_get_axis_positions(pv,i);
    uint8_t pc=su_filter_positions_unset(pv,9,g->expose);
    if (su_detect_and_apply_subgroups(g,pv,pc)) result=1;
  }
  if (result) return 1;

  /* Nothing changed, so we need to expose one randomly and try again.
   */
  uint8_t col,row;
  su_generator_random_hidden_cell(&col,&row,g);
  su_generator_expose_cell(g,col,row);
  return 1;
}

/* For each cell, decide whether it should be exposed.
 * Rewrites (g->expose) from scratch.
 * Returns >0 on success or 0 if we trapped ourselves.
 */
 
static uint8_t su_generator_expose(struct su_generator *g) {
  memset(g->expose,0,81);
  memset(g->possible,0xff,162);
  
  /* TODO This algorithm is clearly not perfect.
   * The puzzles we generate are too easy, they have too much exposed.
   *TODO Instead of randomly exposing one, try exposing from among the least-possible, or most-possible?
   */
  
  /* Start by exposing 15 random cells.
   * I've heard that the minimum for a solvable puzzle is 17, fudging by 2 because I can't prove it.
   * XXX Should be (i=15). To make silly-easy puzzle during testing, bump it up (no more than 80)
   */
  uint8_t i=15; while (i-->0) {
    uint8_t col,row;
    su_generator_random_hidden_cell(&col,&row,g);
    su_generator_expose_cell(g,col,row);
  }
  
  /* Iteratively examine what's possible. If the puzzle isn't solved yet, expose something.
   */
  uint16_t repc=0;
  while (1) {
    repc++;
    int8_t err=su_generator_update_exposure(g);
    if (err<0) return 0;
    if (!err) {
      fprintf(stderr,"--- solved after %d repetitions\n",repc);
      return 1;
    }
  }
}

/* Print the output field.
 */
 
static void su_generator_print(uint16_t *dst,const struct su_generator *g) {
  uint8_t i=81;
  const uint16_t *src=g->possible;
  const uint8_t *expose=g->expose;
  const uint8_t *value=g->value;
  for (;i-->0;dst++,src++,expose++,value++) {
    uint16_t mask=*src;
    uint8_t digit=1;
    for (;mask;mask>>=1,digit++) if (mask&1) break;
    if (*expose) {
      *dst=0x0300|((*value)<<4)|(*value);
    } else {
      *dst=(*value)<<4;
    }
  }
}

/* Generate puzzle, main entry point.
 */
 
void sudoku_generate(uint16_t *v) {
 _again_:;
  struct su_generator g;
  int seed=micros();
  fprintf(stderr,"random seed %d\n",seed);
  srand(seed);
  
  uint16_t fillc=0;
  while (1) {
    fillc++;
    if (su_generator_fill(&g)) break;
  }
  fprintf(stderr,"filled in %d attempts\n",fillc);
  
  if (!su_generator_expose(&g)) {
    fprintf(stderr,"!!!!! failed in exposure phase\n");
    goto _again_;
  }
  
  //dump_generator(&g);
  su_generator_print(v,&g);
  su_generator_cleanup(&g);
}
