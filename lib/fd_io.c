#include <errno.h>
#include <sys/socket.h>
#include <string.h>


#include <gavf.h>
#include <gavfprivate.h>

#include <gavl/log.h>
#define LOG_DOMAIN "fd_io"

typedef struct
  {
  uint64_t map_len;   /* size of this plane's mapping in bytes  */
  uint64_t map_offset; /* byte offset into this plane's mapping  */
  } fd_plane_t;

typedef struct
  {
  uint32_t  n_fds;             /* number of planes / fds (<= GAVL_MAX_PLANES) */
  fd_plane_t planes[GAVL_MAX_PLANES];
  } fd_pass_meta_t;



/* Read/write buffers via Unix domain sockets */

int gavf_write_hw_buffers(gavl_io_t * io, const gavl_hw_buffer_t * buffers, int num, void * ext_data, int ext_len)
  {
  fd_pass_meta_t meta;
  struct iovec iov[2];
  size_t cmsg_size;
  char  *cmsg_buf = NULL;
  struct msghdr msg;
  ssize_t ret;
  struct cmsghdr *cmsg;
  int i;
  int fds[GAVL_MAX_PLANES];
  int fd;
  int result = 0;
  
  if((fd = gavl_io_get_socket(io)) < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot send fds: No socket based I/O");
    goto fail;
    }

  if((num <= 0) || (num > GAVL_MAX_PLANES))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot send fds: Invalid number %d", num);
    return 0;
    }

  if(!buffers)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid buffer");
    return 0;
    }
     
  memset(&meta, 0, sizeof(meta));
  memset(&iov, 0, sizeof(iov));
  memset(&msg, 0, sizeof(msg));
  
  /* Regular payload: plane count + per-plane metadata. */
  meta.n_fds = num;

  for(i = 0; i < num; i++)
    {
    meta.planes[i].map_offset = buffers[i].map_offset;
    meta.planes[i].map_len = buffers[i].map_len;
    fds[i] = buffers[i].fd;
    }
  
  iov[0].iov_base = &meta;
  iov[0].iov_len  = sizeof(meta);

  if(ext_data)
    {
    iov[1].iov_base = ext_data;
    iov[1].iov_len  = ext_len;
    }
  
  /* Ancillary data: SCM_RIGHTS carries the raw file descriptors.
   * CMSG_SPACE rounds up to the required alignment. */
  cmsg_size = CMSG_SPACE(num * sizeof(int));
  cmsg_buf  = calloc(1, cmsg_size);
  if(!cmsg_buf)
    goto fail;
  
  msg.msg_iov        = iov;
  msg.msg_iovlen     = 1 + !!ext_data;
  msg.msg_control    = cmsg_buf;
  msg.msg_controllen = cmsg_size;
  
  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type  = SCM_RIGHTS;
  cmsg->cmsg_len   = CMSG_LEN(num * sizeof(int));
  memcpy(CMSG_DATA(cmsg), fds, num * sizeof(int));

  ret = sendmsg(fd, &msg, 0);

  if(ret < 0)
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "sendmsg failed: %s", strerror(errno));
  else
    result = 1;
  
  fail:
  
  free(cmsg_buf);
  
  return result;
  }

int gavf_read_hw_buffers(gavl_io_t * io, gavl_hw_buffer_t * buffers, int * num, void * ext_data, int ext_len)
  {
  fd_pass_meta_t meta;
  struct iovec iov[2];

  size_t cmsg_size;
  char  *cmsg_buf = NULL;
  struct msghdr msg;
  ssize_t ret;
  struct cmsghdr *cmsg;
  int result = 0;
  int fd;
  
  if((fd = gavl_io_get_socket(io)) < 0)
    goto fail;
  
  memset(&meta, 0, sizeof(meta));
  memset(&iov, 0, sizeof(iov));
  memset(&msg, 0, sizeof(msg));
  
  iov[0].iov_base = &meta;
  iov[0].iov_len  = sizeof(meta);

  if(ext_data)
    {
    iov[1].iov_base = ext_data;
    iov[1].iov_len  = ext_len;
    }
  
  cmsg_size = CMSG_SPACE(GAVL_MAX_PLANES * sizeof(int));
  cmsg_buf  = calloc(1, cmsg_size);

  msg.msg_iov        = iov;
  msg.msg_iovlen     = 1 + !!ext_data;
  msg.msg_control    = cmsg_buf;
  msg.msg_controllen = cmsg_size;

  ret = recvmsg(fd, &msg, 0);
  if(ret < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "recvmsg failed: %s", strerror(errno));
    goto fail;
    }
  
  /* Validate the count before touching ancillary data. */
  if((meta.n_fds <= 0) || (meta.n_fds > GAVL_MAX_PLANES))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid fd count %d", meta.n_fds);
    goto fail;
    }

  if(meta.n_fds > *num)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Insufficient buffers (%d > %d)", meta.n_fds, *num);
    goto fail;
    }

  
  /* Walk the ancillary chain looking for SCM_RIGHTS. */
  cmsg = CMSG_FIRSTHDR(&msg);

  while(cmsg)
    {
    if((cmsg->cmsg_level == SOL_SOCKET) &&
       (cmsg->cmsg_type  == SCM_RIGHTS))
      {
      int i;
      int fds[GAVL_MAX_PLANES];
      size_t expected = CMSG_LEN(meta.n_fds * sizeof(int));

      if(cmsg->cmsg_len < expected)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "cmsg_len mismatch (%d != %d)",
                 (int)cmsg->cmsg_len, (int)expected);
        goto fail;
        }
      
      memcpy(fds, CMSG_DATA(cmsg), meta.n_fds * sizeof(int));
      memset(buffers, 0, meta.n_fds * sizeof(buffers[0]));
      
      for(i = 0; i < meta.n_fds; i++)
        {
        buffers[i].map_offset = meta.planes[i].map_offset;
        buffers[i].map_len    = meta.planes[i].map_len;
        buffers[i].fd         = fds[i];
        }
      *num = meta.n_fds;
      result = 1;
      break;
      }
    cmsg = CMSG_NXTHDR(&msg, cmsg);
    }

  if(!result)
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Got no filedescriptors");
  
  fail:
  
  if(cmsg_buf)
    free(cmsg_buf);
  
  return result;
  
  }
