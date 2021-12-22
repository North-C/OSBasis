#include "ioqueue.h"
#include "interrupt.h"
#include "thread.h"
#include "global.h"
#include "debug.h"

bool ioq_empty(struct ioqueue* ioq);
static int32_t next_pos(int32_t pos);
static void ioq_wait(struct task_struct** waiter);
static void wakeup(struct task_struct** waiter);

/* 初始化环形队列 */
void ioqueue_init(struct ioqueue* ioq){
    lock_init(&ioq->lock);
    ioq->consumer = NULL;
    ioq->producer = NULL;
    ioq->head = 0;
    ioq->tail = 0;
}
/* 查找下一个队列中的下一个位置 */
static int32_t next_pos(int32_t pos){
    return (pos + 1) % bufsize;
}
/* 判断队列是否已满 */
bool ioq_full(struct ioqueue* ioq){
    ASSERT( get_intr_status() == INT_OFF);
    return next_pos(ioq->head) == ioq->tail;
}

/* 判断队列是否为空 */
bool ioq_empty(struct ioqueue* ioq){
    ASSERT(get_intr_status() == INT_OFF);
    return ioq->head == ioq->tail;
}

/* 使当前消费者和生产者在ioq缓冲区上等待 */
static void ioq_wait(struct task_struct** waiter){
    ASSERT(*waiter==NULL && waiter != NULL);
    *waiter = running_thread();
    thread_block(TASK_BLOCKED);
}

/* 唤醒 */
static void wakeup(struct task_struct** waiter){
    ASSERT(*waiter != NULL);
    thread_unblock(*waiter);        // 唤醒当前线程
    *waiter = NULL;
}

/* 消费者从ioq队列当中获取一个字符 */
char ioq_getchar(struct ioqueue* ioq){
    ASSERT(get_intr_status() == INT_OFF);
    // 缓冲区为空
    while(ioq_empty(ioq)){ 
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->consumer);   // 阻塞消费者
        lock_release(&ioq->lock);
    }
    // 取出字符
    char cur_char = ioq->buf[ioq->tail];
    ioq->tail = next_pos(ioq->tail);

    // 唤醒生产者
    if(ioq->producer != NULL){
        wakeup(&ioq->producer);     // 开始生产
    }

    return cur_char;
}

/* 生产者放入字符到ioq队列 */
void ioq_putchar(struct ioqueue* ioq, char byte){
    ASSERT(get_intr_status() == INT_OFF);
    while(ioq_full(ioq)){
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->producer);
        lock_release(&ioq->lock);
    }
    ioq->buf[ioq->head] = byte;
    ioq->head = next_pos(ioq->head);

    if(ioq->consumer != NULL)
        wakeup(&ioq->consumer);
}



