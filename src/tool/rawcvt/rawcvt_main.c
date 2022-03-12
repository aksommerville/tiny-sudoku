#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "tool/common/tool_context.h"

/* Main.
 */
 
int main(int argc,char **argv) {
  struct tool_context ctx={0};
  if (tool_context_configure(&ctx,argc,argv,0)<0) return 1;
  if (tool_context_acquire_input(&ctx)<0) return 1;
  if (tool_context_encode_text(&ctx,ctx.src,ctx.srcc,8)<0) return 1;
  if (tool_context_flush_output(&ctx)<0) return 1;
  return 0;
}
