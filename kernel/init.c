#include "init.h"
#include "print.h"
#include "interrupt.h"

void init_all(){
    put_str("Start init\n");
    idt_init();
}