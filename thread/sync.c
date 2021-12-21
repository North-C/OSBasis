#include "sync.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"
#include "global.h"

// 信号量初始化
void sema_init(struct semaphore* sema, uint8_t value){
    sema->value = value;
    list_init(&sema->waiters);
}

// 初始化锁
void lock_init(struct lock* lock){
    lock->holder = NULL;
    sema_init(&lock->sema, 1);
    lock->holder_repeat_nr = 0;
}

// 信号量down操作,获取锁，信号量减一
void sema_down(struct semaphore* sema){
     // 关闭中断保持原子性
    enum intr_status old_status = disable_intr();
    while(sema->value == 0){
        ASSERT( !list_search(&sema->waiters, &running_thread()->general_tag) );
        if(list_search(&sema->waiters, &running_thread()->general_tag) ){
            PANIC("sema_down: there is running thread in waiters");
        }
        list_append(&sema->waiters, &running_thread()->general_tag);    // 加入到等待队列当中 
        thread_block(TASK_BLOCKED);       // 阻塞当前线程
    }
    // 获得锁
    sema->value--;
    ASSERT(sema->value==0);     // 确保获得到了
    // 开中断
    set_intr_status(old_status);
}

// 信号量up操作,失去锁，信号量加一
void sema_up(struct semaphore* sema){
    enum intr_status old_status = disable_intr();
    ASSERT(sema->value == 0);
    if(!list_empty(&sema->waiters)){    // 希望获取锁的线程
    // 获取线程，进行唤醒
        struct task_struct* pthread = elem2entry( list_pop(&sema->waiters), struct task_struct, general_tag);
        thread_unblock(pthread);
    }
    sema->value++;
    ASSERT(sema->value==1);
    set_intr_status(old_status);
}

// 获取锁
void lock_acquire(struct lock* plock){
    // plock存在
    ASSERT(plock!=NULL);
    // 区分是自己持有 和 不是自己持有锁
    if(plock->holder==running_thread()){  
        plock->holder_repeat_nr ++;
    }else{
        // 对锁进行设置
        sema_down(&plock->sema);      // 获取信号量锁        
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;        // 重复申请次数重置
    }
}

// 释放锁
void lock_release(struct lock* plock){

    ASSERT(plock->holder == running_thread());
    // 参考holder_repeat_nr，来执行释放锁
    if(plock->holder_repeat_nr > 1){
        plock->holder_repeat_nr --;
        return ;
    }
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_up(&plock->sema);      // 需要放到最后一部分，释放后会进行线程的调度？
}

