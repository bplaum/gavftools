
#include <gavl/packetindex.h>
#include <gavl/hw.h>


/* Global flags */

#define FLAG_SEPARATE_STREAMS (1<<1)
#define FLAG_ON_DISK          (1<<2)
#define FLAG_BACKCHANNEL      (1<<3)
#define FLAG_LOCAL            (1<<4)

#define GAVF_PROTOCOL_STRING "GAVF/1.0"

#define STREAM_HAS_ACK        (1<<0)


/* Metadata tags */

typedef struct
  {
  int flags;
  
  gavl_packet_buffer_t * buf;
  gavf_reader_t * reader;     // Reader we belong to
  gavl_io_t * io;

  int idx_pos; // Used only for seeking

  gavl_hw_context_t * hwctx; // Importer
  
  gavl_audio_frame_t * aframe;
  gavl_video_frame_t * vframe;
  
  gavl_audio_frame_t * aframe_hw;
  gavl_video_frame_t * vframe_hw;

  
  gavl_packet_t * pkt;
  gavl_packet_t pkt_priv;
  
  } gavf_reader_stream_t;

typedef struct
  {
  int packet_flags;
  gavf_writer_t * writer;
  
  gavl_io_t * io;
  int server_fd;
  int stream_id;
  
  gavl_audio_sink_t * asink;
  gavl_video_sink_t * vsink;
  gavl_packet_sink_t * psink;
  bg_msg_sink_t * msink;

  gavl_dictionary_t * s;

  gavl_audio_frame_t * aframe;
  gavl_video_frame_t * vframe;

  gavl_packet_t * pkt;
  gavl_packet_t pkt_priv;
  
  gavl_hw_context_t * hwctx; // Creator
  gavl_hw_context_t * hwctx_priv; 

  uint8_t * frames_sent;
  
  } gavf_writer_stream_t;

struct gavf_reader_s
  {
  gavl_chunk_t ch;
  int flags;
  gavl_dictionary_t url_vars;
  gavl_dictionary_t mi;
  gavl_io_t * io;
  bg_controllable_t ctrl;
  bg_media_source_t src;

  gavl_io_t * bkch_io;
  bg_msg_sink_t * bkch_sink;
  
  gavl_packet_index_t * idx;

  pthread_mutex_t demux_mutex;
  pthread_mutex_t msg_mutex;
  
  bg_media_source_stream_t * msg_stream;
  
  };

struct gavf_writer_s
  {
  gavl_dictionary_t mi;
  gavl_dictionary_t * track;
 
  int flags;
  gavl_io_t * io;
  gavl_chunk_t ch;

  gavl_io_t * bkch_io;
  bg_msg_hub_t * bkch_hub;

  gavf_writer_stream_t * streams;
  int num_streams;

  gavl_packet_index_t * idx;

  /* File positions */
  int64_t header_start_pos;
  
  };

int gavf_write_backpointer(gavl_io_t * io,
                           const char * eightcc,
                           int64_t offset);

int gavf_read_backpointer(gavl_io_t * io,
                          const char * eightcc,
                          int64_t * offset);


gavl_source_status_t gavf_demux_iteration(gavf_reader_t * g);
gavl_sink_status_t gavf_mux_iteration(gavf_writer_t * g);

gavl_io_t * gavf_reader_open_io(gavf_reader_t * g, const char * uri);
gavl_io_t * gavf_writer_open_io(gavf_writer_t * g, const char * uri);

int gavf_writer_finish_track(gavf_writer_t * g);

void gavf_writer_set_from_source(gavf_writer_t * g, bg_media_source_t * src);

void gavf_writer_flush(gavf_writer_t * g);

/* Packet-I/O */


int gavf_create_packet_source(bg_media_source_stream_t * st);
int gavf_create_packet_sink(gavf_writer_stream_t * st);

/* Frame I/O */

int gavf_create_audio_source(bg_media_source_stream_t * st);
int gavf_create_audio_sink(gavf_writer_stream_t * st, const gavl_audio_format_t * fmt);

int gavf_create_video_source(bg_media_source_stream_t * st);
int gavf_create_video_sink(gavf_writer_stream_t * st, const gavl_video_format_t * fmt);

int gavf_buf_sent(gavf_writer_stream_t * st, int buf_idx);
void gavf_buf_set_sent(gavf_writer_stream_t * st, int buf_idx);

/* Actual read/write functions */

gavl_source_status_t gavf_read_packet_header(gavl_io_t * io,
                                             gavl_packet_t * p);

gavl_sink_status_t gavf_write_packet_header(gavl_io_t * io,
                                            gavl_packet_t * p,
                                            int packet_flags);

gavl_sink_status_t gavf_write_packet_data(gavl_io_t * io,
                                          gavl_packet_t * p);

gavl_source_status_t gavf_read_packet_data(gavl_io_t * io,
                                           gavl_packet_t * p,
                                           const gavl_packet_t * header);

gavl_source_status_t gavf_read_packet(gavl_io_t * io,
                                      gavl_packet_t * p);

gavl_source_status_t gavf_packet_skip_data(bg_media_source_stream_t * st,
                                           const gavl_packet_t * packet);

gavl_sink_status_t gavf_write_packet(gavl_io_t * io,
                                     gavl_packet_t * p,
                                     int stream_flags);

gavl_sink_status_t gavf_write_discont(gavl_io_t * io, int mode);

//gavl_source_status_t gavf_read_discont(gavl_io_t * io, int block, int * mode);

gavl_source_status_t gavf_stream_read_discont(bg_media_source_stream_t * st,
                                              int block, int * mode);

void gavf_reader_poll_msg(gavf_reader_t * g);

/* Read/write buffers via Unix domain sockets */

int gavf_write_hw_buffers(gavl_io_t * io, const gavl_hw_buffer_t * buffers, int num);
int gavf_read_hw_buffers(gavl_io_t * io, gavl_hw_buffer_t * buffers, int * num);

int gavf_send_ack(gavl_io_t * io, gavl_sink_status_t st);
gavl_sink_status_t gavf_wait_ack(gavl_io_t * io);
