#include "keyboard.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"
#include "debug.h"
#include "ioqueue.h"

#define KBD_BUF_PORT 0x60       // 键盘buffer寄存器端口号0x60
struct ioqueue kbd_buf;     // 缓冲区


/* 转移字符定义部分控制字符 */
#define esc '\033'
#define backspace '\b'
#define tab '\t'
#define enter '\r'
#define delete '\177'

/* 以上不可见字符一律定义为0 */
#define char_invisible 0
#define ctrl_l_char         char_invisible
#define ctrl_r_char         char_invisible
#define shift_l_char        char_invisible
#define shift_r_char        char_invisible
#define alt_l_char          char_invisible
#define alt_r_char          char_invisible
#define caps_lock_char      char_invisible

/*  控制字符的通码和断码 */
#define shift_l_make    0x2a
#define shift_r_make    0x36
#define alt_l_make      0x38
#define alt_r_make      0xe038
#define alt_r_break     0xe0b8
#define ctrl_l_make     0x1d
#define ctrl_r_make     0xe01d
#define ctrl_r_break    0xe09d
#define caps_lock_make  0x3a

/* 记录按键的状态 */
// ext_scancode记录makecode是否以0xe0开头
static bool ctrl_status, shift_status, alt_status, caps_lock_status, ext_scancode;  

/* 二维数组,通码做索引,定义和shift键位组合时的表现 */
static char keymap[][2] = {
            {0, 0},
            {esc, esc},
            {'1', '!'},
            {'2', '@'},
            {'3', '#'},
            {'4', '$'},
            {'5', '%'},
            {'6', '^'},
            {'7', '&'},
            {'8', '*'},
            {'9', '('},
            {'0', ')'},
            {'-', '_'},
            {'=', '+'},
            {backspace, backspace},
            {tab, tab},
            {'q', 'Q'},
            {'w', 'W'},
            {'e', 'E'},
            {'r', 'R'},
            {'t', 'T'},
            {'y', 'Y'},
            {'u', 'U'},
            {'i', 'I'},
            {'o',	'O'},		
/* 0x19 */	{'p',	'P'},		
/* 0x1A */	{'[',	'{'},		
/* 0x1B */	{']',	'}'},		
/* 0x1C */	{enter,  enter},
/* 0x1D */	{ctrl_l_char, ctrl_l_char},
/* 0x1E */	{'a',	'A'},		
/* 0x1F */	{'s',	'S'},		
/* 0x20 */	{'d',	'D'},		
/* 0x21 */	{'f',	'F'},		
/* 0x22 */	{'g',	'G'},		
/* 0x23 */	{'h',	'H'},		
/* 0x24 */	{'j',	'J'},		
/* 0x25 */	{'k',	'K'},		
/* 0x26 */	{'l',	'L'},		
/* 0x27 */	{';',	':'},		
/* 0x28 */	{'\'',	'"'},		
/* 0x29 */	{'`',	'~'},		
/* 0x2A */	{shift_l_char, shift_l_char},	
/* 0x2B */	{'\\',	'|'},		
/* 0x2C */	{'z',	'Z'},		
/* 0x2D */	{'x',	'X'},		
/* 0x2E */	{'c',	'C'},		
/* 0x2F */	{'v',	'V'},		
/* 0x30 */	{'b',	'B'},		
/* 0x31 */	{'n',	'N'},		
/* 0x32 */	{'m',	'M'},		
/* 0x33 */	{',',	'<'},		
/* 0x34 */	{'.',	'>'},		
/* 0x35 */	{'/',	'?'},
/* 0x36	*/	{shift_r_char, shift_r_char},	
/* 0x37 */	{'*',	'*'},    	
/* 0x38 */	{alt_l_char, alt_l_char},
/* 0x39 */	{' ',	' '},		
/* 0x3A */	{caps_lock_char, caps_lock_char}
/*其它按键暂不处理*/

};

/* 键盘中断处理程序 */
static void intr_keyboard_handler(void){
    /* 判断上次中断是否被按下 */
    bool ctrl_down_last = ctrl_status;
    bool shift_down_last = shift_status;
    bool caps_lock_last = caps_lock_status;

    bool break_code ;
    uint16_t scancode = inb(KBD_BUF_PORT);  // 读取扫描码
    
    // 是否为前缀
    if(scancode==0xe000){  
        ext_scancode = true;
        return;
    }

    // 如果前一个扫描码是前缀,则要添加到当前scancode
    if(ext_scancode == true){   
        scancode = ((0xe000) | scancode);
        ext_scancode = false;
    }
    // scancode是否为断码
    break_code = ((scancode & 0x0080) != 0);
    if(break_code){     // 是断码
        // 断码转为通码,便于索引
        uint16_t make_code = (scancode & 0xff7f);       // 减掉0x80

        // 控制键ctrl shift alt的判断,设置对应键的状态
        if(make_code == ctrl_l_make || make_code == ctrl_r_make){
            ctrl_status = false;
        }
        if(make_code == shift_l_make || make_code == shift_r_make){
            shift_status = false;
        }
        if(make_code == alt_l_make || make_code == alt_r_make){
            alt_status = false;
        }
        return;
    }
    // scancode是通码,只处理之前定义好的键和alt和ctrl的右键
    else if((scancode > 0x00 && scancode < 0x3b) || (scancode == alt_r_make) 
                ||(scancode == ctrl_r_make)){
        // 主要对shift和caps_lock的作用进行区分
        bool shift = false;
        // 有两个字符的非字母键
        if((scancode < 0x0e)||(scancode == 0x1a) ||
            (scancode == 0x1b) ||(scancode == 0x2b )|| 
            (scancode <0x2a && scancode > 0x26) || 
            (scancode <0x36 && scancode > 0x32)){
            if(shift_down_last){
                shift = true;
            }
        }
        else{       // 字母键
            if(shift_down_last && caps_lock_last){
                shift = false;
            }// 按下任意一个
            else if(shift_down_last || caps_lock_last){
                shift = true;
            }
            else{       // 一个都没按下
                shift = false;
            }
        }
        // char类型作为索引,大小一个字节
        uint8_t index = (scancode &= 0x00ff);
        char cur_char = keymap[index][shift];       // 决定选择哪一个输出
        // ASCII码不为0
        if(cur_char){   // 加入缓冲区

            if(!ioq_full(&kbd_buf)){
               // put_char(cur_char); // 临时输出一下
                ioq_putchar(&kbd_buf, cur_char);
            }
            // put_char(cur_char);
            return;
        }

        // 控制键判断,为下次做判断
        if( scancode == ctrl_l_make || scancode == ctrl_r_make){
            ctrl_status = true;
        }
        else if(scancode == shift_l_make || scancode == shift_r_make){
            shift_status = true;
        }
        else if(scancode == alt_l_make || scancode == alt_r_make){
            alt_status = true;
        }
        else if(scancode ==caps_lock_make){
            caps_lock_status = !caps_lock_status;
        }
    }
    else{   
        put_str("unknown key\n");
    }
}

/* 键盘初始化 */
void keyboard_init(){
    put_str("keyboard init start\n");
    ioqueue_init(&kbd_buf);     // 初始化缓冲区
    register_handler(0x21, intr_keyboard_handler);  // 注册键盘中断处理函数
    put_str("keyboard init done\n");
}

