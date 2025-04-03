#include <stdlib.h>


#include <config.h>

#include <gavl/gavl.h>
#include <gavl/log.h>
#define LOG_DOMAIN "gavf-read"

#include <gmerlin/translation.h>
#include <gmerlin/cmdline.h>
#include <gmerlin/pluginregistry.h>
#include <gmerlin/utils.h>

#include <gavftools.h>


static bg_cmdline_arg_t global_options[] =
  {
    GAVFTOOLS_OPT_SRC,
    { /* End */ }
  };

const bg_cmdline_app_data_t app_data =
  {
    .package =  PACKAGE,
    .version =  VERSION,
    .synopsis = TRS("[options]\n"),
    .help_before = TRS("Print info about a gavf stream\n"),
    .args = (bg_cmdline_arg_array_t[]) { { TRS("Options"), global_options },
                                         {  } },
  };

int main(int argc, char ** argv)
  {
  /* Global initialization */
  gavftools_init();
  
  bg_cmdline_init(&app_data);
  bg_cmdline_parse(global_options, &argc, &argv, NULL);
  
  /* Initialize source */
  if(!gavftools_init_src())
    return EXIT_FAILURE;

  fprintf(stderr, "Loaded %s\n", gavftools_src_location);
  gavl_dictionary_dump(gavftools_src->track, 2);
  fprintf(stderr, "\n");
  
  return EXIT_SUCCESS;
  }
