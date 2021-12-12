#include "list.h"
#include "interrupt.h"
#include "stdint.h"
#include "debug.h"
#include "thread.h"
#include "print.h"

/* 初始化list */
void list_init(struct list* plist){
    plist->head.front = NULL;
    plist->head.next = &plist->tail;
    plist->tail.front =&plist->head;
    plist->tail.next = NULL;
}

/* 在before元素之前插入elem */
void list_insert_before(struct list_elem* before, struct list_elem* elem){
    enum intr_status status = disable_intr();

    elem->front = before->front;
    elem->front->next = elem;
    elem->next = before;
    before->front = elem;

    set_intr_status(status);
}

/* head作为栈顶，tail作为栈底，入栈到head的下一位 */
void list_push(struct list* plist, struct list_elem* elem){
    list_insert_before(plist->head.next, elem);
}

/* 添加到列表的尾部 */
void list_append(struct list* plist, struct list_elem* elem){
    enum intr_status status = disable_intr();   

    elem->front = plist->tail.front;
    elem->front->next = elem;
    elem->next = &plist->tail;
    plist->tail.front = elem;

    set_intr_status(status);
}

/* 从列表中删除elem元素 */
void list_remove(struct list_elem* elem){
     enum intr_status status = disable_intr();
     
    elem->front->next = elem->next;
    elem->next->front = elem->front;

    set_intr_status(status);
}
/* head的下一个元素出栈 */
struct list_elem* list_pop(struct list* plist){

    struct list_elem* p = plist->head.next;
    list_remove(p);   
    return p;
}

/* 在plist当中查找target元素,存在则返回true,不存在则返回false */
bool list_search(struct list* plist, struct list_elem* target){
    if(list_empty(plist)){
        return false; 
    }
    struct list_elem* item = plist->head.next;
    while(item != &plist->tail){
        if(item == target){
            return true;
        }
        item = item->next;
    }
    return false;
}

/* 把列表plist中的每个元素elem和arg传给回调函数func */
struct list_elem* list_traversal(struct list* plist, function func, int arg){
    
    if(list_empty(plist)){
        return NULL;
    }
    struct list_elem* elem = plist->head.next;
    while(elem != &plist->tail){        
        if(func(elem, arg) ){           // 通过func 函数来进行检验，成功则返回该elem
            return elem;
        }
        elem = elem->next;
    }
    return NULL;
}

/* 打印链表的内容 */
void print_list(struct list* plist, struct list_elem* thread_tag){
    if(list_empty(plist)){
        put_str("plist is NULL");
      //  ASSERT(! (plist->head.next == &plist->tail));
        return;
    }
    struct list_elem* elem = plist->head.next;
    while(elem != &plist->tail){
    // 通过链表的节点，打印出PCB中的name字段
        struct task_struct* task = elem2entry(thread_tag, struct task_struct, general_tag);
        put_str(task->name); put_char('\n');
        elem = elem->next;
    }
}

/* 计算链表plist的长度 */
uint32_t list_len(struct list* plist){
    uint32_t cnt = 0;
    struct list_elem* ptr = plist->head.next;
    while(ptr != &plist->tail){
        ptr = ptr->next;
        cnt++;
    }
    return cnt;
}

/* 判断链表是否为空，空则返回true, 非空则返回false */
bool list_empty(struct list* plist){
    return plist->head.next == &plist->tail ? true: false;
}









