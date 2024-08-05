/*
 *  程序名：syncupdate3.cpp，本程序是数据中心的公共功能模块，采用刷新的方法同步mysql数据库之间的表。
 *  实现分批同步的功能。
    增加一个conn1。
    为了验证程序的正确性，修改了T_ZHOBTCODE3表的数据结构，故意让字段名不同。
 *  作者：吴从周。
*/
#include "_public.h"
#include "_mysql.h"

struct st_arg
{
  char connstr[101];     // 数据库的连接参数。
  char charset[51];      // 数据库的字符集。
  char srctname[31];     // 数据源表名，一般为远程连接表。
  char dsttname[31];     // 接收数据的目的表名。
  char srccols[1001];    // 数据源表的字段列表。
  char dstcols[1001];    // 接收数据的目的表的字段列表。
  char where[1001];      // 同步数据的条件。
  int  synctype;         // 同步方式：1-不分批同步；2-分批同步。
  char srckeycol[31];    // 数据源表的键值字段名。
  char dstkeycol[31];    // 接收数据的目的表的键值字段名。
  int  maxcount;         // 每批执行一次同步操作的记录数。
  int  timeout;          // 本程序运行时的超时时间。
  char pname[51];        // 本程序运行时的程序名。
} starg;

// 把xml解析到参数starg结构中

// 显示程序的帮助
void _help(char *argv[]);

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer);

CLogFile logfile;

connection conn,conn1;

// 业务处理主函数。
bool _syncupdate();
 
void EXIT(int sig);

int main(int argc,char *argv[])
{
  if (argc!=3) { _help(argv); return -1; }

  // 关闭全部的信号和输入输出
  //CloseIOAndSignal();

  // 处理程序退出的信号
  //signal(SIGINT,EXIT); signal(SIGTERM,EXIT);

  if (logfile.Open(argv[1],"a+")==false)
  {
    printf("打开日志文件失败（%s）。\n",argv[1]); return -1;
  }

  // 把xml解析到参数starg结构中
  if (_xmltoarg(argv[2])==false) return -1;

  if (conn.connecttodb(starg.connstr,starg.charset) != 0)
  {
    printf("connect database(%s) failed.\n%s\n",starg.connstr,conn.m_cda.message); EXIT(-1);
  }

  if (conn1.connecttodb(starg.connstr,starg.charset) != 0)
  {
    printf("connect database(%s) failed.\n%s\n",starg.connstr,conn1.m_cda.message); EXIT(-1);
  }

  // logfile.Write("connect database(%s) ok.\n",starg.connstr);

  // 业务处理主函数。
  _syncupdate();
}

// 显示程序的帮助
void _help(char *argv[])
{
  printf("Using:/project/tools/bin/syncupdate3 logfilename xmlbuffer\n\n");

  printf("Sample:/project/tools/bin/procctl 10 /project/tools/bin/syncupdate3 /log/idc/syncupdate3_ZHOBTCODE2.log \"<connstr>127.0.0.1,root,mysqlpwd,mysql,3306</connstr><charset>gbk</charset><srctname>LK_ZHOBTCODE1</srctname><dsttname>T_ZHOBTCODE2</dsttname><srccols>obtid,cityname,provname,lat,lon,height+1,upttime,keyid</srccols><dstcols>obtid,cityname,provname,lat,lon,height,upttime,keyid</dstcols><where>where obtid like '59%%%%'</where><synctype>1</synctype><timeout>50</timeout><pname>syncupdate3_ZHOBTCODE2</pname>\"\n\n");

  printf("       /project/tools/bin/procctl 10 /project/tools/bin/syncupdate3 /log/idc/syncupdate3_ZHOBTCODE3.log \"<connstr>127.0.0.1,root,mysqlpwd,mysql,3306</connstr><charset>gbk</charset><srctname>LK_ZHOBTCODE1</srctname><dsttname>T_ZHOBTCODE3</dsttname><srccols>obtid,cityname,provname,lat,height+1,upttime,keyid</srccols><dstcols>obtid,cityname,provname,wd,height,upttime,id</dstcols><where>where obtid like '59%%%%'</where><synctype>2</synctype><srckeycol>keyid</srckeycol><dstkeycol>id</dstkeycol><maxcount>10</maxcount><timeout>50</timeout><pname>syncupdate3_ZHOBTCODE3</pname>\"\n\n");
  printf("       /project/tools/bin/procctl 10 /project/tools/bin/syncupdate3 /log/idc/syncupdate3_ZHOBTCODE3.log \"<connstr>127.0.0.1,root,mysqlpwd,mysql,3306</connstr><charset>gbk</charset><srctname>LK_ZHOBTCODE1</srctname><dsttname>T_ZHOBTCODE3</dsttname><srccols>obtid,cityname,provname,lat,height+1,upttime,keyid</srccols><dstcols>obtid,cityname,provname,wd,height,upttime,id</dstcols><where>where obtid in ('59849','59855','59948','59954','59981')</where><synctype>2</synctype><srckeycol>keyid</srckeycol><dstkeycol>id</dstkeycol><maxcount>5</maxcount><timeout>50</timeout><pname>syncupdate3_ZHOBTCODE3</pname>\"\n\n");

  printf("本程序是数据中心的公共功能模块，采用刷新的方法同步mysql数据库之间的表。\n");

  printf("logfilename 本程序运行的日志文件。\n");
  printf("xmlbuffer   本程序运行的参数，用xml表示，具体如下：\n\n");

  printf("connstr     数据库的连接参数，格式：ip,username,password,dbname,port。\n");
  printf("charset     数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况。\n");

  printf("srctname    数据源表名，一般为远程连接表。\n");
  printf("dsttname    接收数据的目的表名。\n");

  printf("srccols     数据源表的字段列表，用于填充在select和from之间，所以，srccols可以是真实的字段，也可以是函数的返回值或者运算结果。\n");
  printf("dstcols     接收数据的目的表的字段列表，与srccols不同，它必须是真实存在的字段。\n");

  printf("where       同步数据的条件，即select语句的where部分，可以为空。\n");

  printf("synctype    同步方式：1-不分批同步；2-分批同步。当synctype==2时，srckeycol、dstkeycol和maxcount才有意义。\n");

  printf("srckeycol   数据源表的键值字段名，必须是唯一的索引。\n");
  printf("dstkeycol   接收数据的目的表的键值字段名，必须是唯一的索引。\n");

  printf("maxcount    每批执行一次同步操作的记录数，不能超过MAXPARAMS(256)。\n");

  printf("timeout     本程序的超时时间，单位：秒，视数据量的大小而定，建议设置30以上。\n");
  printf("pname       本程序运行时的进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n");
}

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer)
{
  memset(&starg,0,sizeof(struct st_arg));

  GetXMLBuffer(strxmlbuffer,"connstr",starg.connstr,100);
  if (strlen(starg.connstr)==0) { logfile.Write("connstr is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"charset",starg.charset,50);
  if (strlen(starg.charset)==0) { logfile.Write("charset is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"srctname",starg.srctname,30);
  if (strlen(starg.srctname)==0) { logfile.Write("srctname is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"dsttname",starg.dsttname,30);
  if (strlen(starg.dsttname)==0) { logfile.Write("dsttname is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"srccols",starg.srccols,1000);
  if (strlen(starg.srccols)==0) { logfile.Write("srccols is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"dstcols",starg.dstcols,1000);
  if (strlen(starg.dstcols)==0) { logfile.Write("dstcols is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"where",starg.where,1000);

  GetXMLBuffer(strxmlbuffer,"synctype",&starg.synctype);
  if ( (starg.synctype!=1) && (starg.synctype!=2) ) { logfile.Write("synctype is not in (1,2).\n"); return false; }

  if (starg.synctype==2)
  {
    GetXMLBuffer(strxmlbuffer,"srckeycol",starg.srckeycol,30);
    if (strlen(starg.srckeycol)==0) { logfile.Write("srckeycol is null.\n"); return false; }

    GetXMLBuffer(strxmlbuffer,"dstkeycol",starg.dstkeycol,30);
    if (strlen(starg.dstkeycol)==0) { logfile.Write("dstkeycol is null.\n"); return false; }

    GetXMLBuffer(strxmlbuffer,"maxcount",&starg.maxcount);
    if (starg.maxcount==0) { logfile.Write("maxcount is null.\n"); return false; }
    if (starg.maxcount>MAXPARAMS) starg.maxcount=MAXPARAMS;
  }

  GetXMLBuffer(strxmlbuffer,"timeout",&starg.timeout);
  if (starg.timeout==0) { logfile.Write("timeout is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"pname",starg.pname,50);
  if (strlen(starg.pname)==0) { logfile.Write("pname is null.\n"); return false; }

  return true;
}

void EXIT(int sig)
{
  logfile.Write("程序退出，sig=%d\n\n",sig);

  conn.disconnect();

  exit(0);
}

/*
create table LK_ZHOBTCODE1
(
   obtid                varchar(10) not null comment '站点代码',
   cityname             varchar(30) not null comment '城市名称',
   provname             varchar(30) not null comment '省名称',
   lat                  int not null comment '纬度，单位：0.01度。',
   lon                  int not null comment '经度，单位：0.01度。',
   height               int not null comment '海拔高度，单位：0.1米。',
   upttime              timestamp not null comment '更新时间。',
   keyid                int not null auto_increment comment '记录编号，自动增长列。',
   primary key (obtid),
   unique key LK_ZHOBTCODE1_KEYID (keyid)
) ENGINE=FEDERATED CONNECTION='mysql://root:mysqlpwd@172.21.0.3:4924/mysql/T_ZHOBTCODE1';

create table LK_ZHOBTMIND1
(
   obtid                varchar(10) not null comment '站点代码。',
   ddatetime            datetime not null comment '数据时间，精确到分钟。',
   t                    int comment '湿度，单位：0.1摄氏度。',
   p                    int comment '气压，单位：0.1百帕。',
   u                    int comment '相对湿度，0-100之间的值。',
   wd                   int comment '风向，0-360之间的值。',
   wf                   int comment '风速：单位0.1m/s。',
   r                    int comment '降雨量：0.1mm。',
   vis                  int comment '能见度：0.1米。',
   upttime              timestamp not null comment '更新时间。',
   keyid                bigint not null auto_increment comment '记录编号，自动增长列。',
   primary key (obtid, ddatetime),
   unique key LK_ZHOBTMIND1_KEYID (keyid)
)ENGINE=FEDERATED CONNECTION='mysql://root:mysqlpwd@172.21.0.3:4924/mysql/T_ZHOBTMIND1';

*/

// 业务处理主函数。
bool _syncupdate()
{
  CTimer Timer;

  sqlstatement stmtdel(&conn);
  sqlstatement stmtins(&conn);

  // 如果是不分批同步，表示需要同步的数据量比较少，执行一次SQL语句就可以搞定。
  if (starg.synctype==1)
  {
    // 先删除目的表中满足where条件的记录。
    stmtdel.prepare("delete from %s %s",starg.dsttname,starg.where);
    if (stmtdel.execute()!=0)
    {
      logfile.Write("stmtdel.execute() failed.\n%s\n%s\n",stmtdel.m_sql,stmtdel.m_cda.message); return false;
    }

    // 再把源表中满足where条件的记录插入到目的表中。
    stmtins.prepare("insert into %s(%s) select %s from %s %s",starg.dsttname,starg.dstcols,starg.srccols,starg.srctname,starg.where);
    if (stmtins.execute()!=0)
    {
      logfile.Write("stmtins.execute() failed.\n%s\n%s\n",stmtins.m_sql,stmtins.m_cda.message); 
      conn.rollback(); // 如果这里失败了，一定要回滚事务。
      return false;
    }

    logfile.Write("sync %s to %s(%d rows) in %.2fsec.\n",starg.srctname,starg.dsttname,stmtins.m_cda.rpc,Timer.Elapsed());

    conn.commit();

    return true;
  }

  // 以下是分批同步的流程。
  // 如果表的数据量比较大，采用分批同步的方案，避免数据库产生大事务和长事务。

  char srckeyvalue[51];  // 从数据源表查到的需要同步记录的key字段的值。
  // stmtsel用的是conn1，如果用conn，stmtdel和stmtins在prepare的时候会出现以下错误。
  // 2014,Commands out of sync; you can't run this command now
  sqlstatement stmtsel(&conn1);
  stmtsel.prepare("select %s from %s %s",starg.srckeycol,starg.srctname,starg.where);
  stmtsel.bindout(1,srckeyvalue,50);
  if (stmtsel.execute()!=0)
  {
    logfile.Write("stmtsel.execute() failed.\n%s\n%s\n",stmtsel.m_sql,stmtsel.m_cda.message); return false;
  }

  // 拼接绑定同步记录key的字符串。
  char bindstr[2001];  // 绑定同步记录key的字符串。
  char strtemp[11];

  memset(bindstr,0,sizeof(bindstr));

  for (int ii=0; ii<starg.maxcount; ii++)
  {
    memset(strtemp,0,sizeof(strtemp));
    sprintf(strtemp,":%lu,",ii+1);
    strcat(bindstr,strtemp);
  }

  bindstr[strlen(bindstr)-1]=0;   // 最后一个逗号是多余的。

  char pkcolvalue[starg.maxcount][51];  // 存放key字段的值。

  // 准备删除目的表数据的sql，一次插入starg.maxcount条记录。
  stmtdel.prepare("delete from %s where %s in (%s)",starg.dsttname,starg.dstkeycol,bindstr);
  for (int ii=0; ii<starg.maxcount; ii++)
  {
    stmtdel.bindin(ii+1,pkcolvalue[ii],50);
  }

  // 准备插入目的表数据的sql，一次插入starg.maxcount条记录。
  stmtins.prepare("insert into %s(%s) select %s from %s where %s in (%s)",starg.dsttname,starg.dstcols,starg.srccols,starg.srctname,starg.srckeycol,bindstr);
  for (int ii=0; ii<starg.maxcount; ii++)
  {
    stmtins.bindin(ii+1,pkcolvalue[ii],50);
  }

  int ccount=0;
  
  memset(pkcolvalue,0,sizeof(pkcolvalue));

  while (true)
  {
    if (stmtsel.next() != 0) break;

    strncpy(pkcolvalue[ccount],srckeyvalue,50);

    ccount++;

    // 每starg.maxcount条记录执行一次同步语句。
    if (ccount == starg.maxcount)
    {
      // 从目的表中删除记录。
      if (stmtdel.execute() !=0)
      {
        // 执行从目的表中删除的操作一般不会有问题，如果报错，就一定是数据库的问题或sql语法的问题，流程不必继续。
        logfile.Write("stmtdel.execute() failed.\n%s\n%s\n",stmtdel.m_sql,stmtdel.m_cda.message); return false;
      }

      // 向目的表中插入记录。
      if (stmtins.execute() != 0)
      {
        // 不管报什么错，都写日志。
        logfile.Write("stmtins.execute() failed.\n%s\n%s\n",stmtins.m_sql,stmtins.m_cda.message); 

        // 如果不是键值冲突，就一定是数据库的问题或sql语法的问题，流程不必继续。
        if (stmtins.m_cda.rc != 1062) return false;
      }

      logfile.Write("sync %s to %s(%d rows) in %.2fsec.\n",starg.srctname,starg.dsttname,ccount,Timer.Elapsed());

      conn.commit();

      memset(pkcolvalue,0,sizeof(pkcolvalue));

      ccount=0;
    }
  }
  
  // 如果不足starg.maxcount条记录，再执行一次同步。
  if (ccount > 0)
  {
    // 从目的表中删除记录。
    if (stmtdel.execute() !=0)
    {
      // 执行从目的表中删除的操作一般不会有问题，如果报错，就一定是数据库的问题或sql语法的问题，流程不必继续。
      logfile.Write("stmtdel.execute() failed.\n%s\n%s\n",stmtdel.m_sql,stmtdel.m_cda.message); return false;
    }

    // 向目的表中插入记录。
    if (stmtins.execute() != 0)
    {
      // 不管报什么错，都写日志。
      logfile.Write("stmtins.execute() failed.\n%s\n%s\n",stmtins.m_sql,stmtins.m_cda.message);

      // 如果不是键值冲突，就一定是数据库的问题或sql语法的问题，流程不必继续。
      if (stmtins.m_cda.rc != 1062) return false;
    }

    logfile.Write("sync %s to %s(%d rows) in %.2fsec.\n",starg.srctname,starg.dsttname,ccount,Timer.Elapsed());

    conn.commit();
  }

  //logfile.Write("sync %s to %s(%d rows) in %.2fsec.\n",starg.srctname,starg.dsttname,stmtsel.m_cda.rpc,Timer.Elapsed());
  logfile.Write("sync %s to %s(%d rows) in %.2fsec.\n",starg.srctname,starg.dsttname,ccount,Timer.Elapsed());

  return true;
}
