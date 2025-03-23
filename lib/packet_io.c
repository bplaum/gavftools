
#include <string.h>


#include <gavf.h>
#include <gavfprivate.h>
#include <gavl/connectors.h>
#include <gavl/numptr.h>


gavl_source_status_t gavf_packet_read_multiplex(void * priv, gavl_packet_t ** p)
  {
  gavf_reader_stream_t * gs;
  bg_media_source_stream_t * s = priv;
  gavl_source_status_t st;
  
  gs = s->user_data;

  while((st = gavl_packet_source_read_packet(gavl_packet_buffer_get_source(gs->buf), p))
        == GAVL_SOURCE_AGAIN)
    {
    if(gavf_demux_iteration(gs->reader) != GAVL_SOURCE_OK)
      return GAVL_SOURCE_EOF;
    }
  
  return st;
  }

gavl_source_status_t gavf_packet_read_separate(void * priv, gavl_packet_t ** p)
  {
  gavf_reader_stream_t * gs;
  bg_media_source_stream_t * s = priv;
  gs = s->user_data;
  return gavf_read_packet(gs->io, *p);
  }

gavl_source_status_t gavf_packet_read_multiplex_noncont(void * priv, gavl_packet_t ** p)
  {
  gavf_reader_stream_t * gs;
  bg_media_source_stream_t * s = priv;
  gs = s->user_data;
  return gavl_packet_source_read_packet(gavl_packet_buffer_get_source(gs->buf), p);
  }

gavl_source_status_t gavf_packet_read_separate_noncont(void * priv, gavl_packet_t ** p)
  {
  gavf_reader_stream_t * gs;
  bg_media_source_stream_t * s = priv;
  gs = s->user_data;

  if(!gavl_io_can_read(gs->io, 0))
    return GAVL_SOURCE_AGAIN;

  return gavl_packet_source_read_packet(gavl_packet_buffer_get_source(gs->buf), p);
  }

/* Sink */
gavl_packet_t * gavf_packet_get_multiplex(void * priv)
  {
  gavf_writer_stream_t * gs = priv;
  return gavl_packet_sink_get_packet(gavl_packet_buffer_get_sink(gs->buf));
  }

gavl_sink_status_t gavf_packet_put_multiplex(void * priv, gavl_packet_t * p)
  {
  gavf_writer_stream_t * gs = priv;
  
  gavl_packet_sink_put_packet(gavl_packet_buffer_get_sink(gs->buf), p);
  /* Flush packets */
  return gavf_mux_iteration(gs->writer);
  }

gavl_sink_status_t gavf_packet_put_separate(void * priv, gavl_packet_t * p)
  {
  gavf_writer_stream_t * gs = priv;
  return gavf_write_packet(gs->io, p, gs->packet_flags);
  }

gavl_source_status_t gavf_read_packet_header(gavl_io_t * io,
                                             gavl_packet_t * p)
  {
  char prefix;
  uint32_t packet_flags;
  
  if(gavl_io_read_data(io, (uint8_t*)&prefix, 1) < 1)
    return GAVL_SOURCE_EOF;

  if(prefix != 'P')
    return GAVL_SOURCE_EOF;

  if(!gavl_io_read_uint32v(io, &packet_flags))
    return GAVL_SOURCE_EOF;
  
  if((packet_flags & PACKET_HAS_STREAM_ID) &&
     !gavl_io_read_32_be(io, (uint32_t*)&p->id))
    return GAVL_SOURCE_EOF;


  if((packet_flags & PACKET_HAS_PTS) && !gavl_io_read_int64v(io, &p->pts))
    return GAVL_SOURCE_EOF;
  if((packet_flags & PACKET_HAS_DURATION) && !gavl_io_read_int64v(io, &p->duration))
    return GAVL_SOURCE_EOF;
  if((packet_flags & PACKET_HAS_FIELD2_OFFSET) && !gavl_io_read_uint32v(io, &p->field2_offset))
    return GAVL_SOURCE_EOF;
  if((packet_flags & PACKET_HAS_HEADER_SIZE) && !gavl_io_read_uint32v(io, &p->header_size))
    return GAVL_SOURCE_EOF;
  if((packet_flags & PACKET_HAS_SEQUENCE_END_POS) && !gavl_io_read_uint32v(io, &p->sequence_end_pos))
    return GAVL_SOURCE_EOF;

  if((packet_flags & PACKET_HAS_RECTANGLE) &&
     (!gavl_io_read_int32v(io, &p->src_rect.x) ||
      !gavl_io_read_int32v(io, &p->src_rect.y) ||
      !gavl_io_read_int32v(io, &p->src_rect.w) ||
      !gavl_io_read_int32v(io, &p->src_rect.h) ||
      !gavl_io_read_int32v(io, &p->dst_x) ||
      !gavl_io_read_int32v(io, &p->dst_y)))
     return GAVL_SOURCE_EOF;
  
  if((packet_flags & PACKET_HAS_INTERLACE_MODE) &&
     !gavl_io_read_int32v(io, &p->interlace_mode))
    return GAVL_SOURCE_EOF;
  
  if((packet_flags & PACKET_HAS_TIMECODE) &&
     !gavl_io_read_uint64v(io, &p->timecode))
    return GAVL_SOURCE_EOF;
  
  return GAVL_SOURCE_OK;
  }

gavl_sink_status_t gavf_write_packet_header(gavl_io_t * io,
                                            gavl_packet_t * p,
                                            int stream_flags)
  {
  char prefix = 'P';
  uint32_t packet_flags = p->flags & GAVL_PACKET_FLAG_PUBLIC_MASK;

  packet_flags |= stream_flags;
  
  /* Assemble packet flags */
  if(p->pts != GAVL_TIME_UNDEFINED)
    packet_flags |= PACKET_HAS_PTS;
  if(p->duration > 0)
    packet_flags |= PACKET_HAS_DURATION;

  if(p->field2_offset > 0)
    packet_flags |= PACKET_HAS_FIELD2_OFFSET;
  if(p->sequence_end_pos > 0)
    packet_flags |= PACKET_HAS_SEQUENCE_END_POS;
  if(p->header_size > 0)
    packet_flags |= PACKET_HAS_HEADER_SIZE;

  if((p->src_rect.w > 0) && (p->src_rect.h > 0))
    packet_flags |= PACKET_HAS_RECTANGLE;

  if(p->timecode != GAVL_TIMECODE_UNDEFINED)
    packet_flags |= PACKET_HAS_TIMECODE;
  if(p->interlace_mode != GAVL_INTERLACE_NONE)
    packet_flags |= PACKET_HAS_INTERLACE_MODE;
  
  if(gavl_io_write_data(io, (uint8_t*)&prefix, 1) < 1)
    return GAVL_SINK_ERROR;

  if(!gavl_io_write_uint32v(io, packet_flags))
    return GAVL_SINK_ERROR;

  if((packet_flags & PACKET_HAS_STREAM_ID) &&
     !gavl_io_write_32_be(io, p->id))
    return GAVL_SINK_ERROR;
  
  if((packet_flags & PACKET_HAS_PTS) && !gavl_io_write_int64v(io, p->pts))
    return GAVL_SINK_ERROR;
  if((packet_flags & PACKET_HAS_DURATION) && !gavl_io_write_int64v(io, p->duration))
    return GAVL_SINK_ERROR;
  if((packet_flags & PACKET_HAS_FIELD2_OFFSET) && !gavl_io_write_uint32v(io, p->field2_offset))
    return GAVL_SINK_ERROR;
  if((packet_flags & PACKET_HAS_HEADER_SIZE) && !gavl_io_write_uint32v(io, p->header_size))
    return GAVL_SINK_ERROR;
  if((packet_flags & PACKET_HAS_SEQUENCE_END_POS) && !gavl_io_write_uint32v(io, p->sequence_end_pos))
    return GAVL_SINK_ERROR;

  if((packet_flags & PACKET_HAS_RECTANGLE) &&
     (!gavl_io_write_int32v(io, p->src_rect.x) ||
      !gavl_io_write_int32v(io, p->src_rect.y) ||
      !gavl_io_write_int32v(io, p->src_rect.w) ||
      !gavl_io_write_int32v(io, p->src_rect.h) ||
      !gavl_io_write_int32v(io, p->dst_x) ||
      !gavl_io_write_int32v(io, p->dst_y)))
     return GAVL_SINK_ERROR;
  
  if((packet_flags & PACKET_HAS_INTERLACE_MODE) &&
     !gavl_io_write_int32v(io, p->interlace_mode))
    return GAVL_SINK_ERROR;
  
  if((packet_flags & PACKET_HAS_TIMECODE) &&
     !gavl_io_write_uint64v(io, p->timecode))
    return GAVL_SINK_ERROR;
  
  return GAVL_SINK_OK;
  }

gavl_sink_status_t gavf_write_packet_data(gavl_io_t * io,
                                          gavl_packet_t * p)
  {
  if(!gavl_io_write_uint32v(io, p->buf.len))
    return GAVL_SINK_ERROR;
  if(gavl_io_write_data(io, p->buf.buf, p->buf.len) < p->buf.len)
    return GAVL_SINK_ERROR;
  else
    return GAVL_SINK_OK;
  }

gavl_source_status_t gavf_read_packet_data(gavl_io_t * io,
                                           gavl_packet_t * p,
                                           const gavl_packet_t * header)
  {
  uint32_t size;

  /* Copy header */
  if(p != header)
    {
    gavl_packet_t tmp;
    gavl_packet_init(&tmp);
    memcpy(&tmp.buf, &p->buf, sizeof(p->buf));
    memcpy(&tmp.ext_data, &p->ext_data, sizeof(tmp.ext_data));
    memcpy(p, header, sizeof(*p));
    memcpy(&p->buf, &tmp.buf, sizeof(p->buf));
    memcpy(&p->ext_data, &tmp.ext_data, sizeof(tmp.ext_data));
    }
  
  if(gavl_io_read_uint32v(io, &size))
    return GAVL_SOURCE_EOF;

  gavl_buffer_alloc(&p->buf, size);

  if(gavl_io_read_data(io, p->buf.buf, size) < size)
    return GAVL_SOURCE_EOF;
  else
    {
    p->buf.len = size;
    return GAVL_SOURCE_OK;
    }
  }

gavl_source_status_t gavf_packet_skip_data(gavl_io_t * io,
                                           const gavl_packet_t * p)
  {
  uint32_t size;

  if(gavl_io_read_uint32v(io, &size))
    return GAVL_SOURCE_EOF;
  
  gavl_io_skip(io, size);
  return GAVL_SOURCE_OK;
  }

gavl_source_status_t gavf_read_packet(gavl_io_t * io,
                                      gavl_packet_t * p)
  {
  gavl_source_status_t st;

  if((st = gavf_read_packet_header(io, p)) !=
     GAVL_SOURCE_OK)
    return st;
  
  return gavf_read_packet_data(io, p, p);
  }



gavl_sink_status_t gavf_write_packet(gavl_io_t * io,
                                     gavl_packet_t * p,
                                     int stream_flags)
  {
  gavl_sink_status_t st;

  if((st = gavf_write_packet_header(io, p, stream_flags)) !=
     GAVL_SINK_OK)
    return st;
  
  return gavf_write_packet_data(io, p);
  
  }
