/*
 *  程序名：xmltodb12.cpp，本程序是数据中心的公共功能模块，用于把xml文件入库到mysql的表中。
 *  加上execsql
 *  在_xmltodb返回失败的位置加上回滚事务，让execsql回滚。
 *  加上PActive;
 *  程序可正式启用了。
 *  作者：吴从周。
*/
#include "_public.h"
#include "_mysql.h"

// 表的全部列的结构体。
struct st_columns
{
  char  colname[31];  // 列名。
  char  datatype[31]; // 列的数据类型，分为number、date和char三大类。
  int   collen;       // 列的长度，number固定20，date固定19，char的长度由表结构决定。
  int   pkseq;        // 如果列为主键，存放主键字段的顺序，不为主键取值0。
};

class CTABCOLS
{
public:
  CTABCOLS();
  
  int m_allcount;   // 全部字段的个数
  int m_pkcount;    // 主键字段的个数

  char m_allcols[3001];  // 全部的字段，以字符串存放，中间用半角的逗号分隔
  char m_pkcols[301];    // 全部的主键字段，以字符串存放，中间用半角的逗号分隔

  vector<struct st_columns> m_vallcols;  // 存放全部字段信息的容器
  vector<struct st_columns> m_vpkcols;   // 存放主键字段信息的容器

  void initdata();  // 成员变量初始化。

  // 获取指定表的全部字段信息。
  bool allcols(connection *conn,char *tablename);

  // 获取指定表的主键字段信息。
  bool pkcols(connection *conn,char *tablename);
};

CTABCOLS TABCOLS;

// 判断表的字段数不能超过MAXCOLCOUNT，字段的最大长度不能超过MAXCOLLEN。
bool checkallcols();

// 生成插入和更新表数据的SQL。
char strinsertsql[10241];
char strupdatesql[10241];
void crtsql();

// prepare插入和更新的sql语句，绑定输入变量。
#define MAXCOLCOUNT  100  // 每个表字段的最大数。
#define MAXCOLLEN    500  // 表字段值的最大长度。
char strcolvalue[MAXCOLCOUNT][MAXCOLLEN+1]; // 存放从xml每一行中解析出来的值。
sqlstatement stmtins,stmtupt;
void preparesql();

// 把xml文件的一行拆分到strcolvalue数组中。
void splitbuffer(char *buffer);

CPActive PActive;

struct st_arg
{
  char connstr[101];     // 数据库的连接参数。
  char charset[51];      // 数据库的字符集。
  char inifilename[301]; // 数据入库的参数配置文件。
  char xmlpath[301];     // 待入库xml文件存放的目录。
  char xmlpathbak[301];  // xml文件入库后的备份目录。
  char xmlpatherr[301];  // 入库失败的xml文件存放的目录。
  int  timetvl;          // 本程序运行的时间间隔。
  int  timeout;          // 本程序运行时的超时时间。
  char pname[51];        // 本程序运行时的程序名。
} starg;

// 显示程序的帮助
void _help(char *argv[]);

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer);

struct st_xmltotable
{
  char filename[31];     // xml文件名的前缀。
  char tname[31];         // 入库表名。
  int  uptbz;             // 1-更新；2-不更新。
  char execsql[301];      // 每次处理xml文件之前，执行一个SQL语句。
} stxmltotable;

vector<struct st_xmltotable> vxmltotable;
// 把数据入库的参数配置文件starg.inifilename加载到vxmltotable容器中。
bool loadinifilename();
// 从vxmltotable容器中查找xmlfilename的入库参数，存放在stxmltotable结构体中。
bool findvxmltotable(char *xmlfilename);

CLogFile logfile;

connection conn;
 
void EXIT(int sig);

// 业务处理主函数。
bool _xmltodb();

// 处理具体的xml文件，函数的返回值有多种情况，暂时不确定。
int totalcount,inscount,uptcount;
int _xmltodb(char *fullfilename,char *filename);

// 把xml文件移动到备份目录或错误目录。
bool xmltobakerr(char *fullfilename,char *srcpath,char *dstpath);

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
  if (_xmltoarg(argv[2])==false)  EXIT(-1);

  PActive.AddPInfo(starg.timetvl,starg.pname);
  // 注意，在调试程序的时候，可以启用类似以下的代码，防止超时。
  // PActive.AddPInfo(starg.timetvl*100,starg.pname);

  if (conn.connecttodb(starg.connstr,starg.charset) != 0)
  {
    printf("connect database(%s) failed.\n%s\n",starg.connstr,conn.m_cda.message); EXIT(-1);
  }

  logfile.Write("connect database(%s) ok.\n",starg.connstr);

  // 业务处理主函数。
  _xmltodb();

  EXIT(0);
}

// 显示程序的帮助
void _help(char *argv[])
{
  printf("Using:/project/tools/bin/xmltodb12 logfilename xmlbuffer\n\n");

  printf("Sample:/project/tools/bin/procctl 10 /project/tools/bin/xmltodb12 /log/idc/xmltodb12_vip1.log \"<connstr>127.0.0.1,root,mysqlpwd,mysql,3306</connstr><charset>gbk</charset><inifilename>/project/tools/ini/xmltodb.xml</inifilename><xmlpath>/idcdata/xmltodb/vip1</xmlpath><xmlpathbak>/idcdata/xmltodb/vip1bak</xmlpathbak><xmlpatherr>/idcdata/xmltodb/vip1err</xmlpatherr><timetvl>5</timetvl><timeout>50</timeout><pname>xmltodb12_vip1</pname>\"\n\n");

  printf("本程序是数据中心的公共功能模块，用于把xml文件入库到mysql的表中。\n");
  printf("logfilename 本程序运行的日志文件。\n");
  printf("xmlbuffer   本程序运行的参数，用xml表示，具体如下：\n\n");

  printf("connstr     数据库的连接参数，格式：ip,username,password,dbname,port。\n");
  printf("charset     数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况。\n");
  printf("inifilename 数据入库的参数配置文件。\n");
  printf("xmlpath     待入库xml文件存放的目录。\n");
  printf("xmlpathbak  xml文件入库后的备份目录。\n");
  printf("xmlpatherr  入库失败的xml文件存放的目录。\n");
  printf("timetvl     本程序的时间间隔，单位：秒，视业务需求而定，2-30之间。\n");
  printf("timeout     本程序的超时时间，单位：秒，视xml文件大小而定，建议设置30以上。\n");
  printf("pname       进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n");
}

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer)
{
  memset(&starg,0,sizeof(struct st_arg));

  GetXMLBuffer(strxmlbuffer,"connstr",starg.connstr,100);
  if (strlen(starg.connstr)==0) { logfile.Write("connstr is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"charset",starg.charset,50);
  if (strlen(starg.charset)==0) { logfile.Write("charset is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"inifilename",starg.inifilename,300);
  if (strlen(starg.inifilename)==0) { logfile.Write("inifilename is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"xmlpath",starg.xmlpath,300);
  if (strlen(starg.xmlpath)==0) { logfile.Write("xmlpath is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"xmlpathbak",starg.xmlpathbak,300);
  if (strlen(starg.xmlpathbak)==0) { logfile.Write("xmlpathbak is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"xmlpatherr",starg.xmlpatherr,300);
  if (strlen(starg.xmlpatherr)==0) { logfile.Write("xmlpatherr is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"timetvl",&starg.timetvl);
  if (starg.timetvl< 2) starg.timetvl=2;
  if (starg.timetvl>30) starg.timetvl=30;

  GetXMLBuffer(strxmlbuffer,"timeout",&starg.timeout);
  if (starg.timeout==0) { logfile.Write("timeout is null.\n"); return false; }

  // 强制timeout要大于timetvl，防止sleep超时。
  if (starg.timeout<starg.timetvl) starg.timeout=starg.timetvl+10;

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

// 把数据入库的参数配置文件starg.inifilename加载到vxmltotable容器中。
bool loadinifilename()
{
  vxmltotable.clear();

  CFile File;

  if (File.Open(starg.inifilename,"r") == false)
  {
    logfile.Write("File.Open(%s) 失败。\n",starg.inifilename); return false;
  }

  char strLine[301];

  while (true)
  {
    memset(&stxmltotable,0,sizeof(struct st_xmltotable));

    if (File.FFGETS(strLine,300,"<endl/>")==false) break;

    GetXMLBuffer(strLine,"filename",stxmltotable.filename,30);   // xml文件名的前缀。
    GetXMLBuffer(strLine,"tname",stxmltotable.tname,30);         // 入库表名。
    GetXMLBuffer(strLine,"uptbz",&stxmltotable.uptbz);           // 1-更新；2-不更新。
    GetXMLBuffer(strLine,"execsql",stxmltotable.execsql,300);    // 每次处理xml文件之前，执行一个SQL语句。
    DeleteLRChar(stxmltotable.execsql,' ');  // 删除前后的空格。

    vxmltotable.push_back(stxmltotable);
  }

  logfile.Write("loadinifilename(%s) ok.\n",starg.inifilename);

  return true;
}

// 从vxmltotable容器中查找xmlfilename的入库参数，存放在stxmltotable结构体中。
bool findvxmltotable(char *xmlfilename)
{
  for (int ii=0;ii<vxmltotable.size();ii++)
  {
    if (MatchStr(xmlfilename,vxmltotable[ii].filename)==true) 
    {
      memcpy(&stxmltotable,&vxmltotable[ii],sizeof(struct st_xmltotable));
      return true;
    }
  }

  return false;
}

// 业务处理主函数。
bool _xmltodb()
{
  int counter=50;  // 初始化为50是为了在第一次进入循环的时候就加载参数。

  CDir Dir;

  while (true)
  {
    if (counter++>30)
    {
      counter=0;
      // 把数据入库的参数配置文件starg.inifilename加载到vxmltotable容器中。
      if (loadinifilename()==false) return false;
    }

    // 为了保证先生成的数据入库，打开目录的时候，应该按文件名排序。
    if (Dir.OpenDir(starg.xmlpath,"*.XML",10000,false,true)==false)
    {
      logfile.Write("Dir.OpenDir(%s) failed.\n",starg.xmlpath); return false;
    }

    while (true)
    {
      if (Dir.ReadDir()==false) break;

      logfile.Write("处理文件%s...",Dir.m_FullFileName);

      int iret=_xmltodb(Dir.m_FullFileName,Dir.m_FileName);

      PActive.UptATime();

      // 处理xml文件成功。
      if (iret==0)
      {
        logfile.WriteEx("ok(total=%d,insert=%d,update=%d).\n",totalcount,inscount,uptcount);

        // 把xml文件移动到starg.xmlpathbak参数指定的目录中。这种错误一般不会发生，如果真发生了，程序将退出。
        if (xmltobakerr(Dir.m_FullFileName,starg.xmlpath,starg.xmlpathbak)==false) return false;
      }

      // 1-没有配置入库参数；2-待入库的表不存在；5-表的字段数太多，或字段的取值太长。
      if ( (iret==1) || (iret==2) || (iret==5) )
      {
        if (iret==1) logfile.WriteEx("failed，没有配置入库参数。\n");
        if (iret==2) logfile.WriteEx("failed，待入库的表（%s）不存在。\n",stxmltotable.tname);
        if (iret==5) logfile.WriteEx("failed，待入库的表（%s）的字段数太多，或字段的取值太长。\n",stxmltotable.tname);

        // 把xml文件移动到xmlpatherr参数指定的目录中。这种错误一般不会发生，如果真发生了，程序将退出。
        if (xmltobakerr(Dir.m_FullFileName,starg.xmlpath,starg.xmlpatherr)==false) return false;
      }

      // 打开xml文件错误，这种错误一般不会发生，如果真发生了，程序将退出。
      if (iret==3)
      {
        logfile.WriteEx("failed，打开xml文件失败。\n"); return false;
      }

      // 数据库错误，函数返回，程序将退出。
      if (iret==4)
      {
        logfile.WriteEx("failed，数据库错误。\n"); return false;
      }

      // 在处理xml文件之前，如果执行stxmltotable.execsql失败，函数返回，程序将退出。
      if (iret==6)
      {
        logfile.WriteEx("failed，执行execsql失败。\n"); return false;
      }
    }

    // 如果刚才这次扫描到了有文件，表示不空闲，可能不断的有文件生成，就不sleep了。
    if (Dir.m_vFileName.size()==0) sleep(starg.timetvl);

    PActive.UptATime();
  }

  return true;
}

// 在处理xml文件之前，如果stxmltotable.execsql不为空，就执行它。
bool execsql()
{
  if (strlen(stxmltotable.execsql)==0) return true;

  sqlstatement stmt;
  stmt.connect(&conn);
  stmt.prepare(stxmltotable.execsql);
  if (stmt.execute()!=0)
  {
    logfile.Write("stmt.execute() failed.\n%s\n%s\n",stmt.m_sql,stmt.m_cda.message); return false;
  }

  return true;
}

// 处理具体的xml文件，函数的返回值有多种情况，暂时不确定。
int _xmltodb(char *fullfilename,char *filename)
{
  totalcount=inscount=uptcount=0;

  // 从vxmltotable容器中查找xmlfilename的入库参数，存放在stxmltotable结构体中。
  if (findvxmltotable(filename)==false) return 1;  // 没有配置入库参数。

  // 获取表全部的字段和主键信息。
  TABCOLS.allcols(&conn,stxmltotable.tname);
  TABCOLS.pkcols(&conn,stxmltotable.tname);

  // 如果TABCOLS.m_allcols为空，说明表根本不存在，返回2。
  if (strlen(TABCOLS.m_allcols)==0) return 2;  // 待入库的表不存在。

  // 判断表的字段数不能超过MAXCOLCOUNT，字段的最大长度不能超过MAXCOLLEN。
  if (checkallcols()==false) return 5; // 表的字段数太多，或字段的取值太长。

  // logfile.Write("allcols=%s,pkcols=%s\n",TABCOLS.m_allcols,TABCOLS.m_pkcols);
  // 生成插入和更新表数据的SQL。
  crtsql();

  // prepare插入和更新的sql语句，绑定输入变量。
  preparesql();

  // 在处理xml文件之前，如果stxmltotable.execsql不为空，就执行它。
  if (execsql()==false) return 6;

  CFile File;
  if (File.Open(fullfilename,"r")==false) { conn.rollback(); return 3; } // 打开xml文件错误。

  char strLine[10241];

  while (true)
  {
    if (File.FFGETS(strLine,10240,"<endl/>")==false) break;

    totalcount++;

    // logfile.Write("%s",strLine);
    // 把xml文件的一行拆分到strcolvalue数组中。
    splitbuffer(strLine);

    // 以下代码要注意，不要把stmtins和stmtupt搞混了，抄着抄着就抄错了，我刚才也抄错了。
    if (stmtins.execute() != 0)
    {
      if (stmtins.m_cda.rc==1062) // 违反唯一性约束。
      {
        // 需要更新。
        if (stxmltotable.uptbz==1)
        {
          if (stmtupt.execute() != 0)
          {
            // 如果update失败，记录出错的行和错误内容，函数不返回，继续处理数据，也就是说，不理这一行。
            logfile.Write("%s",strLine);
            logfile.Write("stmtupt.execute() failed.\n%s\n%s\n",stmtupt.m_sql,stmtupt.m_cda.message);

            // 数据库连接已无效。
            if ( (stmtupt.m_cda.rc==1053) || (stmtupt.m_cda.rc==2013) ) return 4;
          }
          else
            uptcount++;
        }
      }
      else
      {
        // 如果insert失败，记录出错的行和错误内容，函数不返回，继续处理数据，也就是说，不理这一行。
        logfile.Write("%s",strLine);
        logfile.Write("stmtins.execute() failed.\n%s\n%s\n",stmtins.m_sql,stmtins.m_cda.message);

        // 数据库连接已无效。
        if ( (stmtins.m_cda.rc==1053) || (stmtins.m_cda.rc==2013) ) return 4;
      }
    }
    else
      inscount++;
  }

  conn.commit();

  return 0;
}

CTABCOLS::CTABCOLS()
{
  initdata();
}

// 成员变量初始化。
void CTABCOLS::initdata()
{
  m_allcount=m_pkcount=0;
  memset(m_allcols,0,sizeof(m_allcols));
  memset(m_pkcols,0,sizeof(m_pkcols));
  m_vallcols.clear();
  m_vpkcols.clear();
}
 
// 获取指定表的主键字段信息。
bool CTABCOLS::allcols(connection *conn,char *tablename)
{
  m_allcount=0;
  memset(m_allcols,0,sizeof(m_allcols));
  m_vallcols.clear();

  struct st_columns stcolumns;

  sqlstatement stmt;
  stmt.connect(conn);
  stmt.prepare("select lower(column_name),lower(data_type),character_maximum_length from information_schema.COLUMNS where table_name=:1");
  stmt.bindin(1,tablename,30);
  stmt.bindout(1, stcolumns.colname,30);
  stmt.bindout(2, stcolumns.datatype,30);
  stmt.bindout(3,&stcolumns.collen);

  if (stmt.execute() != 0) return false;

  while (true)
  {
    memset(&stcolumns,0,sizeof(struct st_columns));

    if (stmt.next() != 0) break;

    // 列的数据类型，分为number、date和char三大类。
    if (strcmp(stcolumns.datatype,"char")==0)      strcpy(stcolumns.datatype,"char");
    if (strcmp(stcolumns.datatype,"varchar")==0)   strcpy(stcolumns.datatype,"char");

    if (strcmp(stcolumns.datatype,"timestamp")==0) strcpy(stcolumns.datatype,"date");
    if (strcmp(stcolumns.datatype,"datetime")==0)  strcpy(stcolumns.datatype,"date");

    if (strcmp(stcolumns.datatype,"tinyint")==0)   strcpy(stcolumns.datatype,"number");
    if (strcmp(stcolumns.datatype,"smallint")==0)  strcpy(stcolumns.datatype,"number");
    if (strcmp(stcolumns.datatype,"mediumint")==0) strcpy(stcolumns.datatype,"number");
    if (strcmp(stcolumns.datatype,"int")==0)       strcpy(stcolumns.datatype,"number");
    if (strcmp(stcolumns.datatype,"integer")==0)   strcpy(stcolumns.datatype,"number");
    if (strcmp(stcolumns.datatype,"bigint")==0)    strcpy(stcolumns.datatype,"number");
    if (strcmp(stcolumns.datatype,"numeric")==0)   strcpy(stcolumns.datatype,"number");
    if (strcmp(stcolumns.datatype,"decimal")==0)   strcpy(stcolumns.datatype,"number");

    // 如果字段的数据类型不在上面列出来的中，忽略它。
    // 如果业务有需要，可以修改上面的代码，增加对更多数据类型的支持。
    if ( (strcmp(stcolumns.datatype,"char")!=0) &&
         (strcmp(stcolumns.datatype,"date")!=0) &&
         (strcmp(stcolumns.datatype,"number")!=0) ) continue;

    // 如果字段类型是date，把长度设置为19。
    if (strcmp(stcolumns.datatype,"date")==0) stcolumns.collen=19;

    // 如果字段类型是number，把长度设置为20。
    if (strcmp(stcolumns.datatype,"number")==0) stcolumns.collen=20;

    strcat(m_allcols,stcolumns.colname);
    strcat(m_allcols,",");

    m_vallcols.push_back(stcolumns);

    m_allcount++;
  }

  if (strlen(m_allcols)>0) m_allcols[strlen(m_allcols)-1]=0;    // 删除m_allcols最后一个多余的逗号。

  return true;
}

// 获取指定表的全部字段信息。
bool CTABCOLS::pkcols(connection *conn,char *tablename)
{
  m_pkcount=0;
  memset(m_pkcols,0,sizeof(m_pkcols));
  m_vpkcols.clear();

  struct st_columns stcolumns;

  sqlstatement stmt;
  stmt.connect(conn);
  stmt.prepare("select lower(column_name),seq_in_index from information_schema.STATISTICS where table_name=:1 and index_name='primary' order by seq_in_index");
  stmt.bindin(1,tablename,30);
  stmt.bindout(1, stcolumns.colname,30);
  stmt.bindout(2,&stcolumns.pkseq);

  if (stmt.execute() != 0) return false;

  while (true)
  {
    memset(&stcolumns,0,sizeof(struct st_columns));

    if (stmt.next() != 0) break;

    strcat(m_pkcols,stcolumns.colname);
    strcat(m_pkcols,",");

    m_vpkcols.push_back(stcolumns);

    m_pkcount++;
  }

  if (strlen(m_pkcols)>0) m_pkcols[strlen(m_pkcols)-1]=0;    // 删除m_pkcols最后一个多余的逗号。

  return true;
}

// 生成插入和更新表数据的SQL。
void crtsql()
{
  memset(strinsertsql,0,sizeof(strinsertsql));
  memset(strupdatesql,0,sizeof(strupdatesql));

  char strinsertp1[5001]; // insert语句的字段列表。
  char strinsertp2[5001]; // insert语句values后的内容。
  memset(strinsertp1,0,sizeof(strinsertp1));
  memset(strinsertp2,0,sizeof(strinsertp2));

  char strtemp[101];

  int colseq=1;  // values部分字段的序号。

  for (int ii=0;ii<TABCOLS.m_vallcols.size();ii++)
  {
    // upttime和keyid这两个字段不需要处理。
    if ( (strcmp(TABCOLS.m_vallcols[ii].colname,"upttime")==0) || 
         (strcmp(TABCOLS.m_vallcols[ii].colname,"keyid")==0) )
      continue;
 
    // 拼接strinsertp1
    strcat(strinsertp1,TABCOLS.m_vallcols[ii].colname);
    strcat(strinsertp1,",");

    // 拼接strinsertp2，需要区分date字段和非date字段。
    memset(strtemp,0,sizeof(strtemp));
    if (strcmp(TABCOLS.m_vallcols[ii].datatype,"date")!=0)
      snprintf(strtemp,100,":%d",colseq);
    else
      snprintf(strtemp,100,"str_to_date(:%d,'%%%%Y%%%%m%%%%d%%%%H%%%%i%%%%s')",colseq);
    
    strcat(strinsertp2,strtemp);
    strcat(strinsertp2,",");
    
    colseq++;
  }
  
  strinsertp1[strlen(strinsertp1)-1]=0;  // 把最后一个多余的逗号删除。
  strinsertp2[strlen(strinsertp2)-1]=0;  // 把最后一个多余的逗号删除。
  
  snprintf(strinsertsql,10240,"insert into %s(%s) values(%s)",stxmltotable.tname,strinsertp1,strinsertp2);

  // 如果入库参数中指定了表数据不需要更新，就不生成update语句了，函数返回。
  if (stxmltotable.uptbz!=1) return;

  // 更新TABCOLS.m_vallcols中的pkseq字段，在拼接update语句的时候要用到它。
  for (int ii=0;ii<TABCOLS.m_vpkcols.size();ii++)
  {
    for (int jj=0;jj<TABCOLS.m_vallcols.size();jj++)
    {
      if (strcmp(TABCOLS.m_vpkcols[ii].colname,TABCOLS.m_vallcols[jj].colname)==0)
      {
        // 更新m_vallcols容器中的pkseq。
        TABCOLS.m_vallcols[jj].pkseq=TABCOLS.m_vpkcols[ii].pkseq; break;
      }
    }
  }

  // 拼接update语句set后面的部分。
  colseq=1;  // set部分字段的序号。
  sprintf(strupdatesql,"update %s set ",stxmltotable.tname);
  for (int ii=0;ii<TABCOLS.m_vallcols.size();ii++)
  {
    // keyid字段不需要处理。
    if (strcmp(TABCOLS.m_vallcols[ii].colname,"keyid")==0) continue;

    // 如果是主键字段，也不需要拼接在set的后面。
    if (TABCOLS.m_vallcols[ii].pkseq!=0) continue;
 
    // upttime字段直接等于now()，这么做是为了考虑数据库的兼容性。
    if (strcmp(TABCOLS.m_vallcols[ii].colname,"upttime")==0) 
    {
      strcat(strupdatesql,"upttime=now(),"); continue;
    }

    memset(strtemp,0,sizeof(strtemp));

    // 其它字段需要区分date字段和非date字段。
    if (strcmp(TABCOLS.m_vallcols[ii].datatype,"date")!=0)
      snprintf(strtemp,100,"%s=:%d",TABCOLS.m_vallcols[ii].colname,colseq);
    else
      snprintf(strtemp,100,"%s=str_to_date(:%d,'%%%%Y%%%%m%%%%d%%%%H%%%%i%%%%s')",TABCOLS.m_vallcols[ii].colname,colseq);
    
    strcat(strupdatesql,strtemp);
    strcat(strupdatesql,",");

    colseq++;
  }

  strupdatesql[strlen(strupdatesql)-1]=0;   // 删除最后一个多余的逗号。
  
  // 然后再拼接update语句where部分。
  strcat(strupdatesql," where 1=1");   // 用1=1是为了后面的拼接方便，这是常用的处理方法。

  for (int ii=0;ii<TABCOLS.m_vallcols.size();ii++)
  {
    if (TABCOLS.m_vallcols[ii].pkseq==0) continue;

    // 需要区分date字段和非date字段。
    memset(strtemp,0,sizeof(strtemp));
    if (strcmp(TABCOLS.m_vallcols[ii].datatype,"date")!=0)
      snprintf(strtemp,100," and %s=:%d",TABCOLS.m_vallcols[ii].colname,colseq);
    else
      snprintf(strtemp,100," and %s=str_to_date(:%d,'%%%%Y%%%%m%%%%d%%%%H%%%%i%%%%s')",TABCOLS.m_vallcols[ii].colname,colseq);
    
    colseq++;

    strcat(strupdatesql,strtemp);
  }

  return;
}

// prepare插入和更新的sql语句，绑定输入变量。
void preparesql()
{
  if (stmtins.m_state==0) stmtins.connect(&conn);
  stmtins.prepare(strinsertsql);

  int colseq=1;  // values部分字段的序号。

  for (int ii=0;ii<TABCOLS.m_vallcols.size();ii++)
  {
    // upttime和keyid这两个字段不需要处理。
    if ( (strcmp(TABCOLS.m_vallcols[ii].colname,"upttime")==0) ||
         (strcmp(TABCOLS.m_vallcols[ii].colname,"keyid")==0) )
      continue;

    // 注意，strcolvalue数组的使用不是连续的，是和TABCOLS.m_vallcols的下标是一一对应的。
    stmtins.bindin(colseq,strcolvalue[ii],TABCOLS.m_vallcols[ii].collen);

    colseq++;
  }

  // 如果入库参数中指定了表数据不需要更新，就不处理update语句了，函数返回。
  if (stxmltotable.uptbz!=1) return;

  if (stmtupt.m_state==0) stmtupt.connect(&conn);
  stmtupt.prepare(strupdatesql);

  colseq=1;  

  // set部分字段的序号。
  for (int ii=0;ii<TABCOLS.m_vallcols.size();ii++)
  {
    // upttime和keyid这两个字段不需要处理。
    if ( (strcmp(TABCOLS.m_vallcols[ii].colname,"upttime")==0) ||
         (strcmp(TABCOLS.m_vallcols[ii].colname,"keyid")==0) )
      continue;

    // 如果是主键字段，也不需要拼接在set的后面。
    if (TABCOLS.m_vallcols[ii].pkseq!=0) continue;
 
    // 注意，strcolvalue数组的使用不是连续的，是和TABCOLS.m_vallcols的下标是一一对应的。
    stmtupt.bindin(colseq,strcolvalue[ii],TABCOLS.m_vallcols[ii].collen);

    colseq++;
  }

  // where后面的序号是主键字段。
  for (int ii=0;ii<TABCOLS.m_vallcols.size();ii++)
  {
    if (TABCOLS.m_vallcols[ii].pkseq==0) continue;

    // 注意，strcolvalue数组的使用不是连续的，是和TABCOLS.m_vallcols的下标是一一对应的。
    stmtupt.bindin(colseq,strcolvalue[ii],TABCOLS.m_vallcols[ii].collen);

    colseq++;
  }

  return;
}

// 把xml文件的一行拆分到strcolvalue数组中。
void splitbuffer(char *buffer)
{
  memset(strcolvalue,0,sizeof(strcolvalue));

  char strtemp[21];
  
  for (int ii=0;ii<TABCOLS.m_vallcols.size();ii++)
  {
    // 不用管upttime和keyid字段，全部取出来就是了，不碍事，反正sql语句中没有upttime和keyid。

    // 如果是日期时间字段，提取数值就可以了。
    // 也就是说，xml文件中的日期时间只要包含了yyyymmddhh24miss就行，可以是任意分隔符。
    if (strcmp(TABCOLS.m_vallcols[ii].datatype,"date")==0)
    {
      GetXMLBuffer(buffer,TABCOLS.m_vallcols[ii].colname,strtemp,TABCOLS.m_vallcols[ii].collen);
      PickNumber(strtemp,strcolvalue[ii],false,false);
      continue;
    }

    // 如果是数字字段，只提取数字、+-符号和圆点，考虑小数。
    if (strcmp(TABCOLS.m_vallcols[ii].datatype,"number")==0)
    {
      GetXMLBuffer(buffer,TABCOLS.m_vallcols[ii].colname,strtemp,TABCOLS.m_vallcols[ii].collen);
      PickNumber(strtemp,strcolvalue[ii],true,true);
      continue;
    }

    // 其它的就是字符字段了。
    GetXMLBuffer(buffer,TABCOLS.m_vallcols[ii].colname,strcolvalue[ii],TABCOLS.m_vallcols[ii].collen);
  }
}

// 把xml文件移动到备份目录或错误目录。
bool xmltobakerr(char *fullfilename,char *srcpath,char *dstpath)
{
  char strtemp[301]; 
  memset(strtemp,0,sizeof(strtemp));

  strcpy(strtemp,fullfilename);
  UpdateStr(strtemp,srcpath,dstpath,false);  // 注意，第三个参数很重要，否则UpdateStr可能会拒绝替换。

  if (RENAME(fullfilename,strtemp)==false) 
  {
    logfile.Write("RENAME(%s,%s) failed.\n",fullfilename,strtemp); return false;
  }

  return true;
}

// 判断表的字段数不能超过MAXCOLCOUNT，字段的最大长度不能超过MAXCOLLEN。
bool checkallcols()
{
  if (TABCOLS.m_allcount>MAXCOLCOUNT) return false;

  for (int ii=0;ii<TABCOLS.m_allcount;ii++)
  {
    if (TABCOLS.m_vallcols[ii].collen>MAXCOLLEN) return false;
  }

  return true;
}

