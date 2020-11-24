#ifndef _CONNECTION_POOL
#define _CONNECTION_POOL
#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool
{
    public:
    MYSQL *GetConnetion();
    bool ReleaseConnection(MYSQL *conn);
    int GetFreeConn();
    void DestroyPool();

    static connection_pool *GetInstance();
    void init(string url,string user, string password,string databasename,int port, int maxconn,int close_log);

    string m_url;
    string m_port;
    string m_user;
    string m_password;
    string m_databasename;
    int m_close_log;
    private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;
    int m_CurConn;
    int m_FreeConn;
    locker lock;
    list<MYSQL *>ConnList;
    sem reserve;
};

class connectionRAII
{
    public:
    connectionRAII(MYSQL **con,connection_pool *connpool);
    ~connectionRAII();
    private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};
#endif // !_CONNECTION_POOL
