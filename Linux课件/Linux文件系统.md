# Linux文件系统

## 常用宏函数：

```
#define ARGS_CHECK(argc,num) {if(argc != num){fprintf(stderr,"args
error!\n");return -1;}}
#define ERROR_CHECK(ret,num,msg) {if(ret == num){perror(msg);return -1;}}
```

## 1基于文件流的文件操作

### 1.1文件流的创建与关闭

#### fopen函数与fclose函数

```c
#include <stdio.h> //头文件包含
FILE* fopen(const char* path, const char* mode);//文件名 模式
int fclose(FILE* stream);
```

#### 读写文件fread和fwrite

外部设备与用户缓存区之间的读写

```c
#include <stdio.h>
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);//写到ptr指向的地址中，每个元素大小， 元素个数， 读取的文件流
size_t fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream//写到ptr指向的地址中，每个元素大小， 元素个数， 读取的文件流
```

#### 格式化读写:

```c
#include <stdio.h>
int printf(const char *format, ...);
//相当于fprintf(stdout,format,…);
int scanf(const char *format, …);
int fprintf(FILE *stream, const char *format, ...);
int fscanf(FILE *stream, const char *format, …);
int sprintf(char *str, const char *format, ...);
//eg:sprintf(buf,”the string is;%s”,str);
int sscanf(char *str, const char *format, …);
```

#### 单个字符读写

```c
#include <stdio.h>
int fgetc(FILE *stream);
int fputc(int c, FILE *stream);
int getc(FILE *stream);//等同于 fgetc(FILE* stream)
int putc(int c, FILE *stream);//等同于 fputc(int c, FILE* stream)
int getchar(void);//等同于 fgetc(stdin);从标准输入流中
int putchar(int c);//等同于 fputc(int c, stdout);从标准输出流中
```

#### 字符串读写

```c
char *fgets(char *s, int size, FILE *stream);
int fputs(const char *s, FILE *stream);
int puts(const char *s);//等同于 fputs(const char *s,stdout); 从标准输出流中
char *gets(char *s);//等同于 fgets(const char *s, int size, stdin);从标准输出流中
```

#### 文件定位函数：

```c
#include <stdio.h>
int feof(FILE * stream);
//通常的用法为while(!feof(fp))
int fseek(FILE *stream, long offset, int whence);
//设置当前读写点到偏移whence 长度为offset处
long ftell(FILE *stream);
//用来获得文件流当前的读写位置
void rewind(FILE *stream);
//把文件流的读写位置移至文件开头 fseek(fp, 0, SEEK_SET);
```

#### *二进制文件与文本文件*

如果文件当中的内容是一串ASCII字符的序列，那么这类文件就是**文本文件**。文件类型存储的是字符串，建议使用fscanf读取。否则就都是二进制文件，**二进制文件写入是什么类型，读取就使用什么类型**

### 1.2文件相关系统调用

#### 修改文件权限chmod

```c
#include <sys/stat.h>
int chmod(const char* path, mode_t mode);
//mode形如：0777 是一个八进制整型
//path参数指定的文件被修改为具有mode参数给出的访问权限。
```

#### 获取和修改当前工作目录getcwd，chdir

```c
#include <unistd.h> //头文件
char *getcwd(char *buf, size_t size); //获取当前目录，相当于pwd命令
int chdir(const char *path); //修改当前目录，即切换目录，相当于cd命令
```

#### 创建和删除空目录mkdir,rmdir

```c
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
int mkdir(const char *pathname, mode_t mode); //创建目录,mode是目录权限
int rmdir(const char *pathname); //删除目录
```

#### 文件目录的存储(目录项)

```c
$ ls -ial
# 查看所有文件的inode信息
$ ln 当前文件 目标
# 建立名为“目标”的硬链接
```

目录项的结构体

```c
struct dirent{
ino_t d_ino; //该文件的inode
off_t d_off; //到下一个dirent的偏移
unsigned short d_reclen;//文件名长度
unsigned char d_type; //所指的文件类型
char d_name[256]; //文件名
};
```

#### 目录流的相关操作

```c
#include <sys/types.h>
#include <dirent.h>
DIR *opendir(const char *name); //打开一个目录流
struct dirent *readdir(DIR *dir); //读取目录流当中的一个目录项，
void rewinddir(DIR *dir); //重新定位到目录文件的头部
void seekdir(DIR *dir,off_t offset);//用来设置目录流目前的读取位置
off_t telldir(DIR *dir); //返回目录流当前的读取位置
int closedir(DIR *dir); //关闭目录文件
```

#### stat命令

```c
// man 2 stat
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
int stat(const char *pathname, struct stat *statbuf);
//结构体stat的定义
struct stat {
dev_t st_dev; /*如果是设备，返回设备表述符，否则为0*/
ino_t st_ino; /* i节点号 */
mode_t st_mode; /* 文件类型 */
nlink_t st_nlink; /* 链接数 */
uid_t st_uid; /* 属主ID */
gid_t st_gid; /* 组ID */
dev_t st_rdev; /* 设备类型*/
off_t st_size; /* 文件大小，字节表示 */
blksize_t st_blksize; /* 块大小*/
blkcnt_t st_blocks; /* 块数 */
time_t st_atime; /* 最后访问时间*/
time_t st_mtime; /* 最后修改时间*/
time_t st_ctime; /* 最后权限修改时间 */
};
```

#### 实现ls -al效果(简化)

```c
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

void listFiles(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileInfo;

    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }
    
    while ((entry = readdir(dir)) != NULL) {
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
    
        if (stat(fullpath, &fileInfo) == -1) {
            perror("stat");
            exit(EXIT_FAILURE);
        }

        struct passwd *user = getpwuid(fileInfo.st_uid);
        struct group *group = getgrgid(fileInfo.st_gid);
    
        char permissions[11];
        permissions[0] = (S_ISDIR(fileInfo.st_mode)) ? 'd' : '-';
        permissions[1] = (fileInfo.st_mode & S_IRUSR) ? 'r' : '-';
        permissions[2] = (fileInfo.st_mode & S_IWUSR) ? 'w' : '-';
        permissions[3] = (fileInfo.st_mode & S_IXUSR) ? 'x' : '-';
        permissions[4] = (fileInfo.st_mode & S_IRGRP) ? 'r' : '-';
        permissions[5] = (fileInfo.st_mode & S_IWGRP) ? 'w' : '-';
        permissions[6] = (fileInfo.st_mode & S_IXGRP) ? 'x' : '-';
        permissions[7] = (fileInfo.st_mode & S_IROTH) ? 'r' : '-';
        permissions[8] = (fileInfo.st_mode & S_IWOTH) ? 'w' : '-';
        permissions[9] = (fileInfo.st_mode & S_IXOTH) ? 'x' : '-'; 
        permissions[10] = '\0'; 
     
        char timeString[80];
        strftime(timeString, sizeof(timeString), "%b %d %H:%M", localtime(&fileInfo.st_mtime));
    
        printf("%s %ld %s %s %ld %s %s\n", permissions, fileInfo.st_nlink,
               user->pw_name, group->gr_name, fileInfo.st_size, timeString, entry->d_name);
    }
    
    closedir(dir);

}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : ".";

    listFiles(path);
    
    return 0;

}
```

#### 实现tree命令(简化)

```c
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

void listFilesRecursively(const char *basePath, int depth) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileInfo;

    if (!(dir = opendir(basePath))) {
        return;
    }   
    
    while ((entry = readdir(dir)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name); //拼接
    
        if (stat(path, &fileInfo) == -1) {
            continue;
        }   
    
        if (S_ISDIR(fileInfo.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }   
    
            for (int i = 0; i < depth; i++) {
                printf("  ");
            }   
    
            printf("|-- %s/\n", entry->d_name);
    
            listFilesRecursively(path, depth + 1); 
        } else {
            for (int i = 0; i < depth; i++) {
                printf("  ");
            }   
    
            printf("|-- %s\n", entry->d_name);                                                                                                                                                            
        }   
    }   

    closedir(dir);

}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : ".";

    printf("%s\n", path);
    listFilesRecursively(path, 0);
    
    return 0;

}
```

### 1.3基于文件描述符的文件操作

#### 文件描述符:即内核区指针数组下标

![image-20240127144235383](C:\Users\king\AppData\Roaming\Typora\typora-user-images\image-20240127144235383.png)

#### open和close函数打开和关闭文件

```c
#include <sys/types.h> //头文件
#include <sys/stat.h>
#include <fcntl.h>
int open(const char *pathname, int flags); //文件名 打开方式
int open(const char *pathname, int flags, mode_t mode);//文件名 打开方式 权限
int close(int fd);//fd表示文件描述词,是先前由open或creat创建文件时的返回值。
//用法：
int fd = open("file",O_RDWR | O_CREAT,0755); //表示给755的权限

```

#### read和write读写文件

```c
#include <unistd.h>
ssize_t read(int fd, void *buf, size_t count);//文件描述符 缓冲区 缓冲区长度上限
ssize_t write(int fd, const void *buf, size_t count);//文件描述符 缓冲区 内容长度
//ssize_t为有符号大小的整形变量，大小与平台有关

//读写往往提前申请一个缓冲区数组
char buf[1024] = {0};
```

读写磁盘时不会阻塞，读写外设文件时会发生阻塞

二进制的读写**按什么类型写入，就用什么类型写出**

通过读写实现拷贝

```c
int main(int argc, char *argv[])
{
// ./cp src dest
ARGS_CHECK(argc,3);
int fdr = open(argv[1],O_RDONLY);
ERROR_CHECK(fdr,-1,"open fdr");
int fdw = open(argv[2],O_WRONLY|O_TRUNC|O_CREAT,0666);
ERROR_CHECK(fdw,-1,"open fdw");
//char buf[4096] = {0};
char buf[4096000] = {0};
// buf选择char数组，不是字符串的含义，而是因为char的字节是1
while(1){
memset(buf,0,sizeof(buf));
ssize_t sret = read(fdr,buf,sizeof(buf));
ERROR_CHECK(sret,-1,"read");
// 读取磁盘文件，返回值为0，则读完
if(sret == 0){
break;
}
// 写入dest
write(fdw,buf,sret);
}
close(fdr);
close(fdw);
return 0;
}
```

#### 文件偏移lseek

```c
#include <sys/types.h>
#include <unistd.h>
off_t lseek(int fd, off_t offset, int whence);//fd文件描述词
//whence 可以是下面三个常量的一个
//SEEK_SET 从文件头开始计算
//SEEK_CUR 从当前指针开始计算
//SEEK_END 从文件尾开始计算
```

#### 文件截断ftruncate

```c
#include <unistd.h>
int ftruncate(int fd, off_t length);
//可以形成文件空洞
```

#### 文件映射mmap

```c
#include <sys/mman.h>
void *mmap(void *adr, size_t len, int prot, int flag, int fd, off_t off);
```

```c
//常见使用步骤
//1open
int fd = open(argv[1], O_RDWR);
//2ftruncate
int ftr = ftruncate(fd, 5);
//3mmap
char *p = (char *)mmap(NULL,5,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
ERROR_CHECK(p,MAP_FAILED,"mmap");//mmap失败返回不是NULL
for(int i = 0; i < 5; ++i){
printf("%c", *(p+i));
}
printf("\n");
*(p+4) = 'O';
//4munmap
munmap(p,5);
close(fd)
```

![image-20240127150710399](C:\Users\king\AppData\Roaming\Typora\typora-user-images\image-20240127150710399.png)

#### 文件流与文件描述符关系fileno

```c
int fileno(FILE* stream)//相当于接口
//输入一个文件流指针，返回一个文件描述符常常嵌套在函数中使用
write(fileno(fp),"hello",5);
```

![image-20240127151148106](C:\Users\king\AppData\Roaming\Typora\typora-user-images\image-20240127151148106.png)

#### 标准输入输出流与重定向

![image-20240127151254408](C:\Users\king\AppData\Roaming\Typora\typora-user-images\image-20240127151254408.png)

**可以通过改变文件描述符所对应的文件对象实现重定向**

```c
//常用重定向函数
#include <unistd.h>
int dup(int oldfd); //返回值为新的文件标识符
int dup2(int oldfd, int newfd);//使新的文件标识符指向旧标识符指向的对象
//多个描述符指向同一个文件对象，即共享机制
```

#### 管道

```c
$ mkfifo [管道名字]
使用cat打开管道可以打开管道的读端
$ cat [管道名字]
打开另一个终端，向管道当中输入内容可以实现写入内容
$ echo “string” > [管道名字]
此时读端也会显示内容
 //管道的打开与关闭与文件的打开关闭一致使用open/close，linux下管道也是文件
    
//若管道的写端先关闭，则之后管道的读端执行 read 操作时会立刻返回，且返回值为0；
//若管道的读端先关闭，则之后管道的写端执行 write 操作时会触发SIGPIPE信号，导致进程异常终止。
```

### I/O多路复用模型

#### 简陋的即时聊天代码

```c
//1号
int main(int argc, char *argv[])
{
ARGS_CHECK(argc,3);
int fdr = open(argv[1],O_RDONLY);//管道打开的时候，必须要先将读写端都打开之后才能继续
int fdw = open(argv[2],O_WRONLY);
printf("I am chat1\n");
char buf[128] = {0};
while(1)
{
memset(buf,0,sizeof(buf));
read(STDIN_FILENO, buf, sizeof(buf));
write(fdw, buf, strlen(buf)-1);
memset(buf,0,sizeof(buf));
read(fdr, buf, sizeof(buf));
printf("buf = %s\n", buf);
}
return 0;
}
//2号
int main(int argc, char *argv[])
{
ARGS_CHECK(argc,3);
int fdw = open(argv[1],O_WRONLY);//管道打开的时候，必须要先将读写端都打开之后才能继续
int fdr = open(argv[2],O_RDONLY);
printf("I am chat2\n");
char buf[128] = {0};
while(1)
{
memset(buf,0,sizeof(buf));
read(fdr, buf, sizeof(buf));
printf("buf = %s\n", buf);
memset(buf,0,sizeof(buf));
read(STDIN_FILENO, buf, sizeof(buf));
write(fdw, buf, strlen(buf)-1);
}
return 0;
}
```

**缺点：stdin和read会相互阻塞**

#### I/0多路复用模型与select

```c
#include <sys/select.h>
#include <sys/time.h>
//readset、writeset、exceptionset都是fd_set集合
//集合的相关操作如下：
void FD_ZERO(fd_set *fdset); /* 将所有fd清零 */
void FD_SET(int fd, fd_set *fdset); /* 增加一个fd */
void FD_CLR(int fd, fd_set *fdset); /* 删除一个fd */
int FD_ISSET(int fd, fd_set *fdset); /* 判断一个fd是否有设置 */
int select(int maxfd, fd_set *readset,fd_set *writeset, fd_set *exceptionset,
struct timeval * timeout);
```

#### 实现简单的即时聊天，说话时打印时间:

```c
//思路：每次说话时调用ctime打印当前时间，再用字符串拼接
#include <54func.h>
int main(int argc, char *argv[])
{
    // ./0_aqiang  1.pipe 2.pipe
    ARGS_CHECK(argc,3);
    int fdr = open(argv[1],O_RDONLY);
    ERROR_CHECK(fdr,-1,"open read");
    int fdw = open(argv[2],O_WRONLY);
    ERROR_CHECK(fdw,-1,"open write");
    printf("aqiang is connected!\n");
    char buf[4096];
    fd_set rdset; // 为fd_set申请内存
    while(1){
        // select之前需要重置监听集合
        FD_ZERO(&rdset); // fd_set 开始是监听集合
        FD_SET(STDIN_FILENO,&rdset);
        FD_SET(fdr,&rdset); //把fdr 和 stdin 加入监听
        select(fdr+1,&rdset,NULL,NULL,NULL);
        // select返回，说明fdr or stdin就绪了 rdset现在是就绪集合
        if(FD_ISSET(fdr,&rdset)){
            // 管道就绪 读取阿珍的消息
            memset(buf,0,sizeof(buf));
            ssize_t sret =read(fdr,buf,sizeof(buf));
            if(sret == 0){
                printf("Hehe\n");
                break;
            }
            printf("buf = %s\n", buf);
        }
        if(FD_ISSET(STDIN_FILENO,&rdset)){
            // 读取stdin的数据
            memset(buf,0,sizeof(buf));
            ssize_t sret = read(STDIN_FILENO,buf,sizeof(buf));
            if(sret == 0){
                write(fdw,"nishigehaoren",13);
                break;
            }
            time_t now = time(NULL);
            char *p = ctime(&now);
            char newbuf[8192] = {0};
            sprintf(newbuf,"%s %s",p,buf);
            write(fdw,newbuf,strlen(newbuf));
        }
    }
    return 0;
}

```

#### select的超时处理timeout

```c
struct timeval
{
long tv_sec;//秒
long tv_usec;//微秒
};
//用法
...
struct timeval timeout;
while(1)
{
bzero(&timeout, sizeof(timeout));
timeout.tv_sec = 3;
ret = select(fdr+1, &rdset, NULL, NULL, &timeout);
if(ret > 0)
{
...
}
else
{
printf("time out!\n");
}
}
```

#### 聊天做一个10s的超时处理:

```c
//思路:创建两个变量一个记录当前时间一个记录对方的最后活跃时间(每当select返回时更新当前时间，当管道就绪时更新对方活跃时间)，在循环的最后判断两者相减是否>= 10,满足则退出循环。
#include <54func.h>
int main(int argc, char *argv[])
{
    // ./0_azhen 1.pipe 2.pipe
    ARGS_CHECK(argc,3);
    int fdw = open(argv[1],O_WRONLY);
    ERROR_CHECK(fdw,-1,"open write");
    int fdr = open(argv[2],O_RDONLY);
    ERROR_CHECK(fdr,-1,"open read");
    printf("azhen is connected!\n");
    char buf[4096];
    fd_set rdset; // 为fd_set申请内存
    time_t curTime;
    time_t lastActive = time(NULL);
    while(1){
        // select之前需要重置监听集合
        FD_ZERO(&rdset); // fd_set 开始是监听集合
        FD_SET(STDIN_FILENO,&rdset);
        FD_SET(fdr,&rdset); //把fdr 和 stdin 加入监听
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int ret = select(fdr+1,&rdset,NULL,NULL,&timeout);
        curTime = time(NULL); // 更新当前时间
        printf("curTime = %s\n", ctime(&curTime));
        // select返回，说明fdr or stdin就绪了 rdset现在是就绪集合
        if(FD_ISSET(fdr,&rdset)){
            // 管道就绪 读取阿强的消息
            // 更新阿强的lastActive
            lastActive = time(NULL);
            memset(buf,0,sizeof(buf));
            ssize_t sret =read(fdr,buf,sizeof(buf));
            if(sret == 0){
                printf("Hehe\n");
                break;
            }
            printf("buf = %s\n", buf);
        }
        if(FD_ISSET(STDIN_FILENO,&rdset)){
            // 读取stdin的数据
            memset(buf,0,sizeof(buf));
            ssize_t sret = read(STDIN_FILENO,buf,sizeof(buf));
            if(sret == 0){
                write(fdw,"nishigehaoren",13);
                break;
            }
            write(fdw,buf,strlen(buf));
        }
        if(curTime - lastActive >= 10){
            printf("byebye!\n");
            break;
        }
    }
    close(fdr);
    close(fdw);
    return 0;
}

```

#### 写集合的原理(写阻塞:当数据在读缓存区与暂存区都满时，写缓冲区有数据时写便会阻塞)

实现一个进程在管道一直读写观察写阻塞:

```c
int main(int argc, char* argv[])
{
ARGS_CHECK(argc, 2);
int fdr = open(argv[1],O_RDWR);//非阻塞方式open管道的一端
int fdw = open(argv[1],O_RDWR);//可以一次性打开管道的读写端
fd_set rdset,wrset;
int ret;
char buf[128];
while(1)
{
FD_ZERO(&rdset);
FD_ZERO(&wrset);
FD_SET(fdr, &rdset);
FD_SET(fdw, &wrset);
ret = select(fdw+1, &rdset, &wrset, NULL, NULL);
if(FD_ISSET(fdr, &rdset))
{
bzero(buf, sizeof(buf));
read(fdr, buf, sizeof(buf));
puts(buf);
usleep(250000);
}
if(FD_ISSET(fdw, &wrset))
{
write(fdw,"helloworld", 10);
usleep(500000);
}
}
}
```

#### 编写程序A和B。A负责将文件的名字、长度和内容通过管道发送B，B需要新建一个目录，并将该文件存储起来。(小火车协议)

```c
//程序A
#include <54func.h>
typedef struct train_s {
    int length;
    char data[1000];
} train_t;
int main(int argc, char *argv[])
{
    // ./2_A 1.pipe file
    ARGS_CHECK(argc,3);
    int fdw_pipe = open(argv[1],O_WRONLY);
    ERROR_CHECK(fdw_pipe,-1,"open");
    
    train_t train;
    train.length = strlen(argv[2]);
    memcpy(train.data,argv[2],train.length);
    // 发送文件名
    write(fdw_pipe,&train.length,sizeof(train.length));
    write(fdw_pipe,train.data,train.length);

    // 发送文件内容
    int fdr_file = open(argv[2],O_RDONLY);
    ERROR_CHECK(fdr_file,-1,"open");
    ssize_t sret = read(fdr_file,train.data,sizeof(train.data));
    train.length = sret;
    write(fdw_pipe,&train.length,sizeof(train.length));
    write(fdw_pipe,train.data,train.length);

    close(fdr_file);
    close(fdw_pipe);
    return 0;
}

```

```c
//程序B
#include <54func.h>
typedef struct train_s {
    int length;
    char data[1000];
} train_t;
int main(int argc, char *argv[])
{
    // ./2_B 1.pipe
    ARGS_CHECK(argc,2);
    int fdr_pipe = open(argv[1],O_RDONLY);
    ERROR_CHECK(fdr_pipe,-1,"open");

    train_t train;
    read(fdr_pipe,&train.length,sizeof(train.length));
    read(fdr_pipe,train.data,train.length);
    char filename[4096] = {0};
    memcpy(filename,train.data,train.length);
    char path[8192] = {0};
    char dir[] = "dir1/";
    mkdir(dir,0777);
    sprintf(path,"%s%s%s",dir,"/",filename);
    int fdw_file = open(path,O_RDWR|O_CREAT|O_TRUNC,0666);
    ERROR_CHECK(fdw_file,-1,"open");
    read(fdr_pipe,&train.length,sizeof(train.length));
    read(fdr_pipe,train.data,train.length);
    write(fdw_file,train.data,train.length);
    
    close(fdw_file);
    close(fdr_pipe);
    return 0;
}

```

