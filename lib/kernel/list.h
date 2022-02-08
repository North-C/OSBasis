#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H
#include "global.h"

#define offset(struct_type, struct_member)  (int)(&((struct_type*)0)->struct_member)
#define elem2entry(elem_ptr, struct_type, struct_member) \
        (struct_type*)((int)elem_ptr - offset(struct_type, struct_member))


/* 双向链表，管理线程的PCB */
struct list_elem{       // 没有存储，只需要链接PCB即可
    struct list_elem* front;
    struct list_elem* next;
};

/* 自定义函数类型，用于list_traversal */
typedef bool (function)(struct list_elem*, int arg);    // 先定义struct list_elem类型
/* 常用链表类型 */
struct list{
    struct list_elem head;      
    struct list_elem tail;
};


void list_init(struct list* plist);
// 指定位置插入前向和后向 删除 更新 查找 在尾部添加 在头部添加
void list_insert_before(struct list_elem* plist, struct list_elem* elem);
void list_push(struct list* plist, struct list_elem* elem);
// void list_iterate(struct list* plist);
void list_append(struct list* plist, struct list_elem* elem);
void list_remove(struct list_elem* elem);       
struct list_elem* list_pop(struct list* plist);

//void list_update(struct list_elem* elem);
bool list_search(struct list* plist, struct list_elem* target);           // 查找
// 设计这个的作用在哪里？
struct list_elem* list_traversal(struct list* plist, function func, int arg);
void print_list(struct list* plist, struct list_elem* general_tag);
uint32_t list_len(struct list* plist);
bool list_empty(struct list* plist);

#endif