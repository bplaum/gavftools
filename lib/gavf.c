
#include <stdlib.h>
#include <string.h>

#include <gavf.h>
#include <gavfprivate.h>
#include <gavl/metatags.h>
#include <gavl/log.h>
#define LOG_DOMAIN "gavf"

#include <gavl/utils.h>

#include <gavl/gavlsocket.h>


gavf_t * gavf_create()
  {
  gavf_t * ret;
  
  ret = calloc(1, sizeof(*ret));
  return ret;
  }


int gavf_open_read(gavf_t * g, const char * uri)
  {
  int ret;
  int num_redirections = 0;
  
  if(!(g->io = gavf_open_io(g, uri, 0)))
    return 0;
  
  /* Read file structure */

  while(1)
    {
    if(!gavl_chunk_read_header(g->io, &g->ch))
      goto fail;

    if(gavl_chunk_is(&g->ch, GAVF_PROGRAM_HEADER))
      {
      const gavl_dictionary_t * m;
      const char * klass;
      const char * new_uri;
      
      g->src.track_priv = gavl_dictionary_create();
      g->src.track = g->src.track_priv;
      
      if(!gavl_dictionary_read(g->io, g->src.track))
        goto fail;
            
      /* Check for redirection */
      if((m = gavl_track_get_metadata(g->src.track)) &&
         (klass = gavl_dictionary_get_string(m, GAVL_META_CLASS)) &&
         !strcmp(klass, GAVL_META_CLASS_LOCATION) &&
         gavl_track_get_src(g->src.track, GAVL_META_SRC, 0, NULL, &new_uri))
        {
        num_redirections++;
        if(num_redirections > 3)
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Too many redirections");
          goto fail;
          }
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got redirected to %s", new_uri);

        gavl_io_destroy(g->io);
        
        if(!(g->io = gavf_open_io(g, new_uri, 0)))
          goto fail;

        gavl_dictionary_destroy(g->src.track_priv);
        g->src.track_priv = NULL;
        g->src.track = NULL;
        }
#if 0      
      else if(gavl_track_get_num_streams_all(g->src.track) > 0)
        {
        
        }
#endif
      }
    else if(gavl_chunk_is(&g->ch, GAVF_PACKETS))
      break;
    }
  
  if(gavl_io_can_seek(g->io))
    {
    //    int64_t pos = gavl_io_position(g->io);

    /* Read footer */
    }
  
  ret = 1;
  fail:

  if(!ret)
    {
    if(g->src.track_priv)
      gavl_dictionary_destroy(g->src.track_priv);
    g->src.track_priv = NULL;
    g->src.track = NULL;
    }
  
  return ret;
  }

int gavf_open_write(gavf_t * g, const char * uri)
  {
  int io_flags;
  if(!(g->io = gavf_open_io(g, uri, 1)))
    return 0;
  
  g->flags |= FLAG_WRITE;

  io_flags = gavl_io_get_flags(g->io);

  if(io_flags & GAVL_IO_IS_LOCAL) // Or check for GAVL_IO_IS_UNIX_SOCKET?
    g->flags |= FLAG_SEPARATE_STREAMS;
  
  return 1;
  }

bg_media_sink_t * gavf_get_sink(gavf_t * g)
  {
  return &g->sink;
  }

bg_media_source_t * gavf_get_source(gavf_t * g)
  {
  return &g->src;
  }

static void free_stream_priv(void * user_data)
  {
  gavf_stream_t * priv = user_data;
  if(priv->buf)
    gavl_packet_buffer_destroy(priv->buf);
  free(priv);
  }

static void * create_stream_priv(gavf_t * g)
  {
  gavf_stream_t * ret = calloc(1, sizeof(*ret));
  ret->g = g;
  return ret;
  }

static void init_multiplex_write(gavf_t * g)
  {
  int i;
  for(i = 0; i < g->sink.num_streams; i++)
    {
    gavf_stream_t * gs = g->sink.streams[i]->user_data;
    gs->packet_flags |= PACKET_HAS_STREAM_ID;
    gs->buf = gavl_packet_buffer_create(g->sink.streams[i]->s);

    g->sink.streams[i]->psink_priv = gavl_packet_sink_create(gavf_packet_get_multiplex,
                                                             gavf_packet_put_multiplex,
                                                             &g->sink.streams[i]);
    }
  }

static void init_separate_write(gavf_t * g)
  {
  int i;
  for(i = 0; i < g->sink.num_streams; i++)
    {
    char * name;
    char * stream_uri;
    
    gavf_stream_t * gs = g->sink.streams[i]->user_data;

    gs->server_fd = gavl_unix_socket_create(&name, 1);

    stream_uri = gavl_sprintf("%s://%s", GAVF_PROTOCOL, name);
    
    g->sink.streams[i]->psink_priv = gavl_packet_sink_create(NULL,
                                                             gavf_packet_put_separate,
                                                             &g->sink.streams[i]);
    free(stream_uri);
    free(name);
    }
  
  }

static int start_write(gavf_t * g)
  {
  gavl_io_t * io;
  int i;
  
  if(!g->sink.track)
    return 0;
  
  /* Finalize track */
  for(i = 0; i < g->sink.num_streams; i++)
    {
    g->sink.streams[i]->user_data = create_stream_priv(g);
    g->sink.streams[i]->free_user_data = free_stream_priv;
    }

  if(g->flags & FLAG_SEPARATE_STREAMS)
    init_separate_write(g);
  else
    init_multiplex_write(g);
  
  /* Write program header and start packet chunk */
  
  io = gavl_chunk_start_io(g->io, &g->ch, GAVF_PROGRAM_HEADER);
  gavl_dictionary_write(io, g->sink.track);
  gavl_chunk_finish_io(g->io, &g->ch, io);
  
  
  return 1;
  }


static int start_read(gavf_t * g)
  {
  int i;
  for(i = 0; i < g->src.num_streams; i++)
    {
    gavf_stream_t * priv = calloc(1, sizeof(*priv));
    g->src.streams[i]->user_data = create_stream_priv(g);
    g->src.streams[i]->free_user_data = free_stream_priv;
    }
  
  /* Set up packet sources */
  
  
  /* Start decoders */
  
  return 0;
  }

/* Start the instance */
int gavf_start(gavf_t * g)
  {
  if(g->flags & FLAG_WRITE)
    return start_write(g);
  else
    return start_read(g);
  }

void gavf_destroy(gavf_t * g)
  {
  
  }

gavl_source_status_t gavf_demux_iteration(gavf_t * g)
  {
  gavl_source_status_t st;
  int stream_id;
  bg_media_source_stream_t * s;
  
  if((st = gavf_peek_packet(g->io, &stream_id)) != GAVL_SOURCE_OK)
    return st;

  if(((s = bg_media_source_get_stream_by_id(&g->src, stream_id))) &&
     (s->action != BG_STREAM_ACTION_OFF))
    {
    gavf_stream_t * gs;
    /* Read packet */
    gavl_packet_t * p;
    gs = s->user_data;
    p = gavl_packet_sink_get_packet(gavl_packet_buffer_get_sink(gs->buf));
    if((st = gavf_read_packet(g->io, p, gs->packet_flags)) != GAVL_SOURCE_OK)
      return st;
    }
  else
    {
    /* Skip */
    if((st = gavf_skip_packet(g->io, PACKET_HAS_STREAM_ID)) != GAVL_SOURCE_OK)
      return st;
    }
  return GAVL_SOURCE_OK;
  }

gavl_sink_status_t gavf_mux_iteration(gavf_t * g)
  {
  return GAVL_SINK_ERROR;
  }

