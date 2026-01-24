

#include <stdlib.h>


#include <config.h>

#include <gavl/gavl.h>
#include <gavl/log.h>
#define LOG_DOMAIN "gavf-cat"

#include <gmerlin/translation.h>
#include <gmerlin/cmdline.h>
#include <gmerlin/pluginregistry.h>
#include <gmerlin/utils.h>

#include <gavftools.h>

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
    .help_before = TRS("Read a source and output a GAVF stream \n"),
    .args = (bg_cmdline_arg_array_t[]) { { TRS("Options"), global_options },
                                         {  } },
  };

int main(int argc, char ** argv)
  {
  /* Global initialization */
  gavftools_init();
  
  bg_cmdline_init(&app_data);
  bg_cmdline_parse(global_options, &argc, &argv, NULL);

  /* We open the sink before the source */
  if(!gavftools_open_sink())
    return EXIT_FAILURE;
  
  /* Initialize source */
  if(!gavftools_init_src())
    return EXIT_FAILURE;
  
  /* Initialize sink */
  if(!gavftools_init_sink(gavftools_src))
    return EXIT_FAILURE;

  gavftools_run();
  
#if 0 
  while(1)
    {
    /* Check for CTRL+C */
    if(bg_got_sigint())
      break;
    
    /* Handle sink messages */
    
    /* Handle media transfers */
    
    if(gavftools_iteration_singlethread(NULL) == GAVL_SOURCE_EOF)
      {
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got EOF");
      break;
      }
    }
#endif
  
  gavftools_cleanup();
  
  return EXIT_SUCCESS;
  }

