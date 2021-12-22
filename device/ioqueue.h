#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64

/* 环形队列 */
struct ioqueue
{
    /* data */
    struct lock lock;
    struct task_struct* producer;   // 记录生产者

    struct task_struct* consumer;   // 消费者
    char buf[bufsize];      // 缓冲区大小
    int32_t head;           // 队首
    int32_t tail;           // 队尾
};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);

#endif
