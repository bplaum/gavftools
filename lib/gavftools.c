#include <string.h>
#include <signal.h>

#include <gavftools.h>
#include <gmerlin/pluginregistry.h>
#include <gmerlin/utils.h>

char * gavftools_src_location = NULL;
char * gavftools_dst_location = NULL;


/* Source */
bg_media_source_t * gavftools_src = NULL;
bg_plugin_handle_t * gavftools_input_handle = NULL;
bg_input_plugin_t * gavftools_input_plugin = NULL;

/* Sink */
gavf_writer_t * gavftools_writer = NULL;

/*
 *  Actions are strings containg:
 * 
 *  r: Read stream
 *  d: Decode stream
 *  m: Mute stream
 *  Default: d for message streams, r for all other streams
 */

char * gavftools_as_actions = NULL;
char * gavftools_vs_actions = NULL;
char * gavftools_ts_actions = NULL;
char * gavftools_os_actions = NULL;


void gavftools_init()
  {
  bg_handle_sigint();
  signal(SIGPIPE, SIG_IGN);
  
  /* Create global config registry */
  //  bg_cfg_registry = gavl_dictionary_create();
  bg_plugins_init();
  
  }

/* Initialise source */

static bg_stream_action_t get_action(const char * mode,
                                     int stream)
  {
  bg_stream_action_t ret = BG_STREAM_ACTION_READRAW;

  if(mode)
    {
    int len = strlen(mode);
    if(stream >= len)
      stream = len - 1;
    switch(mode[stream])
      {
      case 'd':
        ret = BG_STREAM_ACTION_DECODE;
        break;
      case 'r':
        ret = BG_STREAM_ACTION_READRAW;
        break;
      case 'm':
        ret = BG_STREAM_ACTION_OFF;
        break;
      }
    }
  return ret;
  }

static void set_stream_modes(gavl_stream_type_t type, const char * modes)
  {
  int i, num;
  num = gavl_track_get_num_streams(gavftools_src->track, type);

  for(i = 0; i < num; i++)
    bg_media_source_set_stream_action(gavftools_src, type, i, get_action(modes, i));
  }

static int wait_for_message(gavl_msg_t * msg)
  {
  gavl_msg_init(msg);
  while(1)
    {
    if(gavf_writer_read_message(gavftools_writer, msg, 100))
      return 1;
    }
  }

int gavftools_init_src()
  {
  gavl_msg_t msg;
  if(!gavftools_src_location)
    gavftools_src_location = GAVF_PROTOCOL"://-";

  if(gavftools_writer)
    {
    /* TODO: Interactive opening */
    
    gavl_dictionary_t * mi;
    mi = bg_plugin_registry_load_media_info(bg_plugin_reg, gavftools_src_location, 0);
    gavf_writer_send_media_info(gavftools_writer, mi);
    /* Wait for select track */
    while(wait_for_message(&msg))
      {
      if((msg.NS == GAVL_MSG_NS_SRC) &&
         (msg.ID == GAVL_CMD_SRC_SELECT_TRACK))
        {
        break;
        }
      }
    /* Set up source */
    
    /* Wait for start */
    
    }
  else /* No commands expected, let's initialize the source ourselves */
    {
    gavl_dictionary_t track;
    int num_variants = 0;
    gavl_dictionary_init(&track);
    gavl_track_from_location(&track, gavftools_src_location);
    if(!(gavftools_input_handle = bg_load_track(&track, 0, &num_variants)))
      {
      gavl_dictionary_free(&track);
      return 0;
      }
    gavl_dictionary_free(&track);
    gavftools_input_plugin = (bg_input_plugin_t*)gavftools_input_handle->plugin;

    gavftools_src = gavftools_input_plugin->get_src(gavftools_input_handle->priv);
    
    set_stream_modes(GAVL_STREAM_AUDIO,   gavftools_as_actions);
    set_stream_modes(GAVL_STREAM_VIDEO,   gavftools_vs_actions);
    set_stream_modes(GAVL_STREAM_TEXT,    gavftools_ts_actions);
    set_stream_modes(GAVL_STREAM_OVERLAY, gavftools_os_actions);
    }
  
  bg_input_plugin_start(gavftools_input_handle);
  
  return 1;
  }

/* Call before opening the input */
int gavftools_open_sink()
  {
  gavftools_writer = gavf_writer_create();
  if(!gavf_writer_open(gavftools_writer, gavftools_dst_location))
    return 0;

  
  
  return 1;
  }

int gavftools_init_sink(bg_media_source_t * src)
  {
  int i, num_streams = 0;
  
  if(!gavf_writer_init(gavftools_writer, src))
    {
    return 0;
    }
  /* TODO: Initialize connector */

  for(i = 0; i < src->num_streams; i++)
    {
    if(src->streams[i]->action != BG_STREAM_ACTION_OFF)
      num_streams++;
    }
  
  
  return 1;
  }

int gavftools_handle_sink_message(void * data, gavl_msg_t * msg)
  {
  switch(msg->NS)
    {
    case GAVL_MSG_NS_SRC:

      switch(msg->ID)
        {
        case GAVL_CMD_SRC_SELECT_TRACK:
          
          break;
        case GAVL_CMD_SRC_SEEK:
          break;
        case GAVL_CMD_SRC_START:
          break;
        case GAVL_CMD_SRC_PAUSE:
          break;
        case GAVL_CMD_SRC_RESUME:
          break;
        }
      break;
      
    }
  return 1;
  }
