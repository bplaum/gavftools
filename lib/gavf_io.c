
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

static gavl_io_t * open_socket(char * uri, int wr)
  {
  int result = 0;
  int server_fd;
  int socket_fd;
  gavl_socket_address_t * addr = NULL;
  gavl_io_t * ret = NULL;
  char * name = NULL;
  
  if(gavl_string_starts_with(uri, GAVF_PROTOCOL_SERVER":///"))
    {
    /* Unix server */
    server_fd = gavl_listen_socket_create_unix(strstr(uri, ":///") + 2, 1);
    socket_fd = gavl_listen_socket_accept(server_fd, -1, NULL);
    gavl_socket_close(server_fd);
    
    if(socket_fd < 0)
      goto fail;

    ret = gavl_io_create_socket(socket_fd, 3000, 0);

    if(!server_handshake(ret, wr))
      goto fail;
    
    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL_SERVER"://"))
    {
    int server_fd;
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
    socket_fd = gavl_listen_socket_accept(server_fd, -1, NULL);
    gavl_socket_close(server_fd);
    
    ret = gavl_io_create_socket(socket_fd, 3000, 0);

    if(!server_handshake(ret, wr))
      goto fail;

    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL":///"))
    {
    /* Unix client */
    if(!gavl_url_split(uri, NULL, NULL, NULL, NULL, NULL, &name))
      goto fail;
    
    socket_fd = gavl_socket_connect_unix(name);
    ret = gavl_io_create_socket(socket_fd, 3000, 0);

    if(!client_handshake(ret, wr))
      goto fail;

    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL"://"))
    {
    int port;
    addr = gavl_socket_address_create();
    /* TCP client */
    if(!gavl_url_split(uri, NULL, NULL, NULL, &name, &port, NULL))
      goto fail;

    if(!gavl_socket_address_set(addr, name, port, SOCK_STREAM))
      goto fail;
    
    socket_fd = gavl_socket_connect_inet(addr, 3000);
    ret = gavl_io_create_socket(socket_fd, 3000, 0);
    
    if(!client_handshake(ret, wr))
      goto fail;
    
    }  

  result = 1;
  
  fail:
  if(addr)
    gavl_socket_address_destroy(addr);
  if(name)
    free(name);
  
  if(!result && ret)
    {
    gavl_io_destroy(ret);
    ret = NULL;
    }
  
  return ret;
  }

gavl_io_t * gavf_reader_open_io(gavf_reader_t * g, const char * uri1)
  {
  gavl_io_t * ret;
  int timeout = 1000 * 60;
  char * uri;
  int io_flags;

  if(gavl_string_starts_with_i(uri1, "file://"))
    uri1 += 7;
  
  uri = gavl_strdup(uri1);
  gavl_url_get_vars(uri, &g->url_vars);
  gavl_dictionary_get_int(&g->url_vars, "timeout", &timeout);
  
  /* stdin */
  if(!uri || gavl_string_starts_with(uri, GAVF_PROTOCOL"://-"))
    ret = gavl_io_create_file(stdin, 0, 0, 0);
  
  else 
    {
    if(!(ret = open_socket(uri, 0)))
      {
      struct stat st;

      /* Local file */
      if(!stat(uri, &st))
        {
        FILE * f;
        if(!(f = fopen(uri, "r")))
          goto fail;
        
        ret = gavl_io_create_file(f, 0, (st.st_mode & S_IFMT)==S_IFREG, 1);
        }

      }
    }

  if(!ret)
    goto fail;
  
  io_flags = gavl_io_get_flags(ret);
  
  if(io_flags & GAVL_IO_IS_REGULAR)
    g->flags |= FLAG_ON_DISK;
  if(io_flags & GAVL_IO_IS_SOCKET)
    g->bkch_io = gavl_io_create_socket(gavl_io_get_socket(ret), 3000, 0);

  fail:
  
  return ret;
  }

gavl_io_t * gavf_writer_open_io(gavf_writer_t * g, const char * uri1)
  {
  int io_flags;
  gavl_io_t * ret;
  char * uri;
  char * parent = NULL;
  const char * pos;

  
  if(uri1 && gavl_string_starts_with_i(uri1, "file://"))
    uri1 += 7;
  
  uri = gavl_strdup(uri1);
  
  if(!uri || gavl_string_starts_with(uri, GAVF_PROTOCOL"://-"))
    ret = gavl_io_create_file(stdout, 1, 0, 0);
  else 
    {
    if(!(ret = open_socket(uri, 1)))
      {
      struct stat st;
      
      if(!stat(uri, &st))
        {
        FILE * f;
        if(!(f = fopen(uri, "w")))
          goto fail;
        
        ret = gavl_io_create_file(f, 1, (st.st_mode & S_IFMT)==S_IFREG, 1);
        }
      
      if(!(pos = strrchr(uri, '/')))
        parent = gavl_strdup(".");
      else
        {
        parent = gavl_strdup(uri);
        parent[(int)(pos - uri)] = '\0';
        }
      
      if(!access(parent, W_OK))
        {
        FILE * out = fopen(uri, "w");
        gavl_io_create_file(out, 1, 0, 0);
        }
      
      }
    }

  if(!ret)
    goto fail;
  
  io_flags = gavl_io_get_flags(ret);

  if(io_flags & GAVL_IO_IS_PIPE)
    {
    /* Writing to pipe: Redirect to UNIX socket */

    gavl_io_t * sub_io;
    int server_fd;
    int fd;
    
    char * unix_socket = NULL;
    char * new_addr = NULL;
    
    gavl_dictionary_t mi;
    gavl_dictionary_t * dict;
    gavl_dictionary_init(&mi);
    dict = gavl_append_track(&mi, NULL);

    server_fd = gavl_unix_socket_create(&unix_socket, 1);

    new_addr = gavl_sprintf("%s://%s", GAVF_PROTOCOL, unix_socket);
    gavl_track_from_location(dict, new_addr);
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Redirecting to %s", new_addr);

    sub_io = gavl_chunk_start_io(ret, &g->ch, GAVF_HEADER);
    gavl_dictionary_write(sub_io, &mi);
    gavl_chunk_finish_io(ret, &g->ch, sub_io);
    
    fd = gavl_listen_socket_accept(server_fd, 3000, NULL);
    gavl_socket_close(server_fd);

    gavl_dictionary_free(&mi);
    
    if(fd < 0)
      goto fail;

    gavl_io_destroy(ret);
    
    ret = gavl_io_create_socket(fd, 3000, GAVL_IO_SOCKET_DO_CLOSE);
    if(!server_handshake(ret, 1))
      goto fail;   
    
    }

  if(io_flags & GAVL_IO_IS_REGULAR)
    g->flags |= FLAG_ON_DISK;
  if(io_flags & GAVL_IO_IS_SOCKET)
    g->bkch_io = gavl_io_create_socket(gavl_io_get_socket(ret), 3000, 0);
   
  fail:

  if(uri)
    free(uri);
  
  return ret;
  }

#if 0
gavl_io_t * gavf_open_io(gavf_reader_t * g, const char * uri1, int wr)
  {
  gavl_io_t * ret;
  char * name = NULL;
  char * uri = gavl_strdup(uri1);
  struct stat st;
  int io_flags;
  
  if(!uri || !gavl_string_starts_with(uri, GAVF_PROTOCOL"://-"))
    {
    if(wr)
      ret = gavl_io_create_file(stdout, 1, 0, 0);
    else
      ret = gavl_io_create_file(stdin, 0, 0, 0);
    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL_SERVER":///"))
    {
    int server_fd;
    /* Unix server */
    server_fd = gavl_listen_socket_create_unix(strstr(uri, ":///") + 2, 1);
    g->socket_fd = gavl_listen_socket_accept(server_fd, timeout, NULL);
    gavl_socket_close(server_fd);
    
    if(g->socket_fd < 0)
      goto fail;

    ret = gavl_io_create_socket(g->socket_fd, timeout, 0);

    if(!server_handshake(ret, wr))
      goto fail;
    
    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL_SERVER"://"))
    {
    int server_fd;
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
    g->socket_fd = gavl_listen_socket_accept(server_fd, timeout, NULL);
    gavl_socket_close(server_fd);
    
    ret = gavl_io_create_socket(g->socket_fd, timeout, 0);

    if(!server_handshake(ret, wr))
      goto fail;

    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL":///"))
    {
    /* Unix client */
    if(!gavl_url_split(uri, NULL, NULL, NULL, NULL, NULL, &name))
      goto fail;
    
    g->socket_fd = gavl_socket_connect_unix(name);
    ret = gavl_io_create_socket(g->socket_fd, timeout, 0);

    if(!client_handshake(ret, wr))
      goto fail;

    }
  else if(gavl_string_starts_with(uri, GAVF_PROTOCOL"://"))
    {
    int port;
    addr = gavl_socket_address_create();
    /* TCP client */
    if(!gavl_url_split(uri, NULL, NULL, NULL, &name, &port, NULL))
      goto fail;

    if(!gavl_socket_address_set(addr, name, port, SOCK_STREAM))
      goto fail;
    
    g->socket_fd = gavl_socket_connect_inet(addr, timeout);
    ret = gavl_io_create_socket(g->socket_fd, timeout, 0);
    
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

  if(io_flags & GAVL_IO_IS_REGULAR)
    g->flags |= FLAG_ON_DISK;
  else if(g->socket_fd >= 0)
    {
    g->bkch_io = gavl_io_create_socket(g->socket_fd, 3000, 0);
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
#endif


