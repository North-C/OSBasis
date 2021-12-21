#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "list.h"
#include "thread.h"
#include "stdint.h"

// 信号量
struct semaphore{
    uint8_t value;
    struct list waiters;        // 等待序列
};

// 锁结构
struct lock{
    struct task_struct* holder;     // 持有者
    struct semaphore sema;          // 信号量
    uint32_t holder_repeat_nr;      // 持有者的重复申请次数，为了避免重复释放锁
};


void sema_init(struct semaphore* sema, uint8_t value);
void lock_init(struct lock* lock);
void sema_down(struct semaphore* sema);
void sema_up(struct semaphore* sema);
void lock_acquire(struct lock* plock);
void lock_release(struct lock* plock);

#endif
