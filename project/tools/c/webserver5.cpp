/*
 *  程序名：webserver5.cpp，数据服务总线服务。
    增加了数据库重连代码。
    修改了条件变量的触发机制。
    增加了监控线程。
 *  作者：吴从周。
*/
#include "_public.h"
#include "_mysql.h"

void *pthmain(void *arg);   // 线程主函数。
void  pthkillfun(int sig);  // 线程收到SIGUSR1信号被终止的处理函数。

pthread_t pthcheckid;           // 监控线程的id。
void *pthcheckmain(void *arg);  // 监控线程主函数。

// 线程信息的结构体。
typedef struct st_pthinfo
{
  pthread_t  pthid;    // 线程编号。
  time_t     atime;    // 最近一次活动的时间。
}pthinfo;

vector<pthinfo> vpthinfo;  // 存放线程id的容器。

// 信号2和15的处理函数，该信号将终止整个服务程序。
void mainexit(int sig);    // 信号2和15的处理函数。

void pthexit(void *arg);   // 线程清理函数。
 
CLogFile logfile;          // 服务程序的运行日志。
CTcpServer TcpServer;      // 创建服务端对象。

// 读取客户端的报文。
int ReadT(const int sockfd,char *buffer,const int size,const int itimeout);

struct st_arg
{
  char connstr[101];  // 数据库的连接参数。
  char charset[51];   // 数据库的字符集。
  int  port;          // web服务监听的端口。
  int  timeout;       // 线程等待客户端报文的超时时间。
} starg;

// 显示程序的帮助
void _help(char *argv[]);

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer);

// 从GET请求中获取参数。
bool getvalue(const char *buffer,const char *name,char *value,const int len);
// 判断用户名和密码。
bool Login(connection *conn,const char *buffer,const int sockfd);
// 判断用户是否有调用接口的权限。
bool CheckPerm(connection *conn,const char *buffer,const int sockfd);
// 执行接口的SQL语句，把数据返回给客户端。
bool ExecSQL(connection *conn,const char *buffer,const int sockfd);

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;      // 初始化条件变量。
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;   // 初始化互斥锁。
vector<int> sockqueue;  // 客户端连接sock的队列。

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
  if (_xmltoarg(argv[2])==false) return -1;

  // 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程
  // 但请不要用 "kill -9 +进程号" 强行终止
  signal(SIGINT,mainexit); signal(SIGTERM,mainexit);
 
  if (TcpServer.InitServer(starg.port)==false) // 初始化TcpServer的通信端口。
  {
    logfile.Write("TcpServer.InitServer(%s) failed.\n",starg.port); return -1;
  }

  // 启动10个线程，线程数比cpu核数略多。
  for (int ii=0;ii<10;ii++)
  {
    pthinfo pth;
    if (pthread_create(&pth.pthid,NULL,pthmain,(void *)(long)ii)!=0)
    { logfile.Write("pthread_create failed.\n"); mainexit(-1); }

    pth.atime=time(0);  // 设置线程的活动时间。
    vpthinfo.push_back(pth); // 把线程id保存到vpthinfo容器中。
  }

  // 启动监控线程。
  if (pthread_create(&pthcheckid,NULL,pthcheckmain,NULL)!=0)
  { logfile.Write("pthread_create failed.\n"); mainexit(-1); }

  while (true)
  {
    if (TcpServer.Accept()==false)   // 等待客户端连接。
    {
      logfile.Write("TcpServer.Accept() failed.\n"); continue;
    }

    logfile.Write("客户端(%s)已连接。\n",TcpServer.GetIP());

    pthread_mutex_lock(&mutex);  // 加锁。

    sockqueue.push_back(TcpServer.m_connfd); // 把客户端的sock放入队列。

    pthread_mutex_unlock(&mutex); // 解锁。

    pthread_cond_signal(&cond);   // 触发条件，激活一个线程。
  }
}

void *pthmain(void *arg)
{
  int pthnum=(int)(long)arg;  // 线程的编号。

  pthread_detach(pthread_self());  // 分离线程。

  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);       // 把线程设置为可取消。
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);  // 取消方式为立即取消。

  connection conn; // 数据库连接。

  pthread_cleanup_push(pthexit,&conn);  // 设置线程清理函数。

  logfile.Write("phid=%ld(num=%d) ok.\n",pthread_self(),pthnum);

  signal(SIGUSR1,pthkillfun);  // 设置线程被终止的处理函数。

  int  sockfd;
  int  ibuflen=0;
  char strrecvbuf[1024],strsendbuf[1024];

  while (true)
  {
    pthread_mutex_lock(&mutex);  // 加锁。

    // 如果队列为空，等待。 
    while (sockqueue.size() == 0)
    {
      // 设置线程超时间为20秒。
      struct timeval now;
      struct timespec outtime;
      gettimeofday(&now, NULL);
      outtime.tv_sec=now.tv_sec+20;
      outtime.tv_nsec=now.tv_usec;

      pthread_cond_timedwait(&cond,&mutex,&outtime);  // 等待条件被触发。
      
      vpthinfo[pthnum].atime=time(0);  // 更新当前线程的活动时间。
    }

    // 从队列中获取第一条记录，然后删除该记录。
    sockfd=sockqueue[0];
    sockqueue.erase(sockqueue.begin());

    pthread_mutex_unlock(&mutex);  // 解锁。

    logfile.Write("phid=%ld(num=%d),sockfd=%d\n",pthread_self(),pthnum,sockfd);

    memset(strrecvbuf,0,sizeof(strrecvbuf));

    // 读取客户端的报文，如果超时或失败，关闭sockfd，会话结束。
    if (ReadT(sockfd,strrecvbuf,sizeof(strrecvbuf),starg.timeout)<=0) 
    { close(sockfd); continue; }

    // 如果不是GET请求报文不处理，关闭sockfd，会话结束。
    if (strncmp(strrecvbuf,"GET",3)!=0)  
    { close(sockfd); continue; }

    logfile.Write("%s\n\n",strrecvbuf);

    logfile.Write("conn.cda.rc=%ld\n",conn.m_cda.rc);

    // 如果发生了数据库连接的各种错误，就断开数据库连接。
    if ( (conn.m_cda.rc>=2000) && (conn.m_cda.rc<=2013) ) conn.disconnect(); 

    if (conn.m_state==0)  // 如果数据库未连接，就发起连接。
    {
      if (conn.connecttodb(starg.connstr,starg.charset) != 0)
      {
        logfile.Write("connect database(%s) failed.\n%s\n",starg.connstr,conn.m_cda.message); 
        // 这里可以考虑给客户端一个交待，回个报文，但也不是太有必要。
        close(sockfd); continue; 
      }
    }

    // 判断用户名和密码。
    if (Login(&conn,strrecvbuf,sockfd)==false) 
    { close(sockfd); continue; }

    // 判断用户是否有调用接口的权限。
    if (CheckPerm(&conn,strrecvbuf,sockfd)==false) 
    { close(sockfd); continue; }

    // 先把响应报文头部发送给客户端。
    memset(strsendbuf,0,sizeof(strsendbuf));
    sprintf(strsendbuf,\
           "HTTP/1.1 200 OK\r\n"\
           "Server: webserver\r\n"\
           "Content-Type: text/html;charset=gbk\r\n\r\n"\
           "<retcode>0</retcode><message>ok</message>\n");
    Writen(sockfd,strsendbuf,strlen(strsendbuf));

    // 再执行接口的SQL语句，把数据返回给客户端。
    ExecSQL(&conn,strrecvbuf,sockfd);

    close(sockfd);  // 会话结束。
  }

  pthread_cleanup_pop(1);

  pthread_exit(0);
}

// 信号2和15的处理函数，该信号将终止整个服务程序。
void mainexit(int sig)
{
  // logfile.Write("mainexit begin.\n");

  // 关闭监听的socket。
  TcpServer.CloseListen();

  // 取消监控线程。
  pthread_cancel(pthcheckid);

  // 取消全部的线程。
  for (int ii=0;ii<vpthinfo.size();ii++) 
  {
    pthread_cancel(vpthinfo[ii].pthid);
  }

  sleep(1); // 1秒后再销毁锁和条件变量，让线程有足够的时间退出。

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);

  // logfile.Write("mainexit end.\n");

  exit(0);
}

// 线程收到SIGUSR1信号被终止的处理函数。
void pthkillfun(int sig)
{
  logfile.Write("pthread(%ld) killed(sig=%d).\n",pthread_self(),sig);

  pthread_exit(0);  
}

// 线程清理函数。
void pthexit(void *arg)
{
  logfile.Write("pthexit begin.\n");

  // 关闭数据库。
  connection *conn=(connection *)arg;
  conn->disconnect();

  pthread_mutex_unlock(&mutex);

  /*
  A  condition  wait  (whether  timed  or  not)  is  a  cancellation  point. 
When the cancelability type of a thread is set to PTHREAD_CAN_CEL_DEFERRED, a side-effect of acting upon a cancellation request while in a condition wait is that the mutex is (in effect)  re-acquired before  calling the first cancellation cleanup handler. The effect is as if the thread were unblocked, allowed to execute up to the point of returning from the call to pthread_cond_timedwait() or pthread_cond_wait(), but at that point notices  the  cancellation  request  and instead  of  returning to the caller of pthread_cond_timedwait() or pthread_cond_wait(), starts the thread cancellation activities, which includes calling cancellation cleanup handlers.
  意思就是在pthread_cond_wait时执行pthread_cancel后，要先在线程清理函数中要先解锁已与相应条件变量绑定的mutex。这样是为了保证pthread_cond_wait可以返回到调用线程。
  */

  logfile.Write("pthexit end.\n");
}

// 显示程序的帮助
void _help(char *argv[])
{
  printf("Using:/project/tools/bin/webserver5 logfilename xmlbuffer\n\n");

  printf("Sample:/project/tools/bin/procctl 10 /project/tools/bin/webserver5 /log/idc/webserver5.log \"<connstr>127.0.0.1,root,mysqlpwd,mysql,3306</connstr><charset>gbk</charset><port>5008</port><timeout>5</timeout>\"\n\n");

  printf("本程序是数据总线的服务端程序，为数据中心提供http协议的数据访问接口。\n");
  printf("logfilename 本程序运行的日志文件。\n");
  printf("xmlbuffer   本程序运行的参数，用xml表示，具体如下：\n\n");

  printf("connstr     数据库的连接参数，格式：ip,username,password,dbname,port。\n");
  printf("charset     数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况。\n");
  printf("port        web服务监听的端口。\n");
  printf("timeout     线程等待客户端报文的超时时间，单位：秒，建议取值小于10。\n\n");
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
 
  // 强制timeout不能超过20秒，否则会引起线程超时。
  if (starg.timeout>20) starg.timeout=20;  

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

// 从GET请求中获取参数。
bool getvalue(const char *buffer,const char *name,char *value,const int len)
{
  value[0]=0;

  char *start,*end;
  start=end=0;

  start=strstr((char *)buffer,(char *)name);
  if (start==0) return false;

  end=strstr(start,"&");
  if (end==0) end=strstr(start," ");

  if (end==0) return false;

  int ilen=end-(start+strlen(name)+1);
  if (ilen>len) ilen=len;

  strncpy(value,start+strlen(name)+1,ilen);

  value[ilen]=0;

  return true;
}

// 判断用户名和密码。
bool Login(connection *conn,const char *buffer,const int sockfd)
{
  char username[31],passwd[31];
  
  getvalue(buffer,"username",username,30); // 获取用户名。
  getvalue(buffer,"passwd",passwd,30);     // 获取密码。

  sqlstatement stmt;
  stmt.connect(conn);
  stmt.prepare("select count(*) from T_USERINFO where username=:1 and passwd=:2 and rsts=1");
  stmt.bindin(1,username,30);
  stmt.bindin(2,passwd,30);
  int icount=0;
  stmt.bindout(1,&icount);
  stmt.execute();
  stmt.next();

  // logfile.Write("username=%s,passwd=%s,icount=%d\n",username,passwd,icount);

  if (icount!=1)
  {
    char strbuffer[256];
    memset(strbuffer,0,sizeof(strbuffer));
   
    sprintf(strbuffer,\
           "HTTP/1.1 200 OK\r\n"\
           "Server: webserver\r\n"\
           "Content-Type: text/html;charset=gbk\r\n\n\n"\
           "<retcode>-1</retcode><message>username or passwd is invailed</message>");
    Writen(sockfd,strbuffer,strlen(strbuffer));
    
    return false;
  }
  
  return true;
}

// 判断用户是否有调用接口的权限。
bool CheckPerm(connection *conn,const char *buffer,const int sockfd)
{
  char username[31],intername[30];
  
  getvalue(buffer,"username",username,30);    // 获取用户名。
  getvalue(buffer,"intername",intername,30);  // 获取接口名。

  sqlstatement stmt;
  stmt.connect(conn);
  stmt.prepare("select count(*) from T_USERANDINTER where username=:1 and intername=:2 and intername in (select intername from T_INTERCFG where rsts=1)");
  stmt.bindin(1,username,30);
  stmt.bindin(2,intername,30);
  int icount=0;
  stmt.bindout(1,&icount);
  stmt.execute();
  stmt.next();

  // logfile.Write("username=%s,intername=%s,icount=%d\n",username,intername,icount);

  if (icount!=1)
  {
    char strbuffer[256];
    memset(strbuffer,0,sizeof(strbuffer));
  
    sprintf(strbuffer,\
           "HTTP/1.1 200 OK\r\n"\
           "Server: webserver\r\n"\
           "Content-Type: text/html;charset=gbk\r\n\n\n"\
           "<retcode>-1</retcode><message>permission denied</message>");

    Writen(sockfd,strbuffer,strlen(strbuffer));
    
    return false;
  }
  
  return true;
}

// 执行接口的SQL语句，把数据返回给客户端。
bool ExecSQL(connection *conn,const char *buffer,const int sockfd)
{
  char username[31],intername[30],selectsql[1001],colstr[301],bindin[301];
  memset(username,0,sizeof(username));
  memset(intername,0,sizeof(intername));
  memset(selectsql,0,sizeof(selectsql)); // 接口SQL。
  memset(colstr,0,sizeof(colstr));       // 输出列名。
  memset(bindin,0,sizeof(bindin));       // 接口参数。

  getvalue(buffer,"username",username,30);    // 获取用户名。
  getvalue(buffer,"intername",intername,30);  // 获取接口名。
  
  // 把接口的参数取出来。
  sqlstatement stmt;
  stmt.connect(conn);
  stmt.prepare("select selectsql,colstr,bindin from T_INTERCFG where intername=:1");
  stmt.bindin(1,intername,30);     // 接口名。
  stmt.bindout(1,selectsql,1000);  // 接口SQL。
  stmt.bindout(2,colstr,300);      // 输出列名。
  stmt.bindout(3,bindin,300);      // 接口参数。
  stmt.execute();  // 这里基本上不用判断返回值，出错的可能几乎没有。
  stmt.next();

  // prepare接口的SQL语句。
  stmt.prepare(selectsql);
  
  CCmdStr CmdStr;

  ////////////////////////////////////////
  // 拆分输入参数bindin。
  CmdStr.SplitToCmd(bindin,",");

  // 用于存放输入参数的数组，输入参数的值不会太长，100足够。
  char invalue[CmdStr.CmdCount()][101];
  memset(invalue,0,sizeof(invalue));

  // 从http的GET请求报文中解析出输入参数，绑定到SQL中。
  for (int ii=0;ii<CmdStr.CmdCount();ii++)
  {
    getvalue(buffer,CmdStr.m_vCmdStr[ii].c_str(),invalue[ii],100); 
    stmt.bindin(ii+1,invalue[ii],100);
  }
  ////////////////////////////////////////

  ////////////////////////////////////////
  // 拆分colstr，可以得到结果集的字段数。
  CmdStr.SplitToCmd(colstr,",");

  // 用于存放结果集的数组。
  char colvalue[CmdStr.CmdCount()][2001];
  
  // 把结果集绑定到colvalue数组。
  for (int ii=0;ii<CmdStr.CmdCount();ii++)
  {
    stmt.bindout(ii+1,colvalue[ii],2000);
  }
  ////////////////////////////////////////

  if (stmt.execute() != 0)
  {
    logfile.Write("stmt.execute() failed.\n%s\n%s\n",stmt.m_sql,stmt.m_cda.message); return false;
  }

  // logfile.WriteEx("<data>\n");
  Writen(sockfd,"<data>\n",strlen("<data>\n"));

  char strsendbuffer[4001],strtemp[2001];

  // 逐行获取结果集，发送给客户端。
  while (true)
  {
    memset(strsendbuffer,0,sizeof(strsendbuffer));
    memset(colvalue,0,sizeof(colvalue));

    if (stmt.next() != 0) break;

    for (int ii=0;ii<CmdStr.CmdCount();ii++)
    {
      memset(strtemp,0,sizeof(strtemp));
      snprintf(strtemp,2000,"<%s>%s</%s>",CmdStr.m_vCmdStr[ii].c_str(),colvalue[ii],CmdStr.m_vCmdStr[ii].c_str());
      strcat(strsendbuffer,strtemp);
      // logfile.WriteEx("<%s>%s</%s>",CmdStr.m_vCmdStr[ii].c_str(),colvalue[ii],CmdStr.m_vCmdStr[ii].c_str());
    }

    // logfile.WriteEx("<endl/>\n");
    strcat(strsendbuffer,"<endl/>\n");
    Writen(sockfd,strsendbuffer,strlen(strsendbuffer));
  }

  //logfile.WriteEx("</data>\n");
  Writen(sockfd,"</data>\n",strlen("</data>\n"));

  logfile.Write("intername=%s,count=%d\n",intername,stmt.m_cda.rpc);
  
  return true;
}

// 监控线程主函数。
void *pthcheckmain(void *arg)  
{
  while (true)
  {
    time_t now=time(0);

    for (int ii=0;ii<vpthinfo.size();ii++) 
    {
      // 线程超时间为20秒，这里用30秒判断超时，足够。
      if ( (now-vpthinfo[ii].atime)>30)
      {
        logfile.Write("thread(%ld) timeout(%d).\n",vpthinfo[ii].pthid,now-vpthinfo[ii].atime);
        // 杀掉线程。
        pthread_kill(vpthinfo[ii].pthid,SIGUSR1);

        // 重启线程。
        if (pthread_create(&vpthinfo[ii].pthid,NULL,pthmain,(void *)(long)ii)!=0)
        { logfile.Write("pthread_create failed.\n"); mainexit(-1); }

        // 设置线程的活动时间。
        vpthinfo[ii].atime=time(0);
      }
    }

    sleep(10);
  }
}
