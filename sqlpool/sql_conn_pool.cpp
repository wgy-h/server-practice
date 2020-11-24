#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_conn_pool.h"

using namespace std;

connection_pool::connection_pool()
{
    m_CurConn=0;
    m_FreeConn=0;
}

connection_pool *connection_pool::GetInstance()
{
    static connection_pool connpool;
    return &connpool;
}

void connection_pool::init(string url,string user,string password,string databasename,int port,int maxconn,int close_log)
{
    m_url=url;
    m_port=port;
    m_user=user;
    m_password=password;
    m_databasename=databasename;
    m_close_log=close_log;
    for (int i = 0; i < maxconn;++i)
    {
       MYSQL *con=NULL;
       con=mysql_init(con);
       
       if(NULL==con)
       {
           LOG_ERROR("MYSQL Error");
           exit(1);
       }
       
       con=mysql_real_connect(con,url.c_str(),user.c_str(),password.c_str()
                              ,databasename.c_str(),port,NULL,0);

       if(NULL==con)
       {
           LOG_ERROR("MYSQL Error");
           exit(1);
       }
       ConnList.push_back(con);
       ++m_FreeConn;
    }
    reserve=sem(m_FreeConn);
    m_MaxConn=m_FreeConn;
    
}

MYSQL *connection_pool::GetConnetion()
{
    MYSQL *con=NULL;
    if(0==ConnList.size())
        return NULL;
    reserve.wait();
    lock.lock();
    con=ConnList.front();
    ConnList.pop_front();

    --m_FreeConn;
    ++m_CurConn;
    lock.unlock();
    return con;
}

bool connection_pool::ReleaseConnection(MYSQL *con)
{
    if(NULL==con)
        return false;
    lock.lock();
    
    ConnList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();
    reserve.post();

    return true;
}

void connection_pool::DestroyPool()
{
    lock.lock();
    if (ConnList.size()>0)
    {
        for (auto i = ConnList.begin(); i != ConnList.end(); ++i)
        {
           MYSQL *con=*i;
           mysql_close(con);
        }
        m_CurConn=0;
        m_FreeConn=0;
        ConnList.clear();
    }
    lock.unlock();
}

int connection_pool::GetFreeConn()
{
    return this->m_FreeConn;
}

connection_pool::~connection_pool()
{
    DestroyPool();
}

connectionRAII::connectionRAII(MYSQL ** SQL,connection_pool *connpool)
{
    *SQL =connpool->GetConnetion();
    conRAII=*SQL;
    poolRAII=connpool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}