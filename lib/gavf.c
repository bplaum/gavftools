
#include <stdlib.h>
#include <string.h>

#include <gavf.h>
#include <gavfprivate.h>
#include <gavl/metatags.h>
#include <gavl/log.h>
#define LOG_DOMAIN "gavf"

#include <gavl/utils.h>

#include <gavl/gavlsocket.h>

#define HAS_HW 0

static int select_track(gavf_reader_t * g, gavl_msg_t * msg)
  {
  gavl_dictionary_t * dict;
  int track = gavl_msg_get_arg_int(msg, 0);
  
  /* Multitrack support */
  bg_media_source_cleanup(&g->src);
  bg_media_source_init(&g->src);
  
  /* Build media source structure */
  if(!(dict = gavl_get_track_nc(&g->mi, track)))
    return 0;

  bg_media_source_set_from_track(&g->src, dict);

  if(g->flags & FLAG_ON_DISK)
    {
    /* TODO: (re)position file */
    }
  else if(g->flags & FLAG_BACKCHANNEL)
    {
    
    }
  
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


static void send_stream_action(gavf_reader_t * g,
                               gavl_stream_type_t type)
  {
  int i;
  int num;
  bg_media_source_stream_t * st;
  
  num = bg_media_source_get_num_streams(&g->src, type);

  for(i = 0; i < num; i++)
    {
    st = bg_media_source_get_stream(&g->src, type, i);

    if(st->action != BG_STREAM_ACTION_OFF)
      {
      gavl_msg_t msg;
      gavl_msg_init(&msg);
      gavl_msg_set_id_ns(&msg, GAVL_CMD_SRC_SET_STREAM_ACTION, GAVL_MSG_NS_SRC);
      gavl_msg_set_arg_int(&msg, 0, type);
      gavl_msg_set_arg_int(&msg, 1, i);
      gavl_msg_set_arg_int(&msg, 2, 1);
      gavl_msg_write(&msg, g->bkch_io);
      gavl_msg_free(&msg);
      }
    
    }
    
  }


#if 1
static void free_stream_priv(void * user_data)
  {
  gavf_reader_stream_t * priv = user_data;
  if(priv->buf)
    gavl_packet_buffer_destroy(priv->buf);
  if(priv->io)
    gavl_io_destroy(priv->io);
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

  fprintf(stderr, "start_read: %d\n", !!(g->flags & FLAG_SEPARATE_STREAMS));
  
  for(i = 0; i < g->src.num_streams; i++)
    {
    gavl_compression_info_t ci;
    gavf_reader_stream_t * sp;

    bg_media_source_stream_t * s = g->src.streams[i];
    
    if(s->action == BG_STREAM_ACTION_OFF)
      continue;
        
    s->user_data = create_stream_priv(g);
    s->free_user_data = free_stream_priv;
    
    sp = s->user_data;
    
    /* Open stream secific i/o */
    if(g->flags & FLAG_SEPARATE_STREAMS)
      {
      int fd;
      char * name;
      /* Connect stream socket */

      const char * uri = gavl_dictionary_get_string(gavl_stream_get_metadata(s->s), GAVL_META_URI);

      fprintf(stderr, "Got stream URI: %s\n", uri);
      //      gavl_dictionary_dump(g->src.streams[i]->s, 2);

      /* Unix client */
      if(!gavl_url_split(uri, NULL, NULL, NULL, NULL, NULL, &name))
        return 0;
      
      fd = gavl_socket_connect_unix(name);
      sp->io = gavl_io_create_socket(fd, -1, 1);
      free(name);
      }
    
    /* Set up packet sources */
    if(g->flags & FLAG_SEPARATE_STREAMS)
      {
      if(gavl_stream_is_continuous(g->src.streams[i]->s))
        {
        g->src.streams[i]->psrc_priv =                                                         
          gavl_packet_source_create(gavf_packet_read_separate,
                                    g->src.streams[i], GAVL_SOURCE_SRC_ALLOC, g->src.streams[i]->s);
        }
      else
        {
        g->src.streams[i]->psrc_priv =                                                         
          gavl_packet_source_create(gavf_packet_read_separate_noncont,
                                    g->src.streams[i], GAVL_SOURCE_SRC_ALLOC, g->src.streams[i]->s);
        }
      }
    else
      {
      if(gavl_stream_is_continuous(g->src.streams[i]->s))
        {
        g->src.streams[i]->psrc_priv =                                                         
          gavl_packet_source_create(gavf_packet_read_multiplex,
                                    g->src.streams[i], GAVL_SOURCE_SRC_ALLOC, g->src.streams[i]->s);
        }
      else
        {
        g->src.streams[i]->psrc_priv =                                                         
          gavl_packet_source_create(gavf_packet_read_multiplex_noncont,
                                    g->src.streams[i], GAVL_SOURCE_SRC_ALLOC, g->src.streams[i]->s);
        }
      
      sp->buf = gavl_packet_buffer_create(g->src.streams[i]->s);
      }
    g->src.streams[i]->psrc = g->src.streams[i]->psrc_priv;

    gavl_compression_info_init(&ci);
    gavl_stream_get_compression_info(g->src.streams[i]->s, &ci);
    
    if(ci.id == GAVL_CODEC_ID_NONE)
      {
      fprintf(stderr, "Uncompressed stream\n");
      }
    else if(g->src.streams[i]->action == BG_STREAM_ACTION_DECODE)
      {
      fprintf(stderr, "Requested decoding\n");
      
      }

    }
  
  /* Start decoders */

  if(!bg_media_source_load_decoders(&g->src))
    {
    /* Error */
    return 0;
    }
  
  return 1;
  }
#endif



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
          if(!select_track(g, msg))
            return 0;
          }
          break;
          
        case GAVL_CMD_SRC_START:
          {
          gavl_msg_t msg;
          gavl_dictionary_t track;
          if(g->flags & FLAG_ON_DISK)
            {
            if(!bg_media_source_load_decoders(&g->src))
              {
              /* Error */
              }
            }
          else if(g->bkch_io)
            {
            // fprintf(stderr, "Got start\n");
            send_stream_action(g, GAVL_STREAM_AUDIO);
            send_stream_action(g, GAVL_STREAM_VIDEO);
            send_stream_action(g, GAVL_STREAM_TEXT);
            send_stream_action(g, GAVL_STREAM_OVERLAY);
            }
          gavl_msg_init(&msg);
          gavl_msg_set_id_ns(&msg, GAVL_CMD_SRC_START, GAVL_MSG_NS_SRC);
          gavf_reader_write_gavf_message(g, &msg);
          gavl_msg_free(&msg);
          
          /* Read actual header */
          gavl_msg_init(&msg);
          if(!gavf_reader_read_gavf_message(g, &msg, -1))
            {
            gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                     "Reading final track info failed");
            
            gavl_msg_free(&msg);
            return 0;
            }
          if((msg.NS != GAVL_MSG_NS_SRC) ||
             (msg.ID != GAVL_MSG_SRC_STARTED))
            {
            gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                     "Unexpected message: %d %d", msg.NS, msg.ID);
            gavl_msg_free(&msg);
            return 0;
            }
          fprintf(stderr, "Got final header\n");

          gavl_dictionary_init(&track);
          gavl_msg_get_arg_dictionary(&msg, 0, &track);
          gavl_track_merge(g->src.track, &track);
          gavl_dictionary_free(&track);
          gavl_msg_free(&msg);

          /* Initialize sources */
          
          if(!start_read(g))
            return 0;
          
          }
          return 1;
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
  gavf_reader_write_gavf_message(g, msg);
  return 1;
  }


gavf_reader_t * gavf_reader_create()
  {
  gavf_reader_t * ret;
  
  ret = calloc(1, sizeof(*ret));

  bg_controllable_init(&ret->ctrl,
                       bg_msg_sink_create(handle_msg_read, ret, 1),
                       bg_msg_hub_create(1));

  return ret;
  }

gavf_writer_t * gavf_writer_create()
  {
  gavf_writer_t * ret;
  
  ret = calloc(1, sizeof(*ret));

  return ret;
  }

int gavf_reader_open(gavf_reader_t * g, const char * uri)
  {
  int ret = 0;
  int num_redirections = 0;
  
  if(!(g->io = gavf_reader_open_io(g, uri)))
    return 0;
  
  /* Read file structure */

  while(1)
    {
    //    fprintf(stderr, "Reading chunk header\n");
    if(!gavl_chunk_read_header(g->io, &g->ch))
      goto fail;

    //    fprintf(stderr, "Got chunk header %s\n", g->ch.eightcc);

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
        int io_flags;
        num_redirections++;
        if(num_redirections > 3)
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Too many redirections");
          goto fail;
          }
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got redirected to %s", new_uri);
        
        gavl_io_destroy(g->io);
        if(g->bkch_io)
          {
          gavl_io_destroy(g->bkch_io);
          g->bkch_io = NULL;
          }
        
        if(!(g->io = gavf_reader_open_io(g, new_uri)))
          goto fail;

        io_flags = gavl_io_get_flags(g->io);
  
        if(io_flags & GAVL_IO_IS_SOCKET)
          g->bkch_io = gavl_io_create_socket(gavl_io_get_socket(g->io), 3000, 0);
        
        gavl_dictionary_reset(&g->mi);
        }
      else
        {
#if 0
        fprintf(stderr, "Got media info:\n");
        gavl_dictionary_dump(&g->mi, 2);
        fprintf(stderr, "\n");
#endif
        break;
        }
      }
    }

  if(g->bkch_io)
    g->flags |= FLAG_SEPARATE_STREAMS;
  
  /* TODO: Read footer */
  if(gavl_io_can_seek(g->io))
    {
    //    int64_t pos = gavl_io_position(g->io);

    /* Read footer */
    }
  
  ret = 1;
  fail:

  if(!ret)
    {
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
  
  if(g->bkch_io)
    g->flags |= FLAG_SEPARATE_STREAMS;
  
  return 1;
  }


bg_media_source_t * gavf_reader_get_source(gavf_reader_t * g)
  {
  return &g->src;
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


static int create_packet_sink(gavf_writer_t * g, gavf_writer_stream_t * s)
  {
  if(g->flags & FLAG_SEPARATE_STREAMS)
    {
    s->psink = gavl_packet_sink_create(NULL, gavf_packet_put_separate, s);
    }
  else
    {
    /* Multiplex */
    s->packet_flags |= PACKET_HAS_STREAM_ID;
    s->buf = gavl_packet_buffer_create(s->s);
    s->psink = gavl_packet_sink_create(gavf_packet_get_multiplex, gavf_packet_put_multiplex, s);
    }
  return 1;
  }

static gavl_audio_frame_t * get_audio_frame_func_separate_write(void * priv)
  {
  gavf_writer_stream_t * s = priv;
  gavl_audio_frame_set_from_packet(s->aframe,
                                   gavl_audio_sink_get_format(s->asink),
                                   &s->pkt_priv);
  return s->aframe;
  }

static gavl_sink_status_t put_audio_frame_func_separate_write(void * priv, gavl_audio_frame_t * frame)
  {
  gavf_writer_stream_t * s = priv;
  gavl_audio_frame_to_packet_metadata(s->aframe, &s->pkt_priv);
  return gavl_packet_sink_put_packet(s->psink, &s->pkt_priv);
  }

static gavl_audio_frame_t * get_audio_frame_func_multiplex(void * priv)
  {
  gavf_writer_stream_t * s = priv;
  
  s->pkt = gavl_packet_sink_get_packet(s->psink);
  gavl_audio_frame_set_from_packet(s->aframe,
                                   gavl_audio_sink_get_format(s->asink),
                                   s->pkt);
  
  return s->aframe;
  }

static gavl_sink_status_t put_audio_frame_func_multiplex(void * priv, gavl_audio_frame_t * frame)
  {
  gavl_sink_status_t ret;
  gavf_writer_stream_t * s = priv;
  
  gavl_audio_frame_to_packet_metadata(frame, s->pkt);
  ret = gavl_packet_sink_put_packet(s->psink, s->pkt);
  s->pkt = NULL;
  return ret;
  }

static int create_audio_sink(gavf_writer_t * g, gavf_writer_stream_t * s, gavl_audio_source_t * src)
  {
  if(!create_packet_sink(g, s))
    return 0;
  
  if(g->flags & FLAG_SEPARATE_STREAMS)
    {
    if(HAS_HW)
      {
      
      }
    else
      {
      s->asink = gavl_audio_sink_create(get_audio_frame_func_separate_write,
                                        put_audio_frame_func_separate_write, s,
                                        gavl_audio_source_get_src_format(src));
      }
    
    }
  else
    {
    s->asink = gavl_audio_sink_create(get_audio_frame_func_multiplex,
                                      put_audio_frame_func_multiplex, s,
                                      gavl_audio_source_get_src_format(src));
    s->aframe = gavl_audio_frame_create(NULL);
    }
  return 1;
  }

static gavl_video_frame_t * get_video_frame_func_separate_write(void * priv)
  {
  gavf_writer_stream_t * s = priv;
  gavl_video_frame_set_from_packet(s->vframe,
                                   gavl_video_sink_get_format(s->vsink),
                                   &s->pkt_priv);
  return s->vframe;
  }

static gavl_sink_status_t put_video_frame_func_separate_write(void * priv, gavl_video_frame_t * frame)
  {
  gavf_writer_stream_t * s = priv;
  gavl_video_frame_to_packet_metadata(s->vframe, s->pkt);
  return gavl_packet_sink_put_packet(s->psink, &s->pkt_priv);
  }

static gavl_video_frame_t * get_video_frame_func_multiplex(void * priv)
  {
  gavf_writer_stream_t * s = priv;
  s->pkt = gavl_packet_sink_get_packet(s->psink);
  gavl_video_frame_set_from_packet(s->vframe, gavl_video_sink_get_format(s->vsink), s->pkt);
  return s->vframe;
  }

static gavl_sink_status_t put_video_frame_func_multiplex(void * priv, gavl_video_frame_t * frame)
  {
  gavl_sink_status_t st;
  gavf_writer_stream_t * s = priv;
  gavl_video_frame_to_packet_metadata(frame, s->pkt);
  st = gavl_packet_sink_put_packet(s->psink, s->pkt);
  s->pkt = NULL;
  return st;
  }

static int create_video_sink(gavf_writer_t * g, gavf_writer_stream_t * s, gavl_video_source_t * src)
  {
  if(!create_packet_sink(g, s))
    return 0;
  
  if(g->flags & FLAG_SEPARATE_STREAMS)
    {
    if(HAS_HW)
      {
      }
    else
      {
      s->vsink = gavl_video_sink_create(get_video_frame_func_separate_write,
                                        put_video_frame_func_separate_write, s,
                                        gavl_video_source_get_src_format(src));
      }
    }
  else
    {
    s->vsink = gavl_video_sink_create(get_video_frame_func_multiplex,
                                      put_video_frame_func_multiplex, s,
                                      gavl_video_source_get_src_format(src));
    s->vframe = gavl_video_frame_create(NULL);
    }
  return 1;
  }


int gavf_writer_init(gavf_writer_t * g, bg_media_source_t * src)
  {
  int i;
  int idx;
  const gavl_dictionary_t * m_src;
  gavl_dictionary_t * m_dst;
  
  g->num_streams = 0;
  g->track = gavl_append_track(&g->mi, NULL);

  /* Append track and copy metadata */
  m_src = gavl_track_get_metadata(src->track);
  m_dst = gavl_track_get_metadata_nc(g->track);
  gavl_dictionary_copy(m_dst, m_src);

  /* Count and allocate streams */
  for(i = 0; i < src->num_streams; i++)
    {
    if(src->streams[i]->action != BG_STREAM_ACTION_OFF)
      g->num_streams++;
    }

  idx = 0;

  g->streams = calloc(g->num_streams, sizeof(*g->streams));

  /* Set up streams */
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
          if(!create_packet_sink(g, &g->streams[idx]))
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
          create_packet_sink(g, &g->streams[idx]);
          gavl_video_format_copy(gavl_stream_get_video_format_nc(g->streams[idx].s),
                                 gavl_stream_get_video_format(src->streams[i]->s));
          }
        break;
      case GAVL_STREAM_TEXT:
        if(src->streams[i]->psrc)
          create_packet_sink(g, &g->streams[idx]);
        break;
      case GAVL_STREAM_MSG:
        if(src->streams[i]->msghub)
          {
          
          }
        else if(src->streams[i]->psrc)
          create_packet_sink(g, &g->streams[idx]);
        break;
      case GAVL_STREAM_NONE:
        break;
      }

    idx++;
    }

  if(g->flags & FLAG_SEPARATE_STREAMS)
    {
    /* Init separate */
    for(i = 0; i < g->num_streams; i++)
      {
      char * name;
      char * uri;
      gavf_writer_stream_t * s = &g->streams[i];
      
      s->server_fd = gavl_unix_socket_create(&name, 1);

      uri = gavl_sprintf("%s://%s", GAVF_PROTOCOL, name);
      fprintf(stderr, "Set stream URI: %s\n", uri);
      gavl_dictionary_set_string_nocopy(gavl_stream_get_metadata_nc(s->s), GAVL_META_URI, uri);
      free(name);
      }
    }
  else
    gavl_chunk_start(g->io, &g->ch, GAVF_PACKETS);
  
  if(g->bkch_io)
    {
    gavl_msg_t msg;
    fprintf(stderr, "Sending final track info\n");
    
    /* Send final track info */
    gavl_msg_init(&msg);
    gavl_msg_set_id_ns(&msg, GAVL_MSG_SRC_STARTED, GAVL_MSG_NS_SRC);
    gavl_msg_set_arg_dictionary(&msg, 0, g->track);
    if(!gavf_writer_write_gavf_message(g, &msg))
      {
      gavl_msg_free(&msg);
      return 0;
      }
    gavl_msg_free(&msg);

    fprintf(stderr, "Sent final track info\n");
    
    }

  /* Wait for the stream sockets to connect */
  
  if(g->flags & FLAG_SEPARATE_STREAMS)
    {
    /* Init separate */
    for(i = 0; i < g->num_streams; i++)
      {
      int socket_fd;
      gavf_writer_stream_t * s = &g->streams[i];

      if((socket_fd = gavl_listen_socket_accept(s->server_fd, 3000, NULL)) < 0)
        return 0;
      fprintf(stderr, "Accepted stream socket %d\n", socket_fd);
      s->io = gavl_io_create_socket(socket_fd, -1, 1);
      gavl_listen_socket_destroy(s->server_fd);
      s->server_fd = -1;
      }
    }
  
  return 1;
  }

int gavf_writer_send_media_info(gavf_writer_t * g, const gavl_dictionary_t * mi)
  {
  gavl_io_t * sub_io;
  sub_io = gavl_chunk_start_io(g->io, &g->ch, GAVF_HEADER);
  if(!gavl_dictionary_write(sub_io, mi))
    return 0;
  gavl_chunk_finish_io(g->io, &g->ch, sub_io);
  return 1;
  }

int gavf_writer_has_backchannel(gavf_writer_t * g)
  {
  return g->bkch_io ? 1 : 0;
  }

/* Control messages */

int gavf_reader_write_gavf_message(gavf_reader_t * g, const gavl_msg_t * msg)
  {
  if(!g->bkch_io)
    return 1;
  
  return gavl_msg_write(msg, g->bkch_io);
  }

int gavf_writer_read_gavf_message(gavf_writer_t * wr, gavl_msg_t * ret, int timeout)
  {
  if(!wr->bkch_io)
    return -1;
  if(!gavl_io_can_read(wr->bkch_io, timeout))
    return 0;
  if(gavl_msg_read(ret, wr->bkch_io))
    return 1;
  return -1;
  }

int gavf_reader_read_gavf_message(gavf_reader_t * rd, gavl_msg_t * ret, int timeout)
  {
  gavl_chunk_t ch;

  if((timeout > 0) && !gavl_io_can_read(rd->io, timeout))
    return 0;
  
  if(!gavl_chunk_read_header(rd->io, &ch))
    return 0;

  if(!gavl_chunk_is(&ch, GAVF_MSG))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Expected gavf message");
    return 0;
    }

  gavl_msg_read(ret, rd->io);
  
  return 1;
  }

int gavf_writer_write_gavf_message(gavf_writer_t * wr, const gavl_msg_t * msg)
  {
  gavl_io_t * sub_io;
  gavl_chunk_t ch;

  //  fprintf(stderr, "gavf_writer_write_gavf_message:\n");
  //  gavl_msg_dump(msg, 2);
  
  sub_io = gavl_chunk_start_io(wr->io, &ch, GAVF_MSG);
  gavl_msg_write(msg, sub_io);
  gavl_chunk_finish_io(wr->io, &ch, sub_io);
  return 1;
  }

gavl_audio_sink_t * gavf_writer_get_audio_sink(gavf_writer_t * g,
                                               int stream)
  {
  gavf_writer_stream_t * s = &g->streams[stream];
  return s->asink;
  }

gavl_video_sink_t * gavf_writer_get_video_sink(gavf_writer_t * g,
                                               int stream)
  {
  gavf_writer_stream_t * s = &g->streams[stream];
  return s->vsink;

  }

gavl_packet_sink_t * gavf_writer_get_packet_sink(gavf_writer_t * g,
                                                 int stream)
  {
  gavf_writer_stream_t * s = &g->streams[stream];
  return s->psink;
  
  }

bg_msg_sink_t * gavf_writer_get_message_sink(gavf_writer_t * g,
                                             int stream)
  {
  gavf_writer_stream_t * s = &g->streams[stream];
  return s->msink;
  }
