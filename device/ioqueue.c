#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"

void ioqueue_init(struct ioqueue* ioq){}

void ioqueue_init(struct ioqueue* ioq);
static int32_t next_pos(int32_t pos);
bool ioq_full(struct ioqueue* ioq);
static bool ioq_empty(struct ioqueue* ioq);
static void ioq_wait(struct task_struct** waiter);
static void wakeup(struct task_struct** waiter);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);



