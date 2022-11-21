#ifndef __FILESYSTEM_H
#define __FILESYSTEM_H
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>

#define DISKSIZE 1024000     
#define MAX_BLOCK_SIZE 1024
#define MIN_BLOCK_SIZE 64
#define MAX_BLOCK_NUM 16000
#define MIN_BLOCK_NUM 1000
#define END 65535
#define FREE 0  

#define MAX_OPEN_FILE 10 
#define MAX_FILE_NAME 80 
#define MAX_INDEX_BLOCK_NUM DISKSIZE/MIN_BLOCK_SIZE // 1600 
#define MAX_TEXT_SIZE  10000
#define MAX_FILENAME 8

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define divup(x, y) ((x - 1) / (y) +1)
#define GREEN        "\033[0;32;32m"
#define RED          "\033[0;32;31m"
#define NONE         "\033[m"

#define W_truncate 1
#define W_cover 2
#define W_append 3

#define Dir 0
#define File 1



extern unsigned short BLOCKNUM;
extern unsigned short BLOCKSIZE;
extern unsigned short FATBLOCKNUM;
extern unsigned short DATABLOCKNUM;


typedef struct FCB{// Byte : 8+4+8+3+2=25 (in fact : 8+8+8+4=28)
    unsigned char filename[8];//文件名
    unsigned int length;//文件长度(字节数)
    unsigned int time;//文件创建时间

    unsigned short date;//文件创建日期
    unsigned short first;// 首盘号
    unsigned char exname[3];//文件扩展名
    unsigned char attribute;//文件属性字段

    // 值为 0 时表示目录文件，值为 1 时表示数据文件
    unsigned char free;//表示目录项是否为空，若值为0，表示空，值为1，表示已分配

}fcb;

// typedef struct INDEX{
//     unsigned long *idx;
// }index;

typedef struct FAT// Byte: 2
{
    unsigned short id;
}fat;


typedef struct BLOCK0 // Byte : 8+8+8+8=32
{
    unsigned char magic_number[8];// 文件系统的魔数
    
    unsigned short blocksize;
    unsigned short blocknum;
    unsigned short fatblocknum;
    unsigned short datablocknum;
    unsigned short maxopenfile;
   	// 磁盘块大小、磁盘块数量、最多打开文件数
    unsigned short root; //根目录文件的起始盘块号
    unsigned char *startblock; //虚拟磁盘上数据区开始位置
}bootblock;

typedef struct USEROPEN{// 80+8+24+2=116 (in fact : 114)
    //相应打开文件所在的目录名，这样方便快速检查号出指定文件是否已经打开
    unsigned char dir[MAX_FILE_NAME]; 
    //读写指针在文件中的位置
    unsigned int count; 
    //相应打开文件的目录项在父目录文件中的盘块
    unsigned short dirno; 
    // 相应打开文件的目录项在父目录文件的dirno盘块中的目录项序号
    unsigned short diroff;
    // INODE
    fcb open_fcb;  // FCB
    //是否修改了文件的 FCB 的内容，如果修改了置为 1，否则为 0
    unsigned char fcbstate; 
    //表示该用户打开表项是否为空，
    //若值为 0，表示为空，否则表示已被某打开文件占据
    unsigned char topenfile;     
}useropen;

typedef struct USERTABLE{
    useropen openfilelist[MAX_OPEN_FILE]; // 用户打开文件表数组
    int currentFd; // 当前目录的文件描述符fd
    char USERNMAE[100];// 用户名
}UserTable;

typedef struct DISK{ // Byte : 40
    unsigned char* myvhard; // 虚拟磁盘的起始地址
    bootblock* block0; // 引导块
    fat* FAT1; // FAT1
    fat* FAT2; // FAT2
    unsigned char* dataAddr;
}Disk;

extern Disk disk;//
extern UserTable usertable;// Byte : 1264


// 对全局变量进行初始化
void InitVariable();
// 进入文件系统
void startsys();
// 磁盘格式化
void my_format();
// 更改当前目录
int my_cd(char *dirname);
// 创建子目录
int my_mkdir(char *dirname);
// 删除子目录
int my_rmdir(char *dirname, int flag);
// 显示目录
int my_ls(void);
// 创建文件
int my_create (char *filename);
// 删除文件
int my_rm(char *filename);
// 打开文件
int my_open(char *filename);
// 关闭文件
int my_close(int fd);
// 写文件
int my_write(int fd,int pos);
// 实际写文件
int do_write(int fd,char *text,int len,char wstyle, int pos);
// 读文件
int my_read (int fd, int pos, int len);
// 实际读文件
int do_read (int fd, int len,char *text);
// 退出文件系统
int my_exitsys();
// 打印FAT数据(按16进制格式显示，每行16项)
void my_listFAT();
// 分配一个FAT
unsigned short allocFreeBlock();
// 寻找父节点
int find_father_fd(int fd);
// 分配一个usertable
int allocFreeusertable();
// help 界面
void my_help(); 
// Start
void printStart();
// Red
void printRed(char* s);
// pwd
void my_pwd();
// info
void showFileSystem();
// string to int
int str2int(char* s);
// string compare
int mystrcmp(char *s1,char* s2);
// is a file ?
int checkfile(char* s);
// recursive delete 
void RecurDelete(fcb* fcb_p);


#endif 