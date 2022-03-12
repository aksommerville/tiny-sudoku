#include "linux_internal.h"
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

/* Globals.
 */
 
#if BC_USE_pulse
  struct pulse *pulse=0;
#endif

#if BC_USE_x11
  struct x11 *x11=0;
#endif

#if BC_USE_evdev
  struct evdev *evdev=0;
#endif

static volatile int sigc=0;
static uint8_t input=0;

/* Clock.
 */

static double now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}
static int framec=0;
static double starttime=0.0;

uint32_t millis() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (tv.tv_sec*1000)+tv.tv_usec/1000;
}

uint32_t micros() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (tv.tv_sec*1000000)+tv.tv_usec;
}

/* Signal handler.
 */
 
static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++sigc>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* PCM callback.
 */
 
#if BC_USE_pulse
  static void linux_cb_pcm(int16_t *v,int c,struct pulse *pulse) {
    for (;c-->0;v++) *v=audio_next();
  }
#endif

/* X11 callbacks.
 */
 
#if BC_USE_x11
  static int linux_cb_x11_button(struct x11 *x11,uint8_t btnid,int value) {
    if (value) input|=btnid;
    else input&=~btnid;
    return 0;
  }
  
  static int linux_cb_x11_close(struct x11 *x11) {
    sigc=1;
    return 0;
  }
#endif

/* Evdev callback.
 */
 
#if BC_USE_evdev
  static int linux_cb_evdev_button(struct evdev *evdev,uint8_t btnid,int value) {
    if (btnid==EVDEV_BTN_ALTEVENT) switch (value) {
      case EVDEV_ALTEVENT_QUIT: sigc=1; break;
      case EVDEV_ALTEVENT_FULLSCREEN: {
        #if BC_USE_x11
          x11_set_fullscreen(x11,-1);
        #endif
        } break;
    } else {
      if (value) input|=btnid;
      else input&=~btnid;
    }
    return 0;
  }
#endif

/* Init.
 */
 
uint8_t platform_init() {

  #if BC_USE_pulse
    if (!(pulse=pulse_new(22050,1,linux_cb_pcm,0,"Sudoku"))) {
      fprintf(stderr,"Failed to initialize PulseAudio.\n");
      // Not fatal.
    }
  #endif
  
  #if BC_USE_x11
    if (!(x11=x11_new("Sudoku",96,64,0,linux_cb_x11_button,linux_cb_x11_close,0))) {
      fprintf(stderr,"Failed to initialize X11.\n");
      return -1;
    }
  #endif
  
  #if BC_USE_evdev
    if (!(evdev=evdev_new(linux_cb_evdev_button,0))) {
      fprintf(stderr,"Failed to initialize evdev.\n");
      // Not fatal.
    }
  #endif

  return 0;
}

/* Update.
 */
 
uint8_t platform_update() {
  #if BC_USE_x11
    if (x11_update(x11)<0) return 0;
  #endif
  #if BC_USE_evdev
    if (evdev_update(evdev)<0) return 0;
  #endif
  return input;
}

/* Receive framebuffer.
 */
 
void platform_send_framebuffer(const void *fb) {
  #if BC_USE_x11
    x11_swap(x11,fb);
  #endif
}

/* Quit.
 */
 
static void quit() {
  #if BC_USE_pulse
    pulse_del(pulse);
    pulse=0;
  #endif
  #if BC_USE_x11
    x11_del(x11);
    x11=0;
  #endif
  #if BC_USE_evdev
    evdev_del(evdev);
    evdev=0;
  #endif
}

/* Main.
 */
 
int main(int argc,char **argv) {
  signal(SIGINT,rcvsig);
  setup();
  starttime=now();
  while (!sigc) {
    framec++;
    usleep(16000);//TODO we could get more precise about timing
    loop();
  }
  if (framec) {
    double endtime=now();
    double elapsed=endtime-starttime;
    double average=framec/elapsed;
    fprintf(stderr,"Average frame rate over %.0fs: %.03f Hz\n",average);
  }
  quit();
  return 0;
}
