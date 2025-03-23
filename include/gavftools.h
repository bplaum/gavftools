#include <config.h>
#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gavf.h>

/* Code common to most commadline tools */

#define GAVFTOOLS_OPT_SRC \
  {                                         \
    .arg = "-i",                            \
    .help_arg = "<location>",               \
    .help_string = TRS("Source to decode"), \
    .argv = &gavftools_src_location,        \
  }, \
  { \
    .arg = "-as",                            \
    .help_arg = "<options>",               \
      .help_string = TRS("Audio modes: A string of r (read packets), d (decode) or m (mute)"), \
    .argv = &gavftools_as_actions,        \
  },              \
  { \
    .arg = "-vs",                            \
    .help_arg = "<options>",               \
    .help_string = TRS("Video modes: A string of r (read packets), d (decode) or m (mute)"), \
    .argv = &gavftools_vs_actions,        \
  },              \
  { \
    .arg = "-ts",                            \
    .help_arg = "<options>",               \
     .help_string = TRS("Text modes: A string of r (read packets) or m (mute)"), \
    .argv = &gavftools_ts_actions,        \
  },              \
  { \
    .arg = "-os",                            \
    .help_arg = "<options>",               \
     .help_string = TRS("Overlay stream modes: A string of r (read packets), d (decode) or m (mute)"), \
    .argv = &gavftools_ts_actions,        \
  }

#define GAVFTOOLS_OPT_DST                   \
  {                                         \
    .arg = "-o",                            \
    .help_arg = "<location>",               \
    .help_string = TRS("Destination"),      \
    .argv = &gavftools_dst_location,        \
  }


extern char * gavftools_src_location;
extern char * gavftools_dst_location;

extern bg_media_source_t * gavftools_src;

extern char * gavftools_as_actions;
extern char * gavftools_vs_actions;
extern char * gavftools_ts_actions;
extern char * gavftools_os_actions;

typedef struct
  {
  bg_media_source_stream_t * src;

  gavl_audio_sink_t * asink;
  gavl_video_sink_t * vsink;
  gavl_packet_sink_t * psink;

  int timescale;
  gavl_time_t time;
  int discont;
  } gavftools_stream_t;

void gavftools_init();
int gavftools_open_sink();

int gavftools_init_src();
int gavftools_init_sink(bg_media_source_t * src);

int gavftools_handle_sink_message(void * data, gavl_msg_t * msg);
