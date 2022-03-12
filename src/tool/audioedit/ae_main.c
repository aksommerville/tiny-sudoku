#include "ae_internal.h"
#include <signal.h>
#include <unistd.h>

/* Signal handler.
 */
 
static volatile int sigc=0;

static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++sigc>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Main.
 */
 
int main(int argc,char **argv) {
  signal(SIGINT,rcvsig);
  struct audioedit *ae=audioedit_new(argc,argv);
  if (!ae) {
    fprintf(stderr,"%s: Initialization failed.\n",argv[0]);
    return 1;
  }
  fprintf(stderr,"%s: Running. SIGINT to quit.\n",ae->exename);
  while (!sigc) {
    if (audioedit_update(ae)<0) {
      audioedit_del(ae);
      return 1;
    }
  }
  audioedit_del(ae);
  fprintf(stderr,"%s: Normal exit.\n",ae->exename);
  return 0;
}
