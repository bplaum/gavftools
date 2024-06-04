
#include <gavl/gavl.h>
#include <gmerlin/mediaconnector.h>

typedef struct gavf_s gavf_t;

gavf_t * gavf_create();

/*
 * URIs:
 * -:                                  Pipe (will be redirected to UNIX sockets)
 * gavf-server://0.0.0.0               TCP Server
 * gavf-server:///path-to-UNIX-socket  Unix domain server
 * gavf://host:port                    TCP Client
 * gavf:///path-to-UNIX-socket         TCP Client
 * 
 * file.gavf: On disk file
 */

#define GAVF_PROTOCOL_SERVER "gavf-server"
#define GAVF_PROTOCOL        "gavf"

#define GAVF_PROGRAM_HEADER "GAVFHEAD"
#define GAVF_PACKETS        "GAVFPACK"

int gavf_open_read(gavf_t * g, const char * uri);
int gavf_open_write(gavf_t * g, const char * uri);

bg_media_sink_t * gavf_get_sink(gavf_t * g);
bg_media_source_t * gavf_get_source(gavf_t * g);

/* Start the instance */
int gavf_start(gavf_t * g);



void gavf_destroy(gavf_t * g);

