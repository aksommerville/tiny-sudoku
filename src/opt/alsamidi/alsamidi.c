#include "alsamidi.h"
#include <alsa/asoundlib.h>

struct alsamidi {
  snd_rawmidi_t *rawmidi;
};

/* Delete.
 */
 
void alsamidi_del(struct alsamidi *alsamidi) {
  if (!alsamidi) return;
  
  if (alsamidi->rawmidi) {
    snd_rawmidi_close(alsamidi->rawmidi);
  }
  
  free(alsamidi);
}

/* New.
 */
 
struct alsamidi *alsamidi_new() {
  struct alsamidi *alsamidi=calloc(1,sizeof(struct alsamidi));
  if (!alsamidi) return 0;
  
  if (snd_rawmidi_open(&alsamidi->rawmidi,0,"virtual",SND_RAWMIDI_NONBLOCK)<0) {
    fprintf(stderr,"snd_rawmidi_open failed\n");
    alsamidi_del(alsamidi);
    return 0;
  }
  
  return alsamidi;
}

/* Get fd.
 */
 
int alsamidi_get_fd(struct alsamidi *alsamidi) {
  if (!alsamidi) return -1;
  
  int fdc=snd_rawmidi_poll_descriptors_count(alsamidi->rawmidi);
  if (fdc<1) return -1;
  
  struct pollfd pollfd={0};
  int err=snd_rawmidi_poll_descriptors(alsamidi->rawmidi,&pollfd,1);
  if (err<1) return -1;
  return pollfd.fd;
}

/* Update.
 */
 
int alsamidi_update(void *dst,int dsta,struct alsamidi *alsamidi) {
  return snd_rawmidi_read(alsamidi->rawmidi,dst,dsta);
}
