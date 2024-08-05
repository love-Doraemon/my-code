/*
 *  程序名：fileclient4.cpp，
 *  把待传输的文件内容发送给服务端，并接收服务端的确认。
 *  作者：吴从周。
*/
#include "_public.h"

struct st_arg
{
  int  clienttype;          // 客户端类型，1-发送文件；2-接收文件。
  char ip[31];              // 服务器端的IP地址。
  int  port;                // 服务器端的端口。
  int  ptype;               // 文件发送成功后文件的处理方式：1-删除文件；2-移动到备份目录。
  char clientpath[301];     // 本地文件存放的根目录。
  char clientpathbak[301];  // 文件成功发送后，本地文件备份的根目录，当ptype==2时有效。
  bool andchild;            // 是否发送clientpath目录下各级子目录的文件，true-是；false-否。
  char matchname[301];      // 待发送文件名的匹配方式，如"*.TXT,*.XML"，注意用大写。
  char srvpath[301];        // 服务端文件存放的根目录。
  int  timetvl;             // 扫描本地目录文件的时间间隔，单位：秒。
} starg;


// 显示程序的帮助
void _help(char *argv[]);

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer);

CLogFile logfile;
 
char strRecvBuffer[1024];  // 发送报文的buffer。
char strSendBuffer[1024];  // 接收报文的buffer。 
CTcpClient TcpClient;      // 创建客户端的对象。

// 发送文件的主函数。
bool _tcpputfiles();

void EXIT(int sig);

// 登录服务器
bool Login(const char *argv);

// 向服务端发送心跳报文
bool ActiveTest();

// 把文件的内容发送给对端。
bool SendFile(const int sockfd,const char *filename,const char *mtime,const int filesize);

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

  while (true)
  {
    if (ActiveTest()==false) break;

    // 发送文件的主函数。
    if (_tcpputfiles()==false) { logfile.Write("_tcpputfiles() failed.\n"); EXIT(-1); }

    break;

    sleep(starg.timetvl);
  }

  // 程序直接退出，析构函数会释放资源。
}

// 显示程序的帮助
void _help(char *argv[])
{
  printf("\n");
  printf("Using:/project/tools/bin/fileclient4 logfilename xmlbuffer\n\n");

  printf("Sample:/project/tools/bin/fileclient4 /tmp/fileclient4.log \"<clienttype>1</clienttype><ip>172.21.0.3</ip><port>5005</port><ptype>1</ptype><clientpath>/tmp/tcp/surfdata1</clientpath><clientpathbak>/tmp/tcp/surfdata1bak</clientpathbak><andchild>true</andchild><matchname>*.XML,*.CSV</matchname><srvpath>/tmp/tcp/surfdata2</srvpath><timetvl>10</timetvl>\"\n");
  printf("       /project/tools/bin/fileclient4 /tmp/fileclient4.log \"<clienttype>1</clienttype><ip>49.232.185.49</ip><port>5005</port><ptype>1</ptype><clientpath>/tmp/tcp/surfdata1</clientpath><clientpathbak>/tmp/tcp/surfdata1bak</clientpathbak><andchild>true</andchild><matchname>*.XML,*.CSV</matchname><srvpath>/tmp/tcp/surfdata2</srvpath><timetvl>10</timetvl>\"\n\n\n");

  printf("本程序是数据中心的公共功能模块，采用tcp协议把文件发送给服务端。\n");
  printf("logfilename   本程序运行的日志文件。\n");
  printf("xmlbuffer     本程序运行的参数，如下：\n");
  printf("clienttype    客户端类型，1-发送文件；2-接收文件。\n");
  printf("ip            服务器端的IP地址。\n");
  printf("port          服务器端的端口。\n");
  printf("ptype         文件发送成功后的处理方式：1-删除文件；2-移动到备份目录。\n");
  printf("clientpath    本地文件存放的根目录。\n");
  printf("clientpathbak 文件成功发送后，本地文件备份的根目录，当ptype==2时有效。\n");
  printf("andchild      是否发送clientpath目录下各级子目录的文件，true-是；false-否，缺省为false。\n");
  printf("matchname     待发送文件名的匹配方式，如\"*.TXT,*.XML\"，注意用大写。\n");
  printf("srvpath       服务端文件存放的根目录。\n");
  printf("timetvl       扫描本地目录文件的时间间隔，单位：秒，取值在1-50之间。\n\n\n");
}

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer)
{
  memset(&starg,0,sizeof(struct st_arg));

  GetXMLBuffer(strxmlbuffer,"clienttype",&starg.clienttype);
  if ((starg.clienttype!=1)&&(starg.clienttype!=2)) { logfile.Write("clienttype not in (1,2).\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"ip",starg.ip);
  if (strlen(starg.ip)==0) { logfile.Write("ip is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"port",&starg.port);
  if ( starg.port==0) { logfile.Write("port is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"ptype",&starg.ptype);
  if ((starg.ptype!=1)&&(starg.ptype!=2)) { logfile.Write("ptype not in (1,2).\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"clientpath",starg.clientpath);
  if (strlen(starg.clientpath)==0) { logfile.Write("clientpath is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"clientpathbak",starg.clientpathbak);
  if ((starg.ptype==2)&&(strlen(starg.clientpathbak)==0)) { logfile.Write("clientpathbak is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"andchild",&starg.andchild);

  GetXMLBuffer(strxmlbuffer,"matchname",starg.matchname);
  if (strlen(starg.matchname)==0) { logfile.Write("matchname is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"srvpath",starg.srvpath);
  if (strlen(starg.srvpath)==0) { logfile.Write("srvpath is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"timetvl",&starg.timetvl);
  if (starg.timetvl==0) { logfile.Write("timetvl is null.\n"); return false; }

  if (starg.timetvl>50) starg.timetvl=50;

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

  strcpy(strSendBuffer,argv); 

  logfile.Write("strSendBuffer=%s\n",strSendBuffer);  // xxxxxx
  if (TcpClient.Write(strSendBuffer) == false)
  {
    logfile.Write("1 TcpClient.Write() failed.\n"); return false;
  }

  if (TcpClient.Read(strRecvBuffer,20) == false)
  {
    logfile.Write("1 TcpClient.Read() failed.\n"); return false;
  }
  logfile.Write("strRecvBuffer=%s\n",strRecvBuffer);  // xxxxxx

  logfile.Write("login(%s,%d) ok.\n",starg.ip,starg.port);

  return true;
}

// 向服务端发送心跳报文
bool ActiveTest()
{
  memset(strRecvBuffer,0,sizeof(strRecvBuffer));
  memset(strSendBuffer,0,sizeof(strSendBuffer));

  strcpy(strSendBuffer,"<activetest>ok</activetest>");

  logfile.Write("strSendBuffer=%s\n",strSendBuffer);  // xxxxxx
  if (TcpClient.Write(strSendBuffer) == false)
  {
    logfile.Write("2 TcpClient.Write() failed.\n"); TcpClient.Close(); return false;
  }

  if (TcpClient.Read(strRecvBuffer,20) == false)
  {
    logfile.Write("2 TcpClient.Read() failed.\n"); TcpClient.Close(); return false;
  }
  logfile.Write("strRecvBuffer=%s\n",strRecvBuffer);  // xxxxxx

  if (strcmp(strRecvBuffer,"ok") != 0) { TcpClient.Close(); return false; }

  return true;
}

// 把文件的内容发送给对端。
bool SendFile(const int sockfd,const char *filename,const char *mtime,const int filesize)
{
  int  bytes=0;
  int  total_bytes=0;
  int  onread=0;
  char buffer[1000];

  FILE *fp=NULL;

  if ( (fp=FOPEN(filename,"rb")) == NULL ) return false;

  while (true)
  {
    memset(buffer,0,sizeof(buffer));

    if ((filesize-total_bytes) > 1000) onread=1000;
    else onread=filesize-total_bytes;

    bytes=fread(buffer,1,onread,fp);

    if (bytes > 0)
    {
      if (Writen(sockfd,buffer,bytes) == false) { fclose(fp); fp=NULL; return false; }
    }

    total_bytes = total_bytes + bytes;

    if ((int)total_bytes == filesize) break;
  }

  fclose(fp);

  return true;
}

// 发送文件的主函数。
bool _tcpputfiles()
{
  CDir Dir;

  if (Dir.OpenDir(starg.clientpath,starg.matchname,10000,starg.andchild,false)==false)
  {
    logfile.Write("Dir.OpenDir(%s) 失败。\n",starg.clientpath); return false;
  }

  while (true)
  {
    // 遍历目录中的每个文件。
    if (Dir.ReadDir()==false) break;

    logfile.Write("filename=%s,mtime=%s,size=%d\n",Dir.m_FullFileName,Dir.m_ModifyTime,Dir.m_FileSize);

    // 把文件名、修改时间、文件大小组成报文，发送给对端。
    SNPRINTF(strSendBuffer,1000,1000,"<filename>%s</filename><mtime>%s</mtime><size>%d</size>",Dir.m_FullFileName,Dir.m_ModifyTime,Dir.m_FileSize);

    logfile.Write("3 strSendBuffer=%s\n",strSendBuffer);  // xxxxxx
    if (TcpClient.Write(strSendBuffer) == false)
    {
      logfile.Write("3 TcpClient.Write() failed.\n"); TcpClient.Close(); return false;
    }

    // 把文件的内容发送给对端。
    if (SendFile(TcpClient.m_connfd,Dir.m_FullFileName,Dir.m_ModifyTime,Dir.m_FileSize)==false)
    {
      logfile.Write("SendFile() failed.\n"); TcpClient.Close(); return false;
    }

    // 接收对端返回的结果。
    memset(strRecvBuffer,0,sizeof(strRecvBuffer));
    if (TcpClient.Read(strRecvBuffer,2000) == false)
    {
      logfile.Write("3 TcpClient.Read() failed.\n"); TcpClient.Close(); return false;
    }
    logfile.Write("3 strRecvBuffer=%s\n",strRecvBuffer);  // xxxxxx
  }

  return true;
}
