#include "FileSystem.h"


unsigned short BLOCKSIZE;// default : 1024
unsigned short BLOCKNUM; // default : SIZE/1024
unsigned short FATBLOCKNUM;// default : 2*2
unsigned short DATABLOCKNUM;// default : 1000-1-4=95

char USERNMAE[]="20051124-hdbone"; 
char myfsys_name[]="myfsys"; 

Disk disk;
UserTable usertable;


void InitVariable()
{
    BLOCKSIZE=1024;
    BLOCKNUM=DISKSIZE/BLOCKSIZE;
    FATBLOCKNUM=divup(BLOCKNUM*sizeof(unsigned short),BLOCKSIZE)*2;
    DATABLOCKNUM=BLOCKNUM-1-FATBLOCKNUM;
}

// 由 main()函数调用,进入并初始化我们所建立的文件系统,以供用户使用。
void startsys()
{
    // 申请虚拟磁盘空间
    disk.myvhard=(unsigned char*)malloc(DISKSIZE);
    memset(disk.myvhard,0,DISKSIZE);// !!!
    // fopen()打开 myfsys 文件
    FILE* fp;
    fp=fopen(myfsys_name,"rb");
    
    if(fp!=NULL)// 若文件存在
    {
        unsigned char* buf=(unsigned char*)malloc(DISKSIZE);
        int fd=open(myfsys_name,O_RDONLY);
        read(fd, buf, DISKSIZE);
        unsigned char magic[8];
        memcpy(magic,buf,8);
        if(!memcmp(magic,"10101010",8))
        // 判断其开始的 8 个字节内容是否为“10101010”(文件系统魔数)
        {
                       
            memcpy(disk.myvhard,buf,DISKSIZE);
            
            disk.block0=(bootblock*)(disk.myvhard);
            BLOCKNUM=disk.block0->blocknum;
            BLOCKSIZE=disk.block0->blocksize;
            FATBLOCKNUM=disk.block0->fatblocknum;
            DATABLOCKNUM=disk.block0->datablocknum;
            disk.FAT1=(fat*)(disk.myvhard+BLOCKSIZE);
            disk.FAT2=(fat*)(disk.myvhard+BLOCKSIZE*(FATBLOCKNUM/2+1));
            disk.dataAddr=(unsigned char*)(disk.myvhard+BLOCKSIZE*(FATBLOCKNUM+1));
            // 复制到内存中的虚拟磁盘空间中
            printf("读取myfys文件系统成功\n\n");       
            fcb* root=(fcb*)(disk.myvhard+BLOCKSIZE*(FATBLOCKNUM+1));
        }
        printf("文件系统空间大小为: %d B\n",DISKSIZE);
        printf("磁盘块大小为: %d B\n",BLOCKSIZE);
        printf("磁盘块数量为: %d 块\n",BLOCKNUM);
        printf("是否需要更改磁盘块大小?(y/n): \n");
        printRed("更改磁盘块大小会格式化磁盘空间!");
        char ch[20];
        scanf("%s",ch);
        while(strcmp(ch,"y")&&strcmp(ch,"n"))
        {
            printf("error, y or n\n");
            scanf("%s",ch);
        }
        if(!strcmp(ch,"y"))
        {
            my_format();
            fwrite(disk.myvhard,DISKSIZE,1,fp);
        }else{
            
        }

    }else{// 若文件不存在,则创建之
        printf("myfsys 文件系统不存在\n");
        my_format();
        fp=fopen(myfsys_name,"wb");
        fwrite(disk.myvhard,DISKSIZE,1,fp);
    }
    fclose(fp);

    // usertable init
    usertable.currentFd=0;
    memcpy(usertable.USERNMAE,USERNMAE,strlen(USERNMAE));
    fcb* root=(fcb*)(disk.myvhard+(1+FATBLOCKNUM)*BLOCKSIZE);
    memcpy(&usertable.openfilelist[0].open_fcb,root,sizeof(fcb));

    usertable.openfilelist[0].count=0;
    strcpy(usertable.openfilelist[0].dir,"/");
    usertable.openfilelist[0].dirno=1+FATBLOCKNUM;
    usertable.openfilelist[0].diroff=0;
    usertable.openfilelist[0].fcbstate=0;
    usertable.openfilelist[0].topenfile=1;

}

// 对虚拟磁盘进行格式化,布局虚拟磁盘,建立根目录文件(或根目录区)
void my_format()
{
    printf("输入磁盘块大小(B):%d ~ %d\n",MIN_BLOCK_SIZE,MAX_BLOCK_SIZE);
    while(1)
    {
        scanf("%hd",&BLOCKSIZE);
        if(BLOCKSIZE>=MIN_BLOCK_SIZE&&BLOCKSIZE<=MAX_BLOCK_SIZE)
        {
            break;
        }else{
            printf("error, try again\n");
        }
    }


    BLOCKNUM=DISKSIZE/BLOCKSIZE;
    FATBLOCKNUM=divup(BLOCKNUM*sizeof(unsigned short),BLOCKSIZE)*2;
    DATABLOCKNUM=BLOCKNUM-1-FATBLOCKNUM;
    

    // 将虚拟磁盘第一个块作为引导块
    disk.block0=(bootblock*)disk.myvhard;
    // 开始的 8 个字节是文件系统的魔数,记为“10101010”
    // strcpy(disk.block0->magic_number, "10101010");
    // disk.block0->magic_number[8]='\0';
    memcpy(disk.block0->magic_number, "10101010",8);

    disk.block0->root=1+FATBLOCKNUM+1;
    disk.block0->blocknum=BLOCKNUM;
    disk.block0->blocksize=BLOCKSIZE;
    disk.block0->datablocknum=DATABLOCKNUM;
    disk.block0->fatblocknum=FATBLOCKNUM;
    disk.block0->startblock=disk.myvhard + BLOCKSIZE * (1+FATBLOCKNUM);
    disk.block0->maxopenfile=MAX_OPEN_FILE;

    // 在引导块后建立两张完全一样的 FAT 表,
    // 用于记录文件所占据的磁盘块及管理虚拟磁盘块的分配,每个 FAT占据两个磁盘块;
    // 对于每个 FAT 中,前面 1+FATBLOCKNUM 个块设置为已分配
    // 后面 DATABLOCKNUM 个块设置为空闲;
    disk.FAT1=(fat*)(disk.myvhard+BLOCKSIZE*1);
    disk.FAT2=(fat*)(disk.myvhard+BLOCKSIZE*(FATBLOCKNUM/2+1));
    disk.dataAddr=(unsigned char*)(disk.myvhard+BLOCKSIZE*(FATBLOCKNUM+1));
    
    for(int i=0;i<1+FATBLOCKNUM;i++)
        disk.FAT1[i].id=END;
    for(int i=1+FATBLOCKNUM;i<BLOCKNUM;i++)
        disk.FAT1[i].id=FREE;
    memcpy(disk.FAT2,disk.FAT1,FATBLOCKNUM/2*BLOCKSIZE);

    // 在第二张 FAT 后创建根目录文件 root,将数据区的第 1 块
    // 分配给根目录文件,在该磁盘上创建两个特殊的目录项:“.”和“..”,其内容除了文件
    // 名不同之外,其他字段完全相同
    
    disk.FAT1[FATBLOCKNUM+1].id=disk.FAT2[FATBLOCKNUM+1].id=END;
    

    fcb* root1=(fcb*)(disk.myvhard+(1+FATBLOCKNUM)*BLOCKSIZE);
    strcpy(root1->filename,".");
    memcpy(root1->exname,"DIR",3);
    root1->attribute=0;
    root1->first=1+FATBLOCKNUM;
    root1->free=1;
    root1->length=2*sizeof(fcb);

    // time
	time_t rawTime = time(NULL);
	struct tm *time = localtime(&rawTime);
    // date : 2000-01-01 ~ 2127-12-31
    // time : 00-00-00 ~ 23-59-59
    root1->date=((time->tm_year-100)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
    root1->time=((time->tm_hour)<<12)+((time->tm_min)<<6)+(time->tm_sec);
    fcb* root2=root1+1;
    memcpy(root2,root1,sizeof(fcb));
    strcpy(root2->filename,"..");

}

// 改变当前目录到指定的名为 dirname 的目录。
int my_cd(char *dirname)
{
    /*
        dirname：新的当前目录的目录名
    */
   /*
        ① 调用 my_open()打开指定目录名的父目录文件，并调用 do_read()读入该父目录文件内容到内存中；
        ② 在父目录文件中检查新的当前目录名是否存在，如果存在则转③，否则返回，并显示出错信息；
        ③ 调用 my_close()关闭①中打开的父目录文件；
        ④ 调用 my_close()关闭原当前目录文件；
        ⑤ 如果新的当前目录文件没有打开，则打开该目录文件
        ⑥ 设置当前目录为该目录。
   */
    int currentfd=usertable.currentFd;

    char *buf=(char*)malloc(10000);
    usertable.openfilelist[currentfd].count=0;
    int r_len_fa=do_read(currentfd,usertable.openfilelist[currentfd].open_fcb.length,buf);
    if(r_len_fa<0)
    {
        printf("read error\n");
        return -1;
    }    
    int off=-1;
    fcb* fcb_p=(fcb*)buf;
    for(int i=0;i<usertable.openfilelist[currentfd].open_fcb.length/sizeof(fcb);i++,fcb_p++)
    {
        if(!strcmp(fcb_p->filename,dirname)&&fcb_p->attribute==Dir)
        {
            off=i;
            break;
        }
    }   
    if(off<0)
    {
        printRed("cd : this directory doesn't exist\n");
        return -1;
    }
    if(strcmp(fcb_p->exname,"DIR"))
    {
        printRed("just allow to cd to a directory\n");
        return -1;
    }

    if(!strcmp(fcb_p->filename,"."))// cd itself
    {
        return currentfd;
    }
    else if(!strcmp(fcb_p->filename,".."))
    {
        if(currentfd==0)// root
        {
            return currentfd;
        }else{
            usertable.currentFd=my_close(currentfd);// return to its father directory
            return usertable.currentFd;
        }
    }else{
        int fd_new=allocFreeusertable();
        if(fd_new<0)
        {
            printf("the useropentable is full\n");
            return -1;
        }
        memcpy(&usertable.openfilelist[fd_new].open_fcb,fcb_p,sizeof(fcb));
        usertable.openfilelist[fd_new].fcbstate=0;
        usertable.openfilelist[fd_new].topenfile=1;
        usertable.openfilelist[fd_new].count=0;

        // dirname -> dirname_abs
        char dirname_abs[MAX_FILE_NAME];
        memset(dirname_abs,0,sizeof(dirname_abs));
        strcpy(dirname_abs,usertable.openfilelist[currentfd].dir);
        if(currentfd!=0)
            dirname_abs[strlen(dirname_abs)]='/';
        dirname_abs[strlen(dirname_abs)]='\0';
        strcat(dirname_abs,dirname);
        dirname_abs[strlen(dirname_abs)]='\0';
        // copy dirname_abs
        strcpy(usertable.openfilelist[fd_new].dir,dirname_abs);
        usertable.openfilelist[fd_new].dirno=usertable.openfilelist[currentfd].open_fcb.first;
        usertable.openfilelist[fd_new].diroff=off;
        // TODO : dirno
        usertable.currentFd=fd_new;
        return fd_new;
    }
}

// 在当前目录下创建名为 dirname 的子目录。
int my_mkdir(char *dirname)
{
    /*
        dirname：新建目录的目录名   
    */
   /*
        ① 调用 do_read()读入当前目录文件内容到内存，检查当前目录下新建目录文件是否重名，若重名则返回，并显示错误信息；
        ② 为新建子目录文件分配一个空闲打开文件表项，如果没有空闲表项则返回-1，并显示错误信息；
        ③ 检查 FAT 是否有空闲的盘块，如有则为新建目录文件分配一个盘块，否则释放①中分配的打开文件表项，返回，并显示错误信息；
        ④ 在当前目录中为新建目录文件寻找一个空闲的目录项或为其追加一个新的目录项;
        需修改当前目录文件的长度信息，并将当前目录文件的用户打开文件表项中的 fcbstate 置为 1；
        ⑤ 准备好新建目录文件的 FCB 的内容，文件的属性为目录文件，以覆盖写方式调用do_write()将其填写到对应的空目录项中；
        ⑥ 在新建目录文件所分配到的磁盘块中建立两个特殊的目录项“.”和“..”目录项，方法是：首先在用户空间中准备好内容，然后以截断写或者覆盖写方式调用 do_write()将
        其写到③中分配到的磁盘块中；
        ⑦ 返回。
   */
    int currentfd=usertable.currentFd;

    char* fname = strtok(dirname, ".");
	char* exname = strtok(NULL, ".");

	if (exname) {
        printRed("just can make a directory\n");
		return -1;
	}

    char buf[10000];
    usertable.openfilelist[currentfd].count=0;
    int r_len_fa=do_read(currentfd,usertable.openfilelist[currentfd].open_fcb.length,buf);
    if(r_len_fa<0)
    {
        printRed("read error\n");
        return -1;
    }

    fcb* fcb_p=(fcb*)buf;
    for(int i=0;i<r_len_fa/sizeof(fcb);i++)
    {
        if(!strcmp(dirname,fcb_p[i].filename)&&fcb_p[i].attribute==Dir)
        {
            printf("dir %s has existed\n",dirname);
            return -1;
        }
    }

    // existed ?
    int fd_new=allocFreeusertable();
    if(fd_new<0)
    {
        printRed("error, useropentable is full\n");
        return -1;
    }

    // fat
    unsigned short fat_new=allocFreeBlock();
    if(fat_new==END)
    {
        usertable.openfilelist[fd_new].topenfile=0;
        printRed("error, disk is full\n");
        return -1;
    }

    int off=0;
    for(int i=0;i<r_len_fa/sizeof(fcb);i++)
    {
        off=i+1;
        if(fcb_p[i].free==0)// !!! 内部可能有碎片
        {
            off=i;
            break;
        }
    }

    int count=off*sizeof(fcb);
    usertable.openfilelist[currentfd].count=count;
    usertable.openfilelist[currentfd].fcbstate=1;
    

    fcb* fcb_new=(fcb*)malloc(sizeof(fcb));
    fcb_new->attribute=Dir;
    time_t rawtime = time(NULL);
    struct tm* time=localtime(&rawtime);
    // TODO : time and date
    fcb_new->date=((time->tm_year-100)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
    fcb_new->time=((time->tm_hour)<<12)+((time->tm_min)<<6)+(time->tm_sec);
    strcpy(fcb_new->filename,dirname);
    memcpy(fcb_new->exname,"DIR",3);
    fcb_new->first=fat_new;
    fcb_new->length=sizeof(fcb)*2;// ".." and "."
    fcb_new->free=1;
    int w_len=do_write(currentfd,(char *)fcb_new,sizeof(fcb),W_cover,count);

    if(w_len<0)
    {
        printf("write error\n");
        return -1;
    }

    // usertable for fd_new
    memcpy(&usertable.openfilelist[fd_new].open_fcb,fcb_new,sizeof(fcb));
    usertable.openfilelist[fd_new].count=0;
    // dirname -> dirname_abs
    char dirname_abs[MAX_FILE_NAME];
    strcpy(dirname_abs,usertable.openfilelist[currentfd].dir);
    if(currentfd!=0)
        dirname_abs[strlen(dirname_abs)]='/';
    strcat(dirname_abs,dirname);
    dirname_abs[strlen(dirname_abs)]='\0';
    // copy dirname_abs
    strcpy(usertable.openfilelist[fd_new].dir,dirname_abs);
    
    usertable.openfilelist[fd_new].dirno=usertable.openfilelist[currentfd].open_fcb.first;
    usertable.openfilelist[fd_new].diroff=off;
    usertable.openfilelist[fd_new].fcbstate=0;
    usertable.openfilelist[fd_new].topenfile=1;

    // "."
    fcb* fcb_1=(fcb*)malloc(sizeof(fcb));
    fcb_1->attribute=Dir;
    fcb_1->date=((time->tm_year-100)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
    fcb_1->time=((time->tm_hour)<<12)+((time->tm_min)<<6)+(time->tm_sec);
    // TODO : date and time
    strcpy(fcb_1->filename,".");
    memcpy(fcb_1->exname,"DIR",3);
    fcb_1->first=fat_new;
    fcb_1->free=1;
    fcb_1->length=sizeof(fcb)*2;


    int w_len_fcb1=do_write(fd_new,(char*)fcb_1,sizeof(fcb),W_cover,0);
    if(w_len_fcb1<0)
    {
        printf("write error\n");
        return -1;
    }

    // ".."
    fcb* fcb_2=(fcb*)malloc(sizeof(fcb));
    memcpy(fcb_2,fcb_1,sizeof(fcb));
    strcpy(fcb_2->filename,"..");
    memcpy(fcb_2->exname,"DIR",3);
    fcb_2->first=usertable.openfilelist[currentfd].open_fcb.first;
    fcb_2->length=usertable.openfilelist[currentfd].open_fcb.length;
    fcb_2->time=usertable.openfilelist[currentfd].open_fcb.time;
    fcb_2->date=usertable.openfilelist[currentfd].open_fcb.date;

    int w_len_fcb2=do_write(fd_new,(char*)fcb_2,sizeof(fcb),W_cover,sizeof(fcb));

    
    // close fd_new
    my_close(fd_new);

    // currentfd fcb update
    fcb_p=(fcb*)buf;
    
    fcb_p->length=usertable.openfilelist[currentfd].open_fcb.length;
    if(currentfd==0){
        fcb* fcb_p_2=fcb_p+1;
        fcb_p_2->length=usertable.openfilelist[currentfd].open_fcb.length;    
        usertable.openfilelist[currentfd].count=sizeof(fcb);
        int w_len_cur=do_write(currentfd,(char*)fcb_p_2,sizeof(fcb),W_cover,sizeof(fcb));
    }
    
    usertable.openfilelist[currentfd].count=0;
    int w_len_cur=do_write(currentfd,(char*)fcb_p,sizeof(fcb),W_cover,0);
    usertable.openfilelist[currentfd].fcbstate=1;
        
    free(fcb_1);
    free(fcb_2);

    return fat_new;
}

// 在当前目录下删除名为 dirname 的子目录
int my_rmdir(char *dirname, int flag)
{
    /*
        ① 调用 do_read()读入当前目录文件内容到内存，检查当前目录下欲删除目录文件是
        否存在，若不存在则返回，并显示错误信息；
        ② 检查欲删除目录文件是否为空（除了“.”和“..”外没有其他子目录和文件），可
        根据其目录项中记录的文件长度来判断，若不为空则返回，并显示错误信息；
        ③ 检查该目录文件是否已经打开，若已打开则调用 my_close()关闭掉；
        ④ 回收该目录文件所占据的磁盘块，修改 FAT；
        ⑤ 从当前目录文件中清空该目录文件的目录项，且 free 字段置为 0：以覆盖写方式调
        用 do_write()来实现；
        ⑥ 修改当前目录文件的用户打开表项中的长度信息，并将表项中的 fcbstate 置为 1；
        ⑦ 返回。
    */ 
    if(!strcmp(dirname,".")||!strcmp(dirname,".."))
    {
        printRed("can not rmdir .. or .\n");
        return -1;
    }
    char* fname = strtok(dirname,".");
    if(strcmp(fname,dirname))
    {
        printRed("rmdir a file is not invalid\n");
        return -1;
    }

    int currentfd=usertable.currentFd;
    char buf[10000];
    usertable.openfilelist[currentfd].count=0;
    int r_len_cur=do_read(currentfd,usertable.openfilelist[currentfd].open_fcb.length,buf);
    if(r_len_cur<0)
    {
        printRed("1 read error\n");
        return -1;
    }
    int off=-1;

    fcb* fcb_p =(fcb*)buf;
    for(int i=0;i<usertable.openfilelist[currentfd].open_fcb.length/sizeof(fcb);i++,fcb_p++)
    {
        if(!strcmp(fcb_p->filename,dirname)&&fcb_p->attribute==Dir)
        {
            off=i;
            break;
        }
    }
    if(off<0)
    {
        printRed("this dir does not exist\n");
        return -1;
    }


    if(fcb_p->length>sizeof(fcb)*2&&flag==0)
    {
        printRed("error, this directory contains other files\n");
        return -1;
    }

    int first;
    int next;
    int fat_iter;
    // free FAT1  (directory  . and ..)
  
    RecurDelete(fcb_p);
    int len_current=usertable.openfilelist[currentfd].open_fcb.length;
    /*begin 消除碎片*/
    char buffer[10000];
    usertable.openfilelist[currentfd].count=(off+1)*sizeof(fcb);
    int len_fragment=len_current-(off+1)*sizeof(fcb);
    r_len_cur=do_read(currentfd,len_fragment,buffer);
    if(r_len_cur<0)
    {
        printRed("read error\n");
        return -1;
    }
    usertable.openfilelist[currentfd].count=off*sizeof(fcb);
    int w_len_cur=do_write(currentfd,(char*)buffer,len_fragment,W_cover,off*sizeof(fcb));

    /*end 消除碎片*/

    // free fcb
    fcb_p->attribute=0;
    fcb_p->date=0;
    fcb_p->time=0;
    fcb_p->exname[0]='\0';
    fcb_p->filename[0]='\0';
    fcb_p->first=0;
    fcb_p->free=0;
    fcb_p->length=0;
    // fcb->block
    usertable.openfilelist[currentfd].count=len_current-sizeof(fcb);
    w_len_cur=do_write(currentfd,(char*)fcb_p,sizeof(fcb),W_cover,len_current-sizeof(fcb));
    // length - = sizeof(fcb)
    usertable.openfilelist[currentfd].open_fcb.length-=sizeof(fcb);

    usertable.openfilelist[currentfd].count=0;
    r_len_cur=do_read(currentfd,usertable.openfilelist[currentfd].open_fcb.length,buffer);
    if(r_len_cur<0)
    {
        printRed("read error\n");
        return -1;
    }

    // FAT move reconstruction
    fcb* fcb_iter=(fcb*)buffer;
    next = disk.FAT1[fcb_iter[0].first].id;
    int cnt_iter=2*sizeof(fcb);
    if(r_len_cur/sizeof(fcb)==2){
        if(next!=END)
        {
            disk.FAT1[fcb_iter[0].first].id=END;
            disk.FAT1[next].id=FREE;
        }          
    }
    fat_iter=next;

    for(int i=2;i<r_len_cur/sizeof(fcb);i++)
    {
        cnt_iter+=sizeof(fcb);
        if(cnt_iter>BLOCKSIZE)
        {
            next=disk.FAT1[fat_iter].id;
            cnt_iter-=BLOCKSIZE;
            fat_iter=next;
            
            int check=disk.FAT1[fat_iter].id;
            if(check==END)
            {
                disk.FAT1[check].id=FREE;
                disk.FAT1[fat_iter].id=END;
            }
        }
    }


    // free FAT2
    memcpy(disk.FAT2,disk.FAT1,BLOCKSIZE*FATBLOCKNUM/2);

    // current -> block (update length)
    fcb_p=(fcb*)buffer;
    fcb_p->length=usertable.openfilelist[currentfd].open_fcb.length;
    if(currentfd==0){
        fcb* fcb_p_2=fcb_p+1;
        fcb_p_2->length=usertable.openfilelist[currentfd].open_fcb.length;    
        usertable.openfilelist[currentfd].count=sizeof(fcb);
        int w_len_cur=do_write(currentfd,(char*)fcb_p_2,sizeof(fcb),W_cover,sizeof(fcb));
    }

    usertable.openfilelist[currentfd].count=0;
    w_len_cur=do_write(currentfd,(char*)fcb_p,sizeof(fcb),W_cover,0);
    usertable.openfilelist[currentfd].fcbstate=1;

    return 1;
}

// 显示当前目录的内容（子目录和文件信息）
int my_ls(void)
{
    /*
        ① 调用 do_read()读出当前目录文件内容到内存；
        ② 将读出的目录文件的信息按照一定的格式显示到屏幕上；
        ③ 返回。
    */
    int currentfd=usertable.currentFd;

    char buf[10000];
    usertable.openfilelist[currentfd].count=0;
    int r_len_cur=do_read(currentfd,usertable.openfilelist[currentfd].open_fcb.length,buf);
    if(r_len_cur<0)
    {
        printRed("read error\n");
        return -1;
    }
    fcb* fcb_p=(fcb*)buf;
    for(int i=0;i<usertable.openfilelist[currentfd].open_fcb.length/sizeof(fcb);i++)
    {
        // printf("%d %s %d\n",i,fcb_p[i].filename,fcb_p[i].free);
        if(fcb_p[i].free==1)
        {
            unsigned short year=(fcb_p[i].date>>9)+2000;
            unsigned short month=(fcb_p[i].date>>5)&0x000f;
            unsigned short day=(fcb_p[i].date)&0x001f;
            unsigned short hour =(fcb_p[i].time>>12);
            unsigned short minute =(fcb_p[i].time>>6)&0x003f;
            unsigned short sencond =(fcb_p[i].time)&0x003f;
            // root1->date=(time->tm_year-100)<<9+(time->tm_mon+1)<<5+(time->tm_mday);
            // root1->time=(time->tm_hour)<<12+(time->tm_min)<<6+(time->tm_sec);
            char exname_tmp[4];
            if(fcb_p[i].attribute==Dir)
            {
                printf("%-8s\t<DIR>\t%ld\t%02d-%02d-%02d %02d.%02d.%02d\n",fcb_p[i].filename,fcb_p[i].length/sizeof(fcb)-2,
                        year,month,day,hour,minute,sencond);
            }else if(fcb_p[i].attribute==File)
            {
                memcpy(exname_tmp,fcb_p[i].exname,3);
                exname_tmp[3]='\0';

                // check open ?
                // abs path
                char filename_abs[MAX_FILE_NAME];
                memset(filename_abs,0,sizeof(filename_abs));
                strcpy(filename_abs,usertable.openfilelist[currentfd].dir);
                if(currentfd!=0)
                    filename_abs[strlen(filename_abs)]='/';
                strcat(filename_abs,fcb_p[i].filename);
                filename_abs[strlen(filename_abs)]='.';
                strcat(filename_abs,exname_tmp);
                filename_abs[strlen(filename_abs)]='\0';
                // dirname -> fd
                // find fd! 
                int fd=-1;
                for(int i=0;i<MAX_OPEN_FILE;i++)
                if(usertable.openfilelist[i].topenfile==1)
                {
                    if(!strcmp(usertable.openfilelist[i].dir,filename_abs)){
                        fd=i;
                    }
                }
                if(fd!=-1)
                {
                    if(usertable.openfilelist[fd].fcbstate)
                        printf(GREEN"%-8s\t<%3s>\t%dB\t%02d-%02d-%02d %02d.%02d.%02d open(need close to save)"NONE,fcb_p[i].filename,exname_tmp,fcb_p[i].length,
                        year,month,day,hour,minute,sencond);
                    else
                        printf(GREEN"%-8s\t<%3s>\t%dB\t%02d-%02d-%02d %02d.%02d.%02d open"NONE,fcb_p[i].filename,exname_tmp,fcb_p[i].length,
                        year,month,day,hour,minute,sencond);
                    
                    printf("\n");
                }
                else
                    printf("%-8s\t<%3s>\t%dB\t%02d-%02d-%02d %02d.%02d.%02d\n",fcb_p[i].filename,exname_tmp,fcb_p[i].length,
                        year,month,day,hour,minute,sencond);
            }
        }
    }
    return 1;
}

// 创建名为 filename 的新文件
int my_create (char *filename)
{
    /*
        ① 为新文件分配一个空闲打开文件表项，如果没有空闲表项则返回-1，并显示错误信息；
        ② 若新文件的父目录文件还没有打开，则调用 my_open()打开；若打开失败，则释放①中为新建文件分配的空闲文件打开表项，返回-1，并显示错误信息；
        ③ 调用 do_read()读出该父目录文件内容到内存，检查该目录下新文件是否重名，my_create若
        重名则释放①中分配的打开文件表项，并调用 my_close()关闭②中打开的目录文件；然后
        返回-1，并显示错误信息；
        ④ 检查 FAT 是否有空闲的盘块，如有则为新文件分配一个盘块，否则释放①中分配的
        打开文件表项，并调用 my_close()关闭②中打开的目录文件；返回-1，并显示错误信息；
        ⑤ 在父目录中为新文件寻找一个空闲的目录项或为其追加一个新的目录项;需修改该
        目录文件的长度信息，并将该目录文件的用户打开文件表项中的 fcbstate 置为 1；
        ⑥ 准备好新文件的 FCB 的内容，文件的属性为数据文件，长度为 0，以覆盖写方式调
        用 do_write()将其填写到⑤中分配到的空目录项中；
        ⑦ 为新文件填写①中分配到的空闲打开文件表项，fcbstate 字段值为 0，读写指针值为 0；
        ⑧ 调用 my_close()关闭②中打开的父目录文件；
        ⑨ 将新文件的打开文件表项序号作为其文件描述符返回。
    */
    int currentfd=usertable.currentFd;

    char* fname = strtok(filename, ".");
	char* exname = strtok(NULL, ".");
    if(!strcmp(fname,""))
    {
        printRed("filename can not start with . \n");
        return -1;
    }
    if(!exname)
    {
        printRed("without exname\n");
        return -1;
    }

    char buf[10000];
    usertable.openfilelist[currentfd].count=0;
    int r_len_fa=do_read(currentfd,usertable.openfilelist[currentfd].open_fcb.length,buf);
    if(r_len_fa<0)
    {
        printRed("read error\n");
        return -1;
    }

    fcb* fcb_p=(fcb*)buf;
    for(int i=0;i<r_len_fa/sizeof(fcb);i++)
    {
        // printf("%s %s %s %s\n",fcb_p[i].fname,fcb_p[i].exname,fname,exname);
        if(!memcmp(fcb_p[i].filename,fname,strlen(fname))&&!memcmp(fcb_p[i].exname,exname,3))
        {
            printRed("this file has existed\n");
            return -1;
        }
    }

    // fat
    unsigned short fat_new=allocFreeBlock();
    if(fat_new==END)
    {
        printRed("error, disk is full\n");
        return -1;
    }

    int off=0;
    for(int i=0;i<r_len_fa/sizeof(fcb);i++)
    {
        off=i+1;
        if(fcb_p[i].free==0)// !!! 内部可能有碎片
        {
            off=i;
            break;
        }
    }

    fcb* fcb_new=(fcb*)malloc(sizeof(fcb));
    fcb_new->attribute=File;

    time_t rawtime = time(NULL);
    struct tm* time=localtime(&rawtime);
    // TODO : time and date
    fcb_new->date=((time->tm_year-100)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
    fcb_new->time=((time->tm_hour)<<12)+((time->tm_min)<<6)+(time->tm_sec);
    strcpy(fcb_new->filename,filename);
    memcpy(fcb_new->exname,exname,3);// 
    fcb_new->first=fat_new;
    fcb_new->length=0;// !!!
    fcb_new->free=1;


    int count=off*sizeof(fcb);
    usertable.openfilelist[currentfd].count=count;
    int w_len=do_write(currentfd,(char *)fcb_new,sizeof(fcb),W_cover,count);
    usertable.openfilelist[currentfd].fcbstate=1;

    if(w_len<0)
    {
        printf("write error\n");
        return -1;
    }

    // currentfd fcb update
    fcb_p=(fcb*)buf;
    
    fcb_p->length=usertable.openfilelist[currentfd].open_fcb.length;
    if(currentfd==0){
        fcb* fcb_p_2=fcb_p+1;
        fcb_p_2->length=usertable.openfilelist[currentfd].open_fcb.length;    
        usertable.openfilelist[currentfd].count=sizeof(fcb);
        int w_len_cur=do_write(currentfd,(char*)fcb_p_2,sizeof(fcb),W_cover,sizeof(fcb));
    }
    
    usertable.openfilelist[currentfd].count=0;
    int w_len_cur=do_write(currentfd,(char*)fcb_p,sizeof(fcb),W_cover,0);


    return fat_new;
}

// 删除名为 filename 的文件
int my_rm(char *filename)
{
    /*
        ① 若欲删除文件的父目录文件还没有打开，则调用 my_open()打开；若打开失败，则
        返回，并显示错误信息；
        ② 调用 do_read()读出该父目录文件内容到内存，检查该目录下欲删除文件是否存在，
        若不存在则返回，并显示错误信息；
        ③ 检查该文件是否已经打开，若已打开则关闭掉；
        ④ 回收该文件所占据的磁盘块，修改 FAT；
        ⑤ 从文件的父目录文件中清空该文件的目录项，且 free 字段置为 0：以覆盖写方式调
        用 do_write()来实现；；
        ⑥ 修改该父目录文件的用户打开文件表项中的长度信息，并将该表项中的 fcbstate
        置为 1；
    */
    int currentfd=usertable.currentFd;
    // abs path
    char filename_abs[MAX_FILE_NAME];
    memset(filename_abs,0,sizeof(filename_abs));
    strcpy(filename_abs,usertable.openfilelist[currentfd].dir);
    if(currentfd!=0)
        filename_abs[strlen(filename_abs)]='/';
    strcat(filename_abs,filename);
    filename_abs[strlen(filename_abs)]='\0';
    for(int i=0;i<MAX_OPEN_FILE;i++)
    {
        if(!strcmp(usertable.openfilelist[i].dir,filename_abs))
        {
            printRed("file has not closed, you can use close command to close a open file\n");
            return -1;
        }
    }


    char* fname=strtok(filename,".");
    char* exname=strtok(NULL,".");
    if(!strcmp(fname,""))
    {
        printRed("filename can not start with . \n");
        return -1;
    }
    if(!exname)
    {
        printRed("without exname\n");
        return -1;
    }
    
    // read current directory
    char buf[10000];
    usertable.openfilelist[currentfd].count=0;
    int r_len_cur=do_read(currentfd,usertable.openfilelist[currentfd].open_fcb.length,buf);
    if(r_len_cur<0)
    {
        printRed("read error\n");
        return -1;
    }
    // existed ?
    fcb* fcb_p=(fcb*)buf;
    int off=-1;
    for(int i=0;i<usertable.openfilelist[currentfd].open_fcb.length/sizeof(fcb);i++,fcb_p++)
    {

        if(!memcmp(fcb_p->filename,fname,strlen(fname))&&!memcmp(fcb_p->exname,exname,3))
        {
            off=i;
            break;
        }
    }
    if(off<0)
    {
        printRed("this file does not exist\n");
        return -1;
    }


    
    // free FAT1  (directory  . and ..)
    int first=fcb_p->first;
    int fat_iter=first;
    int next=disk.FAT1[fat_iter].id;
    while(next!=FREE)
    {
        disk.FAT1[fat_iter].id=FREE;
        fat_iter=next;
        next=disk.FAT1[fat_iter].id;
    }

    int len_current=usertable.openfilelist[currentfd].open_fcb.length;
    /*begin 消除碎片*/
    char buffer[10000];
    usertable.openfilelist[currentfd].count=(off+1)*sizeof(fcb);
    int len_fragment=len_current-(off+1)*sizeof(fcb);
    r_len_cur=do_read(currentfd,len_fragment,buffer);
    if(r_len_cur<0)
    {
        printRed("read error\n");
        return -1;
    }
    usertable.openfilelist[currentfd].count=off*sizeof(fcb);
    int w_len_cur=do_write(currentfd,(char*)buffer,len_fragment,W_cover,off*sizeof(fcb));
    /*end 消除碎片*/

    // free fcb
    fcb_p->attribute=0;
    fcb_p->date=0;
    fcb_p->time=0;
    fcb_p->exname[0]='\0';
    fcb_p->filename[0]='\0';
    fcb_p->first=0;
    fcb_p->free=0;
    fcb_p->length=0;

    // fcb->block
    usertable.openfilelist[currentfd].count=len_current-sizeof(fcb);
    w_len_cur=do_write(currentfd,(char*)fcb_p,sizeof(fcb),W_cover,len_current-sizeof(fcb));
    // lenght - = sizeof(fcb)
    usertable.openfilelist[currentfd].open_fcb.length-=sizeof(fcb);


    usertable.openfilelist[currentfd].count=0;
    r_len_cur=do_read(currentfd,usertable.openfilelist[currentfd].open_fcb.length,buffer);
    if(r_len_cur<0)
    {
        printRed("read error\n");
        return -1;
    }

    // FAT move reconstruction
    fcb* fcb_iter=(fcb*)buffer;
    next = disk.FAT1[fcb_iter[0].first].id;
    int cnt_iter=2*sizeof(fcb);
    if(r_len_cur/sizeof(fcb)==2){
        if(next!=END)
        {
            disk.FAT1[fcb_iter[0].first].id=END;
            disk.FAT1[next].id=FREE;
        }          
    }
    fat_iter=next;
    for(int i=2;i<r_len_cur/sizeof(fcb);i++)
    {
        cnt_iter+=sizeof(fcb);
        if(cnt_iter>BLOCKSIZE)
        {
            next=disk.FAT1[fat_iter].id;
            cnt_iter-=BLOCKSIZE;
            fat_iter=next;
            
            int check=disk.FAT1[fat_iter].id;
            if(check==END)
            {
                disk.FAT1[check].id=FREE;
                disk.FAT1[fat_iter].id=END;
            }
        }
    }
    // free FAT2
    memcpy(disk.FAT2,disk.FAT1,BLOCKSIZE*FATBLOCKNUM/2);

    // current -> block
    fcb_p=(fcb*)buf;
    fcb_p->length=usertable.openfilelist[currentfd].open_fcb.length;
    if(currentfd==0){
        fcb* fcb_p_2=fcb_p+1;
        fcb_p_2->length=usertable.openfilelist[currentfd].open_fcb.length;    
        usertable.openfilelist[currentfd].count=sizeof(fcb);
        int w_len_cur=do_write(currentfd,(char*)fcb_p_2,sizeof(fcb),W_cover,sizeof(fcb));
    }
    usertable.openfilelist[currentfd].count=0;
    w_len_cur=do_write(currentfd,(char*)fcb_p,sizeof(fcb),W_cover,0);

    usertable.openfilelist[currentfd].fcbstate=1;
    return 1;
}
// 打开当前目录下名为 filename 的文件
int my_open(char *filename)
{
    /*
        1 检查该文件是否已经打开,若已打开则返回-1,并显示错误信息;
        2 调用 do_read()读出父目录文件的内容到内存,检查该目录下欲打开文件是否存在,
            若不存在则返回-1,并显示错误信息;
        3 检查用户打开文件表中是否有空表项,若有则为欲打开文件分配一个空表项,
            若没有则返回-1,并显示错误信息;
        4 为该文件填写空白用户打开文件表表项内容,读写指针置为 0;
        5 将该文件所分配到的空白用户打开文件表表项序号(数组下标)作为文件描述符 fd 返回。
    */
    int fd=usertable.currentFd;
        char filename_abs[MAX_FILE_NAME];
    strcpy(filename_abs,usertable.openfilelist[fd].dir);
    if(fd!=0)
        filename_abs[strlen(filename_abs)]='/';
    strcat(filename_abs,filename);
    filename_abs[strlen(filename_abs)]='\0';

    char* filename_tmp=strtok(filename,".");
    char* exname=strtok(NULL,".");
    if(exname==NULL)
    {
        printRed("no exname\n");
        return -1;
    }

    int flag=0;
    for(int i=0;i<MAX_OPEN_FILE;i++)
    {
        if(!strcmp(usertable.openfilelist[i].dir,filename_abs))
        {
            printRed("this file has open\n");
            return -1;
        }
    }

    char buf[10000];
    usertable.openfilelist[fd].count=0;
    int len_r_fa=do_read(fd,usertable.openfilelist[fd].open_fcb.length,buf);
    // father memory -> buf
    int off=-1;
    fcb* fcb_p=(fcb*)buf;
    for(int i=0;i<usertable.openfilelist[fd].open_fcb.length/sizeof(fcb);i++,fcb_p++)
    {
        if(!memcmp(fcb_p->filename,filename_tmp,strlen(filename_tmp))&&!memcmp(fcb_p->exname,exname,3)&&fcb_p->attribute==File)
        {
            off=i;
            break;
        }
    }
    if(off<0)
    {
        printf("this file doesn't exist\n");
        return -1;
    }
    int fd_new=allocFreeusertable();
    if(fd_new<0)
    {
        printf("usertable is full\n");
        return -1;
    }


    memcpy(&usertable.openfilelist[fd_new].open_fcb,fcb_p,sizeof(fcb));

    usertable.openfilelist[fd_new].count=0;
    usertable.openfilelist[fd_new].fcbstate=0;
    usertable.openfilelist[fd_new].topenfile=1;
    strcpy(usertable.openfilelist[fd_new].dir,filename_abs);
    usertable.openfilelist[fd_new].dirno=usertable.openfilelist[fd].open_fcb.first;
    usertable.openfilelist[fd_new].diroff=off;
    
    // usertable.currentFd=fd_new;
    return fd_new;

}
//关闭前面由 my_open()打开的文件描述符为 fd 的文件。
int my_close(int fd)
{
    // 检查 fd 的有效性
    if(fd>=MAX_OPEN_FILE || fd<0)
    {
        printf("文件无效\n");
        return -1;
    }
    /*
        检查用户打开文件表表项中的 fcbstate 字段的值,
        如果为 1 则需要将该文件的 FCB的内容保存到虚拟磁盘上该文件的目录项中,
        打开该文件的父目录文件,以覆盖写方式调用 do_write()将欲关闭文件的 FCB 写入父目录文件的相应盘块中;
        回收该文件占据的用户打开文件表表项(进行清空操作),并将 topenfile 字段置为 0
    */
    int father_fd=find_father_fd(fd);
    if(father_fd==-1)
    {
        printf("error, father directory doesn't exist\n");
        return -1;
    }
    if(usertable.openfilelist[fd].fcbstate==1)
    {
        char buf[10000];
        // father->buf
        usertable.openfilelist[father_fd].count=0;// !!!
        int len_r=do_read(father_fd,usertable.openfilelist[father_fd].open_fcb.length,buf);
        if(len_r<0)
        {
            printf("error, read fail\n");
            return -1;
        }
        // fcb_fd update
        int off=usertable.openfilelist[fd].diroff;
        fcb* fcb_fd=(fcb*)(buf+sizeof(fcb)*off);
        memcpy(fcb_fd,&usertable.openfilelist[fd].open_fcb,sizeof(fcb));
        int pos=off*sizeof(fcb);
        usertable.openfilelist[father_fd].count=pos;
        int len_w=do_write(father_fd,(char*)fcb_fd,sizeof(fcb),W_cover,off);
        
        if(len_w<0)
        {
            printf("error, write fail\n");
            return -1;
        }
    }
    memset(&usertable.openfilelist[fd],0,sizeof(useropen));
    usertable.currentFd=father_fd;
    return father_fd;
}


// 将用户通过键盘输入的内容写到 fd 所指定的文件中。
int my_write(int fd, int pos)
{
    // 检查 fd 的有效性
    if(fd>=MAX_OPEN_FILE || fd<0)
    {
        printf("文件无效\n");
        return -1;
    }

    // 提示并等待用户输入写方式 (1:截断写;2:覆盖写;3:追加写)
    int w_op;
    printf("1: truncate write\n2: cover write\n3: append write\n");
    int ch_tmp;
    ch_tmp;
    while((ch_tmp = getchar()) != '\n' && ch_tmp != EOF);

    scanf("%d",&w_op);
    if(w_op<1||w_op>3)
    {  
        printf("error, try again\n");
        return -1;
    }

    // 提示用户:整个输入内容通过 wq! 键(或其他设定的键)结束
    // 用户可分多次输入写入内容,每次用回车结束
    printf("请输入… 以wq!结束\n");
    printf("可分多次输入写入内容,每次用回车结束\n");

	char text[MAX_TEXT_SIZE] = "\0";
    char text_tmp[MAX_TEXT_SIZE]="\0";
    int len=0;


    char ch;
    while(1)
    {
        while((ch_tmp = getchar()) != '\n' && ch_tmp != EOF);
        scanf("%[^\n]",text_tmp);
        if(mystrcmp(text_tmp,"wq!")==1) // similar to vim
            break;
        strcat(text,text_tmp);
        text[strlen(text)]='\n';
    }
    len=strlen(text);
    int w_len;
    usertable.openfilelist[fd].count=usertable.openfilelist[fd].open_fcb.length;
    w_len=do_write(fd,text,len,w_op,pos); 

    if(w_len<0)
    {
        printRed("write error, file is full\n");
        return -1;
    }

    // 调用 do_write()函数将通过键盘键入的内容写到文件中
    // 如果 do_write()函数的返回值为非负值,
    // 则将实际写入字节数增加 do_write()函数返回值,否则显示出错信息

    // 如果当前读写指针位置大于用户打开文件表项中的文件长度,
    // 则修改打开文件表项 中的文件长度信息,并将 fcbstate 置1
    int c=usertable.openfilelist[fd].count;
    int l=usertable.openfilelist[fd].open_fcb.length;
    usertable.openfilelist[fd].open_fcb.length=max(c,l);

    usertable.openfilelist[fd].fcbstate=1;
    // after write file, must close!!!
    
    // setbuf(stdin, NULL);
    return 1;

}


// 被写文件函数 my_write()调用,用来将键盘输入的内容写到相应的文件中去
int do_write(int fd,char *text,int len,char wstyle, int pos)
{
    /*
        params
        fd: open()函数的返回值,文件的描述符;
        text:指向要写入的内容的指针;
        len:本次要求写入字节数
        wstyle:写方式
        return 实际写入的字节数。
    */
    // 用 malloc()申请 BLOCKSIZE 的内存空间作为读写磁盘的缓冲区 buf,
    // 申请失败则返回-1,并显示出错信息   
    unsigned char *buf=(unsigned char*)malloc(BLOCKSIZE);
    if(buf==NULL)
    {
        printf("malloc error\n");
        return -1;
    }
    /*
        将读写指针转化为逻辑块块号和块内偏移 off,
        并利用打开文件表表项中的首块号及 FAT 表的相关内容将逻辑块块号转换成对应的磁盘块块号 blkno;
        如果找不到对应的磁盘块,则需要检索 FAT 为该逻辑块分配一新的磁盘块,
        并将对应的磁盘块块号 blkno 登记到FAT 中,
        若分配失败,则返回-1,并显示出错信息
    */
    unsigned short first=usertable.openfilelist[fd].open_fcb.first;

    // 文件打开之后立即清空原有内容
    if(wstyle==W_truncate)
    {
        usertable.openfilelist[fd].count=0;
        usertable.openfilelist[fd].open_fcb.length=0;
    }
    // 文件打开之后不清空原有内容，可以在文件任意位置写入
    else if(wstyle==W_cover)
    {
        if(usertable.openfilelist[fd].open_fcb.attribute==File&&usertable.openfilelist[fd].open_fcb.length!=0)
        {
            if(pos!=-1&&pos<usertable.openfilelist[fd].open_fcb.length){
                usertable.openfilelist[fd].count=pos;            
            }else{
                usertable.openfilelist[fd].count-=1;
            }
        }
    }
    // 文件打开之后不清空原有内容，每次只能在文件最后写入
    else if(wstyle==W_append)
    {
        if(usertable.openfilelist[fd].open_fcb.attribute==Dir)
        {
            usertable.openfilelist[fd].count=usertable.openfilelist[fd].open_fcb.length;
        }
        else if(usertable.openfilelist[fd].open_fcb.attribute==File&&usertable.openfilelist[fd].open_fcb.length!=0)
        {
            usertable.openfilelist[fd].count-=1;
        }   
    }

    // use fat to move count!!!
    int count = usertable.openfilelist[fd].count;
    unsigned short fat_iter=first;

    // printf("%d, %d\n",count,BLOCKSIZE);
    while(count>=BLOCKSIZE)
    {
        if(disk.FAT1[fat_iter].id==END)
        {
            int fat_new=allocFreeBlock();
            if(fat_new==END)
            {
                printf("error , disk is full\n");
                return -1;
            }
            else{
                disk.FAT1[fat_iter].id=fat_new;
            }
        }
        fat_iter=disk.FAT1[fat_iter].id;
        count-=BLOCKSIZE;
    }
    

    // 1. short text : text+block->buf->block
    // 2. long text : text -> buf -> block
    unsigned char* block_p=(unsigned char*)(disk.myvhard+BLOCKSIZE*fat_iter);
    int text_p=0;
    while(text_p<len)
    {
        memcpy(buf,block_p,BLOCKSIZE);// block->buf            
        while(count<BLOCKSIZE)// text->buf
        {   
            // buf[count]=text[text_p];
            memcpy(buf+count,text+text_p,1);
            text_p++;
            count++;
            if(text_p==len)// text is short , break directly
                break;
        }
        memcpy(block_p,buf,BLOCKSIZE);// buf->block
    
        if(count==BLOCKSIZE&&text_p!=len)// text is too long
        {
            count=0;
            // next block
            if(disk.FAT1[fat_iter].id==END)
            {
                int fat_new=allocFreeBlock();
                if(fat_new==END)
                {
                    printf("error , disk is full\n");
                    return text_p;// !!! 
                }else{
                    // !!! need to update block_p
                    block_p=(unsigned char*)(disk.myvhard+BLOCKSIZE*fat_new);
                    disk.FAT1[fat_iter].id=fat_new;
                    fat_iter=fat_new;
                }
            }else{
                // !!! need to update block_p
                fat_iter=disk.FAT1[fat_iter].id;
                block_p=(unsigned char*)(disk.myvhard+BLOCKSIZE*fat_iter);
            }
        }
    }

    usertable.openfilelist[fd].count+=len;

    int c=usertable.openfilelist[fd].count;
    int l=usertable.openfilelist[fd].open_fcb.length;
    usertable.openfilelist[fd].open_fcb.length=max(c,l);
    free(buf);

    // // truncate may cause some block invalid
    // fat_iter=disk.FAT1[fat_iter].id;
    // int tmp;
    // while(fat_iter!=FREE)
    // {
    //     tmp=disk.FAT1[fat_iter].id;
    //     disk.FAT1[fat_iter].id=FREE;
    //     fat_iter=tmp;
    // }

    // Fat2
    memcpy(disk.FAT2,disk.FAT1,BLOCKSIZE*FATBLOCKNUM/2);
    
    return len;
}

// 读出指定文件中从读写指针开始的长度为 len 的内容到用户空间中
int my_read (int fd, int pos, int len)
{
    // 检查 fd 的有效性
    if(fd>=MAX_OPEN_FILE || fd<0)
    {
        printf("文件无效\n");
        return -1;
    }
    if(len==-1)
    {
        len=usertable.openfilelist[fd].open_fcb.length-pos;
    }
    char text[1000]="\0";
    usertable.openfilelist[fd].count=pos;
    printf("pos = %d, len = %d\n",pos,len);
    int readlen=do_read(fd,len,text);

    // 如果 do_read()的返回值为负,则显示出错信息;
    // 否则将 text[]中的内容显示到屏幕上
    if(readlen<0)
    {
        printf("read error\n");
        return -1;
    }else{
        printf("%s",text);
        if(text[strlen(text)-1]!='\n')
            printf("\n");
        return 1;
    }
}

// 被 my_read()调用,读出指定文件中从读写指针开始的长度为 len 的内容到用户空间的 text 中
int do_read (int fd, int len,char *text)
{
    /*
        fd: open()函数的返回值,文件的描述符
        len: 要求从文件中读出的字节数
        text: 指向存放读出数据的用户区地址
        return : 实际读出的字节数
    */
    
    // 用 malloc()申请 BLOCKSIZE 的内存空间作为读写磁盘的缓冲区 buf,
    // 申请失败则返回-1,并显示出错信息   
    unsigned char *buf=(unsigned char*)malloc(BLOCKSIZE);
    if(buf==NULL)
    {
        printf("malloc error\n");
        return -1;
    }
    int count=usertable.openfilelist[fd].count;
    unsigned short first=usertable.openfilelist[fd].open_fcb.first;
    unsigned short fcb_iter=first;

    // move count, len doesn't change 
    while(count>=BLOCKSIZE)
    {
        fcb_iter=disk.FAT1[fcb_iter].id;
        if(fcb_iter==END)
        {
            printf("error, no such block\n");
            return -1;
        }
        count-=BLOCKSIZE;
    }

    unsigned char* block_p=(unsigned char*)(disk.myvhard+BLOCKSIZE*fcb_iter);
    memcpy(buf,block_p,BLOCKSIZE);// block->buf

    
    int lenTmp=len;
    char* text_p=text;
    while(len>0)
    {
        if(len<=BLOCKSIZE-count)// is too short
        {
            memcpy(text_p,buf+count,len);// buf->text
            
            text_p+=len;
            count+=len;
            usertable.openfilelist[fd].count+=len;
            len=0;
        }else{// is longer than BLOCKSIZE-count
            memcpy(text_p,buf+count,BLOCKSIZE-count);

            text_p+=BLOCKSIZE-count;
            len-=BLOCKSIZE-count;
            count=0;  
            fcb_iter=disk.FAT1[fcb_iter].id;
            if(fcb_iter==END)
            {
                printf("error, len is too long\n");
                return -1;
            }

            // !!! block->buf
            block_p=disk.myvhard+BLOCKSIZE*fcb_iter;
            memcpy(buf,block_p,BLOCKSIZE);
        }
    }

    free(buf);
    return lenTmp-len; 
}

// 退出文件系统
int my_exitsys()
{
    /*
        ① 使用 C 库函数 fopen()打开磁盘上的 myfsys 文件；
        ② 将虚拟磁盘空间中的所有内容保存到磁盘上的 myfsys 文件中；
        ③ 使用 c 语言的库函数 fclose()关闭 myfsys 文件；
        ④ 撤销用户打开文件表，释放其内存空间 释放虚拟磁盘空间。
    */
    FILE* fp=fopen(myfsys_name,"wb");
    while(usertable.currentFd)
    {
        int fd=my_close(usertable.currentFd);
        if(fd<0)
        {
            printf("close error\n");
            return -1;
        }
    }
    fwrite(disk.myvhard,1,DISKSIZE,fp);
    fclose(fp);

}

void my_listFAT()
{
    int fatnum=BLOCKSIZE*FATBLOCKNUM/2/sizeof(fat);
    for(int i=0;i<fatnum;i++)
    {
        if(i<1+FATBLOCKNUM)
            printf("\033[31m%05d(%05d)\033[0m ",disk.FAT1[i].id,i);
        else    
            printf("%05d(%05d) ",disk.FAT1[i].id,i);
        if((i+1)%16==0)
            printf("\n");
    }
}

unsigned short allocFreeBlock()
{
    fat* fat_tmp=disk.FAT1;
    for(unsigned short i=0;i<(int)(BLOCKNUM);i++)
    {
        if(fat_tmp[i].id==FREE)
        {
            fat_tmp[i].id=END;// !!!
            disk.FAT2[i].id=END;// FAT2
            return i;
        }
    }
    return END;
}

int find_father_fd(int fd)
{
    int ret=-1;
    for(int i=0;i<MAX_OPEN_FILE;i++)
    {
        if(usertable.openfilelist[i].open_fcb.first==usertable.openfilelist[fd].dirno)
        {
            ret=i;
            break;
        }
    }
    return ret;
}

int allocFreeusertable()
{
    int ret=-1;
    for(int i=0;i<MAX_OPEN_FILE;i++)
    {
        if(!usertable.openfilelist[i].topenfile)
        {
            usertable.openfilelist[i].topenfile=1;
            ret=i;
            break;
        }
    }
    return ret;
}

void my_help()
{
    int cnt_tmp=150;
    char str_tmp[200]="─";
    char str_tmp_c[200]="│";
    printf("┌─");
    for(int i=0;i<cnt_tmp;i++)
        printf("%s",str_tmp);
    printf("┐");
    printf("\n");
    printf("%scommand name\t\t\tcommand parameters\t\t\tcommand function\t\t\t\t\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("├─");
    for(int i=0;i<cnt_tmp;i++)
        printf("%s",str_tmp);    
    printf("┤");
    printf("\n");
    printf("%smkdir\t\t\t\tdirectory name\t\t\t\tcreate a new directory in the current directory\t\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%srmdir\t\t\t\tdirectory name\t\t\t\tdelete the specified directory from the current directory\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%sls\t\t\t\t----\t\t\t\t\tdisplay directories and files in the current directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%scd\t\t\t\tdirectory name or path name\t\tswitch the current directory to the specified directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%screate\t\t\t\tfile name\t\t\t\tcreate the specified file in the current directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%sopen\t\t\t\tfile name\t\t\t\topen the specified file in the current directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%sclose\t\t\t\tfile name\t\t\t\tclose the specified file in the current directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%swrite\t\t\t\tfile name\t\t\t\tin the open file state, write the specified file in the current directory\t%s\n",str_tmp_c,str_tmp_c);
    printf("%sread\t\t\t\tfile name\t\t\t\tin the open file state, read the specified file in the current directory\t%s\n",str_tmp_c,str_tmp_c);
    printf("%srm\t\t\t\tfile name\t\t\t\tdelete the specified file from the current directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%sexit\t\t\t\t----\t\t\t\t\tlog out\t\t\t\t\t\t\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("└─");
    for(int i=0;i<cnt_tmp;i++)
        printf("%s",str_tmp);
    printf("┘");
    printf("\n");
}


void printStart(){   
    printf("♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♡♡♡♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♥♡♡♡♡♡♡♥♥♡♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♥♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♥♥♡♥♥♥♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♡♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♥♥♥♡♥♥♥♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♡♡♡♡♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♡♥♥♥♥♥♥♥♥♡♡♡\n"
           "♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♥♥♥♡♥♥♥♡♥♥♥♡♡♥♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♡♡♡♡♡♡♡♡♥♥♡♥♡♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡\n"
           "♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♥♥♥♡♡♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♥♥♥♥♥♡♡♥♥♥♥♥♡♡♡♡♡♡♡♥♥♥♡♥♥♥♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♥♥♥♡♡♡♡♥♥♥♡♡♥♥♥♡♡♡♡\n"
           "♡♡♥♡♡♥♥♥♡♡♡♡♥♥♥♥♥♥♥♡♡♡♡♥♥♥♥♡♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♡♡♥♥♥♡♡♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♥♥♡♡♥♥♡♡♡♥♥♥♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♡♥♥♥♡♡♡♡\n"
           "♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♡♡♡♡♡♡♡♥♥♥♥♥♡♥♥♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♡♥♥♥♡♡♡♡♡♡♥♥♥♥♥♥♡♡♥♥♥♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♡♥♥♡♡♥♥♡♡♡♥♥♥♡♡♡♥♥♥♥♥♥♥♥♡♡♥♥♥♥♥♡♡♡♡♡\n"
           "♡♡♡♡♡♡♥♥♡♡♡♡♥♥♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♡♡♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♡♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♡♥♥♡♡♥♥♥♥♡♡♥♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♥♥♥♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♡♥♥♡♡♡♥♥♥♡♡♥♥♡♡♡♡♡♥♥♥♥♥♥♡♥♥♥♥♥♥♥♥♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♡♡♡♡♡♥♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♡♥♥♥♥♡♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♥♥♥♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♥♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡\n"
           "♡♡♡♡♡♡♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♡♡♡♥♥♥♡♡♡♡♥♥♥♥♥♥♡♡♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♥♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡\n"
           "♡♡♡♡♡♡♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♥♥♡♥♥♥♡♡♡♡♡♡♡♥♥♥♥♡♡♡♡♥♥♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♡♡♡♥♥♡♡♡\n"
           "♡♡♡♡♡♡♡♡♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♥♥♥♥♡♡♥♥♡♥♥♥♥♡♡♡♡♡♡♡♡♡♡♥♥♥♡♥♥♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♥♥♥♥♥♥♡♡♥♡♡♡♡♡♡♡♥♥♥♥♥♡♡♥♥♡♡♡♥♥♡♡♡\n"
           "♡♡♡♡♡♡♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♥♥♥♥♡♡♡♥♥♡♡♥♥♥♥♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♡♥♥♡♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♡♥♥♥♡♡♥♥♡♡♡♡♡♡♥♥♥♥♥♡♡♥♥♡♡♡♥♥♡♡♡\n"
           "♡♡♡♡♥♥♥♥♥♥♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♥♥♥♡♡♡♥♥♥♥♡♡♡♥♥♥♥♡♡♡♥♥♥♥♥♥♡♥♥♥♡♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♡♥♥♥♥♥♥♥♡♡♡♡♡♡♥♥♥♥♡♡♡♥♥♥♥♥♥♥♡♡♡\n"
           "♡♡♥♥♥♥♥♥♡♡♡♡♥♥♥♥♥♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♥♥♡♡♡♥♥♥♥♥♡♡♡♡♥♥♡♡♡♡♡♥♥♡♡♡♥♥♥♥♡♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♥♥♥♥♡♡♥♥♥♥♥♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♥♥♥♥♥♡♡\n"
           "♡♡♥♥♥♥♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♥♥♥♡♡♡♡♥♥♥♥♥♡♡♡♡♡♡♡♡♥♡♡♡♡♡♥♥♡♡♡♥♥♥♡♡\n"
           "♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡\n");
}

void printRed(char* s){
    printf("\033[31m%s\033[0m\n",s); 
}

void my_pwd(){
    int currentfd=usertable.currentFd;
    printf("%s\n",usertable.openfilelist[currentfd].dir);
    printf("current fd : %d\n",currentfd);
}   

void showFileSystem(){
    system("clear");
    printf("********************File System Based On FAT********************\n");
    printf("********************author : 20051124 Shenming******************\n");
    printf("磁盘总大小 : %dB\n",disk.block0->blocksize*disk.block0->blocknum);
    printf("磁盘数量 : %d\n",disk.block0->blocknum);
    printf("磁盘大小 : %dB\n",disk.block0->blocksize);
    printf("数据块数量 : %d\n",disk.block0->datablocknum);
    printf("FAT块数量 : %d\n",disk.block0->fatblocknum);
    printf("bootblock 大小 : %ldB\n",sizeof(bootblock));
    printf("fcb大小 : %ldB\n",sizeof(fcb));
    char magic[9];
    memcpy(magic,disk.block0->magic_number,8);
    magic[8]='\0';
    printf("file system magic number : %s\n",magic);
    printf("file system max open file number : %d\n",disk.block0->maxopenfile);
}

int str2int(char* s)
{
    int l=strlen(s);
    int ret=0;
    for(int i=0;i<l;i++)
    {
        ret*=10;
        ret+=(s[i]-'0');
    }
    return ret;
}

int mystrcmp(char *s1,char* s2)
{
    int a1=strlen(s1);
    int a2=strlen(s2);
    if(a1!=a2)
        return -1;
    for(int i=0;i<a1;i++)
        if(s1[i]!=s2[i])
            return -1;
    return 1;
}

int checkfile(char* s)
{
    int l=strlen(s);
    for(int i=0;i<l;i++)
    {
        if(s[i]=='.')
            return 1;
    }
    return 0;
}

void RecurDelete(fcb* fcb_p)
{    
    if(fcb_p->attribute==File || (fcb_p->attribute==Dir&&fcb_p->length==2*sizeof*(fcb_p)))
    {
        int first=fcb_p->first;
        int fat_iter=first;
        int next=disk.FAT1[fat_iter].id;
        while(next!=FREE)
        {
            disk.FAT1[fat_iter].id=FREE;
            fat_iter=next;
            next=disk.FAT1[fat_iter].id;
        }
        return;
    }
    else if(fcb_p->attribute==Dir&&fcb_p->length>2*sizeof(fcb_p))
    {   
        char dirname[20];
        memcpy(dirname,fcb_p->filename,8);
        dirname[8]='\0';

        my_cd(dirname);
        char file_tmp[MAX_TEXT_SIZE];
        int fd_tmp=usertable.currentFd;
        usertable.openfilelist[fd_tmp].count=0;
        int len_r = do_read(fd_tmp,usertable.openfilelist[fd_tmp].open_fcb.length,file_tmp);
        fcb* fcb_tmp=(fcb*)file_tmp;
        
        fcb_tmp+=2;
        for(int i=2;i<len_r/sizeof(fcb);i++,fcb_tmp++)
        {
            RecurDelete(fcb_tmp);
        }
        my_cd("..");

        int first=fcb_p->first;
        int fat_iter=first;
        int next=disk.FAT1[fat_iter].id;
        while(next!=FREE)
        {
            disk.FAT1[fat_iter].id=FREE;
            fat_iter=next;
            next=disk.FAT1[fat_iter].id;
        }
    }

}