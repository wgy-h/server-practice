#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

using namespace std;

class config
{
    public:
    config();
    ~config(){};
    
    void parse_arg(int argc,char* argv[]);

    int port;
    int logwrite;
    int trigmode;
    int listentrigmode;
    int conntrigmode;
    int opt_linger;
    int sql_num;
    int thread_num;
    int close_log;
    int actor_model;
};

#endif
