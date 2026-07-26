#include <string.h>
#include <signal.h>

#include <gavftools.h>
#include <gmerlin/pluginregistry.h>
#include <gmerlin/utils.h>
#include <gmerlin/cmdline.h>

#include <gavl/hw.h>

#include <gavl/log.h>
#define LOG_DOMAIN "gavftools"

int gavftools_flags = 0;

/* Source */
bg_media_source_t * gavftools_src = NULL;
bg_plugin_handle_t * gavftools_input_handle = NULL;
bg_input_plugin_t * gavftools_input_plugin = NULL;

/* Sink */
gavf_writer_t * gavftools_writer = NULL;


//int num_gavftools_streams = 0;
// gavftools_stream_t * gavftools_streams = NULL;


static gavftools_thread_t gavftools_thread = { 0 };

bg_media_source_t gavftools_encoder = { 0 };

/* Used for gavtools_start() */
pthread_barrier_t gavftools_barrier;

static gavl_array_t audio_buffer_formats = { 0 };
static gavl_array_t video_buffer_formats = { 0 };


void gavftools_init()
  {
  bg_handle_sigint();
  signal(SIGPIPE, SIG_IGN);
  
  /* Create global config registry */
  bg_plugins_init();
  
  }

/* Initialise source */

static void set_stream_actions_auto(gavl_stream_type_t type)
  {
  int i, num;
  bg_media_source_stream_t * s;

  //  fprintf(stderr, "set_stream_actions_auto %s\n", gavl_stream_type_name(type));
  
  num = gavl_track_get_num_streams(gavftools_src->track, type);

  for(i = 0; i < num; i++)
    {
    gavl_compression_info_t ci;
    
    s = bg_media_source_get_stream(gavftools_src, type, i);
    gavl_compression_info_init(&ci);
    
    if(gavl_stream_get_compression_info(s->s, &ci) &&
       (ci.id != GAVL_CODEC_ID_NONE))
      bg_media_source_set_stream_action(gavftools_src, type, i, BG_STREAM_ACTION_READRAW);
    else
      bg_media_source_set_stream_action(gavftools_src, type, i, BG_STREAM_ACTION_DECODE);
    }
  }

static void enable_msg_stream()
  {
  if(bg_media_source_set_msg_action_by_id(gavftools_src, GAVL_META_STREAM_ID_MSG_PROGRAM,
                                          BG_STREAM_ACTION_DECODE))
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Enabled message stream");
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

int gavftools_open_src(void)
  {
  gavl_dictionary_t track;
  int num_variants = 0;

  const char * gavftools_src_location =
    gavl_dictionary_get_string(&bg_cmdline_options, "i");

  if(!gavftools_src_location)
    gavftools_src_location = GAVF_PROTOCOL"://-";
  
  //  fprintf(stderr, "gavftools_open_src %s\n", gavftools_src_location);
  
  gavl_dictionary_init(&track);
  gavl_track_from_location(&track, gavftools_src_location);
  if(!(gavftools_input_handle = bg_load_track(&track, 0, &num_variants)))
    {
    gavl_dictionary_free(&track);
    return 0;
    }
  gavftools_input_plugin = (bg_input_plugin_t*)gavftools_input_handle->plugin;
  gavftools_src = gavftools_input_plugin->get_src(gavftools_input_handle->priv);
  gavl_dictionary_free(&track);
  return 1;
  }

void gavftools_set_stream_actions(void)
  {
  set_stream_actions_auto(GAVL_STREAM_AUDIO);
  set_stream_actions_auto(GAVL_STREAM_VIDEO);
  set_stream_actions_auto(GAVL_STREAM_TEXT);
  set_stream_actions_auto(GAVL_STREAM_OVERLAY);
  }

int gavftools_init_src(void)
  {
  gavl_msg_t msg;
  int done;


  //  fprintf(stderr, "gavftools_init_src %s\n", gavftools_src_location);
  
  /* Interactive opening */
  if(gavftools_flags & GAVFTOOLS_OUT_BACKCHANNEL)
    {
    gavl_dictionary_t * mi;

    const char * gavftools_src_location =
      gavl_dictionary_get_string(&bg_cmdline_options, "i");
  
    if(!gavftools_src_location)
      gavftools_src_location = GAVF_PROTOCOL"://-";
    
    mi = bg_plugin_registry_load_media_info(bg_plugin_reg, gavftools_src_location, 0);

    if(!mi)
      return 0;
    
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
          gavl_dictionary_destroy(mi);
          gavl_msg_free(&msg);
          return 0;
          }
        
        free(real_uri);
        gavl_dictionary_free(&track);

        gavftools_input_plugin = (bg_input_plugin_t*)gavftools_input_handle->plugin;
        gavftools_src = gavftools_input_plugin->get_src(gavftools_input_handle->priv);
        gavl_msg_reset(&msg);
        break;
        }
      gavl_msg_reset(&msg);
      }

    if(!gavftools_input_plugin)
      {
      gavl_dictionary_destroy(mi);
      return 0;
      }
    
    /* Set up source */
    done = 0;
    while(wait_for_message(&msg))
      {
      switch(msg.NS)
        {
        case GAVL_MSG_NS_SRC:
          {
          switch(msg.ID)
            {
            case GAVL_CMD_SRC_SET_STREAM_ACTION:
              {
              gavl_stream_type_t type;
              int idx;
              int action;
              bg_media_source_stream_t * st;
        
        
              type   = gavl_msg_get_arg_int(&msg, 0);
              idx    = gavl_msg_get_arg_int(&msg, 1);
              action = gavl_msg_get_arg_int(&msg, 2);

              if(type == GAVL_STREAM_MSG)
                fprintf(stderr, "Setting stream action: %d %d %d\n", type, idx, action);

              st = bg_media_source_get_stream(gavftools_src, type, idx);
              st->action = action;
              }
              break;
            case GAVL_CMD_SRC_START:
              /*
              fprintf(stderr, "Got start %d %d\n",
                      audio_buffer_formats.num_entries,
                      video_buffer_formats.num_entries);
              */
              
              /* Try zero copy */
              if(gavftools_flags & GAVFTOOLS_OUT_LOCAL)
                {
                // fprintf(stderr, "Trying zero copy\n");
                
                if(!audio_buffer_formats.num_entries)
                  {
                  gavl_hw_buf_desc_append(&audio_buffer_formats, GAVL_HW_MEMFD);
                  // gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Enabling zero copy for audio");
                  }
                if(!video_buffer_formats.num_entries)
                  {
                  gavl_hw_buf_desc_append(&video_buffer_formats, GAVL_HW_MEMFD);
                  // gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Enabling zero copy for video");
                  }
                gavl_hw_buf_desc_set_shared(&audio_buffer_formats);
                gavl_hw_buf_desc_set_shared(&video_buffer_formats);
                
                bg_input_plugin_set_audio_buffer_formats(gavftools_input_handle,
                                                         &audio_buffer_formats);
                bg_input_plugin_set_video_buffer_formats(gavftools_input_handle,
                                                         &video_buffer_formats);
                
                
                }
              
              done = 1;
              break;
            case GAVL_CMD_SRC_SET_BUFFER_FORMATS:
              {
              int type = gavl_msg_get_arg_int(&msg, 0);
              // fprintf(stderr, "Got buffer format\n");
              switch(type)
                {
                case GAVL_STREAM_AUDIO:
                  //                  fprintf(stderr, "Got audio buffer format\n");
                  gavl_msg_get_arg_array(&msg, 1, &audio_buffer_formats);
                  break;
                case GAVL_STREAM_VIDEO:
                  //                  fprintf(stderr, "Got video buffer format\n");
                  gavl_msg_get_arg_array(&msg, 1, &video_buffer_formats);
                  break;
                }
              }
              break;
            }
          break;
          }
        }

      gavl_msg_reset(&msg);

      if(done)
        break;
      }

    //    enable_msg_stream();
    
    bg_input_plugin_start(gavftools_input_handle);
    //    gavl_dictionary_dump(gavftools_src->track, 2);
    
    gavl_dictionary_destroy(mi);
    }
  else /* No commands expected, let's initialize the source ourselves */
    {
    if(!gavftools_open_src())
      return 0;

    gavftools_set_stream_actions();
    
    enable_msg_stream();
    
    bg_input_plugin_start(gavftools_input_handle);
    }
  
  return 1;
  }

/* Call before opening the input */
int gavftools_open_sink()
  {
  const char * gavftools_dst_location =
    gavl_dictionary_get_string(&bg_cmdline_options, "o");
  
  gavftools_writer = gavf_writer_create();
  if(!gavf_writer_open(gavftools_writer, gavftools_dst_location))
    return 0;

  if(gavf_writer_has_backchannel(gavftools_writer))
    gavftools_flags |= (GAVFTOOLS_OUT_BACKCHANNEL|GAVFTOOLS_MULTI_THREAD);
  if(gavf_writer_is_local(gavftools_writer))
    gavftools_flags |= GAVFTOOLS_OUT_LOCAL;
  
  return 1;
  }



int gavftools_init_sink(bg_media_source_t * src)
  {
  int i = 0;
  int idx = 0;
  bg_encoder_stream_t * s;
  bg_media_source_stream_t * st;

  gavl_dictionary_t * m;
  
  /* Set metadata from commanline */
  if((m = gavl_track_get_metadata_nc(src->track)))
    gavftools_set_metadata(m);
  
  if(!gavf_writer_init(gavftools_writer, src))
    {
    return 0;
    }

  bg_media_source_set_from_source(&gavftools_encoder, src);

  /* Set up streams and sinks */

  for(i = 0; i < gavftools_encoder.num_streams; i++)
    {
    st = gavftools_encoder.streams[i];
    if((st->action == BG_STREAM_ACTION_OFF) ||
       (st->type == GAVL_STREAM_MSG))
      continue;

    s = bg_encoder_stream_create(&gavftools_encoder, st);
    s->asink = gavf_writer_get_audio_sink(gavftools_writer, idx);
    s->vsink = gavf_writer_get_video_sink(gavftools_writer, idx);
    s->psink = gavf_writer_get_packet_sink(gavftools_writer, idx);
    
    idx++;
    }
  
  bg_media_encoder_finalize(&gavftools_encoder);
  
  return 1;
  }

/* Called from main()'s thread */
int gavftools_handle_sink_message(gavl_msg_t * msg)
  {
  switch(msg->NS)
    {
    case GAVL_MSG_NS_SRC:

      switch(msg->ID)
        {
        case GAVL_CMD_SRC_SELECT_TRACK:
          break;
        case GAVL_CMD_SRC_SEEK:
          {
          int64_t time;
          int scale;
          
          time = gavl_msg_get_arg_long(msg, 0);
          scale = gavl_msg_get_arg_int(msg, 1);

          fprintf(stderr, "Got seek command %"PRId64" %d\n", time, scale);
          
          gavftools_stop();

          fprintf(stderr, "Stopped %p\n", gavftools_writer);
          
          if(gavftools_writer)
            gavf_writer_write_discont(gavftools_writer, GAVF_PACKET_DISCONT_RESYNC);
          
          if(gavftools_input_handle)
            bg_input_plugin_seek(gavftools_input_handle, time, scale);

          gavftools_start();

          }
          break;
        case GAVL_CMD_SRC_START:
          break;
        case GAVL_CMD_SRC_PAUSE:
          if(gavftools_input_handle)
            bg_input_plugin_pause(gavftools_input_handle);          
          break;
        case GAVL_CMD_SRC_RESUME:
          if(gavftools_input_handle)
            bg_input_plugin_resume(gavftools_input_handle);          
          break;
        }
      break;
    }
  return 1;
  }


void gavftools_cleanup(void)
  {
  if(gavftools_writer)
    {
    gavf_writer_destroy(gavftools_writer);
    gavftools_writer = NULL;
    }
  
  if(gavftools_input_handle)
    {
    bg_plugin_unref(gavftools_input_handle);
    gavftools_input_handle = NULL;
    }

  gavl_array_reset(&audio_buffer_formats);
  gavl_array_reset(&video_buffer_formats);


  bg_media_source_cleanup(&gavftools_encoder);
  
  
  bg_global_cleanup();
  }

static void * thread_func(void * data)
  {
  gavftools_thread_t * th = data;
  
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Started processing thread");
  
  while(1)
    {
    pthread_mutex_lock(&th->mutex);
    if(th->state == THREAD_STATE_STOP)
      {
      pthread_mutex_unlock(&th->mutex);
      break;
      }
    pthread_mutex_unlock(&th->mutex);
    
    if(bg_media_encoder_process(&gavftools_encoder, NULL) == GAVL_SOURCE_EOF)
      {
      pthread_mutex_lock(&th->mutex);
      th->state = THREAD_STATE_EOF;
      pthread_mutex_unlock(&th->mutex);
      break;
      }
    }

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Ending processing thread");
  
  return NULL;
  }

static void thread_start(gavftools_thread_t * th)
  {
  pthread_create(&th->thread, NULL, thread_func, th);
  pthread_mutex_lock(&th->mutex);
  th->state = THREAD_STATE_RUNNING;
  pthread_mutex_unlock(&th->mutex);
  }

static void thread_stop(gavftools_thread_t * th)
  {
  pthread_mutex_lock(&th->mutex);

  switch(th->state)
    {
    case THREAD_STATE_RUNNING:
      th->state = THREAD_STATE_STOP;
      break;
    case THREAD_STATE_EOF:
      break;
    case THREAD_STATE_STOP:
      break;
    case THREAD_STATE_INIT:
    case THREAD_STATE_FINISHED:
      /* Thread isn't running */
      pthread_mutex_unlock(&th->mutex);
      return;
      break;
    }
  pthread_mutex_unlock(&th->mutex);

  pthread_join(th->thread, NULL);
  
  pthread_mutex_lock(&th->mutex);
  th->state = THREAD_STATE_FINISHED;
  pthread_mutex_unlock(&th->mutex);
  
  }

void gavftools_start(void)
  {
  if(gavftools_flags & GAVFTOOLS_MULTI_THREAD)
    {
    bg_media_encoder_start(&gavftools_encoder);
    }
  else
    {
    pthread_mutex_init(&gavftools_thread.mutex, 0);
    thread_start(&gavftools_thread);
    }
  }

void gavftools_stop(void)
  {
  if(gavftools_flags & GAVFTOOLS_MULTI_THREAD)
    {
    bg_media_encoder_stop(&gavftools_encoder);
    }
  else
    {
    thread_stop(&gavftools_thread);
    }
  }

void gavftools_run(void)
  {
  gavl_time_t delay_time = GAVL_TIME_SCALE / 20; // 50 ms
  gavl_msg_t msg;
  int result;
  
  gavftools_start();

  gavl_msg_init(&msg);
  
  while(1)
    {
    result = 0;
    
    if(bg_got_sigint())
      break;

    if(gavftools_flags & GAVFTOOLS_OUT_BACKCHANNEL)
      {
      result = gavf_writer_read_gavf_message(gavftools_writer, &msg, 0);
      
      if(result < 0)
        break; /* Backchannel disconnected: Assume sink just died */

      else if(result > 0)
        {
        if(!gavftools_handle_sink_message(&msg))
          break;
        }
      }
    
    /* Check for EOF */
    if(bg_media_encoder_eof(&gavftools_encoder))
      break;
    
    if(!result) // Idle
      gavl_time_delay(&delay_time);
    }
  
  gavftools_stop();
  }

/* Return 0 on wrong options */
int gavftools_set_metadata(gavl_dictionary_t * m)
  {
  int i;
  const gavl_array_t * arr = bg_cmdline_get_params("m");

  const char * opt;
  const char * pos;
  char * key;
  
  if(!arr)
    return 1;

  for(i = 0; i < arr->num_entries; i++)
    {
    opt = gavl_string_array_get(arr, i);

    if(!opt)
      return 0; // Impossible

    if(!(pos = strchr(opt, '=')))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid option: %s", opt);
      return 0;
      }

    key = gavl_strndup(opt, pos);

    pos++;

    //    fprintf(stderr, "Got metadata option: %s = %s\n", key, pos);

    if(!gavl_metadata_set_from_string(m, key, pos))
      {
      free(key);
      return 0;
      }
    
    free(key);
    
    }
  return 1;
  }
