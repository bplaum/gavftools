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
    .flags = BG_CMDLINE_ARG_STRING,         \
  },                                        \
  {                                         \
    .arg = "-m",                            \
    .help_arg = "key=val",                  \
    .help_string = TRS("Set metadata key (can be used multiple times)"), \
    .flags = BG_CMDLINE_ARG_PARAM,         \
  }

#define GAVFTOOLS_OPT_DST                   \
  {                                         \
    .arg = "-o",                            \
    .help_arg = "<location>",               \
    .help_string = TRS("Destination"),      \
    .flags = BG_CMDLINE_ARG_STRING,         \
  }

#define GAVFTOOLS_OUT_BACKCHANNEL (1<<0)
#define GAVFTOOLS_MULTI_THREAD    (1<<1)
#define GAVFTOOLS_OUT_LOCAL       (1<<2)

extern int gavftools_flags;


extern bg_media_source_t * gavftools_src;
extern bg_plugin_handle_t * gavftools_input_handle;

extern bg_media_source_t gavftools_encoder;

/* Codec options */
extern char * gavftools_ac_options;
extern char * gavftools_vc_options;
extern char * gavftools_oc_options;

#define STREAM_DISCONT         (1<<0)
#define STREAM_HAVE_SINK_FRAME (1<<1)
#define STREAM_HAVE_SRC_FRAME  (1<<2)
#define STREAM_B_FRAMES        (1<<3)

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


void gavftools_set_stream_actions(void);


void gavftools_init(void);
int gavftools_open_sink(void);

int gavftools_init_src(void);
int gavftools_open_src(void);

int gavftools_init_sink(bg_media_source_t * src);
int gavftools_handle_sink_message(gavl_msg_t * msg);

/* Set metadata from commandline */
int gavftools_set_metadata(gavl_dictionary_t * m);

void gavftools_cleanup(void);

// gavl_source_status_t gavftools_iteration_singlethread(void * data);
gavl_source_status_t gavftools_iteration_multithread(void * data);

void gavftools_start(void);
void gavftools_stop(void);

void gavftools_run(void);

