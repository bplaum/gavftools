/* Packet flags (per stream) */
#define PACKET_HAS_STREAM_ID   (1<<0)

/* Packet flags */
#define PACKET_HAS_PTS              (GAVL_PACKET_FLAG_PRIV<<0)
#define PACKET_HAS_DURATION         (GAVL_PACKET_FLAG_PRIV<<1)
#define PACKET_HAS_HEADER_SIZE      (GAVL_PACKET_FLAG_PRIV<<2)
#define PACKET_HAS_SEQUENCE_END_POS (GAVL_PACKET_FLAG_PRIV<<3)
#define PACKET_HAS_FIELD2_OFFSET    (GAVL_PACKET_FLAG_PRIV<<4)
#define PACKET_HAS_RECTANGLE        (GAVL_PACKET_FLAG_PRIV<<5)
#define PACKET_HAS_TIMECODE         (GAVL_PACKET_FLAG_PRIV<<6)
#define PACKET_HAS_INTERLACE_MODE   (GAVL_PACKET_FLAG_PRIV<<7)

/* Global flags */

#define FLAG_WRITE            (1<<0)
#define FLAG_SEPARATE_STREAMS (1<<1)

#define GAVF_PROTOCOL_STRING "GAVF/1.0"

/* Metadata tags */

#define GAVF_META_STREAM_URI        "stream-uri"
#define GAVF_META_SEPARATE_STREAMS  "separate"

typedef struct
  {
  int packet_flags;
  gavl_packet_buffer_t * buf;
  gavf_t * g;
  
  gavl_io_t * io;
  int server_fd;
  } gavf_stream_t;

struct gavf_s
  {
  gavl_io_t * io;
  gavl_dictionary_t m;
  
  bg_media_source_t src;
  bg_media_sink_t sink;

  int flags;
  gavl_dictionary_t url_vars;
  
  gavl_chunk_t ch;
  };

gavl_source_status_t gavf_demux_iteration(gavf_t * g);
gavl_sink_status_t gavf_mux_iteration(gavf_t * g);

gavl_io_t * gavf_open_io(gavf_t * g, const char * uri, int wr);

/* Packet-I/O */

gavl_source_status_t gavf_packet_read_multiplex(void * priv, gavl_packet_t ** p);
gavl_source_status_t gavf_packet_read_separate(void * priv, gavl_packet_t ** p);

gavl_source_status_t gavf_packet_read_multiplex_noncont(void * priv, gavl_packet_t ** p);
gavl_source_status_t gavf_packet_read_separate_noncont(void * priv, gavl_packet_t ** p);


gavl_packet_t * gavf_packet_get_multiplex(void * priv);
gavl_sink_status_t gavf_packet_put_multiplex(void * priv, gavl_packet_t * p);
gavl_sink_status_t gavf_packet_put_separate(void * priv, gavl_packet_t * p);

/* Actual read/write functions */
gavl_source_status_t gavf_peek_packet(gavl_io_t * io,
                                      int * stream_id);

gavl_source_status_t gavf_read_packet_header(gavl_io_t * io,
                                             gavl_packet_t * p,
                                             int stream_flags);

gavl_sink_status_t gavf_write_packet_header(gavl_io_t * io,
                                            gavl_packet_t * p,
                                            int stream_flags);

gavl_sink_status_t gavf_write_packet_data(gavl_io_t * io,
                                          gavl_packet_t * p);
gavl_source_status_t gavf_read_packet_data(gavl_io_t * io,
                                           gavl_packet_t * p);
gavl_source_status_t gavf_skip_packet_data(gavl_io_t * io,
                                           gavl_packet_t * p);

gavl_source_status_t gavf_read_packet(gavl_io_t * io,
                                      gavl_packet_t * p,
                                      int stream_flags);

gavl_source_status_t gavf_skip_packet(gavl_io_t * io,
                                      int stream_flags);

gavl_sink_status_t gavf_write_packet(gavl_io_t * io,
                                     gavl_packet_t * p,
                                     int stream_flags);
