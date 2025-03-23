
#include <gavl/gavl.h>
#include <gmerlin/mediaconnector.h>

typedef struct gavf_reader_s gavf_reader_t;
typedef struct gavf_writer_s gavf_writer_t;

gavf_reader_t * gavf_reader_create();
gavf_writer_t * gavf_writer_create();

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

#define GAVF_HEADER         "GAVFHEAD"
#define GAVF_PACKETS        "GAVFPACK"
#define GAVF_FOOTER         "GAVFFOOT"
#define GAVF_TAIL           "GAVFTAIL"
#define GAVF_INDEX          "GAVFINDX"
#define GAVF_TRACK          "GAVFTRAC"

// GAVL_MSG_NS_GAVF

#define GAVF_MSG_END           1 /* Stream is finished */
#define GAVF_MSG_DISCONTINUITY 2 /* Discontinuity   */
#define GAVF_MSG_RESYNC        3 /* Post seek resync   */

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
  S->C GAVF_END_ (EOF)
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
 * GAVFINFO
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

int gavf_reader_skip_packets(gavf_reader_t * g);

int gavf_writer_send_media_info(gavf_writer_t * g, const gavl_dictionary_t * mi);

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

int gavf_writer_read_message(gavf_writer_t * wr, gavl_msg_t * ret, int timeout); 


bg_media_source_t * gavf_reader_get_source(gavf_reader_t * g);

/* Start the instance */
int gavf_reader_start(gavf_reader_t * g);
// int gavf_writer_start(gavf_writer_t * g);

bg_controllable_t * gavf_reader_get_controllable(gavf_reader_t * g);

void gavf_reader_destroy(gavf_reader_t * g);
void gavf_writer_destroy(gavf_writer_t * g);

