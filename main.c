
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
    fprintf(stderr, "error: %s\n", strerror(errno));
    return 1;
  }
  for (int i = 0; i < 100; i++) {
    const char *hello = "Hello";
    uint64_t nwrote = 0;
    if (!stream_write(stream, hello, 5, &nwrote)) {
      fprintf(stderr, "error: %s\n", strerror(errno));
      return 1;
    }
  } 
  stream_close(stream); // 已写入500个Hello\0

  // 读
  stream = stream_file_open("tmp", O_RDONLY, 0644);
  char buffer[1024] = {0};
  uint64_t nread = 0;
  if(!stream_read(stream, buffer, sizeof(buffer), &nread)) {
    fprintf(stderr, "error: %s\n", strerror(errno));
    return 1;
  }
  printf("Read: %s\n", buffer);
  stream_close(stream);
  // 移除文件
  remove("tmp");
  return 0;
}