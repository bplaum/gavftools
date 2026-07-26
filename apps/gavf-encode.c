

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
#include <gmerlin/bggavl.h>

#include <gavftools.h>

static bg_plugin_handle_t * encoder_handle = NULL;
static bg_encoder_plugin_t * encoder_plugin;

static bg_cmdline_arg_t global_options[] =
  {
    GAVFTOOLS_OPT_SRC,
    GAVFTOOLS_OPT_DST,
    {                                         \
      .arg = "-ae",                            \
      .help_arg = "key=val",                  \
      .help_string = TRS("Audio encoder options"), \
      .flags = BG_CMDLINE_ARG_PARAM | BG_CMDLINE_ARG_PER_STREAM,        \
    },
    {                                         \
      .arg = "-ve",                            \
      .help_arg = "key=val",                  \
      .help_string = TRS("Video encoder options"), \
      .flags = BG_CMDLINE_ARG_PARAM | BG_CMDLINE_ARG_PER_STREAM,        \
    },
    BG_PLUGIN_OPT_LIST_ENC,                     \
    BG_PLUGIN_OPT_LIST_OPTIONS,                 \
    { /* End */ },
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

#if 0
static void set_stream_parameter(void * data, const char * name, const gavl_value_t * val)
  {
  apply_stream_params_t * as = data;
  as->set_param(as->priv, as->idx, name, val);
  }
#endif


static int setup_encoder_config(const bg_plugin_info_t * info)
  {
  int i, num;
  gavl_dictionary_t * section;
  bg_media_source_stream_t * st;
  gavl_array_t arr;
  
  /* Set plugin config */
  
  /* Set audio encoder configs */
  
  if(info->audio_parameters)
    {
    num = bg_media_source_get_num_streams(gavftools_src, GAVL_STREAM_AUDIO);
    
    for(i = 0; i < num; i++)
      {
      st = bg_media_source_get_stream(gavftools_src, GAVL_STREAM_AUDIO, i);
      section = bg_track_get_config_nc(st->s, BG_TRACK_CONFIG_ENCODER);

      /* Create default */
      bg_cfg_section_create_items(section, info->audio_parameters);
      
      /* Apply commandline */
      gavl_array_init(&arr);
      bg_cmdline_get_stream_params("ae", i, &arr);

      
      if(!bg_cmdline_apply_params(section, info->audio_parameters,
                                  &arr))
        return 0;

      fprintf(stderr, "Section\n");
      gavl_dictionary_dump(section, 2);
      
      
      gavl_array_free(&arr);
      }
    
    }

  if(info->video_parameters)
    {
    num = bg_media_source_get_num_streams(gavftools_src, GAVL_STREAM_VIDEO);
    
    for(i = 0; i < num; i++)
      {
      st = bg_media_source_get_stream(gavftools_src, GAVL_STREAM_VIDEO, i);
      section = bg_track_get_config_nc(st->s, BG_TRACK_CONFIG_ENCODER);

      /* Create default */
      bg_cfg_section_create_items(section, info->video_parameters);
      
      /* Apply commandline */
      gavl_array_init(&arr);
      bg_cmdline_get_stream_params("ve", i, &arr);

      if(!bg_cmdline_apply_params(section, info->video_parameters,
                                  &arr))
        return 0;
      
      gavl_array_free(&arr);
      }
    
    }

  return 1;
  }

static int init_encoder(void)
  {
  gavl_dictionary_t * m;
  const char * pos;
  const bg_plugin_info_t * info = NULL;
  bg_media_source_stream_t * st;
  
  const char * gavftools_dst_location =
    gavl_dictionary_get_string(&bg_cmdline_options, "o");
  
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
                                      BG_PLUGIN_ENCODER);

  /* Set encoder config from the plugin and from the commandline */
  setup_encoder_config(info);
  
  
  if(!info)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Found no encoder for %s", gavftools_dst_location);
    return 0;
    }

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Using encoder: %s", bg_plugin_info_get_long_name(info));

  if(info->flags & BG_PLUGIN_NOMUX)
    {
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
             "Encoder has independent output channels, switching to multithread mode");
    gavftools_flags |= GAVFTOOLS_MULTI_THREAD;
    }
  
  /* Open encoder  */

  if(!(encoder_handle = bg_plugin_load(info)))
    return 0;

  encoder_plugin = (bg_encoder_plugin_t*)encoder_handle->plugin;

  
  m = gavl_track_get_metadata_nc(gavftools_src->track);

  gavftools_set_metadata(m);
  
  if(!encoder_plugin->open(encoder_handle->priv,
                           gavftools_dst_location, m))
    return 0;
  
  /* 1st pass: Decide decoding modes */

  if(!bg_media_encoder_init(gavftools_src, encoder_handle))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No streams to encode");
    return 0;
    }
  
  /* Start source, create streams */
  bg_input_plugin_start(gavftools_input_handle);

  /*
    Set encoder config in the source track
    Needs to be done after the source is started
  */
  
  setup_encoder_config(info);
    
  /* TODO: Connect message strean */

  if(encoder_plugin->add_msg_stream &&
     (st = bg_media_source_get_stream_by_id(gavftools_src, GAVL_META_STREAM_ID_MSG_PROGRAM)))
    {
    bg_msg_sink_t * sink;
    sink = encoder_plugin->add_msg_stream(encoder_handle->priv, GAVL_META_STREAM_ID_MSG_PROGRAM);
    bg_msg_hub_connect_sink(st->msghub, sink);
    }
  
  if(!bg_media_encoder_connect(&gavftools_encoder,
                               gavftools_src, encoder_handle))
    return 0;

  
  return 1;
  
  }

int main(int argc, char ** argv)
  {
  /* Global initialization */
  gavftools_init();
  
  bg_cmdline_init(&app_data);
  bg_cmdline_parse(global_options, &argc, &argv);

#if 0  
  fprintf(stderr, "Got commandline options:\n");
  gavl_dictionary_dump(&bg_cmdline_options, 2);
  fprintf(stderr, "\n");
#endif
  
  if(!gavftools_open_src())
    return EXIT_FAILURE;

  /* Set default actions, will be refined by the encoder later on */
  gavftools_set_stream_actions();
  
  //  if(!gavftools_init_src())
  //    return EXIT_FAILURE;
  
  if(!init_encoder())
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Initializing encoders failed");
    return EXIT_FAILURE;
    }
  
  gavftools_run();

  /* End */
  encoder_plugin->close(encoder_handle->priv, 0);
  bg_plugin_unref(encoder_handle);
  
  gavftools_cleanup();
  
  return EXIT_SUCCESS;

  
  }
