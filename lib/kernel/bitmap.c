#include "bitmap.h"
#include "io.h"
#include "print.h"
#include "debug.h"
#include "interrupt.h"
#include "string.h"

/* 初始化位图 */
void bitmap_init(struct bitmap* btmp){
    memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

/* 判断位图btmp中第bit_idx位是否为1 */
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx){
    uint32_t byte_idx = bit_idx / 8;        // 索引数组下标
    uint32_t bit_odd = bit_idx % 8;

    return (btmp->bits[byte_idx]) & ( BITMAP_MASK << bit_odd);
}

/*  位图btmp当中找到连续的cnt个可用位，返回可用区域的开始地址，没有则返回-1 */
int bitmap_scan(struct bitmap* btmp, uint32_t cnt){
    uint32_t free_byte_idx = 0;      // 空闲位所在的字节

    // 逐个字节判断是否字节中全部已用
    while( (btmp->bits[free_byte_idx] == 0xff) && (free_byte_idx < btmp->btmp_bytes_len)){
        free_byte_idx++;
    }

    ASSERT( free_byte_idx < btmp->btmp_bytes_len);
    if(free_byte_idx >= btmp->btmp_bytes_len)
        return -1;

    // 判断有几个连续的空闲位
    int free_bit_in_byte = 0;   // 空闲位在字节当中的下标 
    while( btmp->bits[free_byte_idx] & ((uint8_t)(BITMAP_MASK << free_bit_in_byte)))
        free_bit_in_byte++;
    
    // 空闲位在位图内的下标
    int free_bit_start = free_byte_idx * 8 + free_bit_in_byte;
    if(cnt == 1) return free_bit_start;
    
    // 剩下多少个位没检查
    uint32_t bit_left = (btmp->btmp_bytes_len * 8 - free_bit_start);
    uint32_t next_bit = free_bit_start + 1;
    uint32_t count = 1;        // 记录找到的空闲位的个数

    free_bit_start = -1;  
    while(bit_left-- >0){
        // 不为1,则为空闲
        if( !bitmap_scan_test(btmp, next_bit)){
            count++;
        }
        else{
            count = 0;
        }
        // 如果找到足够的空闲位
        if(count==cnt){
             free_bit_start= next_bit - count + 1;
             break;
        }
        next_bit++;
    }
    return free_bit_start;
}

/* 将位图btmp当中的第bit_idx位设置为 value */
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value){
    ASSERT((value == 0) || (value == 1));
    uint32_t byte_idx = bit_idx / 8;        // 索引数组下标
    uint32_t bit_odd = bit_idx % 8;     // 索引字节内的下标

    if(value){
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    }else{
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);      // ~ 取反
    }
}
