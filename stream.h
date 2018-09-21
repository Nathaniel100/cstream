#ifndef stream_stream_H
#define stream_stream_H

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _stream stream_t;

/// 根据已打开的文件fd打开流
stream_t *stream_fd_open(int fd, int sflags, uint64_t bufsize);

/// 打开文件流
stream_t *stream_file_open(const char *filename, int oflags, int mode);

/// 根据字符串打开流
// stream_t *stream_string_open(string_t *str);

/// 关闭流
bool stream_close(stream_t *stream);

/// 读取流数据到指定的内存buffer中
bool stream_read(stream_t *stream, void *buf, uint64_t count, uint64_t *nread);

/// 请求预读取流数据
bool stream_readahead(stream_t *stream, uint64_t count);

/// 将指定的内存数据写到入流中
bool stream_write(stream_t *stream, const void *buf, uint64_t count,
                  uint64_t *nwrote);

/// 通过分散-聚集(scatter-gather)接口方式写入流
bool stream_writev(stream_t *stream, const struct iovec *iov, int iovcnt,
                   uint64_t *nwrote);

/// 将格式化数据写入流中
int stream_vprintf(stream_t *stream, const char *fmt, va_list ap);

/// 将格式化数据写入流中
int stream_printf(stream_t *stream, const char *fmt, ...);

/// 刷新流
bool stream_flush(stream_t *stream);

/// 设置流当前读写的位置
bool stream_seek(stream_t *stream, int64_t delta, int whence, uint64_t *newpos);

/// 将流当前读写位置设置为起始
bool stream_rewind(stream_t *stream);

/// 复制src流到dest流中
bool stream_copy(stream_t *src, stream_t *dest, uint64_t num_bytes,
                 uint64_t *nread, uint64_t nwrote);

/// 错误码
int stream_errno(stream_t *stream);

#ifdef __cplusplus
}
#endif

#endif  // stream_stream_H
