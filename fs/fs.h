#ifndef  __FS_FS_H
#define __FS_FS_H
#include "stdint.h"

#define MAX_FILES_PER_PART 4096         // 分区最大文件数
#define BITS_PER_SECTOR 4096        // 扇区的位数
#define SECTOR_SIZE 512             // 扇区字节大小
#define BLOCK_SIZE SECTOR_SIZE      // 块字节大小
#define MAX_PATH_LEN 512            // 路径最大长度

/* 文件类型 */
enum file_types{
    FT_UNKNOWN,     // 不支持的文件类型
    FT_REGULAR,     // 普通文件
    FT_DIRECTORY        // 目录
};

/* 打开文件的选项 */
enum oflags{
    O_RDONLY,           // 只读
    O_WRONLY,           // 只写
    O_RDWR,             // 读写
    O_CREAT = 4             //创建 
};

/* 文件读写位置偏移量 */
enum whence{
    SEEK_SET = 1,       
    SEEK_CUR,           
    SEEK_END
};

extern struct partition* cur_partition;
/* 记录查找过程当中，已经找到的上级路径 */
struct path_search_record{
    char searched_path[MAX_PATH_LEN];   // 查找过程中的父路径，最大512
    struct dir* parent_dir;             // 文件或目录所在的直接父目录
    enum file_types file_type;      // 文件类型
};
void filesys_init(void);
int32_t path_depth_cnt(char* pathname);
int32_t sys_open(const char* pathname, uint8_t flags);
int32_t sys_close(int32_t fd);
int32_t sys_write(int32_t fd, const void* buf, uint32_t count);
int32_t sys_read(int32_t fd, void* buf, uint32_t count);
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t sys_unlink(const char* pathname);
int32_t sys_mkdir(const char* pathname);
struct dir* sys_opendir(const char* name);
int32_t sys_closedir(struct dir* dir);
struct dir_entry* sys_readdir(struct dir* dir);
void sys_rewinddir(struct dir* dir);
int32_t sys_rmdir(const char* pathname);
#endif