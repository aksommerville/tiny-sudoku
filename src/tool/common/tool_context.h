/* tool_context.h
 * Some shared top-level logic that I expect all tools to use.
 * Some assumptions:
 *  - Tool invocation requires one input file and one output file.
 *  - Wrapper logs all errors that it can detect.
 *  - Object name (tool_context.name) is the base name of the source file, until the first dot.
 */
 
#ifndef TOOL_CONTEXT_H
#define TOOL_CONTEXT_H

#include "decoder.h"

struct tool_context {
  const char *srcpath;
  const char *dstpath;
  const char *platform;
  const char *name;
  int namec;
  void *src;
  int srcc;
  struct encoder dst;
};

void tool_context_cleanup(struct tool_context *context);

/* If you implement cb_option, you will get every argument first.
 * You must return one of:
 *   <0 real error, abort
 *    0 ignored, allow general processing
 *   >0 processed, wrapper will skip it
 */
int tool_context_configure(
  struct tool_context *ctx,
  int argc,char **argv,
  int (*cb_option)(struct tool_context *ctx,const char *k,int kc,const char *v,int vc)
);

/* Read input file (populate (src,srcc) from (srcpath)).
 * If unset, generate (name,namec) from (srcpath).
 */
int tool_context_acquire_input(struct tool_context *ctx);

/* Overwrite (dst) with the canonical C text.
 * If you're going pure raw, (src,srcc) is (ctx->src,ctx->srcc).
 * (type) is (8,-8,16,-16,32,-32), and (srcc) is in words of that size.
 */
int tool_context_encode_text(struct tool_context *ctx,const void *src,int srcc,int type);

/* Write (dst) to (dstpath).
 */
int tool_context_flush_output(struct tool_context *ctx);

int tool_context_is_tiny(const struct tool_context *ctx);

#endif
