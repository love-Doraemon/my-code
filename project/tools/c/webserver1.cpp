/*
 *  程序名：webserver1.cpp，数据服务总线服务。
 *  最简单的web服务器，收到客户请求的，返回html文件。
 *  作者：吴从周。
*/
#include "_public.h"
#include "_mysql.h"

void *pthmain(void *arg);  // 线程主函数。

vector<long> vpthid;       // 存放线程id的容器。

void mainexit(int sig);    // 信号2和15的处理函数。

void pthexit(void *arg);   // 线程清理函数。
 
CLogFile logfile;          // 服务程序的运行日志。
CTcpServer TcpServer;      // 创建服务端对象。

// 读取客户端的报文。
int ReadT(const int sockfd,char *buffer,const int size,const int itimeout);

// 把html文件的内容发送给客户端。
bool SendHtmlFile(const int sockfd,const char *filename);

struct st_arg
{
  char connstr[101];  // 数据库的连接参数。
  char charset[51];   // 数据库的字符集。
  int  port;          // web服务监听的端口。
  int  timeout;       // 线程超时时间。
} starg;

// 显示程序的帮助
void _help(char *argv[]);

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer);

int main(int argc,char *argv[])
{
  if (argc!=3) { _help(argv); return -1; }

  // 关闭全部的信号和输入输出
  CloseIOAndSignal();

  // 处理程序退出的信号
  signal(SIGINT,mainexit); signal(SIGTERM,mainexit);

  if (logfile.Open(argv[1],"a+")==false)
  {
    printf("打开日志文件失败（%s）。\n",argv[1]); return -1;
  }
 
  // 把xml解析到参数starg结构中
  if (_xmltoarg(argv[2])==false)  mainexit(-1);

  // 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程
  // 但请不要用 "kill -9 +进程号" 强行终止
  signal(SIGINT,mainexit); signal(SIGTERM,mainexit);
 
  if (TcpServer.InitServer(starg.port)==false) // 初始化TcpServer的通信端口。
  {
    logfile.Write("TcpServer.InitServer(%s) failed.\n",starg.port); return -1;
  }
 
  while (true)
  {
    if (TcpServer.Accept()==false)   // 等待客户端连接。
    {
      logfile.Write("TcpServer.Accept() failed.\n"); continue;
    }

    // logfile.Write("客户端(%s)已连接。\n",TcpServer.GetIP());

    pthread_t pthid;
    if (pthread_create(&pthid,NULL,pthmain,(void *)(long)TcpServer.m_connfd)!=0)
    { logfile.Write("pthread_create failed.\n"); mainexit(-1); }

    vpthid.push_back(pthid); // 把线程id保存到vpthid容器中。
  }

  return 0;
}

void *pthmain(void *arg)
{
  pthread_cleanup_push(pthexit,arg);  // 设置线程清理函数。

  pthread_detach(pthread_self());  // 分离线程。

  pthread_setcanceltype(PTHREAD_CANCEL_DISABLE,NULL);  // 设置取消方式为立即取消。

  int sockfd=(int)(long)arg;  // 与客户端的socket连接。

  int  ibuflen=0;
  char strbuffer[1024];

  while (true)
  {
    memset(strbuffer,0,sizeof(strbuffer));

    // 读取客户端的报文，如果超时或失败，线程退出。
    if (ReadT(sockfd,strbuffer,sizeof(strbuffer),starg.timeout)<=0) pthread_exit(0);

    // 如果不是GET请求报文不处理，线程退出。
    if (strncmp(strbuffer,"GET",3)!=0)  pthread_exit(0);

    logfile.Write("%s\n\n",strbuffer);

    // 先把响应报文头部发送给客户端。
    memset(strbuffer,0,sizeof(strbuffer));
    sprintf(strbuffer,\
           "HTTP/1.1 200 OK\r\n"\
           "Server: webserver\r\n"\
           "Content-Type: text/html;charset=utf-8\r\n"\
           "Content-Length: 108904\r\n\r\n");
    if (Writen(sockfd,strbuffer,strlen(strbuffer))== false) pthread_exit(0); 

    logfile.Write("%s",strbuffer);

    // 再把html文件的内容发送给客户端。
    SendHtmlFile(sockfd,"/project/tools/c/index.html");
  }
  
  pthread_cleanup_pop(1);

  pthread_exit(0);
}

// 信号2和15的处理函数。
void mainexit(int sig)
{
  // logfile.Write("mainexit begin.\n");

  // 关闭监听的socket。
  TcpServer.CloseListen();

  // 取消全部的线程。
  for (int ii=0;ii<vpthid.size();ii++)
  {
    logfile.Write("cancel %ld\n",vpthid[ii]);
    pthread_cancel(vpthid[ii]);
  }

  // logfile.Write("mainexit end.\n");

  exit(0);
}

// 线程清理函数。
void pthexit(void *arg)
{
  // logfile.Write("pthexit begin.\n");

  // 关闭与客户端的socket。
  close((int)(long)arg);

  // 从vpthid中删除本线程的id。
  for (int ii=0;ii<vpthid.size();ii++)
  {
    if (vpthid[ii]==pthread_self())
    {
      vpthid.erase(vpthid.begin()+ii);
    }
  }

 // logfile.Write("pthexit end.\n");
}


// 把html文件的内容发送给客户端。
bool SendHtmlFile(const int sockfd,const char *filename)
{
  int  bytes=0;
  char buffer[5000];

  FILE *fp=NULL;

  if ( (fp=FOPEN(filename,"rb")) == NULL ) return false;

  while (true)
  {
    memset(buffer,0,sizeof(buffer));

    if ((bytes=fread(buffer,1,5000,fp)) ==0) break;

    if (Writen(sockfd,buffer,bytes) == false) { fclose(fp); fp=NULL; return false; }
  }

  fclose(fp);

  return true;
}

// 显示程序的帮助
void _help(char *argv[])
{
  printf("Using:/project/tools/bin/webserver1 logfilename xmlbuffer\n\n");

  printf("Sample:/project/tools/bin/procctl 10 /project/tools/bin/webserver1 /log/idc/webserver1.log \"<connstr>127.0.0.1,root,mysqlpwd,mysql,3306</connstr><charset>utf8</charset><port>5008</port><timeout>5</timeout>\"\n\n");

  printf("本程序是数据总线的服务端程序，为数据中心提供http协议的数据访问接口。\n");
  printf("logfilename 本程序运行的日志文件。\n");
  printf("xmlbuffer   本程序运行的参数，用xml表示，具体如下：\n\n");

  printf("connstr     数据库的连接参数，格式：ip,username,password,dbname,port。\n");
  printf("charset     数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况。\n");
  printf("port        web服务监听的端口。\n");
  printf("timeout     线程超时的时间，单位：秒，建议取值小于10。\n\n");
}

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer)
{
  memset(&starg,0,sizeof(struct st_arg));

  GetXMLBuffer(strxmlbuffer,"connstr",starg.connstr,100);
  if (strlen(starg.connstr)==0) { logfile.Write("connstr is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"charset",starg.charset,50);
  if (strlen(starg.charset)==0) { logfile.Write("charset is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"port",&starg.port);
  if (starg.port==0) { logfile.Write("port is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"timeout",&starg.timeout);
  if (starg.timeout==0) { logfile.Write("timeout is null.\n"); return false; }

  return true;
}

// 读取客户端的报文。
int ReadT(const int sockfd,char *buffer,const int size,const int itimeout)
{
  if (itimeout > 0)
  {
    fd_set tmpfd;

    FD_ZERO(&tmpfd);
    FD_SET(sockfd,&tmpfd);

    struct timeval timeout;
    timeout.tv_sec = itimeout; timeout.tv_usec = 0;

    int iret;
    if ( (iret = select(sockfd+1,&tmpfd,0,0,&timeout)) <= 0 ) return iret;
  }

  return recv(sockfd,buffer,size,0);
}
