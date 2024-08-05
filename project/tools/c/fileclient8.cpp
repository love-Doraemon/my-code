/*
 *  程序名：fileclient8.cpp，采用tcp协议，实现文件接收的客户端。
 *  作者：吴从周。
*/
#include "_public.h"

struct st_arg
{
  char ip[31];              // 服务器端的IP地址。
  int  port;                // 服务器端的端口。
  char clientpath[301];     // 本地文件存放的根目录。
  char srvpath[301];        // 服务端文件存放的根目录。
  int  ptype;               // 文件接收成功后服务端的理方式：1-删除文件；2-移动到备份目录。
  char srvpathbak[301];     // 文件成功接收后，服务端文件备份的根目录，当ptype==2时有效。
  bool andchild;            // 是否接收srvpath目录下各级子目录的文件，true-是；false-否。
  char matchname[301];      // 待接收文件名的匹配方式，如"*.TXT,*.XML"，注意用大写。
  int  timetvl;             // 扫描本地目录文件的时间间隔，单位：秒。
  int  timeout;             // 本程序的超时时间。
  char pname[51];           // 本程序的进程名。
} starg;

// 显示程序的帮助
void _help(char *argv[]);

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer);

CLogFile logfile;
 
char strRecvBuffer[1024];  // 发送报文的buffer。
char strSendBuffer[1024];  // 接收报文的buffer。 
CTcpClient TcpClient;      // 创建客户端的对象。

// 接收文件的主函数。
bool _tcpgetfiles();

void EXIT(int sig);

// 登录服务器
bool Login(const char *argv);

// 接收对端发送过来的文件内容。
bool RecvFile(const int sockfd,const char *filename,const char *mtime,const int filesize);

int main(int argc,char *argv[])
{
  if (argc!=3) { _help(argv); return -1; }

  // 关闭全部的信号和输入输出
  // CloseIOAndSignal();

  // 处理程序退出的信号
  signal(SIGINT,EXIT); signal(SIGTERM,EXIT);

  if (logfile.Open(argv[1],"a+")==false)
  {
    printf("打开日志文件失败（%s）。\n",argv[1]); return -1;
  }


  // 把xml解析到参数starg结构中
  if (_xmltoarg(argv[2])==false) return -1;

  if (TcpClient.ConnectToServer(starg.ip,starg.port)==false) // 向服务端发起连接请求。
  {
    logfile.Write("TcpClient.ConnectToServer(%s,%d) failed.\n",starg.ip,starg.port); EXIT(-1);
  }

  // 身份验证
  if (Login(argv[2])==false)
  {
    logfile.Write("Login() failed.\n"); EXIT(-1);
  }

  // 接收文件的主函数。
  _tcpgetfiles();

  // 程序直接退出，析构函数会释放资源。
}

// 显示程序的帮助
void _help(char *argv[])
{
  printf("\n");
  printf("Using:/project/tools/bin/fileclient8 logfilename xmlbuffer\n\n");

  printf("Sample:/project/tools/bin/procctl 20 /project/tools/bin/fileclient8 /tmp/fileclient8.log \"<ip>172.21.0.3</ip><port>5005</port><clientpath>/tmp/tcp/surfdata3</clientpath><srvpath>/tmp/tcp/surfdata4</srvpath><ptype>1</ptype><srvpathbak>/tmp/tcp/surfdata4bak</srvpathbak><andchild>true</andchild><matchname>*.XML,*.CSV</matchname><timetvl>10</timetvl><timeout>30</timeout><pname>fileclient8</pname>\"\n");
  printf("       /project/tools/bin/procctl 20 /project/tools/bin/fileclient8 /tmp/fileclient8.log \"<ip>172.21.0.3</ip><port>5005</port><clientpath>/tmp/tcp/surfdata3</clientpath><srvpath>/tmp/tcp/surfdata4</srvpath><ptype>2</ptype><srvpathbak>/tmp/tcp/surfdata4bak</srvpathbak><andchild>true</andchild><matchname>*.XML,*.CSV</matchname><timetvl>10</timetvl><timeout>30</timeout><pname>fileclient8</pname>\"\n");

  printf("本程序是数据中心的公共功能模块，采用tcp协议从服务端接收文件。\n");
  printf("logfilename   本程序运行的日志文件。\n");
  printf("xmlbuffer     本程序运行的参数，如下：\n");
  printf("ip            服务器端的IP地址。\n");
  printf("port          服务器端的端口。\n");
  printf("clientpath    本地文件存放的根目录。\n");
  printf("srvpath       服务端文件存放的根目录。\n");
  printf("ptype         文件接收成功后服务端的理方式：1-删除文件；2-移动到备份目录。\n");
  printf("srvpathbak    文件成功接收后，服务端文件备份的根目录，当ptype==2时有效。\n");
  printf("andchild      是否接收srvpath目录下各级子目录的文件，true-是；false-否，缺省为false。\n");
  printf("matchname     待接收文件名的匹配方式，如\"*.TXT,*.XML\"，注意用大写。\n");
  printf("timetvl       扫描本地目录文件的时间间隔，单位：秒，取值在1-50之间。\n");
  printf("timeout       本程序的超时时间，单位：秒，视文件大小和网络带宽而定。\n");
  printf("pname         进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n");
}

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer)
{
  memset(&starg,0,sizeof(struct st_arg));

  GetXMLBuffer(strxmlbuffer,"ip",starg.ip);
  if (strlen(starg.ip)==0) { logfile.Write("ip is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"port",&starg.port);
  if ( starg.port==0) { logfile.Write("port is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"clientpath",starg.clientpath);
  if (strlen(starg.clientpath)==0) { logfile.Write("clientpath is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"srvpath",starg.srvpath);
  if (strlen(starg.srvpath)==0) { logfile.Write("srvpath is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"ptype",&starg.ptype);
  if ((starg.ptype!=1)&&(starg.ptype!=2)) { logfile.Write("ptype not in (1,2).\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"srvpathbak",starg.srvpathbak);
  if ((starg.ptype==2)&&(strlen(starg.srvpathbak)==0)) { logfile.Write("srvpathbak is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"andchild",&starg.andchild);

  GetXMLBuffer(strxmlbuffer,"matchname",starg.matchname);
  if (strlen(starg.matchname)==0) { logfile.Write("matchname is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"timetvl",&starg.timetvl);
  if (starg.timetvl==0) { logfile.Write("timetvl is null.\n"); return false; }

  if (starg.timetvl>50) starg.timetvl=50;

  GetXMLBuffer(strxmlbuffer,"timeout",&starg.timeout);
  if (starg.timeout==0) { logfile.Write("timeout is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"pname",starg.pname,50);
  if (strlen(starg.pname)==0) { logfile.Write("pname is null.\n"); return false; }

  return true;
}

void EXIT(int sig)
{
  logfile.Write("程序退出，sig=%d\n\n",sig);

  TcpClient.Close();

  exit(0);
}

// 登录服务器
bool Login(const char *argv)
{
  memset(strRecvBuffer,0,sizeof(strRecvBuffer));
  memset(strSendBuffer,0,sizeof(strSendBuffer));

  // clienttype固定填2。
  SNPRINTF(strSendBuffer,1000,1000,"%s<clienttype>2</clienttype>",argv); 

  logfile.Write("strSendBuffer=%s\n",strSendBuffer);  // xxxxxx
  if (TcpClient.Write(strSendBuffer) == false)
  {
    logfile.Write("1 TcpClient.Write() failed.\n"); return false;
  }

  if (TcpClient.Read(strRecvBuffer,20) == false)
  {
    logfile.Write("1 TcpClient.Read() failed.\n"); return false;
  }
  // logfile.Write("strRecvBuffer=%s\n",strRecvBuffer);  // xxxxxx

  logfile.Write("login(%s,%d) ok.\n",starg.ip,starg.port);

  return true;
}

// 接收文件的主函数。
bool _tcpgetfiles()
{
  while (true)
  {
    memset(strRecvBuffer,0,sizeof(strRecvBuffer));
    memset(strSendBuffer,0,sizeof(strSendBuffer));

    if (TcpClient.Read(strRecvBuffer,80) == false)
    {
      logfile.Write("2 TcpClient.Read() failed.\n"); return false;
    }
    // logfile.Write("2 strRecvBuffer=%s\n",strRecvBuffer);  // xxxxxx

    // 处理心跳报文
    if (strstr(strRecvBuffer,"activetest")!=0)
    {
      strcpy(strSendBuffer,"ok");
      // logfile.Write("2 strSendBuffer=%s\n",strSendBuffer);  // xxxxxx
      if (TcpClient.Write(strSendBuffer) == false)
      {
        logfile.Write("2 TcpClient.Write() failed.\n"); return false;
      }

      continue;
    }

    // 如果报文是接收文件请求。
    if (strstr(strRecvBuffer,"<filename>")!=0)
    {
      // 这里将插入接收文件内容的代码。
      // 解析待接收的文件的时间和大小
      char serverfilename[301]; memset(serverfilename,0,sizeof(serverfilename));
      char mtime[21]; memset(mtime,0,sizeof(mtime));
      int  filesize=0;
      GetXMLBuffer(strRecvBuffer,"filename",serverfilename,300);
      GetXMLBuffer(strRecvBuffer,"mtime",mtime,20);
      GetXMLBuffer(strRecvBuffer,"size",&filesize);

      // 客户端和服务端文件的目录是不一样的，以下代码生成客户端的文件名。
      // 把文件名中的srvpath替换成clientpath，要小心第三个参数
      char clientfilename[301]; memset(clientfilename,0,sizeof(clientfilename));
      strcpy(clientfilename,serverfilename);
      UpdateStr(clientfilename,starg.srvpath,starg.clientpath,false);

      logfile.Write("recv %s(%d) ...",clientfilename,filesize);
      // 接收对端发送过来的文件内容。
      if (RecvFile(TcpClient.m_connfd,clientfilename,mtime,filesize)==true)
      {
        logfile.WriteEx("ok.\n");
        SNPRINTF(strSendBuffer,1000,1000,"<filename>%s</filename><result>ok</result>",serverfilename);
      }
      else
      {
        logfile.WriteEx("failed.\n");
        SNPRINTF(strSendBuffer,1000,1000,"<filename>%s</filename><result>failed</result>",serverfilename);
      }

      // 把接收结果返回给对端。
      // logfile.Write("3 strSendBuffer=%s\n",strSendBuffer);  // xxxxxx
      if (TcpClient.Write(strSendBuffer) == false)
      {
        logfile.Write("3 TcpClient.Write() failed.\n"); EXIT(-1);
      }

      continue;
    }
  }
}

// 接收对端发送过来的文件内容。
bool RecvFile(const int sockfd,const char *filename,const char *mtime,const int filesize)
{
  char strfilenametmp[301]; memset(strfilenametmp,0,sizeof(strfilenametmp));
  sprintf(strfilenametmp,"%s.tmp",filename);

  FILE *fp=NULL;

  if ( (fp=FOPEN(strfilenametmp,"wb")) ==NULL) return false;

  int  total_bytes=0;
  int  onread=0;
  char buffer[1000];

  while (true)
  {
    memset(buffer,0,sizeof(buffer));

    if ((filesize-total_bytes) > 1000) onread=1000;
    else onread=filesize-total_bytes;

    if (Readn(sockfd,buffer,onread) == false) { fclose(fp); fp=NULL; return false; }

    fwrite(buffer,1,onread,fp);

    total_bytes = total_bytes + onread;

    if ((int)total_bytes == filesize) break;
  }

  fclose(fp);

  // 重置文件的时间
  UTime(strfilenametmp,mtime);

  if (RENAME(strfilenametmp,filename)==false) return false;

  return true;
}

