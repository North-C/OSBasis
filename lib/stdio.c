#include "stdio.h"
#include "stdint.h"
#include "user/syscall.h"
#include "global.h"
#include "string.h"

#define MAX_LENGTH 1024
#define va_start(ap, v)    ap = (va_list)&v     // 指向函数的第一个固定参数
#define va_arg(ap, type)  *((type*)(ap+=4))     // 获取type类型的ap指向的参数
#define va_end(ap)  ap=NULL                     // 将参数指针置位NULL

/* 将整型number(考虑进制base)转换成字符类型,存放到buf_ptr_addr当中 */
static char iota(uint32_t number, char** buf_ptr_addr, uint8_t base){
    uint32_t m = number % base;
    uint32_t i = number / base;

    if(i){
        iota(i, buf_ptr_addr, base);        // 递归，先写入最高位
    }
    // 传入二重指针，可以直接修改缓冲区的指针，避免值传递的无法更改的问题，将结果写入缓冲区并且更新缓冲区指针
    if(m < 10){
        *((*buf_ptr_addr)++) = m + '0';
    }
    else{
        *((*buf_ptr_addr)++) = m - 10 + 'A';
    }

    return '0';
}

/* 将format字符串转换成字符串str,并返回其长度 */
uint32_t vsprintf(char *str, const char *format,  va_list ap){
    char *buf = str;        // 存储更换的字符串
    const char *index = format;   // 遍历格式字符串
    int arg_int = 0;

    while(*index !='\0'){      
        if( *index != '%'){     // 是否为格式符
            *(buf++) = *index;
            index++;
            continue;
        }

        switch(*(++index)){     // 判断 % 之后的 格式说明符
            case 'x':
                arg_int = va_arg(ap, int);
                iota(arg_int, &buf, 16);
                ++index;
                break;
            case 'd':       // 整数类型, 考虑正负数
                arg_int = va_arg(ap, int);
                if(arg_int < 0){
                    arg_int = 0 - arg_int;      // 变为正数
                    *buf++ = '-';
                }
                iota(arg_int, &buf, 10);
                ++index;
                break;
            case 'c':       // 字符类型
                *buf++ = va_arg(ap, char);
                ++index;
                break;
            case 's':       // 字符串类型
                char* arg_str = va_arg(ap, char*);
                strcpy( buf, arg_str);
                buf += strlen(arg_str);
                ++index;
                break;
        }
    }


    return strlen(str);
}

/* 输出格式字符串(const只读) */
uint32_t printf(const char *format, ...){
    char buf[MAX_LENGTH] = {0};     // 注意：最好进行初始化
    va_list ap;
    va_start(ap, format);       // 指向第一个固定参数      
    vsprintf(buf, format, ap);
    va_end(ap); 
    return write(buf);         // 调用系统调用进行输出
}

/* 将字符串写入到buf,返回字符串的长度 */
uint32_t sprintf(char *buf, const char *format, ...){
    va_list ap;
    uint32_t len = 0;
    va_start(ap, format);
    len = vsprintf(buf, format, ap);
    va_end(ap);
    return len; 
}