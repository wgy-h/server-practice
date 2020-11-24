#include "config.h"
#include <iostream>
int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    string user = "root";
    string passwd = "123";
    string databasename = "mydb";

    //命令行解析
    config config;
    config.parse_arg(argc, argv);

    webserver server;

    //初始化
    server.init(config.port, user, passwd, databasename, config.logwrite,
                config.opt_linger, config.trigmode, config.sql_num, config.thread_num,
                config.close_log, config.actor_model);

    std::cout << "begin" << std::endl;
    //日志
    server.log_write();

    //数据库
    server.sql_pool();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventlisten();

    //运行
    server.eventloop();

    std::cout << "end" << std::endl;

    return 0;
}