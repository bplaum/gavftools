#include <config.h>
#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gavf.h>

/* Code common to most commadline tools */
/* Since this is used for commandline tools
   exclusively, we make heavy use of global variables */

#define GAVFTOOLS_OPT_SRC \
  {                                         \
    .arg = "-i",                            \
    .help_arg = "<location>",               \
    .help_string = TRS("Source to decode"), \
    .argv = &gavftools_src_location,        \
  }

#define GAVFTOOLS_OPT_DST                   \
  {                                         \
    .arg = "-o",                            \
    .help_arg = "<location>",               \
    .help_string = TRS("Destination"),      \
    .argv = &gavftools_dst_location,        \
  }

#define GAVFTOOLS_OUT_BACKCHANNEL (1<<0)
#define GAVFTOOLS_MULTI_THREAD    (1<<1)
#define GAVFTOOLS_OUT_LOCAL       (1<<2)

extern int gavftools_flags;

extern char * gavftools_src_location;
extern char * gavftools_dst_location;

extern bg_media_source_t * gavftools_src;

/* Codec options */
extern char * gavftools_ac_options;
extern char * gavftools_vc_options;
extern char * gavftools_oc_options;

#define STREAM_DISCONT         (1<<0)
#define STREAM_HAVE_SINK_FRAME (1<<1)
#define STREAM_HAVE_SRC_FRAME  (1<<2)

#define THREAD_STATE_INIT     0
#define THREAD_STATE_RUNNING  1
#define THREAD_STATE_STOP     2
#define THREAD_STATE_EOF      3
#define THREAD_STATE_FINISHED 4

typedef struct
  {
  gavl_source_status_t (*process_func)(void * data);
  void * data;

  pthread_mutex_t mutex;
  int state;

  pthread_t thread;
  } gavftools_thread_t;

typedef struct gavftools_stream_s
  {
  bg_media_source_stream_t * src;

  gavl_audio_sink_t * asink;
  gavl_video_sink_t * vsink;
  gavl_packet_sink_t * psink;
  bg_msg_sink_t * msink;

  int timescale;
  gavl_time_t time;
  int64_t time_scaled;
  
  int flags;
  
  gavl_source_status_t last_status;
  /* Process one packet / frame */
  gavl_source_status_t (*process)(struct gavftools_stream_s * s);

  gavl_packet_t      * pkt;
  gavl_video_frame_t * vframe;

  gavftools_thread_t thread;
  
  } gavftools_stream_t;

extern int num_gavftools_streams;
extern gavftools_stream_t * gavftools_streams;

void gavftools_init(void);
int gavftools_open_sink(void);

int gavftools_init_src(void);
int gavftools_init_sink(bg_media_source_t * src);
int gavftools_handle_sink_message(gavl_msg_t * msg);


void gavftools_cleanup(void);

gavl_source_status_t gavftools_iteration_singlethread(void * data);
gavl_source_status_t gavftools_iteration_multithread(void * data);

void gavftools_start(void);
void gavftools_stop(void);

void gavftools_run(void);

