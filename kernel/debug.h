#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H
void panic_spin(char* filename, int line, const char* func, const char* condition);

// __VA_ARGS__ 预处理器的标识符，允许可变参数在宏替换列表出现
#define PANIC(...) panic_spin (__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef NDEBUG       // 在gcc的时候进行设置
    #define ASSERT(CONDITION) ((void)0)     // 将宏设置为空，使得断言失效
#else
// 符号`#`将宏的参数转化为字符串字面量
    #define ASSERT(CONDITION)  \
        if(CONDITION){}else{ PANIC(#CONDITION);}

#endif /* __NODEBUG */

#endif  /* __KERNEL_DEBUG_H */