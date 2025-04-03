#include <string.h>
#include <signal.h>

#include <gavftools.h>
#include <gmerlin/pluginregistry.h>
#include <gmerlin/utils.h>

char * gavftools_src_location = NULL;
char * gavftools_dst_location = NULL;
int gavftoools_flags = 0;

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

int num_gavftools_streams = 0;
gavftools_stream_t * gavftools_streams = NULL;

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
  int result;
  gavl_msg_init(msg);
  while(1)
    {
    result = gavf_writer_read_gavf_message(gavftools_writer, msg, 100);
    
    if(result < 0)
      return 0;
    
    else if(result > 0)
      return 1;
    }
  
  }

int gavftools_init_src()
  {
  gavl_msg_t msg;
  int done;
  
  if(!gavftools_src_location)
    gavftools_src_location = GAVF_PROTOCOL"://-";

  /* Interactive opening */
  if(gavftoools_flags & GAVLTOOLS_OUT_BACKCHANNEL)
    {
    gavl_dictionary_t * mi;
    mi = bg_plugin_registry_load_media_info(bg_plugin_reg, gavftools_src_location, 0);
    gavf_writer_send_media_info(gavftools_writer, mi);
    /* Wait for select track */
    while(wait_for_message(&msg))
      {
      if((msg.NS == GAVL_MSG_NS_SRC) &&
         (msg.ID == GAVL_CMD_SRC_SELECT_TRACK))
        {
        int track_idx;
        int num_variants;
        char * real_uri = gavl_strdup(gavftools_src_location);
        gavl_dictionary_t track;
        gavl_dictionary_init(&track);
        
        track_idx = gavl_msg_get_arg_int(&msg, 0);
        //        fprintf(stderr, "Got select track %d\n", track_idx);
        
        if(gavl_get_num_tracks(mi) > 1)
          {
          gavl_dictionary_t vars;
          gavl_dictionary_init(&vars);
          gavl_url_get_vars(real_uri, &vars);
          gavl_dictionary_set_int(&vars, GAVL_URL_VAR_TRACK, track_idx+1);

          real_uri = gavl_url_append_vars(real_uri, &vars);
          gavl_dictionary_free(&vars);
          
          }

        gavl_track_from_location(&track, gavftools_src_location);
        
        if(!(gavftools_input_handle = bg_load_track(&track, 0, &num_variants)))
          {
          gavl_dictionary_free(&track);
          free(real_uri);
          return 0;
          }
        
        free(real_uri);
        gavl_dictionary_free(&track);

        gavftools_input_plugin = (bg_input_plugin_t*)gavftools_input_handle->plugin;
        gavftools_src = gavftools_input_plugin->get_src(gavftools_input_handle->priv);
        break;
        }
      
      }

    if(!gavftools_input_plugin)
      return 0;
      
    /* Set up source */
    done = 0;
    while(wait_for_message(&msg))
      {
      if((msg.NS == GAVL_MSG_NS_SRC) &&
         (msg.ID == GAVL_CMD_SRC_SET_STREAM_ACTION))
        {
        gavl_stream_type_t type;
        int idx;
        int action;
        bg_media_source_stream_t * st;
        
        
        type   = gavl_msg_get_arg_int(&msg, 0);
        idx    = gavl_msg_get_arg_int(&msg, 1);
        action = gavl_msg_get_arg_int(&msg, 2);

        fprintf(stderr, "Setting stream action: %d %d %d\n", type, idx, action);

        st = bg_media_source_get_stream(gavftools_src, type, idx);
        if(action)
          st->action = BG_STREAM_ACTION_READRAW;
        else
          st->action = BG_STREAM_ACTION_OFF;
        }
      
      if((msg.NS == GAVL_MSG_NS_SRC) &&
         (msg.ID == GAVL_CMD_SRC_START))
        {
        fprintf(stderr, "Got start\n");
        done = 1;
        break;
        }
      }

    if(!done)
      return 0;
    
    bg_input_plugin_start(gavftools_input_handle);

    gavl_dictionary_dump(gavftools_src->track, 2);
    
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
    gavftools_input_plugin = (bg_input_plugin_t*)gavftools_input_handle->plugin;
    gavftools_src = gavftools_input_plugin->get_src(gavftools_input_handle->priv);
    
    set_stream_modes(GAVL_STREAM_AUDIO,   gavftools_as_actions);
    set_stream_modes(GAVL_STREAM_VIDEO,   gavftools_vs_actions);
    set_stream_modes(GAVL_STREAM_TEXT,    gavftools_ts_actions);
    set_stream_modes(GAVL_STREAM_OVERLAY, gavftools_os_actions);
    
    gavl_dictionary_free(&track);
    bg_input_plugin_start(gavftools_input_handle);
    }
  
  return 1;
  }

/* Call before opening the input */
int gavftools_open_sink()
  {
  gavftools_writer = gavf_writer_create();
  if(!gavf_writer_open(gavftools_writer, gavftools_dst_location))
    return 0;

  if(gavf_writer_has_backchannel(gavftools_writer))
    gavftoools_flags |= GAVLTOOLS_OUT_BACKCHANNEL;
  
  return 1;
  }

static int process_stream_audio(gavftools_stream_t * s)
  {
  gavl_audio_frame_t * aframe = gavl_audio_sink_get_frame(s->asink);
  gavl_source_status_t src_st;
  
  src_st = gavl_audio_source_read_frame(s->src->asrc, &aframe);
  if(src_st == GAVL_SOURCE_EOF)
    return 0;
  
  if(gavl_audio_sink_put_frame(s->asink, aframe) != GAVL_SINK_OK)
    return 0;

  if(s->time_scaled == GAVL_TIME_UNDEFINED)
    s->time_scaled = aframe->timestamp;
  s->time_scaled += aframe->valid_samples;
  
  return 1;
  }

static int process_stream_video(gavftools_stream_t * s)
  {
  gavl_video_frame_t * vframe = gavl_video_sink_get_frame(s->vsink);
  gavl_source_status_t src_st;
  int64_t pts;
  
  src_st = gavl_video_source_read_frame(s->src->vsrc, &vframe);
  if(src_st == GAVL_SOURCE_EOF)
    return 0;
  else if(src_st == GAVL_SOURCE_AGAIN)
    return 1;

  pts = vframe->timestamp;
  if(vframe->duration > 0)
    pts += vframe->duration;
  
  if(s->time_scaled == GAVL_TIME_UNDEFINED)
    s->time_scaled = pts;
  else if(pts > s->time_scaled)
    s->time_scaled = pts;
  
  if(gavl_video_sink_put_frame(s->vsink, vframe) != GAVL_SINK_OK)
    return 0;
  
  return 1;
  }

#if 0
static int process_stream_message(gavftools_stream_t * s)
  {
  return 1;
  }
#endif

static int process_stream_packet(gavftools_stream_t * s)
  {
  gavl_packet_t * packet = gavl_packet_sink_get_packet(s->psink);
  gavl_source_status_t src_st;
  int64_t pts;

  src_st = gavl_packet_source_read_packet(s->src->vsrc, &packet);
  if(src_st == GAVL_SOURCE_EOF)
    return 0;
  else if(src_st == GAVL_SOURCE_AGAIN)
    return 1;

  pts = packet->pts;
  if(packet->duration > 0)
    pts += packet->duration;

  if(s->time_scaled == GAVL_TIME_UNDEFINED)
    s->time_scaled = pts;
  else if(pts > s->time_scaled)
    s->time_scaled = pts;
  
  if(gavl_packet_sink_put_packet(s->psink, packet) != GAVL_SINK_OK)
    return 0;
  
  return 1;
  }

int gavftools_init_sink(bg_media_source_t * src)
  {
  int i = 0;
  int idx;
  
  if(!gavf_writer_init(gavftools_writer, src))
    {
    return 0;
    }
  
  /* Initialize connector */
  for(i = 0; i < src->num_streams; i++)
    {
    if(src->streams[i]->action != BG_STREAM_ACTION_OFF)
      num_gavftools_streams++;
    }
  gavftools_streams = calloc(num_gavftools_streams, sizeof(*gavftools_streams));
  idx = 0;
  
  for(i = 0; i < src->num_streams; i++)
    {
    if(src->streams[i]->action == BG_STREAM_ACTION_OFF)
      continue;
    gavftools_streams[idx].src = src->streams[i];

    if((gavftools_streams[idx].asink = gavf_writer_get_audio_sink(gavftools_writer, idx)))
      {
      const gavl_audio_format_t * fmt =  gavl_audio_sink_get_format(gavftools_streams[idx].asink);

      gavftools_streams[idx].process = process_stream_audio;
      gavftools_streams[idx].timescale = fmt->samplerate;
      }
    else if((gavftools_streams[idx].vsink = gavf_writer_get_video_sink(gavftools_writer, idx)))
      {
      const gavl_video_format_t * fmt =  gavl_video_sink_get_format(gavftools_streams[idx].vsink);
      
      gavftools_streams[idx].process = process_stream_video;
      gavftools_streams[idx].timescale = fmt->timescale;
      
      }
    else if((gavftools_streams[idx].msink = gavf_writer_get_message_sink(gavftools_writer, idx)))
      {
      if(!gavl_dictionary_get_int(gavl_stream_get_metadata(gavftools_streams[idx].src->s),
                                  GAVL_META_STREAM_SAMPLE_TIMESCALE,
                                  &gavftools_streams[idx].timescale))
        {
        gavftools_streams[idx].timescale = GAVL_TIME_SCALE;
        }
      }
    else
      {
      gavftools_streams[idx].psink = gavf_writer_get_packet_sink(gavftools_writer, idx);
      gavftools_streams[idx].process = process_stream_packet;

      if(!gavl_dictionary_get_int(gavl_stream_get_metadata(gavftools_streams[idx].src->s),
                                  GAVL_META_STREAM_SAMPLE_TIMESCALE,
                                  &gavftools_streams[idx].timescale))
        {
        gavftools_streams[idx].timescale = GAVL_TIME_SCALE;
        }
      }
    
    gavftools_streams[idx].time = GAVL_TIME_UNDEFINED;
    gavftools_streams[idx].time_scaled = GAVL_TIME_UNDEFINED;
    gavftools_streams[idx].discont = !gavl_stream_is_continuous(src->streams[i]->s);
    idx++;
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


int gavltools_iteration_st()
  {
  int i;
  gavl_time_t min_time = GAVL_TIME_UNDEFINED;
  
  for(i = 0; i < num_gavftools_streams; i++)
    {
    if(gavftools_streams[i].process)
      continue;
    
    if(!gavftools_streams[i].discont &&
       (gavftools_streams[i].time_scaled == GAVL_TIME_UNDEFINED))
      {
      if(!gavftools_streams[i].process(&gavftools_streams[i]))
        return 0;

      gavftools_streams[i].time =
        gavl_time_unscale(gavftools_streams[i].timescale,
                          gavftools_streams[i].time_scaled);
      
      }
    else if(gavftools_streams[i].discont)
      {
      if(!gavftools_streams[i].process(&gavftools_streams[i]))
        return 0;

      if(gavftools_streams[i].time_scaled != GAVL_TIME_UNDEFINED)
        {
        gavftools_streams[i].time =
          gavl_time_unscale(gavftools_streams[i].timescale,
                            gavftools_streams[i].time_scaled);
        
        }
      }
    }
  
  /* Get stream with smallest timestamp */
  for(i = 0; i < num_gavftools_streams; i++)
    {
    if(gavftools_streams[i].discont)
      continue;

    if((min_time == GAVL_TIME_UNDEFINED) &&
       (gavftools_streams[i].time != GAVL_TIME_UNDEFINED))
      {
      
      }
    
    }
    
  
  return 1;
  }
