#include <config.h>

#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>

#include <gavf.h>
#include <gavfprivate.h>

static void * create_gavf()
  {
  return gavf_reader_create();
  }

static void destroy_gavf(void * data)
  {
  return gavf_reader_destroy(data);
  }

static const bg_parameter_info_t * get_parameters_gavf(void * data)
  {
  return NULL;
  }

static void set_parameter_gavf(void * data,
                               const char * name,
                               const gavl_value_t * val)
  {

  }

static bg_controllable_t * get_controllable_gavf(void * data)
  {
  return gavf_reader_get_controllable(data);
  }

static char const * const protocols = GAVF_PROTOCOL" "GAVF_PROTOCOL_SERVER;
static char const * const extensions = "gavf";

static const char * get_protocols(void * priv)
  {
  return protocols;
  }

static const char * get_extensions(void * priv)
  {
  return extensions;
  }

static int open_gavf(void * priv, const char * location)
  {
  return gavf_reader_open(priv, location);
  }

static gavl_dictionary_t * get_media_info_gavf(void * priv)
  {
  gavf_reader_t * g = priv;
  return &g->mi;
  }

static bg_media_source_t * get_src_gavf(void * priv)
  {
  gavf_reader_t * g = priv;
  return &g->src;
  }

static void close_gavf(void * priv)
  {
  //  gavf_reader_close(priv);
  }

const bg_input_plugin_t the_plugin =
  {
    .common =
    {
      BG_LOCALE,
      .name =           "i_gavf",
      .long_name =      TRS("GAVF input plugin"),
      .description =    TRS("Reader for GAVF streams."),
      .type =           BG_PLUGIN_INPUT,
      .flags =          0,
      .priority =       BG_PLUGIN_PRIORITY_MAX,
      .create =         create_gavf,
      .destroy =        destroy_gavf,
      .get_parameters = get_parameters_gavf,
      .set_parameter =  set_parameter_gavf,
      .get_controllable = get_controllable_gavf,
      .get_extensions = get_extensions,
      .get_protocols = get_protocols,
    },
    /* Open file/device */
    .open = open_gavf,

    //    .open_io = open_io_gavf,
    
    /* For file and network plugins, this can be NULL */
    .get_media_info = get_media_info_gavf,
    .get_src = get_src_gavf,
    
    /* Stop playback, close all decoders */
    .stop = NULL,
    .close = close_gavf,
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
