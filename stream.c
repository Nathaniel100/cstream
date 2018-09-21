#include "stream.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STREAM_UNGET 8
#define STREAM_BUFFER_SIZE 8192

typedef bool (*stream_close_f)(stream_t *stream);
typedef bool (*stream_readv_f)(stream_t *stream, const struct iovec *iov,
                               int iovcnt, uint64_t *nread);
typedef bool (*stream_writev_f)(stream_t *stream, const struct iovec *iov,
                                int iovcnt, uint64_t *nwrote);
typedef bool (*stream_seek_f)(stream_t *stream, int64_t delta, int whence,
                              uint64_t *newpos);
struct stream_funcs {
  stream_close_f close;
  stream_readv_f readv;
  stream_writev_f writev;
  stream_seek_f seek;
};

struct _stream {
  const struct stream_funcs *funcs;
  // 如果读模式，rpos, rend不为NULL
  uint8_t *rpos, *rend;
  // 如果写模式, wbase, wpos, wend不为NULL
  uint8_t *wpos, *wend;
  uint8_t *wbase;
  // 如果buffer_size为0，说明不使用buffer
  uint8_t *buffer;
  uint64_t buffer_size;
  void *cookie;
  int last_error;
};

//////////////////////////////////////////////////////////////////////////////////
// fd --

static bool fd_close(stream_t *stream);
static bool fd_readv(stream_t *stream, const struct iovec *iov, int iovcnt,
                     uint64_t *nread);
static bool fd_writev(stream_t *stream, const struct iovec *iov, int iovcnt,
                      uint64_t *nwrote);
static bool fd_seek(stream_t *stream, int64_t delta, int whence,
                    uint64_t *newpos);
struct stream_funcs stream_funcs_fd = {fd_close, fd_readv, fd_writev, fd_seek};

static bool fd_close(stream_t *stream) {
  int fd = (int)stream->cookie;
  if (close(fd) == 0) {
    return true;
  }
  stream->last_error = errno;
  return false;
}

static bool fd_readv(stream_t *stream, const struct iovec *iov, int iovcnt,
                     uint64_t *nread) {
  int fd = (int)stream->cookie;
  int64_t r = readv(fd, iov, iovcnt);
  if (r < 0) {
    stream->last_error = errno;
    return false;
  }
  if (nread) *nread = (uint64_t)r;
  return true;
}

static bool fd_writev(stream_t *stream, const struct iovec *iov, int iovcnt,
                      uint64_t *nwrote) {
  int fd = (int)stream->cookie;
  int64_t w = writev(fd, iov, iovcnt);
  if (w < 0) {
    stream->last_error = errno;
    return false;
  }
  if (nwrote) *nwrote = (uint64_t)w;
  return true;
}

static bool fd_seek(stream_t *stream, int64_t delta, int whence,
                    uint64_t *newpos) {
  int fd = (int)stream->cookie;
  int64_t pos = lseek(fd, delta, whence);
  if (pos < 0) {
    stream->last_error = errno;
    return false;
  }
  if (newpos) *newpos = (uint64_t)pos;
  return true;
}

// -- fd
//////////////////////////////////////////////////////////////////////////////////

static stream_t *stream_make(const struct stream_funcs *funcs, void *cookie,
                             uint64_t buffer_size) {
  stream_t *stream =
      (stream_t *)malloc(sizeof(stream_t) + STREAM_UNGET + buffer_size);
  if (!stream) {
    return NULL;
  }
  memset(stream, 0, sizeof(stream_t));
  stream->buffer = (uint8_t *)stream + sizeof(stream_t) + STREAM_UNGET;
  stream->buffer_size = buffer_size;
  stream->funcs = funcs;
  stream->cookie = cookie;
  return stream;
}

static void stream_destory(stream_t *stream) {
  stream->buffer = NULL;
  stream->funcs = NULL;
  stream->cookie = NULL;
  free(stream);
}

/// 根据已打开的文件fd打开流
stream_t *stream_fd_open(int fd, int sflags, uint64_t bufsize) {
  return stream_make(&stream_funcs_fd, INT2VOIDPTR(fd), bufsize);
}

/// 打开文件流
stream_t *stream_file_open(const char *filename, int oflags, int mode) {
  int fd = open(filename, oflags, mode);
  if (fd < 0) {
    return NULL;
  }
  stream_t *stream = stream_fd_open(fd, 0, STREAM_BUFFER_SIZE);
  if (!stream) {
    close(fd);
    return NULL;
  }
  return stream;
}

/// 关闭流
bool stream_close(stream_t *stream) {
  if (!stream_flush(stream)) {
    return false;
  }
  if (!stream->funcs->close(stream)) {
    errno = stream_errno(stream);
    return false;
  }
  stream_destory(stream);
  return true;
}

/// 读取流数据到指定的内存buffer中
bool stream_read(stream_t *stream, void *buf, uint64_t count, uint64_t *nread) {
  if (!stream_flush(stream)) {
    return false;
  }
  stream->last_error = 0;
  if (count == 0) {
    if (nread) *nread = count;
    return true;
  }
  uint8_t *dest = (uint8_t *)buf;
  uint64_t res = 0;
  if (stream->rend > stream->rpos) {
    uint64_t x = FAST_MIN(stream->rend - stream->rpos, count);
    memcpy(dest, stream->rpos, x);
    stream->rpos += x;
    dest += x;
    count -= x;
    res += x;
  }
  while (count > 0) {
    struct iovec vecs[2] = {
        // Read into the dest buffer, but overflow any excess into our buffer.
        // Handle edge case with readv as discussed in
        // http://git.musl-libc.org/cgit/musl/commit/src/stdio/__stdio_read.c?id=2cff36a84f268c09f4c9dc5a1340652c8e298dc0
        {.iov_base = dest, .iov_len = count - !!stream->buffer_size},
        {.iov_base = stream->buffer, .iov_len = stream->buffer_size}};
    struct iovec *pvec = vecs;
    uint64_t cnt = 0;
    if (!stream->funcs->readv(stream, pvec, 2, &cnt) || cnt == 0) {
      stream->rpos = stream->rend = 0;
      if (res) {
        // 部分成功
        break;
      }
      errno = stream_errno(stream);
      return false;
    }
    if (cnt > vecs[0].iov_len) {
      cnt -= vecs[0].iov_len;
      stream->rpos = stream->buffer;
      stream->rend = stream->buffer + cnt;
      if (stream->buffer_size) {
        dest[count - 1] = *stream->rpos++;
      }
      cnt = count;
    }
    res += cnt;
    dest += cnt;
    count -= cnt;
  }
  if (nread) *nread = res;
  return true;
}

/// 预读取流数据
bool stream_readahead(stream_t *stream, uint64_t count) {
  if (!stream_flush(stream)) {
    return false;
  }
  if (!stream->buffer_size) {
    // 没有buffer, 我们不需要读
    return true;
  }
  if (stream->rend > stream->rpos + count) {
    return true;
  }
  uint64_t buf_avail;
  if (stream->rend) {
    buf_avail = stream->buffer + stream->buffer_size - stream->rend;
  } else {
    stream->rpos = stream->buffer;
    stream->rend = stream->buffer;
    buf_avail = stream->buffer_size;
  }
  if (!buf_avail) {
    // 没有足够的buffer
    return true;
  }
  struct iovec vec = {.iov_base = stream->rpos, .iov_len = buf_avail};
  uint64_t nread;
  if (!stream->funcs->readv(stream, &vec, 1, &nread)) {
    errno = stream_errno(stream);
    return false;
  }
  stream->rend += nread;
  return true;
}

static bool do_write(stream_t *stream, const void *buf, uint64_t count,
                     uint64_t *nwrote) {
  if (!stream->wend) {
    stream->rpos = stream->rend = 0;
    stream->wbase = stream->wpos = stream->buffer;
    stream->wend = stream->buffer + stream->buffer_size;
  }
  if (count && (stream->wend > stream->wpos + count)) {
    memcpy(stream->wpos, buf, count);
    stream->wpos += count;
    if (nwrote) *nwrote = count;
    return true;
  }
  struct iovec vecs[2] = {
      {.iov_base = stream->wbase, .iov_len = stream->wpos - stream->wbase},
      {.iov_base = (void *)buf, .iov_len = count}};
  struct iovec *pvec = vecs;
  int vec_count = 2;
  uint64_t to_wrote = vecs[0].iov_len + vecs[1].iov_len;
  uint64_t x = 0;
  for (;;) {
    if (!stream->funcs->writev(stream, pvec, vec_count, &x)) {
      stream->wpos = stream->wbase = stream->wend = 0;
      if (vec_count == 2) {
        errno = stream_errno(stream);
        return false;
      }
      if (nwrote) {
        *nwrote = count - pvec[0].iov_len;
      }
      return true;
    }
    if (x == to_wrote) {
      stream->wbase = stream->wpos = stream->buffer;
      stream->wend = stream->buffer + stream->buffer_size;
      if (nwrote) *nwrote = count;
      return true;
    }

    to_wrote -= x;
    if (x > pvec[0].iov_len) {
      stream->wpos = stream->wbase = stream->buffer;
      x -= pvec[0].iov_len;
      pvec++;
      vec_count--;
    } else if (vec_count == 2) {
      stream->wbase += x;
    }
    pvec[0].iov_base = (char *)pvec[0].iov_base + x;
    pvec[0].iov_len -= x;
  }
}

/// 将指定的内存数据写到入流中
bool stream_write(stream_t *stream, const void *buf, uint64_t count,
                  uint64_t *nwrote) {
  stream->last_error = 0;
  if (count == 0) {
    if (nwrote) *nwrote = 0;
    return true;
  }
  return do_write(stream, buf, count, nwrote);
}

/// 刷新流
bool stream_flush(stream_t *stream) {
  if (stream->wpos > stream->wbase) {
    do_write(stream, 0, 0, 0);
    if (!stream->wpos) {
      errno = stream_errno(stream);
      return false;
    }
  }
  return true;
}

int stream_errno(stream_t *stream) {
  return stream->last_error;
}