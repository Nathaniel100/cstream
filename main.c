
#include "stream.h"

#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

int main() {
  // 写
  stream_t *stream = stream_file_open("tmp", O_RDWR | O_CREAT, 0644);
  if (!stream) {
    fprintf(stderr, "open error: %s\n", strerror(errno));
    return 1;
  }
  uint64_t nwrote = 0;
  for (int i = 0; i < 100; i++) {
    const char *hello = "Hello";
    if (!stream_write(stream, hello, 5, &nwrote)) {
      fprintf(stderr, "write error: %s\n", strerror(errno));
      return 1;
    }
  }

  // 使用wrotev写
  nwrote = 0;
  struct iovec iov[2] = {{"World", 5}, {"World", 5}};
  if (!stream_writev(stream, iov, 2, &nwrote)) {
    fprintf(stderr, "writev error: %s\n", strerror(errno));
    return 1;
  }

  // seek到开头继续写
  if(!stream_seek(stream, 0, SEEK_SET, 0)) {
    fprintf(stderr, "seek error: %s\n", strerror(errno));
    return 1;
  }

  if (!stream_writev(stream, iov, 2, &nwrote)) { // 覆盖开始的2个Hello
    fprintf(stderr, "writev error: %s\n", strerror(errno));
    return 1;
  }

  stream_close(stream); // 已写入2个World+98个Hello + 2个World

  // 读
  stream = stream_file_open("tmp", O_RDONLY, 0644);
  if (!stream) {
    fprintf(stderr, "open error: %s\n", strerror(errno));
    return 1;
  }
  char buffer[1024] = {0};
  uint64_t nread = 0;
  if (!stream_read(stream, buffer, sizeof(buffer), &nread)) {
    fprintf(stderr, "read error: %s\n", strerror(errno));
    return 1;
  }
  printf("Read: %s\n", buffer); //2个World+98个Hello + 2个World 
  stream_close(stream);

  stream = stream_file_open("tmp", O_RDONLY, 0644);
  if (!stream) {
    fprintf(stderr, "open error: %s\n", strerror(errno));
    return 1;
  }

  memset(buffer, 0, sizeof(buffer));
  if (!stream_read(stream, buffer, 10, &nread)) {
    fprintf(stderr, "read error: %s\n", strerror(errno));
    return 1;
  }
  printf("Read first 10: %s\n", buffer); // 2个World

  // 跳过98个Hello + 1个World
  if (!stream_seek(stream, 99 * 5, SEEK_CUR, 0)) {
    fprintf(stderr, "read error: %s\n", strerror(errno));
    return 1;
  }
  memset(buffer, 0, sizeof(buffer));
  if (!stream_read(stream, buffer, sizeof(buffer), &nread)) {
    fprintf(stderr, "read error: %s\n", strerror(errno));
    return 1;
  }
  printf("Read: %s\n", buffer); // World

  // seek到倒数第10个
  if(!stream_seek(stream, -10, SEEK_END, 0)) {
    fprintf(stderr, "seek error: %s\n", strerror(errno));
    return 1;
  }

  memset(buffer, 0, sizeof(buffer));
  if (!stream_read(stream, buffer, sizeof(buffer), &nread)) {
    fprintf(stderr, "error: %s\n", strerror(errno));
    return 1;
  }
  printf("Read last 10: %s\n", buffer); // 2个World

  stream_close(stream);
  // 移除文件
  remove("tmp");
  return 0;
}