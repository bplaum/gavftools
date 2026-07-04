

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

#if 0
static void set_stream_parameter(void * data, const char * name, const gavl_value_t * val)
  {
  apply_stream_params_t * as = data;
  as->set_param(as->priv, as->idx, name, val);
  }
#endif

static int init_encoder(void)
  {
  
  const char * pos;
  const bg_plugin_info_t * info = NULL;

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
  
  if(!info)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Found no encoder for %s", gavftools_dst_location);
    return 0;
    }

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Using encoder: %s", bg_plugin_info_get_long_name(info));
  
  
  /* Open encoder  */

  if(!(encoder_handle = bg_plugin_load(info)))
    return 0;

  encoder_plugin = (bg_encoder_plugin_t*)encoder_handle->plugin;

  if(!encoder_plugin->open(encoder_handle->priv,
                           gavftools_dst_location,
                           gavl_track_get_metadata(gavftools_src->track)))
    return 0;
  
  /* 1st pass: Decide decoding modes */

  if(!bg_media_encoder_init(gavftools_src, encoder_handle))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No streams to encode");
    return 0;
    }
  
  /* Start source, create streams */
  bg_input_plugin_start(gavftools_input_handle);

  /* TODO: Connect message strean */
  
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
  bg_cmdline_parse(global_options, &argc, &argv, NULL);

  /* Open source */
  if(!gavftools_src_location)
    gavftools_src_location = GAVF_PROTOCOL"://-";

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
  
  gavftools_cleanup();
  
  return EXIT_SUCCESS;

  
  }
