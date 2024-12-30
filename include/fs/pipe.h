#ifndef __PIPE_H__
#define __PIPE_H__

// 管道文件的最大大小
#define PIPE_SIZE 500
// 如果读写出错返回这个值
#define PIPE_ERROR 512

#include "common.h"
#include "lib/lock.h"
typedef struct file file_t;

typedef struct pipe {
  spinlock_t lock;
  char data[PIPE_SIZE];
  uint32 nread;     // 读取的字节数
  uint32 nwrite;    // 写入的字节数
  int readopen;   // 是否仍然可读
  int writeopen;  // 是否仍然可写
} pipe_t;

int pipe_alloc(file_t **f1,file_t **f2);
uint32 pipe_read(pipe_t *pipe, uint64 addr, uint32 len);
uint32 pipe_write(pipe_t *pipe, uint64 addr, uint32 len);
void pipe_close(pipe_t *pipe, int writable);

#endif
