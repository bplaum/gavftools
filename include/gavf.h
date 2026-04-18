
#include <gavl/gavl.h>
#include <gmerlin/mediaconnector.h>

typedef struct gavf_reader_s gavf_reader_t;
typedef struct gavf_writer_s gavf_writer_t;

gavf_reader_t * gavf_reader_create();
gavf_writer_t * gavf_writer_create();

int gavf_writer_is_local(const gavf_writer_t *);

/*
 * URIs:
 *
 * -:                                  Pipe (will be redirected to UNIX sockets)
 * gavf-server://0.0.0.0               TCP Server
 * gavf-server:///path-to-UNIX-socket  Unix domain server
 * gavf://host:port                    TCP Client
 * gavf:///path-to-UNIX-socket         Unix domain Client
 * 
 * file.gavf: On disk file
 */

#define GAVF_PROTOCOL_SERVER "gavf-server"
#define GAVF_PROTOCOL        "gavf"

#define GAVF_HEADER         "GAVFHEAD"
#define GAVF_PACKETS        "GAVFPACK"
#define GAVF_FOOTER         "GAVFFOOT"
#define GAVF_TAIL           "GAVFTAIL"
#define GAVF_INDEX          GAVF_TAG_PACKET_INDEX
#define GAVF_TRACK          "GAVFTRAC"
#define GAVF_MSG            "GAVFMESG"

// GAVL_MSG_NS_GAVF

#define GAVF_MSG_END           1 /* Stream is finished */
#define GAVF_MSG_DISCONTINUITY 2 /* Discontinuity   */
#define GAVF_MSG_RESYNC        3 /* Post seek resync   */

/* Format specific packet flags (not used in plain gavl) */

/* Packet flags */
#define GAVF_PACKET_HAS_PTS              (GAVL_PACKET_FLAG_PRIV<<0)
#define GAVF_PACKET_HAS_DURATION         (GAVL_PACKET_FLAG_PRIV<<1)
#define GAVF_PACKET_HAS_HEADER_SIZE      (GAVL_PACKET_FLAG_PRIV<<2)
#define GAVF_PACKET_HAS_SEQUENCE_END_POS (GAVL_PACKET_FLAG_PRIV<<3)
#define GAVF_PACKET_HAS_FIELD2_OFFSET    (GAVL_PACKET_FLAG_PRIV<<4)
#define GAVF_PACKET_HAS_RECTANGLE        (GAVL_PACKET_FLAG_PRIV<<5)
#define GAVF_PACKET_HAS_TIMECODE         (GAVL_PACKET_FLAG_PRIV<<6)
#define GAVF_PACKET_HAS_INTERLACE_MODE   (GAVL_PACKET_FLAG_PRIV<<7)
#define GAVF_PACKET_HAS_STREAM_ID        (GAVL_PACKET_FLAG_PRIV<<8)

/* The transmitter of this packet passes file descriptors
   via sendmsg() right after this packet */
#define GAVF_PACKET_HAS_BUFFERS          (GAVL_PACKET_FLAG_PRIV<<9)

#define GAVF_PACKET_HAS_BUF_IDX          (GAVL_PACKET_FLAG_PRIV<<10)


/* For interactive streams, we use packets with special flags as
   "markers", so they can be handled on a low level.
*/

/* Inserted at a seek boundary */
#define GAVF_PACKET_DISCONT_RESYNC       (GAVL_PACKET_FLAG_PRIV<<11)

/*
 *  EOF means that no packets follow immediately. Decoding might get
 *  restarted through
 */

#define GAVF_PACKET_DISCONT_EOF          (GAVL_PACKET_FLAG_PRIV<<12)

/* For testing */
#define GAVF_PACKET_DISCONT  (GAVF_PACKET_DISCONT_RESYNC | GAVF_PACKET_DISCONT_EOF)


   


/* If transmitted over a pipe for fifo, one single GAVFHEAD chunk is sent, which
   contains one track of class GAVL_META_CLASS_LOCATION and a unix socket address to
   connect to as a client. This way pipes are transparently updated to duplex sockets */

/* The network format works like this: */

/* GET */

/*
  C->S GET /path/to/stream.gavf GAVF/1.0\n (path is * for 1 to 1 connections) 
  S->C 200 OK\n\n
  S->C GAVFHEAD (Preliminary Header)
  
  Client and server set up I/O backchannel for commands
  
  C->S GAVL_MSG_NS_SRC:GAVL_CMD_SRC_SELECT_TRACK
  C->S GAVL_MSG_NS_SRC:GAVL_CMD_SRC_SET_STREAM_ACTION (called for each stream, which is not switched off)
  C->S GAVL_MSG_NS_SRC:GAVL_CMD_SRC_START

  S->C GAVFTRAC (Final track header, single dictionary, optional)
  S->C GAVFPACK (Start of packets)
  S->C GAVL packets...

  Possile end scenarios:
  S->C GAVF_END (EOF)
  S: Disconnect

  Possile end scenarios:
  C->S GAVL_MSG_NS_SRC:GAVL_CMD_SRC_SELECT_TRACK
  S->C Remaining GAVL packets...
  S->C GAVL_MSG_NS_GAVF:GAVF_MSG_DISCONTINUITY
  C->S GAVL_MSG_NS_SRC:GAVL_CMD_SRC_SET_STREAM_ACTION
  continue like above

  Seeking:
  C->S GAVL_MSG_NS_SRC:GAVL_CMD_SRC_SEEK
  S->C Remaining GAVL packets...
  S->C GAVL_MSG_NS_GAVF:GAVF_MSG_RESYNC
  S->C GAVL packets from new position
  
 */

/* PUT (Server upstream protocol, not there jet, maybe never comes) */

/*
 * On Disk layout of a single track:
 *
 * GAVFHEAD
 * Media info (contains a media info with a single track inside)
 *
 * GAVFPACK
 * Stream packets
 *
 * GAVFFOOT
 * File position of preceeding GAVFINFO relative to end of GAVFFOOT
 *
 * GAVFINDX
 * Packet index
 * 
 * GAVFTAIL
 * File position of preceeding GAVFFOOT relative to end of GAVFTAIL
 *
 * Multitrack files can be made simply by concatenating single files.
 */


int gavf_reader_open(gavf_reader_t * g, const char * uri);
int gavf_writer_open(gavf_writer_t * g, const char * uri);

// int gavf_reader_skip_packets(gavf_reader_t * g);
int gavf_reader_select_track(gavf_reader_t * g, int t);

int gavf_writer_send_media_info(gavf_writer_t * g, const gavl_dictionary_t * mi);
int gavf_writer_send_track_info(gavf_writer_t * g, const gavl_dictionary_t * ti);

int gavf_writer_init(gavf_writer_t * g, bg_media_source_t * src);
int gavf_writer_reset(gavf_reader_t * g, int msg_id);

gavl_audio_sink_t * gavf_writer_get_audio_sink(gavf_writer_t * g,
                                                 int stream);
gavl_video_sink_t * gavf_writer_get_video_sink(gavf_writer_t * g,
                                               int stream);
gavl_packet_sink_t * gavf_writer_get_packet_sink(gavf_writer_t * g,
                                                 int stream);
bg_msg_sink_t * gavf_writer_get_message_sink(gavf_writer_t * g,
                                             int stream);

/* Messages through the backchannel */
int gavf_writer_read_gavf_message(gavf_writer_t * wr, gavl_msg_t * ret, int timeout); 
int gavf_reader_write_gavf_message(gavf_reader_t * rd, const gavl_msg_t * ret); 

/* Messages through the forward channel */
int gavf_reader_read_gavf_message(gavf_reader_t * rd, gavl_msg_t * ret, int timeout); 
int gavf_writer_write_gavf_message(gavf_writer_t * wr, const gavl_msg_t * msg); 


int gavf_writer_has_backchannel(gavf_writer_t * wr);

bg_media_source_t * gavf_reader_get_source(gavf_reader_t * g);

/* Start the instance */
int gavf_reader_start(gavf_reader_t * g);
// int gavf_writer_start(gavf_writer_t * g);

bg_controllable_t * gavf_reader_get_controllable(gavf_reader_t * g);

void gavf_reader_destroy(gavf_reader_t * g);
void gavf_writer_destroy(gavf_writer_t * g);

gavl_source_status_t gavf_reader_drain(gavf_reader_t * g, int * mode);
void gavf_writer_write_discont(gavf_writer_t * g, int mode);

gavl_dictionary_t * gavf_stream_get_hwinfo_nc(gavl_dictionary_t * s);
const gavl_dictionary_t * gavf_stream_get_hwinfo(const gavl_dictionary_t * s);

