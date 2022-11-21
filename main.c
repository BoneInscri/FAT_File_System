#include "FileSystem.h"

char *cmd[]={"mkdir","rmdir","cd","create","open",
            "close","write","read","rm","ls","exit",
            "help","clear","showfat","pwd","showinfo"};
int CMD_len=16;
char command[20];
char* sp;
char dirname[20];
int cmd_len;
const int CMD_MAX_LEN=40;// command
const int ARGV_MAX_LEN=16;// argv
const int BUFFER_MAX_LEN=80;// buffer

int ParseArg(char* buf, char**argv)
{
    int argv_idx;
    int argc;
    argv_idx=argc=0;
    while(argv_idx<ARGV_MAX_LEN)
        argv[argv_idx++]=NULL;
    while(1)
    {
        while(*buf == ' ')
            buf++;
        if(*buf=='\n')
            break;
        argv[argc]=buf;
        while(*buf != ' ' && *buf)
            buf++;// 读取一个argv
        *buf++='\0';
        if(argc>ARGV_MAX_LEN)
        {
            printf("argc is too large!\n");
            return -1;
        }
        argc++;
    }
    return argc;// 返回的是参数字符串的个数
}


int CommandSwitch(int cmdIdx,int argc, char* argv[CMD_MAX_LEN])
{
    int flag=0;
    if(cmdIdx>=0&&cmdIdx<=8)
    {
        if(argc==3)
            strcpy(dirname,argv[2]);
        else    
            strcpy(dirname,argv[1]);
        int len_dir=strlen(dirname);
        if(len_dir>MAX_FILENAME)
        {
            printRed("filename is to long(>8)");
            return 0;
        }else if(len_dir==0)
        {
            printRed("filename is NULL");
            return 0;
        }
    }
    
    int pos=0;
    int r_len=-1;
    if(cmdIdx==6&&argc==3)// pos!=0 write
    {
        pos=str2int(argv[1]);// string -> int
    }
    if(cmdIdx==7)
    {
        if(argc==4){
            strcpy(dirname,argv[2]);
            pos=str2int(argv[1]);
            r_len=str2int(argv[3]);                
        }else if(argc==3)
        {
            if(checkfile(argv[2]))// pos is not default
            {
                pos=str2int(argv[1]);
            }else// r_len is not default 
            {
                r_len=str2int(argv[2]);
                strcpy(dirname,argv[1]);    
            }

        }
    }
    int fd=-1;
    if(cmdIdx>=5&&cmdIdx<=7)//close write read
    {
        int currentfd=usertable.currentFd;
        // abs path
        char filename_abs[MAX_FILE_NAME];
        memset(filename_abs,0,sizeof(filename_abs));
        strcpy(filename_abs,usertable.openfilelist[currentfd].dir);
        if(currentfd!=0)
            filename_abs[strlen(filename_abs)]='/';
        strcat(filename_abs,dirname);
        filename_abs[strlen(filename_abs)]='\0';
        // dirname -> fd
        for(int i=0;i<MAX_OPEN_FILE;i++)
            if(!strcmp(usertable.openfilelist[i].dir,filename_abs))
                fd=i;

        // file has open?
        if(usertable.openfilelist[fd].topenfile==0)
        {
            printRed("this file has not open!");
            return 0;
        }
    }

    switch(cmdIdx)
    {   
        case 0:
            // mkdir
            my_mkdir(dirname);
            return 0;  
        case 1:
            // rmdir
            if(argc==3&&!strcmp(argv[1],"-r"))
                flag=1;
            int ret = my_rmdir(dirname,flag);
            if(ret==0)
            {
                printRed("argv is error\n");
                return 0;
            }
            break;
        case 2:
            // cd
            my_cd(dirname);
            break;
        case 3:
            // create
            my_create(dirname);
            break;
        case 4:
            // open
            my_open(dirname);
            break;   
        case 5:
            // close
            my_close(fd);
            break;
        case 6:
            // write
            my_write(fd, pos);
            break;
        case 7:
            // read
            my_read(fd, pos,r_len);
            break;
        case 8:
            // rm
            my_rm(dirname);
            break;
        case 9:
            int ls_ret=my_ls();
            break;
        case 10:
            // exit
            my_exitsys();
            printf("退出文件系统\n");
            return -1;
        case 11:
            // help
            my_help();
            break;
        case 12:
            system("clear");
            printf("输入help显示帮助页面...\n");
            break;
        case 13:
            my_listFAT();
            break;
        case 14:
            my_pwd();
            break;
        case 15:
            showFileSystem();
            break;
        default:
            printf("shell: command not found: %s\n",argv[0]);
            return 0;
    }
    return 1;
}

int Runcmd(const char* CMD)
{
    char *argv[CMD_MAX_LEN];// argv array
    char buf[BUFFER_MAX_LEN];// buffer
    strcpy(buf,CMD);
    buf[cmd_len-1] = ' ';// '\n'->' '
    buf[cmd_len] = '\n'; // '\0'->'\n'

    int argc = ParseArg(buf,argv);
    if(argc==0)
    {
        printRed("error, command is empty");
        return 0;
    }
    
    int cmd_idx=-1;

    for(int i=0;i<CMD_len;i++)
    {
        if(!strcmp(cmd[i],argv[0]))
        {
            cmd_idx=i;
            break;
        }
    }
    if(argc==1&&cmd_idx<=8&&cmd_idx>=0)
    {
        printRed("error, no command object");
        return 0;
    }

    return CommandSwitch(cmd_idx,argc,argv);    
}   

// 系统主函数
void main()
{
    InitVariable();
    // printStart();
    startsys();
    printf("文件系统正在开启...\n");   
    // sleep(3);
    system("clear");
    showFileSystem();
    printf("输入help显示帮助页面...\n");
    // shell
    while(1)
    {
        printf("%s🦴@Ubuntu22.04:%s > ",usertable.USERNMAE,usertable.openfilelist[usertable.currentFd].dir);
        fflush(stdout);        
        memset(command,0,sizeof(command));
        if((cmd_len = read(0, command, CMD_MAX_LEN)) >= 2)
        {
            int ret = Runcmd(command);
            if(ret<0)
                break;
        }

    }
}

