
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

// #include <stdlib.h>

#include <gavf.h>
#include <gavfprivate.h>
#include <gavl/utils.h>
#include <gavl/gavlsocket.h>
#include <gavl/http.h>
#include <gavl/log.h>
#define LOG_DOMAIN "gavfio"

static int server_handshake(gavl_io_t * io, int wr)
  {
  int ret = 0;
  gavl_dictionary_t req;
  gavl_dictionary_t res;
  const char * method;
  
  gavl_dictionary_init(&req);
  gavl_dictionary_init(&res);

  if(!gavl_http_request_read(io, &req))
    goto fail;

  if(!(method = gavl_http_request_get_method(&req)))
    goto fail;

  if((wr && strcmp(method, "GET")) ||
     (!wr && strcmp(method, "PUT")))
    {
    gavl_http_response_init(&res, GAVF_PROTOCOL_STRING, 405, "Method Not Allowed");
    gavl_http_response_write(io, &res);
    goto fail;
    }
  
  gavl_http_response_init(&res, GAVF_PROTOCOL_STRING, 200, "OK");
  gavl_http_response_write(io, &res);
  
  ret = 1;
  fail:
  
  gavl_dictionary_free(&req);
  gavl_dictionary_free(&res);
  
  return ret;
  }

static int client_handshake(gavl_io_t * io, int wr)
  {
  int ret = 0;
  gavl_dictionary_t req;
  gavl_dictionary_t res;
  
  gavl_dictionary_init(&req);
  gavl_dictionary_init(&res);

  gavl_http_request_init(&req, wr ? "PUT" : "GET",
                         "*", GAVF_PROTOCOL_STRING);

  if(!gavl_http_request_write(io, &req))
    goto fail;

  if(!gavl_http_response_read(io, &res))
    goto fail;

  if(gavl_http_response_get_status_int(&res) != 200)
    goto fail;
  
  ret = 1;
  fail:
  
  gavl_dictionary_free(&req);
  gavl_dictionary_free(&res);
  
  return ret;
  }

gavl_io_t * gavf_open_io(gavf_t * g, const char * uri1, int wr)
  {
  gavl_io_t * ret;
  char * name = NULL;
  char * uri = gavl_strdup(uri1);
  int timeout = 1000 * 60;
  gavl_socket_address_t * addr = NULL;
  struct stat st;
  int io_flags;
  
  gavl_url_get_vars(uri, &g->url_vars);

  gavl_dictionary_get_int(&g->url_vars, "timeout", &timeout);
  
  if(!uri || !strcmp(uri, "-"))
    {
    if(wr)
      ret = gavl_io_create_file(stdout, 1, 0, 0);
    else
      ret = gavl_io_create_file(stdin, 0, 0, 0);
    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL_SERVER":///"))
    {
    int server_fd;
    int fd;
    char * name = NULL;
    /* Unix server */
    server_fd = gavl_unix_socket_create(&name, 1);
    
    fd = gavl_listen_socket_accept(server_fd, timeout, NULL);

    gavl_socket_close(server_fd);
    
    if(fd < 0)
      goto fail;

    ret = gavl_io_create_socket(fd, timeout, GAVL_IO_SOCKET_DO_CLOSE);

    if(!server_handshake(ret, wr))
      goto fail;
    
    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL_SERVER"://"))
    {
    int server_fd;
    int fd;
    int port = -1;
    /* TCP server */
    addr = gavl_socket_address_create();

    if(!gavl_url_split(uri, NULL, NULL, NULL, &name, &port, NULL))
      goto fail;

    if(port < 0)
      port = 0;
    
    if(!gavl_socket_address_set(addr, name, port, SOCK_STREAM))
      goto fail;
    
    server_fd = gavl_listen_socket_create_inet(addr, port, 1, 0);
    fd = gavl_listen_socket_accept(server_fd, timeout, NULL);
    gavl_socket_close(server_fd);
    
    ret = gavl_io_create_socket(fd, timeout, GAVL_IO_SOCKET_DO_CLOSE);

    if(!server_handshake(ret, wr))
      goto fail;

    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL":///"))
    {
    int fd;
    /* Unix client */
    if(!gavl_url_split(uri, NULL, NULL, NULL, NULL, NULL, &name))
      goto fail;
    
    fd = gavl_socket_connect_unix(name);
    ret = gavl_io_create_socket(fd, timeout, GAVL_IO_SOCKET_DO_CLOSE);

    if(!client_handshake(ret, wr))
      goto fail;

    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL"://"))
    {
    int port;
    int fd;
    addr = gavl_socket_address_create();
    /* TCP client */
    if(!gavl_url_split(uri, NULL, NULL, NULL, &name, &port, NULL))
      goto fail;

    if(!gavl_socket_address_set(addr, name, port, SOCK_STREAM))
      goto fail;
    
    fd = gavl_socket_connect_inet(addr, timeout);
    ret = gavl_io_create_socket(fd, timeout, GAVL_IO_SOCKET_DO_CLOSE);
    
    if(!client_handshake(ret, wr))
      goto fail;
    
    }
  /* Check for local file */
  else if(!stat(uri, &st))
    {
    FILE * f = fopen(uri, wr ? "w" : "r");
    ret = gavl_io_create_file(f, wr, 1, 1);
    }
  else
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Don't know how to open %s", uri1);
    goto fail;
    }
  
  /* If we have a writable pipe: Redirect to UNIX socket. */
  io_flags = gavl_io_get_flags(ret);
  if((io_flags & GAVL_IO_IS_PIPE) && wr)
    {
    gavl_io_t * sub_io;
    int server_fd;
    int fd;
    
    char * unix_socket;
    char * new_addr;
    
    gavl_dictionary_t dict;
    gavl_dictionary_init(&dict);

    server_fd = gavl_unix_socket_create(&unix_socket, 1);

    new_addr = gavl_sprintf("%s://%s", GAVF_PROTOCOL, unix_socket);
    gavl_track_from_location(&dict, new_addr);
    gavl_log(GAVL_LOG_INFO, "Redirecting to %s", new_addr);

    sub_io = gavl_chunk_start_io(ret, &g->ch, GAVF_PROGRAM_HEADER);
    gavl_dictionary_write(sub_io, &dict);
    gavl_chunk_finish_io(ret, &g->ch, sub_io);
    
    fd = gavl_listen_socket_accept(server_fd, timeout, NULL);
    gavl_socket_close(server_fd);
    
    if(fd < 0)
      goto fail;

    gavl_io_destroy(ret);
    
    ret = gavl_io_create_socket(fd, timeout, GAVL_IO_SOCKET_DO_CLOSE);
    if(!server_handshake(ret, wr))
      goto fail;
    
    }
  
  goto cleanup;
  
  fail:
  gavl_io_destroy(ret);
  ret = NULL;

  cleanup:

  if(name)
    free(name);
  
  return ret;
  }
