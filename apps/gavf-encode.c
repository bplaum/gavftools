

#include <stdlib.h>
#include <string.h>


#include <config.h>

#include <gavl/gavl.h>
#include <gavl/log.h>
#define LOG_DOMAIN "gavf-encode"

#include <gmerlin/translation.h>
#include <gmerlin/cmdline.h>
#include <gmerlin/pluginregistry.h>
#include <gmerlin/utils.h>

#include <gavftools.h>

static bg_plugin_handle_t * encoder_handle = NULL;
static bg_encoder_plugin_t * encoder_plugin;

static bg_cmdline_arg_t global_options[] =
  {
    GAVFTOOLS_OPT_SRC,
    GAVFTOOLS_OPT_DST,
    { /* End */ }
  };

const bg_cmdline_app_data_t app_data =
  {
    .package =  PACKAGE,
    .version =  VERSION,
    .synopsis = TRS("[options]\n"),
    .help_before = TRS("Encode a source using gmerlins encoder plugins\n"),
    .args = (bg_cmdline_arg_array_t[]) { { TRS("Options"), global_options },
                                         {  } },
  };

typedef struct
  {
  int idx;
  void (*set_param)(void * priv, int idx, const char * name, const gavl_value_t * val);
  void * priv;
  
  } apply_stream_params_t;

static void set_stream_parameter(void * data, const char * name, const gavl_value_t * val)
  {
  apply_stream_params_t * as = data;
  as->set_param(as->priv, as->idx, name, val);
  }

static int init_encoder(void)
  {
  int i;
  int idx;
  int max_audio_streams;
  int max_video_streams;
  int max_text_streams;
  int max_overlay_streams;

  int num_audio_streams = 0;
  int num_video_streams = 0;
  int num_text_streams = 0;
  int num_overlay_streams = 0;
  apply_stream_params_t as;
  
  const char * pos;
  const bg_plugin_info_t * info = NULL;

  bg_media_source_stream_t * st;
  gavl_compression_info_t ci;
  const gavl_audio_format_t * afmt;
  const gavl_video_format_t * vfmt;
  
  if(!gavftools_dst_location)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No destination given");
    return 1;
    }

  if((pos = strstr(gavftools_dst_location, "://")))
    {
    char * protocol = gavl_strndup(gavftools_dst_location, pos);
    info = bg_plugin_find_by_protocol(protocol, BG_PLUGIN_ENCODER);
    free(protocol);
    }

  if(!info)
    info = bg_plugin_find_by_filename(gavftools_dst_location,
                                      BG_PLUGIN_ENCODER |
                                      BG_PLUGIN_ENCODER_AUDIO |
                                      BG_PLUGIN_ENCODER_VIDEO);
  
  if(!info)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Found no encoder for %s", gavftools_dst_location);
    return 0;
    }

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Using encoder: %s", bg_plugin_info_get_long_name(info));
  
  //  gavl_dictionary_dump(&info->dict, 2);
  
  max_audio_streams   = bg_plugin_info_get_max_audio_streams(info);
  max_video_streams   = bg_plugin_info_get_max_video_streams(info);
  max_text_streams    = bg_plugin_info_get_max_text_streams(info);
  max_overlay_streams = bg_plugin_info_get_max_overlay_streams(info);


  /* Open encoder and go through all streams */

  if(!(encoder_handle = bg_plugin_load(info)))
    return 0;

  encoder_plugin = (bg_encoder_plugin_t*)encoder_handle->plugin;

  if(!encoder_plugin->open(encoder_handle->priv,
                           gavftools_dst_location,
                           gavl_track_get_metadata(gavftools_src->track)))
    return 0;
  
  /* 1st pass: Decide decoding modes */
  
  for(i = 0; i < gavftools_src->num_streams; i++)
    {
    st = gavftools_src->streams[i];
    
    switch(st->type)
      {
      case GAVL_STREAM_AUDIO:
        {
        if((max_audio_streams >= 0) && (num_audio_streams >= max_audio_streams))
          {
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Dropping audio stream (not supported by encoder)");
          st->action = BG_STREAM_ACTION_OFF;
          continue;
          }

        afmt = gavl_stream_get_audio_format(st->s);
        
        gavl_compression_info_init(&ci);
        gavl_stream_get_compression_info(st->s, &ci);
        if(ci.id != GAVL_CODEC_ID_NONE)
          {
          fprintf(stderr, "Got compressed audio stream: %s\n",
                  gavl_compression_get_long_name(ci.id));

          if(encoder_plugin->writes_compressed_audio &&
             encoder_plugin->writes_compressed_audio(encoder_handle->priv,
                                                     afmt, &ci))
            {
            gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Copying compressed audio stream (%s)",
                     gavl_compression_get_long_name(ci.id));
            st->action = BG_STREAM_ACTION_READRAW;
            }
          else
            {
            gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Decompressing audio stream (%s)",
                     gavl_compression_get_long_name(ci.id));
            st->action = BG_STREAM_ACTION_DECODE;
            }
          }
        else
          {
          fprintf(stderr, "Got uncompressed audio stream\n");
          st->action = BG_STREAM_ACTION_DECODE;
          }
        gavl_compression_info_free(&ci);
        }
        break;
      case GAVL_STREAM_VIDEO:
        {
        if((max_video_streams >= 0) && (num_video_streams >= max_video_streams))
          {
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Dropping video stream (not supported by encoder)");
          st->action = BG_STREAM_ACTION_OFF;
          continue;
          }

        vfmt = gavl_stream_get_video_format(st->s);
        gavl_compression_info_init(&ci);
        gavl_stream_get_compression_info(st->s, &ci);
        if(ci.id != GAVL_CODEC_ID_NONE)
          {
          fprintf(stderr, "Got compressed video stream: %s\n",
                  gavl_compression_get_long_name(ci.id));

          if(encoder_plugin->writes_compressed_video &&
             encoder_plugin->writes_compressed_video(encoder_handle->priv,
                                                     vfmt, &ci))
            {
            gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Copying compressed video stream (%s)",
                     gavl_compression_get_long_name(ci.id));
            st->action = BG_STREAM_ACTION_READRAW;
            }
          else
            {
            gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Decompressing video stream (%s)",
                     gavl_compression_get_long_name(ci.id));
            st->action = BG_STREAM_ACTION_DECODE;
            }
          }
        else
          {
          fprintf(stderr, "Got uncompressed video stream\n");
          st->action = BG_STREAM_ACTION_DECODE;
          }
        gavl_compression_info_free(&ci);
        }
        break;
      case GAVL_STREAM_TEXT:
        if((max_text_streams >= 0) && (num_text_streams >= max_text_streams))
          {
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Dropping text stream (not supported by encoder)");
          st->action = BG_STREAM_ACTION_OFF;
          continue;
          }
        break;
      case GAVL_STREAM_OVERLAY:
        {
        if((max_overlay_streams >= 0) && (num_overlay_streams >= max_overlay_streams))
          {
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Dropping overlay stream (not supported by encoder)");
          st->action = BG_STREAM_ACTION_OFF;
          continue;
          }
        gavl_compression_info_init(&ci);
        gavl_stream_get_compression_info(st->s, &ci);
        vfmt = gavl_stream_get_video_format(st->s);

        if(ci.id != GAVL_CODEC_ID_NONE)
          {
          fprintf(stderr, "Got compressed overlay stream: %s\n",
                  gavl_compression_get_long_name(ci.id));
          
          if(encoder_plugin->writes_compressed_overlay &&
             encoder_plugin->writes_compressed_overlay(encoder_handle->priv,
                                                       vfmt, &ci))
            {
            gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Copying compressed overlay stream (%s)",
                     gavl_compression_get_long_name(ci.id));
            st->action = BG_STREAM_ACTION_READRAW;
            }
          else
            {
            gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Decompressing overlay stream (%s)",
                     gavl_compression_get_long_name(ci.id));
            st->action = BG_STREAM_ACTION_DECODE;
            }
          
          }
        else
          {
          fprintf(stderr, "Got uncompressed overlay stream\n");
          st->action = BG_STREAM_ACTION_DECODE;
          }
        gavl_compression_info_free(&ci);
        }
        break;
      case GAVL_STREAM_MSG:
        if(encoder_plugin->add_msg_stream)
          st->action = BG_STREAM_ACTION_DECODE;
        break;
      case GAVL_STREAM_NONE:
        break;
      }

    if((st->action != BG_STREAM_ACTION_OFF) &&
       (st->type != GAVL_STREAM_MSG))
      num_gavftools_streams++;
    
    }

  bg_media_source_set_msg_action_by_id(gavftools_src,
                                       GAVL_META_STREAM_ID_MSG_PROGRAM,
                                       BG_STREAM_ACTION_DECODE);
  
  /* Start source, create streams */
  bg_input_plugin_start(gavftools_input_handle);
  gavftools_streams = calloc(num_gavftools_streams, sizeof(*gavftools_streams));
  idx = 0;
  
  
  /* 2nd pass: Add encoder streams */
  for(i = 0; i < gavftools_src->num_streams; i++)
    {
    st = gavftools_src->streams[i];
    
    if(st->action == BG_STREAM_ACTION_OFF)
      continue;

    switch(st->type)
      {
      case GAVL_STREAM_AUDIO:
        gavftools_streams[idx].src = st;
        afmt = gavl_stream_get_audio_format(st->s);
        gavftools_streams[idx].timescale = afmt->samplerate;

        
        if(st->action == BG_STREAM_ACTION_DECODE)
          {
          const bg_parameter_info_t * parameters = NULL;
          

          /* TODO: Enable codecs from commandline */
          
          gavftools_streams[idx].out_idx =
            encoder_plugin->add_audio_stream(encoder_handle->priv,
                                             gavl_stream_get_metadata(st->s),
                                             gavl_stream_get_audio_format(st->s));
          gavftools_streams[idx].process = gavftools_process_stream_audio;

          parameters = encoder_plugin->get_audio_parameters ? 
            encoder_plugin->get_audio_parameters(encoder_handle->priv) : NULL;

          if(parameters)
            {
            gavl_dictionary_t dict;
            gavl_dictionary_init(&dict);

            bg_cfg_section_create_items(&dict, parameters);
            
            as.idx = gavftools_streams[idx].out_idx;
            as.set_param = encoder_plugin->set_audio_parameter;
            as.priv = encoder_handle->priv;
            bg_cfg_section_apply(&dict, parameters, set_stream_parameter, &as);
            gavl_dictionary_free(&dict);
            }
          
          }
        else if(st->action == BG_STREAM_ACTION_READRAW)
          {
          gavl_compression_info_init(&ci);
          gavl_stream_get_compression_info(st->s, &ci);

          gavftools_streams[idx].out_idx =
            encoder_plugin->add_audio_stream_compressed(encoder_handle->priv,
                                                        gavl_stream_get_metadata(st->s),
                                                        gavl_stream_get_audio_format(st->s), &ci);
          gavl_compression_info_free(&ci);
          gavftools_streams[idx].process = gavftools_process_stream_packet;
          }
        idx++;
        break;
      case GAVL_STREAM_VIDEO:
      case GAVL_STREAM_OVERLAY:
        gavftools_streams[idx].src = st;
        vfmt = gavl_stream_get_video_format(st->s);
        
        gavftools_streams[idx].timescale = vfmt->timescale;
        
        if(st->action == BG_STREAM_ACTION_DECODE)
          {
          const bg_parameter_info_t * parameters = NULL;

          if(st->type == GAVL_STREAM_VIDEO)          
            {
            gavftools_streams[idx].out_idx =
              encoder_plugin->add_video_stream(encoder_handle->priv,
                                               gavl_stream_get_metadata(st->s),
                                               gavl_stream_get_video_format(st->s));
            gavftools_streams[idx].process = gavftools_process_stream_video;

            parameters = encoder_plugin->get_video_parameters ? 
              encoder_plugin->get_video_parameters(encoder_handle->priv) : NULL;

            if(parameters)
              {
              gavl_dictionary_t dict;
              gavl_dictionary_init(&dict);

              bg_cfg_section_create_items(&dict, parameters);
            
              as.idx = gavftools_streams[idx].out_idx;
              as.set_param = encoder_plugin->set_video_parameter;
              as.priv = encoder_handle->priv;
              bg_cfg_section_apply(&dict, parameters, set_stream_parameter, &as);
              gavl_dictionary_free(&dict);
              }
            
            }
          else
            {
            gavftools_streams[idx].out_idx =
              encoder_plugin->add_overlay_stream(encoder_handle->priv,
                                                 gavl_stream_get_metadata(st->s),
                                                 gavl_stream_get_video_format(st->s));
            gavftools_streams[idx].process = gavftools_process_stream_video_discont;

            parameters = encoder_plugin->get_overlay_parameters ? 
              encoder_plugin->get_overlay_parameters(encoder_handle->priv) : NULL;

            if(parameters)
              {
              gavl_dictionary_t dict;
              gavl_dictionary_init(&dict);

              bg_cfg_section_create_items(&dict, parameters);
            
              as.idx = gavftools_streams[idx].out_idx;
              as.set_param = encoder_plugin->set_overlay_parameter;
              as.priv = encoder_handle->priv;
              bg_cfg_section_apply(&dict, parameters, set_stream_parameter, &as);
              gavl_dictionary_free(&dict);
              }

            
            }
          }
        else if(st->action == BG_STREAM_ACTION_READRAW)
          {
          gavl_compression_info_init(&ci);
          gavl_stream_get_compression_info(st->s, &ci);

          if(st->type == GAVL_STREAM_VIDEO)          
            {
            gavftools_streams[idx].out_idx =
              encoder_plugin->add_video_stream_compressed(encoder_handle->priv,
                                                          gavl_stream_get_metadata(st->s),
                                                        gavl_stream_get_video_format(st->s), &ci);

            if(ci.flags & GAVL_COMPRESSION_HAS_B_FRAMES)
              gavftools_streams[idx].flags |= STREAM_B_FRAMES;
            gavftools_streams[idx].process = gavftools_process_stream_packet;
            }
          else
            {
            gavftools_streams[idx].out_idx =
              encoder_plugin->add_overlay_stream_compressed(encoder_handle->priv,
                                                            gavl_stream_get_metadata(st->s),
                                                            gavl_stream_get_video_format(st->s), &ci);

            gavftools_streams[idx].process = gavftools_process_stream_packet_discont;
            }
          gavl_compression_info_free(&ci);
          }
        idx++;
        break;
      case GAVL_STREAM_TEXT:
        {
        const gavl_dictionary_t * m;
        
        gavftools_streams[idx].src = st;
        gavftools_streams[idx].timescale = GAVL_TIME_SCALE;
        
        if((m = gavl_stream_get_metadata(st->s)))
          gavl_dictionary_get_int(m, GAVL_META_STREAM_SAMPLE_TIMESCALE,
                                  &gavftools_streams[idx].timescale);
        
        gavftools_streams[idx].out_idx =
          encoder_plugin->add_text_stream(encoder_handle->priv,
                                          m, (uint32_t*)&gavftools_streams[idx].timescale);
        gavftools_streams[idx].process = gavftools_process_stream_packet_discont;
        idx++;
        }
        break;
      case GAVL_STREAM_MSG:
        {
        if(encoder_plugin->add_msg_stream && st->msghub)
          {
          bg_msg_sink_t * sink =
            encoder_plugin->add_msg_stream(encoder_handle->priv,
                                           GAVL_META_STREAM_ID_MSG_PROGRAM);
          bg_msg_hub_connect_sink(st->msghub, sink);
          }
        }
        break;
      case GAVL_STREAM_NONE:
        break;
      }
    }

  encoder_plugin->start(encoder_handle->priv);
  
  /* 3rd pass: Get sinks and set source formats from sinks */
  
  for(i = 0; i < num_gavftools_streams; i++)
    {
    if(!gavl_stream_is_continuous(gavftools_streams[i].src->s))
      gavftools_streams[i].flags |= STREAM_DISCONT;

    gavftools_streams[i].last_status = GAVL_SOURCE_OK;
    
    switch(gavftools_streams[i].src->type)
      {
      case GAVL_STREAM_AUDIO:
        
        if(gavftools_streams[i].src->action == BG_STREAM_ACTION_DECODE)
          {
          gavftools_streams[i].asink =
            encoder_plugin->get_audio_sink(encoder_handle->priv, gavftools_streams[i].out_idx);
          gavl_audio_source_set_dst(gavftools_streams[i].src->asrc, 0, gavl_audio_sink_get_format(gavftools_streams[i].asink));
          }
        else if(gavftools_streams[i].src->action == BG_STREAM_ACTION_READRAW)
          {
          gavftools_streams[i].psink =
            encoder_plugin->get_audio_packet_sink(encoder_handle->priv, gavftools_streams[i].out_idx);
          }
        
        break;
      case GAVL_STREAM_VIDEO:
        if(gavftools_streams[i].src->action == BG_STREAM_ACTION_DECODE)
          {
          gavftools_streams[i].vsink =
            encoder_plugin->get_video_sink(encoder_handle->priv, gavftools_streams[i].out_idx);
          gavl_video_source_set_dst(gavftools_streams[i].src->vsrc, 0, gavl_video_sink_get_format(gavftools_streams[i].vsink));
          }
        else if(gavftools_streams[i].src->action == BG_STREAM_ACTION_READRAW)
          {
          gavftools_streams[i].psink =
            encoder_plugin->get_video_packet_sink(encoder_handle->priv, gavftools_streams[i].out_idx);
          }
        
        break;
      case GAVL_STREAM_TEXT:
        gavftools_streams[i].psink =
          encoder_plugin->get_text_sink(encoder_handle->priv, gavftools_streams[i].out_idx);
        break;
      case GAVL_STREAM_OVERLAY:
        if(gavftools_streams[i].src->action == BG_STREAM_ACTION_DECODE)
          {
          gavftools_streams[i].vsink =
            encoder_plugin->get_overlay_sink(encoder_handle->priv, gavftools_streams[i].out_idx);
          gavl_video_source_set_dst(gavftools_streams[i].src->vsrc, 0, gavl_video_sink_get_format(gavftools_streams[i].vsink));
          }
        else if(gavftools_streams[i].src->action == BG_STREAM_ACTION_READRAW)
          {
          gavftools_streams[i].psink =
            encoder_plugin->get_overlay_packet_sink(encoder_handle->priv, gavftools_streams[i].out_idx);
          }
        break;
      case GAVL_STREAM_MSG:
        break;
      case GAVL_STREAM_NONE:
        break;
      }
    }
  
  return 1;
  
  }

int main(int argc, char ** argv)
  {
  /* Global initialization */
  gavftools_init();
  
  bg_cmdline_init(&app_data);
  bg_cmdline_parse(global_options, &argc, &argv, NULL);

  /* Open source */
  if(!gavftools_open_src())
    return EXIT_FAILURE;
  
  if(!init_encoder())
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Initializing encoders failed");
    return EXIT_FAILURE;
    }
  
  gavftools_run();

  /* End */
  encoder_plugin->close(encoder_handle->priv, 0);
  
  gavftools_cleanup();
  
  return EXIT_SUCCESS;

  
  }
