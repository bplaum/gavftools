
#include <string.h>
#include <unistd.h>

#include <gavf.h>
#include <gavfprivate.h>
#include <gavl/connectors.h>
#include <gavl/hw.h>
#include <gavl/hw_memfd.h>
#include <gavl/hw_dmabuf.h>

#include <gavl/log.h>
#define LOG_DOMAIN "frame_io"


static gavl_source_status_t read_audio_frame_ram(void * data, gavl_audio_frame_t ** frame)
  {
  gavl_source_status_t status;
  gavl_packet_t * pkt = NULL;
  bg_media_source_stream_t * s = data;
  gavf_reader_stream_t * st = s->user_data;

  gavf_reader_poll_msg(st->reader);
    
  if((status = gavl_packet_source_read_packet(s->psrc, &pkt)) != GAVL_SOURCE_OK)
    return status;
  
  gavl_audio_frame_set_from_packet(st->aframe,
                                   gavl_stream_get_audio_format(s->s),
                                   pkt);

  *frame = st->aframe;
  
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t read_video_frame_ram(void * data, gavl_video_frame_t ** frame)
  {
  gavl_source_status_t status;
  gavl_packet_t * pkt = NULL;
  bg_media_source_stream_t * s = data;
  gavf_reader_stream_t * st = s->user_data;

  gavf_reader_poll_msg(st->reader);

  if((status = gavl_packet_source_read_packet(s->psrc, &pkt)) != GAVL_SOURCE_OK)
    return status;
  
  gavl_video_frame_set_from_packet(st->vframe,
                                   gavl_stream_get_video_format(s->s),
                                   pkt);

  *frame = st->vframe;

  return GAVL_SOURCE_OK;
  }


static gavl_source_status_t read_audio_frame_memfd(void * data, gavl_audio_frame_t ** frame)
  {
  gavl_hw_buffer_t * buf;
  int num = 1;
  gavl_source_status_t status;
  bg_media_source_stream_t * s = data;
  gavf_reader_stream_t * st = s->user_data;

  if(st->aframe_hw)
    {
    gavl_hw_audio_frame_unref(st->aframe_hw);
    st->aframe_hw = NULL;
    }
  
  gavf_reader_poll_msg(st->reader);

  if((status = gavf_read_packet_header(st->io, &st->pkt_priv)) != GAVL_SOURCE_OK)
    return status;

  if(st->pkt_priv.buf_idx < 0)
    {
    /* Error */
    return GAVL_SOURCE_EOF;
    }
  
  if(st->pkt_priv.flags & GAVF_PACKET_HAS_BUFFERS)
    {
    if((*frame = gavl_hw_ctx_get_imported_aframe(st->hwctx, st->pkt_priv.buf_idx)))
      {
      //      fprintf(stderr, "Re-Read buffer FDs\n");

      /* Frame already exists -> Close old fd and take new one */
      buf = (*frame)->storage;
      close(buf->fd);
      }
    else
      {
      //      fprintf(stderr, "Read buffer FDs\n");

      *frame = gavl_hw_ctx_create_import_aframe(st->hwctx, st->pkt_priv.buf_idx);
      buf = (*frame)->storage;
      }
    if(!gavf_read_hw_buffers(st->io, buf, &num, NULL, 0))
      return GAVL_SOURCE_EOF;
    }
  else
    {
    //    fprintf(stderr, "Re-use buffer FDs\n");
    if(!(*frame = gavl_hw_ctx_get_imported_aframe(st->hwctx, st->pkt_priv.buf_idx)))
      {
      return GAVL_SOURCE_EOF;
      }
    }

  gavl_packet_to_audio_frame_metadata(&st->pkt_priv, *frame);

  //  fprintf(stderr, "Ref %d (buf_idx: %d)\n",
  //          gavl_hw_audio_frame_refcount(*frame), (*frame)->buf_idx);

  st->aframe_hw = *frame;
  
  gavl_hw_audio_frame_ref(st->aframe_hw);
  gavl_hw_audio_frame_map(st->aframe_hw, 0);
  
  //  fprintf(stderr, "Send ack... %d\n", gavl_hw_audio_frame_refcount(*frame));
  gavf_send_ack(st->io, GAVL_SINK_OK);
  //  fprintf(stderr, "Send ack done\n");
  return GAVL_SOURCE_OK;
  
  }


static gavl_source_status_t read_video_frame_fd(void * data, gavl_video_frame_t ** frame)
  {
  gavl_hw_buffer_t * buf;
  int num = 1;
  gavl_source_status_t status;
  bg_media_source_stream_t * s = data;
  gavf_reader_stream_t * st = s->user_data;

  if(st->vframe_hw)
    {
    gavl_hw_video_frame_unref(st->vframe_hw);
    st->vframe_hw = NULL;
    }
  gavf_reader_poll_msg(st->reader);

  if((status = gavf_read_packet_header(st->io, &st->pkt_priv)) != GAVL_SOURCE_OK)
    return status;

  if(st->pkt_priv.buf_idx < 0)
    {
    /* Error */
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot read frame: buf_idx: %d", st->pkt_priv.buf_idx);
    return GAVL_SOURCE_EOF;
    }
  
  if(st->pkt_priv.flags & GAVF_PACKET_HAS_BUFFERS)
    {
    gavl_hw_type_t t = gavl_hw_ctx_get_type(st->hwctx);
    switch(t)
      {
      case GAVL_HW_MEMFD:
        {
        if((*frame = gavl_hw_ctx_get_imported_vframe(st->hwctx, st->pkt_priv.buf_idx)))
          {
          /* Frame already exists -> Close old fd and take new one */
          buf = (*frame)->storage;
          if(buf->fd > 0)
            close(buf->fd);
          }
        else
          {
          *frame = gavl_hw_ctx_create_import_vframe(st->hwctx, st->pkt_priv.buf_idx);
          buf = (*frame)->storage;
          }
        if(!gavf_read_hw_buffers(st->io, buf, &num, NULL, 0))
          return GAVL_SOURCE_EOF;
        }
        break;
      case GAVL_HW_DMABUFFER:
        {
        int i;
        gavl_dmabuf_video_frame_t * dma;
        if((*frame = gavl_hw_ctx_get_imported_vframe(st->hwctx, st->pkt_priv.buf_idx)))
          {
          /* Frame already exists -> Close old fd and take new one */
          dma = (*frame)->storage;
          
          for(i = 0; i < dma->num_buffers; i++)
            {
            if(dma->buffers[i].fd > 0)
              close(dma->buffers[i].fd);
            }
          }
        else
          {
          *frame = gavl_hw_ctx_create_import_vframe(st->hwctx, st->pkt_priv.buf_idx);
          dma = (*frame)->storage;
          }
        dma->num_buffers = GAVL_MAX_PLANES;
        if(!gavf_read_hw_buffers(st->io, dma->buffers, &dma->num_buffers, &dma->frame_info, sizeof(dma->frame_info)))
          return GAVL_SOURCE_EOF;
        for(i = 0; i < dma->frame_info.num_planes; i++)
          (*frame)->strides[i] = dma->frame_info.planes[i].stride;
        }
        break;
      default:
        return GAVL_SOURCE_EOF;
        
      }
    }
  else
    {
    if(!(*frame = gavl_hw_ctx_get_imported_vframe(st->hwctx, st->pkt_priv.buf_idx)))
      {
      return GAVL_SOURCE_EOF;
      }
    }
  gavl_packet_to_video_frame_metadata(&st->pkt_priv, *frame);
  st->vframe_hw = *frame;
  gavl_hw_video_frame_ref(st->vframe_hw);
  
  gavf_send_ack(st->io, GAVL_SINK_OK);
  
  return GAVL_SOURCE_OK;
  }

int gavf_create_audio_source(bg_media_source_stream_t * s)
  {
  const gavl_dictionary_t * hwinfo;
  gavl_audio_source_func_t func = NULL;
  
  gavf_reader_stream_t * st = s->user_data;
  
  if((hwinfo = gavf_stream_get_hwinfo(s->s)))
    {
    fprintf(stderr, "Got hw audio frames\n");
    gavl_dictionary_dump(hwinfo, 2);
    
    st->hwctx = gavl_hw_ctx_load(hwinfo);
    gavl_hw_ctx_set_audio_importer(st->hwctx, NULL, gavl_stream_get_audio_format_nc(s->s));
    
    switch(gavl_hw_ctx_get_type(st->hwctx))
      {
      case GAVL_HW_MEMFD:
        func = read_audio_frame_memfd;
        break;
      default:
        break;
      }
    
    if(func)
      {
      s->asrc_priv =
        gavl_audio_source_create(func, s, GAVL_SOURCE_SRC_ALLOC,
                                 gavl_stream_get_audio_format(s->s));
      s->asrc = s->asrc_priv;
      return 1;
      }
    else
      return 0;
    }
  
  gavf_create_packet_source(s);
  
  s->asrc_priv =
    gavl_audio_source_create(read_audio_frame_ram, s, GAVL_SOURCE_SRC_ALLOC,
                             gavl_stream_get_audio_format(s->s));
  st->aframe = gavl_audio_frame_create(NULL);
  return 1;
  }

int gavf_create_video_source(bg_media_source_stream_t * s)
  {
  const gavl_dictionary_t * hwinfo;
  gavl_video_source_func_t func = NULL;
  
  gavf_reader_stream_t * st = s->user_data;
  
  if((hwinfo = gavf_stream_get_hwinfo(s->s)))
    {
    fprintf(stderr, "Got hw video frames\n");
    gavl_dictionary_dump(hwinfo, 2);
    st->hwctx = gavl_hw_ctx_load(hwinfo);
    gavl_hw_ctx_set_video_importer(st->hwctx, NULL, gavl_stream_get_video_format_nc(s->s));
    
    switch(gavl_hw_ctx_get_type(st->hwctx))
      {
      case GAVL_HW_MEMFD:
      case GAVL_HW_DMABUFFER:
        func = read_video_frame_fd;
        break;
      default:
        break;
      }
    
    if(func)
      {
      s->vsrc_priv =
        gavl_video_source_create(func, s, GAVL_SOURCE_SRC_ALLOC,
                                 gavl_stream_get_video_format(s->s));
      s->vsrc = s->vsrc_priv;
      return 1;
      }
    else
      return 0;
    }
  
  gavf_create_packet_source(s);
  
  s->vsrc_priv =
    gavl_video_source_create(read_video_frame_ram, s, GAVL_SOURCE_SRC_ALLOC,
                             gavl_stream_get_video_format(s->s));
  st->vframe = gavl_video_frame_create(NULL);
  return 1;
  }

static gavl_sink_status_t put_audio_memfd(void * data, gavl_audio_frame_t * frame)
  {
  gavl_sink_status_t status;
  int send_fds = 0;
  gavf_writer_stream_t * st = data;
  int packet_flags = st->packet_flags;
  
  gavl_hw_audio_frame_unmap(frame);
  
  gavl_audio_frame_to_packet_metadata(frame, &st->pkt_priv);

  //  fprintf(stderr, "put_audio_memfd %d\n", frame->buf_idx);
  
  if(!gavf_buf_sent(st, frame->buf_idx))
    {
    send_fds = 1;
    packet_flags |= GAVF_PACKET_HAS_BUFFERS;
    }

  if((status = gavf_write_packet_header(st->io, &st->pkt_priv, packet_flags) != GAVL_SINK_OK))
    return status;
  
  if(send_fds && !gavf_write_hw_buffers(st->io, frame->storage, 1, NULL, 0))
    return GAVL_SINK_ERROR;

  gavf_buf_set_sent(st, frame->buf_idx);
  
  //  fprintf(stderr, "wait ack...\n");
  status = gavf_wait_ack(st->io);
  //  fprintf(stderr, "wait ack done 1 %d\n", gavl_hw_audio_frame_refcount(frame));

  if(st->hwctx_priv)
    {
    gavl_hw_audio_frame_unref(frame);
    //    fprintf(stderr, "unref done\n");
    }

  //  fprintf(stderr, "wait ack done 2 %d\n", gavl_hw_audio_frame_refcount(frame));

  return status;
  
  }

static gavl_audio_frame_t * get_audio_hw(void * data)
  {
  gavl_audio_frame_t * ret;
  gavf_writer_stream_t * st = data;
  //  if((ret = gavl_hw_audio_frame_get_write(st->hwctx)))
  //  gavl_hw_audio_frame_map(ret, 1);
  ret = gavl_hw_audio_frame_get_write(st->hwctx);
  //  fprintf(stderr, "get_audio_hw %p\n", ret);
  return ret;
  }

static gavl_audio_frame_t * get_audio_ram(void * data)
  {
  gavf_writer_stream_t * st = data;
  return st->aframe;
  }

static gavl_sink_status_t put_audio_ram(void * data, gavl_audio_frame_t * frame)
  {
  gavf_writer_stream_t * st = data;
  gavl_audio_frame_to_packet_metadata(frame, &st->pkt_priv);
  return gavl_packet_sink_put_packet(st->psink, &st->pkt_priv);
  }

int gavf_create_audio_sink(gavf_writer_stream_t * st, const gavl_audio_format_t * afmt)
  {
  //  gavl_stream_set_compression_info(st->s, NULL);
  
  if(st->writer->flags & FLAG_LOCAL)
    {
    /* Bypass packet sink */
    if(afmt->hwctx)
      {
      gavl_hw_type_t type = gavl_hw_ctx_get_type(afmt->hwctx);
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got hw audio frames: %s\n", gavl_hw_type_to_string(type));

      switch(type)
        {
        case GAVL_HW_MEMFD:
          st->asink = gavl_audio_sink_create(NULL, put_audio_memfd, st, afmt);
          gavl_hw_ctx_store(afmt->hwctx, gavf_stream_get_hwinfo_nc(st->s));
          st->hwctx = afmt->hwctx;
          //    gavl_dictionary_dump(gavf_stream_get_hwinfo(st->s), 2);
          return 1;
          break;
        default:
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Unsupported buffer format: %s\n", gavl_hw_type_to_string(type));
          break;
        }
      }
    else // Create local hwctx
      {
      gavl_audio_format_t fmt;
      st->hwctx_priv = gavl_hw_ctx_create_memfd();
      st->hwctx = st->hwctx_priv; 
      gavl_hw_ctx_set_shared(st->hwctx);

      gavl_audio_format_copy(&fmt, afmt);
      gavl_hw_ctx_set_audio_creator(st->hwctx, &fmt, GAVL_HW_FRAME_MODE_MAP);
      st->asink = gavl_audio_sink_create(get_audio_hw, put_audio_memfd, st, afmt);
      gavl_hw_ctx_store(st->hwctx, gavf_stream_get_hwinfo_nc(st->s));
      }
    return 1;
    }

  gavf_create_packet_sink(st);
  st->asink = gavl_audio_sink_create(get_audio_ram, put_audio_ram, st, afmt);
  gavl_packet_alloc(&st->pkt_priv, gavl_audio_format_buffer_size(afmt));
  st->aframe = gavl_audio_frame_create(NULL);
  gavl_audio_frame_set_from_packet(st->aframe, afmt, &st->pkt_priv);
  return 1;
  }

/*
 *  Video frame i/0
 */

static gavl_sink_status_t put_video_hw_start(gavf_writer_stream_t * st, gavl_video_frame_t * frame, int * send_fds)
  {
  int packet_flags = st->packet_flags;

  gavl_hw_video_frame_unmap(frame);

  gavl_video_frame_to_packet_metadata(frame, &st->pkt_priv);

  if(!gavf_buf_sent(st, frame->buf_idx))
    {
    *send_fds = 1;
    packet_flags |= GAVF_PACKET_HAS_BUFFERS;
    gavf_buf_set_sent(st, frame->buf_idx);
    }
  
  return gavf_write_packet_header(st->io, &st->pkt_priv, packet_flags);
  }

static gavl_sink_status_t put_video_hw_end(gavf_writer_stream_t * st, gavl_video_frame_t * frame)
  {
  gavl_sink_status_t status;
  status = gavf_wait_ack(st->io);
  
  if(st->hwctx_priv)
    gavl_hw_video_frame_unref(frame);
  
  return status;
  }

static gavl_sink_status_t put_video_memfd(void * data, gavl_video_frame_t * frame)
  {
  gavl_sink_status_t status;
  int send_fds = 0;
  gavf_writer_stream_t * st = data;
  
  if((status = put_video_hw_start(data, frame, &send_fds) != GAVL_SINK_OK))
    return status;
  
  if(send_fds && !gavf_write_hw_buffers(st->io, frame->storage, 1, NULL, 0))
    return GAVL_SINK_ERROR;

  return put_video_hw_end(st, frame);
  }

static gavl_sink_status_t put_video_dmabuf(void * data, gavl_video_frame_t * frame)
  {
  gavl_sink_status_t status;
  int send_fds = 0;
  gavf_writer_stream_t * st = data;

  //  fprintf(stderr, "put_video_dmabuf: buf_idx: %d strides: %d %d %d\n", frame->buf_idx,
  //          frame->strides[0], frame->strides[1], frame->strides[2]);
  
  if((status = put_video_hw_start(data, frame, &send_fds) != GAVL_SINK_OK))
    return status;

  if(send_fds)
    {
    gavl_dmabuf_video_frame_t * dma = frame->storage;
    if(!gavf_write_hw_buffers(st->io, dma->buffers, dma->num_buffers, &dma->frame_info, sizeof(dma->frame_info)))
      return GAVL_SINK_ERROR;
    }
  
  return put_video_hw_end(st, frame);
  }

static gavl_video_frame_t * get_video_hw(void * data)
  {
  gavl_video_frame_t * ret;
  gavf_writer_stream_t * st = data;
  if((ret = gavl_hw_video_frame_get_write(st->hwctx)))
    gavl_hw_video_frame_map(ret, 1);
  return ret;
  }

static gavl_video_frame_t * get_video_ram(void * data)
  {
  gavf_writer_stream_t * st = data;
    
  return st->vframe;
  }

static gavl_sink_status_t put_video_ram(void * data, gavl_video_frame_t * frame)
  {
  gavf_writer_stream_t * st = data;
  gavl_video_frame_to_packet_metadata(frame, &st->pkt_priv);
  return gavl_packet_sink_put_packet(st->psink, &st->pkt_priv);
  }


int gavf_create_video_sink(gavf_writer_stream_t * st, const gavl_video_format_t * vfmt)
  {
  //  gavl_stream_set_compression_info(st->s, NULL);
    
  if(st->writer->flags & FLAG_LOCAL)
    {
    /* Bypass packet sink */
    if(vfmt->hwctx)
      {
      gavl_hw_type_t type = gavl_hw_ctx_get_type(vfmt->hwctx);
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got hw video frames: %s\n", gavl_hw_type_to_string(type));

      switch(type)
        {
        case GAVL_HW_MEMFD:
          st->vsink = gavl_video_sink_create(NULL, put_video_memfd, st, vfmt);
          gavl_hw_ctx_store(vfmt->hwctx, gavf_stream_get_hwinfo_nc(st->s));
          st->hwctx = vfmt->hwctx;
          //    gavl_dictionary_dump(gavf_stream_get_hwinfo(st->s), 2);
          return 1;
          break;
        case GAVL_HW_DMABUFFER:
          st->vsink = gavl_video_sink_create(NULL, put_video_dmabuf, st, vfmt);
          gavl_hw_ctx_store(vfmt->hwctx, gavf_stream_get_hwinfo_nc(st->s));
          st->hwctx = vfmt->hwctx;
          //    gavl_dictionary_dump(gavf_stream_get_hwinfo(st->s), 2);
          return 1;
          break;
        default:
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Unsupported buffer format: %s\n", gavl_hw_type_to_string(type));
          break;
        }
      
      }
    else // Create local hwctx
      {
      gavl_video_format_t fmt;
      st->hwctx_priv = gavl_hw_ctx_create_memfd();
      st->hwctx = st->hwctx_priv; 
      
      gavl_hw_ctx_set_shared(st->hwctx);

      gavl_video_format_copy(&fmt, vfmt);
      gavl_hw_ctx_set_video_creator(st->hwctx, &fmt, GAVL_HW_FRAME_MODE_MAP);
      st->vsink = gavl_video_sink_create(get_video_hw, put_video_memfd, st, vfmt);
      gavl_hw_ctx_store(st->hwctx, gavf_stream_get_hwinfo_nc(st->s));
      }
    }

  gavf_create_packet_sink(st);
  st->vsink = gavl_video_sink_create(get_video_ram, put_video_ram, st, vfmt);
  gavl_packet_alloc(&st->pkt_priv, gavl_video_format_get_image_size(vfmt));
  st->vframe = gavl_video_frame_create(NULL);
  gavl_video_frame_set_from_packet(st->vframe, vfmt, &st->pkt_priv);
  return 1;
  }

