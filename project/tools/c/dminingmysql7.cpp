/*
 *  程序名：dminingmysql7.cpp，本程序是数据中心的公共功能模块，用于从mysql数据库源表挖掘数据，生成xml文件。
 *  记录增量字段的值。
 *  把增量字段的值保存到文件中。
 *  注意，目前采用把增量字段值保存到文件中的方案是有缺陷的，如果挖掘过程中被中断，会导致重新挖掘。
 *  但这情况不容易出现。
 *  mysql的自增字段是从1开始的，如果表中的数据有被清理过，就需要手工删除incfilename文件。
 *  作者：吴从周。
*/
#include "_public.h"
#include "_mysql.h"

struct st_arg
{
  char connstr[101];     // 数据库的连接参数。
  char charset[51];      // 数据库的字符集。
  char selectsql[1024];  // 从数据源数据库挖掘数据的SQL语句。
  char fieldstr[501];    // 挖掘数据的SQL语句输出结果集字段名。
  char fieldlen[501];    // 挖掘数据的SQL语句输出结果集字段的长度。
  char bfilename[31];    // 输出xml文件的前缀。
  char efilename[31];    // 输出xml文件的后缀。
  char outpath[301];     // 输出xml文件存放的目录。
  char starttime[51];    // 程序运行的时间区间
  char incfield[31];     // 递增字段名。
  char incfilename[301]; // 已挖掘数据的递增字段最大值存放的文件。
} starg;


#define MAXFIELDCOUNT  100// 结果集字段的最大数。
#define MAXFIELDLEN    500  // 结果集字段值的最大长度。

char strfieldname[MAXFIELDCOUNT][31];    // 结果集字段名数组，从starg.fieldstr解析得到。
int  ifieldlen[MAXFIELDCOUNT];           // 结果集字段的长度数组，从starg.fieldlen解析得到。
char strfieldvalue[MAXFIELDCOUNT][MAXFIELDLEN+1]; // 挖掘数据的SQL执行后，存放结果集字段值的数组。
int  ifieldcount;                        // strfieldname和ifieldlen数组中有效字段的个数。

// 读取增量采集标志字段的值存放的文件，结果保存在incfieldvalue变量中。
int  incfieldpos=-1;  // 递增字段在结果集数组中的位置。
long incfieldvalue;   // 递增字段的最大值。
bool readincfile();   // 从已挖掘数据的递增字段最大值存放的文件读取递增字段的最大值。
bool writeincfile();  // 把增量字段的值写入文件中

// 显示程序的帮助
void _help(char *argv[]);

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer);

// 判断当前时间是否在程序运行的时间区间内。
bool instarttime();

CLogFile logfile;

connection conn;

// 数据挖掘主函数。
bool _dmining();

// 生成xml文件名。
char strxmlfilename[301];
void crtxmlfilename();
 
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
  if (_xmltoarg(argv[2])==false) EXIT(-1);

  // 判断当前时间是否在程序运行的时间区间内。
  if (instarttime()==false) EXIT(0);

  if (conn.connecttodb(starg.connstr,starg.charset) != 0)
  {
    printf("connect database(%s) failed.\n%s\n",starg.connstr,conn.m_cda.message); EXIT(-1);
  }

  logfile.Write("connect database(%s) ok.\n",starg.connstr);

  _dmining();
}

// 显示程序的帮助
void _help(char *argv[])
{
  printf("Using:/project/tools/bin/dminingmysql7 logfilename xmlbuffer\n\n");

  printf("Sample:/project/tools/bin/procctl 3600 /project/tools/bin/dminingmysql7 /tmp/project/log/dminingmysql7.log \"<connstr>127.0.0.1,root,mysqlpwd,mysql,3306</connstr><charset>gbk</charset><selectsql>select obtid,date_format(ddatetime,'%%%%Y-%%%%m-%%%%d %%%%H:%%%%m:%%%%s'),t,p,u,wd,wf,r,vis,keyid from t_zhobtmind where ddatetime>timestampadd(minute,-10,now())</selectsql><fieldstr>obtid,ddatetime,t,p,u,wd,wf,r,vis,keyid</fieldstr><fieldlen>10,19,8,8,8,8,8,8,8,15</fieldlen><bfilename>ZHOBTMIND</bfilename><efilename>HYCZ</efilename><outpath>/tmp/project/dmindata</outpath><starttime></starttime><incfield>keyid</incfield><incfilename>/idcdata/dmining/dminingmysql_ZHOBTMIND_HYCZ.list</incfilename>\"\n\n");
  printf("       /project/tools/bin/procctl 3600 /project/tools/bin/dminingmysql7 /tmp/project/log/dminingmysql7.log \"<connstr>127.0.0.1,root,mysqlpwd,mysql,3306</connstr><charset>gbk</charset><selectsql>select obtid,date_format(ddatetime,'%%%%Y-%%%%m-%%%%d %%%%H:%%%%m:%%%%s'),t,p,u,wd,wf,r,vis,keyid from t_zhobtmind where keyid>:1 and ddatetime>timestampadd(minute,-10,now())</selectsql><fieldstr>obtid,ddatetime,t,p,u,wd,wf,r,vis,keyid</fieldstr><fieldlen>10,19,8,8,8,8,8,8,8,15</fieldlen><bfilename>ZHOBTMIND</bfilename><efilename>HYCZ</efilename><outpath>/tmp/project/dmindata</outpath><starttime></starttime><incfield>keyid</incfield><incfilename>/idcdata/dmining/dminingmysql_ZHOBTMIND_HYCZ.list</incfilename>\"\n\n");

  printf("本程序是数据中心的公共功能模块，用于从mysql数据库源表挖掘数据，生成xml文件。\n");
  printf("logfilename   本程序运行的日志文件。\n");
  printf("xmlbuffer     本程序运行的参数，用xml表示，具体如下：\n\n");

  printf("connstr     数据库的连接参数，格式：ip,username,password,dbname,port。\n");
  printf("charset     数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况。\n");
  printf("selectsql   从数据源数据库挖掘数据的SQL语句，注意：1）时间函数的百分号%需要四个，显示出来才有两个，被prepare之后将剩一个。\n");
  printf("fieldstr    挖掘数据的SQL语句输出结果集字段名，中间用逗号分隔，将作为xml文件的字段名。\n");
  printf("fieldlen    挖掘数据的SQL语句输出结果集字段的长度，中间用逗号分隔。fieldstr与fieldlen的字段必须一一对应。\n");
  printf("bfilename   输出xml文件的前缀。\n");
  printf("efilename   输出xml文件的后缀。\n");
  printf("outpath     输出xml文件存放的目录。\n");
  printf("starttime   程序运行的时间区间，例如02,13表示：如果程序启动时，踏中02时和13时则运行，其它时间不运行。"\
         "如果starttime为空，那么starttime参数将失效，只要本程序启动就会执行数据挖掘，为了减少数据源"\
         "的压力，从数据库挖掘数据的时候，一般在对方数据库最闲的时候时进行。\n");
  printf("incfield    递增字段名，它必须是fieldstr中的字段名，并且只能是整型，一般为自增字段。"\
          "如果incfield为空，表示不采用增量挖掘方案。");
  printf("incfilename 已挖掘数据的递增字段最大值存放的文件，如果该文件丢失，将重挖全部的数据。\n\n\n");
}

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer)
{
  memset(&starg,0,sizeof(struct st_arg));

  GetXMLBuffer(strxmlbuffer,"connstr",starg.connstr,100);
  if (strlen(starg.connstr)==0) { logfile.Write("connstr is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"charset",starg.charset,50);
  if (strlen(starg.charset)==0) { logfile.Write("charset is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"selectsql",starg.selectsql,1000);
  if (strlen(starg.selectsql)==0) { logfile.Write("selectsql is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"fieldstr",starg.fieldstr,500);
  if (strlen(starg.fieldstr)==0) { logfile.Write("fieldstr is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"fieldlen",starg.fieldlen,500);
  if (strlen(starg.fieldlen)==0) { logfile.Write("fieldlen is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"bfilename",starg.bfilename,30);
  if (strlen(starg.bfilename)==0) { logfile.Write("bfilename is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"efilename",starg.efilename,30);
  if (strlen(starg.efilename)==0) { logfile.Write("efilename is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"outpath",starg.outpath,300);
  if (strlen(starg.outpath)==0) { logfile.Write("outpath is null.\n"); return false; }

  GetXMLBuffer(strxmlbuffer,"starttime",starg.starttime,50);

  GetXMLBuffer(strxmlbuffer,"incfield",starg.incfield,30);

  GetXMLBuffer(strxmlbuffer,"incfilename",starg.incfilename,301);

  /////////////////////////////////////////////////////
  // 把starg.fieldstr解析到strfieldname数组中，把starg.fieldlen解析到ifieldlen数组中。
  // 并做合法性判断。
  CCmdStr CmdStr;

  // 把starg.fieldstr解析到strfieldname数组中。
  CmdStr.SplitToCmd(starg.fieldstr,",");

  if (CmdStr.CmdCount()>MAXFIELDCOUNT)
  {
    logfile.Write("fieldstr的字段数太多，超出了最大限制%d。\n",MAXFIELDCOUNT); return false;
  }

  int ii=0;
  for (ii=0;ii<CmdStr.CmdCount();ii++) 
  {
    CmdStr.GetValue(ii,strfieldname[ii],30);

    // 记下增量字段的位置。
    if ( (strlen(starg.incfield)>0) && (strcmp(strfieldname[ii],starg.incfield)==0) ) incfieldpos=ii;
  }

  // 检查递增字段名，它必须是fieldstr中的字段名。
  if ( (strlen(starg.incfield)>0) && (incfieldpos==-1) ) 
  { 
    logfile.Write("递增字段名%s不在列表%s中。\n",starg.incfield,starg.fieldstr); return false;
  }

  // 把starg.fieldlen解析到strfieldname数组中。
  CmdStr.SplitToCmd(starg.fieldlen,",");

  if (CmdStr.CmdCount()>MAXFIELDCOUNT)
  {
    logfile.Write("fieldlen的字段数太多，超出了最大限制%d。\n",MAXFIELDCOUNT); return false;
  }

  int jj=0;
  for (jj=0;jj<CmdStr.CmdCount();jj++) 
  {
    CmdStr.GetValue(jj,&ifieldlen[jj]);
    // 如果参数中的字段长度大于MAXFIELDLEN，就取MAXFIELDLEN，否则会造成strfieldvalue数组内存溢出。
    if (ifieldlen[jj]>MAXFIELDLEN) ifieldlen[jj]=MAXFIELDLEN;
  }

  // 判断strfieldname和ifieldlen两个数组中的字段是否一致。
  if (ii!=jj)
  {
    logfile.Write("fieldstr和fieldlen的元素数量不一致。\n"); return false;
  }

  ifieldcount=ii;

  return true;
}

void EXIT(int sig)
{
  logfile.Write("程序退出，sig=%d\n\n",sig);

  conn.disconnect();

  exit(0);
}

// 判断当前时间是否在程序运行的时间区间内。
bool instarttime()
{
  if (strlen(starg.starttime) != 0)
  {
    char strLocalTime[21];
    memset(strLocalTime,0,sizeof(strLocalTime));
    LocalTime(strLocalTime,"hh24mi");
    strLocalTime[2]=0;
    if (strstr(starg.starttime,strLocalTime) == 0) return false;
  }

  return true;
}

// 数据挖掘主函数。
bool _dmining()
{
  // 读取增量采集标志字段的值存放的文件，结果保存在incfieldvalue变量中。
  readincfile();

  CFile File;

  sqlstatement stmt;
  stmt.connect(&conn);
  stmt.prepare(starg.selectsql);
  // 绑定输出结果集的全部字段，如果结果集中的字段与fieldstr和fieldlen不符，会产生意外。
  // 但是，在本程序中，不方便判断结果集中的字段与fieldstr和fieldlen是否相符。
  for (int ii=0;ii<ifieldcount;ii++)
  {
    stmt.bindout(ii+1,strfieldvalue[ii],ifieldlen[ii]);
  }

  // 如果是增量挖掘。
  if (strlen(starg.incfield)!=0) stmt.bindin(1,&incfieldvalue);

  if (stmt.execute()!=0)
  {
    logfile.Write("stmt.execute() failed.\n%s\n%s\n",stmt.m_sql,stmt.m_cda.message); return false;
  }

  while (true)
  {
    memset(strfieldvalue,0,sizeof(strfieldvalue));

    if (stmt.next() != 0) break;

    if (File.IsOpened()==false)
    {
      crtxmlfilename(); // 生成xml文件名。

      if (File.OpenForRename(strxmlfilename,"w+")==false)
      {
        logfile.Write("File.OpenForRename(%s) failed.\n",strxmlfilename); return false;
      }

      File.Fprintf("<data>\n");
    }

    for (int jj=0;jj<ifieldcount;jj++)
      File.Fprintf("<%s>%s</%s>",strfieldname[jj],strfieldvalue[jj],strfieldname[jj]);

    File.Fprintf("<endl/>\n");

    // 如果记录数达到1000行。
    if (stmt.m_cda.rpc%1000==0)
    {
      File.Fprintf("</data>");

      if (File.CloseAndRename()==false)
      {
        logfile.Write("File.CloseAndRename(%s) failed.\n",strxmlfilename); return false;
      }

      logfile.Write("生成文件%s(1000)。\n",strxmlfilename,stmt.m_cda.rpc);
    }

    // 更新自增字段的最大值。
    if (incfieldvalue<atol(strfieldvalue[incfieldpos])) incfieldvalue=atol(strfieldvalue[incfieldpos]);
  }

  if (File.IsOpened()==true)
  {
    File.Fprintf("</data>");

    if (File.CloseAndRename()==false)
    {
      logfile.Write("File.CloseAndRename(%s) failed.\n",strxmlfilename); return false;
    }

    logfile.Write("生成文件%s(%d)。\n",strxmlfilename,stmt.m_cda.rpc%1000);
  }

  // 把增量字段的值写入文件中。
  if (stmt.m_cda.rpc>0) writeincfile();  

  return true;
}

// 生成xml文件名。
void crtxmlfilename()
{
  static int ii=1;
  char strlocaltime[21];
  memset(strlocaltime,0,sizeof(strlocaltime));
  LocalTime(strlocaltime,"yyyymmddhh24miss");

  memset(strxmlfilename,0,sizeof(strxmlfilename));
  snprintf(strxmlfilename,300,"%s/%s_%s_%s_%d.xml",starg.outpath,starg.bfilename,strlocaltime,starg.efilename,ii++);
}

// 读取增量采集标志字段的值存放的文件，结果保存在incfieldvalue变量中。
bool readincfile()
{
  incfieldvalue=0;

  if (strlen(starg.incfield)==0) return true;

  char strincfieldvalue[51];
  memset(strincfieldvalue,0,sizeof(strincfieldvalue));

  CFile File;

  // 如果打开已挖掘数据的递增字段最大值存放的文件失败，表示是第一次运行程序，也不必返回失败。
  // 也可以是文件丢了，那也没办法，只能重新挖掘。
  if (File.Open(starg.incfilename,"r") == false) return true;

  File.FFGETS(strincfieldvalue,30);

  incfieldvalue=atol(strincfieldvalue);

  logfile.Write("上次挖掘数的位置（%s=%ld）。\n",starg.incfield,incfieldvalue);

  File.Close();

  return true;
}

// 把增量字段的值写入文件中
bool writeincfile()
{
  if (strlen(starg.incfield)==0) return true;

  CFile File;

  if (File.Open(starg.incfilename,"w+") == false)
  {
    logfile.Write("File.Open(%s) failed.\n",starg.incfilename); return false;
  }

  File.Fprintf("%ld",incfieldvalue);

  File.Close();

  return true;
}

