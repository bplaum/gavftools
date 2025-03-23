
#include <stdlib.h>
#include <string.h>

#include <gavf.h>
#include <gavfprivate.h>
#include <gavl/metatags.h>
#include <gavl/log.h>
#define LOG_DOMAIN "gavf"

#include <gavl/utils.h>

#include <gavl/gavlsocket.h>

static int select_track(gavf_reader_t * g, gavl_msg_t * msg)
  {
  gavl_dictionary_t * dict;
  int track = gavl_msg_get_arg_int(msg, 0);
  
  /* TODO: Multitrack support */
  bg_media_source_cleanup(&g->src);
  bg_media_source_init(&g->src);
  
  /* Build media source structure */
  if((dict = gavl_get_track_nc(&g->mi, track)))
    bg_media_source_set_from_track(&g->src, dict);
  return 1;
  }
#if 0
static int drain_read(gavf_reader_t * g)
  {
  while(1)
    {
    
    }
  
  }
#endif

static int forward_msg_to_source(gavf_reader_t * g, const gavl_msg_t * msg)
  {
  if(!g->bkch_io)
    return 1;
  
  return gavl_msg_write(msg, g->bkch_io);
  }

static int handle_msg_read(void * data, gavl_msg_t * msg)
  {
  gavf_reader_t * g = data;

  //  int do_resync = 0;
  //  int do_restart = 0;
  
  switch(msg->NS)
    {
    case GAVL_MSG_NS_SRC:
      switch(msg->ID)
        {
        case GAVL_CMD_SRC_SELECT_TRACK:
          {
          if(g->flags & FLAG_ON_DISK)
            {
            if(!select_track(g, msg))
              {
              
              }
            }
          
          }
          break;
          
        case GAVL_CMD_SRC_START:
          if(g->flags & FLAG_ON_DISK)
            {
            if(!bg_media_source_load_decoders(&g->src))
              {
              /* Error */
              }
            
            }
          else
            {
            
            }
          
          break;
        case GAVL_CMD_SRC_SEEK:
          {
          if(g->flags & FLAG_ON_DISK)
            {
            //            int64_t time = gavl_msg_get_arg_long(msg, 0);
            //            int scale = gavl_msg_get_arg_int(msg, 1);

            return 1;
            }
          }
          break;
        }
    }
  forward_msg_to_source(g, msg);
  return 1;
  }


gavf_reader_t * gavf_reader_create()
  {
  gavf_reader_t * ret;
  
  ret = calloc(1, sizeof(*ret));

  //  ret->socket_fd = -1;
  
  bg_controllable_init(&ret->ctrl,
                       bg_msg_sink_create(handle_msg_read, ret, 0),
                       bg_msg_hub_create(1));

  return ret;
  }

gavf_writer_t * gavf_writer_create()
  {
  gavf_writer_t * ret;
  
  ret = calloc(1, sizeof(*ret));

  //  ret->socket_fd = -1;
  
  return ret;
  }


int gavf_reader_open(gavf_reader_t * g, const char * uri)
  {
  int ret;
  int num_redirections = 0;
  
  if(!(g->io = gavf_reader_open_io(g, uri)))
    return 0;
  
  /* Read file structure */

  while(1)
    {
    if(!gavl_chunk_read_header(g->io, &g->ch))
      goto fail;

    if(gavl_chunk_is(&g->ch, GAVF_HEADER))
      {
      const gavl_dictionary_t * m;
      const gavl_dictionary_t * t;
      const char * klass;
      const char * new_uri;

      if(!gavl_dictionary_read(g->io, &g->mi))
        goto fail;
      
      /* Check for redirection */
      
      if((t = gavl_get_track(&g->mi, 0)) &&
         (m = gavl_track_get_metadata(t)) &&
         (klass = gavl_dictionary_get_string(m, GAVL_META_CLASS)) &&
         !strcmp(klass, GAVL_META_CLASS_LOCATION) &&
         gavl_track_get_src(t, GAVL_META_SRC, 0, NULL, &new_uri))
        {
        num_redirections++;
        if(num_redirections > 3)
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Too many redirections");
          goto fail;
          }
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got redirected to %s", new_uri);

        gavl_io_destroy(g->io);
        
        if(!(g->io = gavf_reader_open_io(g, new_uri)))
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

int gavf_writer_open(gavf_writer_t * g, const char * uri)
  {
  int io_flags;
  if(!(g->io = gavf_writer_open_io(g, uri)))
    return 0;
  
  io_flags = gavl_io_get_flags(g->io);

  if(io_flags & GAVL_IO_IS_TTY) // TTY
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Won't write binary data to a terminal");
    return 0;
    }
  
  if(io_flags & GAVL_IO_IS_LOCAL) // Or check for GAVL_IO_IS_UNIX_SOCKET?
    g->flags |= FLAG_SEPARATE_STREAMS;
  
  return 1;
  }


bg_media_source_t * gavf_reader_get_source(gavf_reader_t * g)
  {
  return &g->src;
  }

static void free_stream_priv(void * user_data)
  {
  gavf_reader_stream_t * priv = user_data;
  if(priv->buf)
    gavl_packet_buffer_destroy(priv->buf);
  free(priv);
  }

static void * create_stream_priv(gavf_reader_t * r)
  {
  gavf_reader_stream_t * ret = calloc(1, sizeof(*ret));
  ret->reader = r;
  return ret;
  }

static int start_read(gavf_reader_t * g)
  {
  int i;
  for(i = 0; i < g->src.num_streams; i++)
    {
    gavf_reader_stream_t * priv = calloc(1, sizeof(*priv));
    g->src.streams[i]->user_data = create_stream_priv(g);
    g->src.streams[i]->free_user_data = free_stream_priv;
    }
  
  /* Set up packet sources */
    
  /* Start decoders */
  
  return 0;
  }

void gavf_reader_destroy(gavf_reader_t * g)
  {
  
  }

void gavf_writer_destroy(gavf_writer_t * g)
  {
  
  }

gavl_source_status_t gavf_demux_iteration(gavf_reader_t * g)
  {
  gavl_source_status_t st;
  bg_media_source_stream_t * s;

  gavl_packet_t header;
  gavl_packet_init(&header);
  
  if((st = gavf_read_packet_header(g->io, &header)) != GAVL_SOURCE_OK)
    return st;

  if(header.id == GAVL_META_STREAM_ID_MSG_GAVF)
    {
    /* TODO: Maybe read and report message */
    return GAVL_SOURCE_EOF;
    }
  
  if(((s = bg_media_source_get_stream_by_id(&g->src, header.id))) &&
     (s->action != BG_STREAM_ACTION_OFF))
    {
    gavf_reader_stream_t * gs;
    /* Read packet */
    gavl_packet_t * p;
    gs = s->user_data;
    p = gavl_packet_sink_get_packet(gavl_packet_buffer_get_sink(gs->buf));
    if((st = gavf_read_packet(g->io, p)) != GAVL_SOURCE_OK)
      return st;
    }
  else
    {
    /* Skip */
    if((st = gavf_packet_skip_data(g->io, &header)) != GAVL_SOURCE_OK)
      return st;
    }
  return GAVL_SOURCE_OK;
  }

gavl_sink_status_t gavf_mux_iteration(gavf_writer_t * g)
  {
  return GAVL_SINK_ERROR;
  }

bg_controllable_t * gavf_reader_get_controllable(gavf_reader_t * g)
  {
  return &g->ctrl;
  }

int gavf_reader_skip_packets(gavf_reader_t * g)
  {
  gavl_source_status_t st;
  gavl_packet_t p;
  gavl_packet_init(&p);
  while(1)
    {
    gavl_packet_init(&p);
    if((st = gavf_read_packet_header(g->io, &p)) != GAVL_SOURCE_OK)
      return 0;

    if(p.id == GAVL_META_STREAM_ID_MSG_GAVF)
      {
      /* TODO: Parse packet */
      }
    else
      {
      gavf_packet_skip_data(g->io, &p);
      }
    }


  }

int gavf_writer_reset(gavf_reader_t * g, int msg_id)
  {
  return 0;
  }


static int create_packet_sink(gavf_writer_t * g, gavf_writer_stream_t * s, gavl_packet_source_t * src)
  {
  if(g->flags & FLAG_SEPARATE_STREAMS)
    {
    char * name;
    char * stream_uri;
    
    s->server_fd = gavl_unix_socket_create(&name, 1);

    stream_uri = gavl_sprintf("%s://%s", GAVF_PROTOCOL, name);
    
    s->psink = gavl_packet_sink_create(NULL, gavf_packet_put_separate, s);

    free(stream_uri);
    free(name);

    }
  else
    {
    /* Multiplex */
    s->packet_flags |= PACKET_HAS_STREAM_ID;
    s->buf = gavl_packet_buffer_create(s->s);
    
    s->psink = gavl_packet_sink_create(gavf_packet_get_multiplex, gavf_packet_put_multiplex, s);
    
    }
  return 0;
  }

static int create_audio_sink(gavf_writer_t * g, gavf_writer_stream_t * s, gavl_audio_source_t * src)
  {
  return 0;
  
  }

static int create_video_sink(gavf_writer_t * g, gavf_writer_stream_t * s, gavl_video_source_t * src)
  {
  return 0;
  
  }

static int handle_program_message(void * data, gavl_msg_t * msg)
  {
  /* Message to packet */
  return 0;
  
  }

int gavf_writer_init(gavf_writer_t * g, bg_media_source_t * src)
  {
  int i;
  int idx;
  const gavl_dictionary_t * m_src;
  gavl_dictionary_t * m_dst;
  gavl_io_t * sub_io;
  
  g->num_streams = 0;

  g->track = gavl_append_track(&g->mi, NULL);

  m_src = gavl_track_get_metadata(src->track);
  m_dst = gavl_track_get_metadata_nc(g->track);
  gavl_dictionary_copy(m_dst, m_src);
  
  for(i = 0; i < src->num_streams; i++)
    {
    if(src->streams[i]->action != BG_STREAM_ACTION_OFF)
      g->num_streams++;
    }

  idx = 0;

  g->streams = calloc(g->num_streams, sizeof(*g->streams));
  
  for(i = 0; i < src->num_streams; i++)
    {
    if(src->streams[i]->action == BG_STREAM_ACTION_OFF)
      continue;


    g->streams[idx].s = gavl_track_append_stream(g->track, src->streams[i]->type);
    
    m_dst = gavl_stream_get_metadata_nc(g->streams[idx].s);
    m_src = gavl_stream_get_metadata(src->streams[i]->s);
    gavl_dictionary_copy(m_dst, m_src);
    
    switch(src->streams[i]->type)
      {
      case GAVL_STREAM_AUDIO:
        
        if(src->streams[i]->asrc)
          {
          const gavl_audio_format_t * fmt = gavl_audio_source_get_src_format(src->streams[i]->asrc);
          
          if(!create_audio_sink(g, &g->streams[idx], src->streams[i]->asrc))
            return 0;
          gavl_audio_format_copy(gavl_stream_get_audio_format_nc(g->streams[idx].s), fmt);
          }
        else if(src->streams[i]->psrc)
          {
          if(!create_packet_sink(g, &g->streams[idx], src->streams[i]->psrc))
            return 0;
          
          gavl_audio_format_copy(gavl_stream_get_audio_format_nc(g->streams[idx].s),
                                 gavl_stream_get_audio_format(src->streams[i]->s));
          }
        break;
      case GAVL_STREAM_VIDEO:
      case GAVL_STREAM_OVERLAY:
        if(src->streams[i]->vsrc)
          {
          create_video_sink(g, &g->streams[idx], src->streams[i]->vsrc);
          gavl_video_format_copy(gavl_stream_get_video_format_nc(g->streams[idx].s),
                                 gavl_video_source_get_src_format(src->streams[i]->vsrc));
          }
        else if(src->streams[i]->psrc)
          {
          create_packet_sink(g, &g->streams[idx], src->streams[i]->psrc);
          gavl_video_format_copy(gavl_stream_get_video_format_nc(g->streams[idx].s),
                                 gavl_stream_get_video_format(src->streams[i]->s));
          }
        break;
      case GAVL_STREAM_TEXT:
        if(src->streams[i]->psrc)
          create_packet_sink(g, &g->streams[idx], src->streams[i]->psrc);
        
        break;
      case GAVL_STREAM_MSG:
        if(src->streams[i]->msghub)
          {
          
          }
        else if(src->streams[i]->psrc)
          {
          
          }
        break;
      case GAVL_STREAM_NONE:
        break;
      }
    
    }

  sub_io = gavl_chunk_start_io(g->io, &g->ch, GAVF_TRACK);
  gavl_dictionary_write(sub_io, g->track);
  gavl_chunk_finish_io(g->io, &g->ch, sub_io);

  if(g->flags & FLAG_SEPARATE_STREAMS)
    {
    
    }
  else
    gavl_chunk_start(g->io, &g->ch, GAVF_PACKETS);
  
  
  return 1;
  }

int gavf_writer_send_media_info(gavf_writer_t * g, const gavl_dictionary_t * mi)
  {
  gavl_io_t * sub_io;
  sub_io = gavl_chunk_start_io(g->io, &g->ch, GAVF_HEADER);
  if(gavl_dictionary_write(sub_io, mi))
    return 0;
  gavl_chunk_finish_io(g->io, &g->ch, sub_io);
  return 1;
  }


int gavf_writer_read_message(gavf_writer_t * wr, gavl_msg_t * ret, int timeout)
  {
  if(!gavl_io_can_read(wr->bkch_io, timeout))
    return 0;
  if(gavl_msg_read(ret, wr->bkch_io))
    return 1;
  return -1;
  }
