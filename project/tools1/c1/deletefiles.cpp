#include "_public.h"

// 程序退出和信号2、15的处理函数。
void EXIT(int sig);

int main(int argc,char *argv[])
{
  if (argc != 4)
  {
    printf("\n");
    printf("Using:/project/tools1/bin/deletefiles pathname matchstr timeout\n\n");

    printf("Example:/project/tools1/bin/deletefiles /idcdata/ftp/surfdata \"*\" 0.01\n");
    printf("        /project/tools1/bin/procctl 300 /project/tools1/bin/deletefiles /tmp/idc/surfdata \"*\" 0.01\n\n");

    printf("这是一个工具程序，用于清理历史的数据文件或日志文件。\n");
    printf("本程序把pathname目录及子目录中timeout天之前的matchstr文件全部清理，timeout可以是小数。\n");
    printf("本程序不写日志文件，也不会在控制台输出任何信息。\n\n\n");

    return -1;
  }

  // 关闭全部的信号和输入输出
  // 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程
  // 但请不要用 "kill -9 +进程号" 强行终止
  // CloseIOAndSignal();
  signal(SIGINT,EXIT); signal(SIGTERM,EXIT);

  char strTimeOut[21];

  LocalTime(strTimeOut,"yyyy-mm-dd hh24:mi:ss",0-(int)(atof(argv[3])*24*60*60));

  CDir Dir;

  // 打开目录，读取文件，包括它的子目录
  if (Dir.OpenDir(argv[1],argv[2],10000,true) == false)
  {
    printf("Dir.OpenDir(%s) failed.\n",argv[1]); exit(-1);
  }

  while (Dir.ReadDir() == true)
  {
    if (strcmp(Dir.m_ModifyTime,strTimeOut) < 0) 
    {
      if (REMOVE(Dir.m_FullFileName)==true)
        printf("delete %s ok.\n",Dir.m_FullFileName);
      else
        printf("delete %s failed.\n",Dir.m_FullFileName);
    }
  }

  return 0;
}

void EXIT(int sig)
{
  printf("程序退出，sig=%d\n\n",sig);

  exit(0);
}
