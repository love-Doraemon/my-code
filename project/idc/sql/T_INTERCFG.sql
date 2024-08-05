delete from T_INTERCFG;

/* 获取全部的站点参数。*/
insert into T_INTERCFG(intername,intercname,selectsql,colstr,bindin) 
values ('getzhobtcode','全国站点参数','select obtid,cityname,provname,lat,lon,height from T_ZHOBTCODE','obtid,cityname,provname,lat,lon,height',null);

/* 按站点获取全国站点分钟观测数据。 */
insert into T_INTERCFG(intername,intercname,selectsql,colstr,bindin) 
values ('getzhobtmind1','全国站点分钟观测数据(站点)','select obtid,date_format(ddatetime,''%%Y%%m%%d%%H%%i%%s''),t,p,u,wd,wf,r,vis from T_ZHOBTMIND where obtid=:1','obtid,ddatetime,t,p,u,wd,wf,r,vis','obtid');

/* 按时间段获取全国站点分钟观测数据。 */
insert into T_INTERCFG(intername,intercname,selectsql,colstr,bindin) 
values ('getzhobtmind2','全国站点分钟观测数据(时间段)''select obtid,date_format(ddatetime,''%%Y%%m%%d%%H%%i%%s''),t,p,u,wd,wf,r,vis from T_ZHOBTMIND where ddatetime>=str_to_date(:1,''%%Y%%m%%d%%H%%i%%s'') and ddatetime<=str_to_date(:2,''%%Y%%m%%d%%H%%i%%s'')','obtid,ddatetime,t,p,u,wd,wf,r,vis','begintime,endtime');

/* 按站点和时间段获取全国站点分钟观测数据。 */
insert into T_INTERCFG(intername,intercname,selectsql,colstr,bindin) 
values ('getzhobtmind3','全国站点分钟观测数据(站点和时间)','select obtid,date_format(ddatetime,''%%Y%%m%%d%%H%%i%%s''),t,p,u,wd,wf,r,vis from T_ZHOBTMIND where obtid=:1 and ddatetime>=str_to_date(:2,''%%Y%%m%%d%%H%%i%%s'') and ddatetime<=str_to_date(:3,''%%Y%%m%%d%%H%%i%%s'')','obtid,ddatetime,t,p,u,wd,wf,r,vis','obtid,begintime,endtime');



