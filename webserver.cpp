#include "webserver.h"

webserver::webserver()
{
    users=new http_conn[MAX_FD];
    char server_path[200];
    getcwd(server_path,200);
    char root[6]="/root";
    m_root=(char *)malloc(strlen(server_path)+strlen(root)+1);
    strcpy(m_root,server_path);
    strcat(m_root,root);
    users_timer=new client_data[MAX_FD];
}

webserver::~webserver()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void webserver::init(int port,string user,string password, string databasename,int log_wrirte,
                     int opt_linger,int trigmode,int sql_num,int thread_num,int close_log,int actor_model)
{
    m_port=port;
    m_user=user;
    m_password=password;
    m_databasename=databasename;
    m_sql_num=sql_num;
    m_thread_num=thread_num;
    m_log_write=log_wrirte;
    m_opt_linger=opt_linger;
    m_trigmode=trigmode;
    m_close_log=close_log;
    m_actormodel=actor_model;
}

void webserver::trig_mode()
{
    m_listentrigmode=2&m_trigmode;
    m_conntrigmode=1&m_trigmode;
}

void webserver::log_write()
{
    if(0==m_close_log)
    {
        if(1==m_log_write)
        {
            log::get_instance()->init("./serverlog",m_close_log,2000,800000,800);
        }
        else
        {
            log::get_instance()->init("./serverlog",m_close_log,2000,800000,0);
        }
        
    }
}

void webserver::sql_pool()
{
    m_connpool=connection_pool::GetInstance();
    m_connpool->init("localhost",m_user,m_password,m_databasename,3306,m_sql_num,m_close_log);
    users->initmysql_result(m_connpool);
}
void webserver::thread_pool()
{
    m_pool=new threadpool<http_conn>(m_actormodel,m_connpool,m_thread_num);
}
void webserver::eventlisten()
{
    m_listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(m_listenfd>=0);

    if(0==m_opt_linger)
    {
        struct linger tmp={0,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }
    else if (1==m_opt_linger)
    {
        struct linger tmp={1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }

    int ret=0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(m_port);

    int flag=1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));
    ret=bind(m_listenfd,(struct sockaddr *)&address,sizeof(address));
    assert(ret>=0);
    ret=listen(m_listenfd,5);
    assert(ret>=0);

    util.init(TIMESLOT);

    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd=epoll_create(5);
    assert(m_epollfd!=-1);

    util.addfd(m_epollfd,m_listenfd,false,m_listentrigmode);
    http_conn::m_epollfd=m_epollfd;

    ret=socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd);
    assert(ret!=-1);
    util.setnonblocking(m_pipefd[1]);
    util.addfd(m_epollfd,m_pipefd[0],false,0);

    util.addsig(SIGPIPE,SIG_IGN);
    util.addsig(SIGALRM,util.sig_handler,false);
    util.addsig(SIGTERM,util.sig_handler,false);

    alarm(TIMESLOT);
    utils::u_pipefd=m_pipefd;
    utils::u_epollfd=m_epollfd;
}

void webserver::timer(int connfd,struct sockaddr_in client_address)
{
    users[connfd].init(connfd,client_address,m_root,m_conntrigmode,m_close_log,m_user,m_password,m_databasename);
    
    users_timer[connfd].address=client_address;
    users_timer[connfd].sockfd=connfd;
    util_timer *timer=new util_timer;
    timer->user_data=&users_timer[connfd];
    timer->cb_func=cb_func;
    time_t cur=time(NULL);
    timer->expire=cur+3*TIMESLOT;
    users_timer[connfd].timer=timer;
    util.m_timer_lst.add_timer(timer);
}

void webserver::adjust_timer(util_timer *timer)
{
    time_t cur=time(NULL);
    timer->expire=cur+3*TIMESLOT;
    util.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s","adjust timer once");
}

void webserver::deal_timer(util_timer *timer,int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if(timer)
    {
        util.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d",users_timer[sockfd].sockfd);
}

bool webserver::dealclinetdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength=sizeof(client_address);
    if(0==m_listentrigmode)
    {
        int connfd =accept(m_listenfd,(struct sockaddr *)&client_address,&client_addrlength);
        if(connfd<0)
        {
            LOG_ERROR("%s:errno is %d","accept error",errno);
            return false;
        }
        if(http_conn::m_user_count>=MAX_FD)
        {
            util.show_error(connfd,"internal server busy");
            LOG_ERROR("%s","internal server busy");
            return false;
        }
        timer(connfd,client_address);
    }
    else
    {
        while (1)
        {
            int connfd=accept(m_listenfd,(struct sockaddr *)&client_address,&client_addrlength);
            if(connfd<0)
            {
                LOG_ERROR("%s:errno is %d","accept error",errno);
                break;
            }
            if(http_conn::m_user_count>=MAX_FD)
            {
                util.show_error(connfd,"internal server busy");
                LOG_ERROR("%s","internal server busy");
                break;
            }
            timer(connfd,client_address);
        }
        return false;
        
    }
    return true;
}

bool webserver::dealwithsigal(bool &timeout,bool &stop_server)
{
    int ret=0;
    int sig;
    char signals[1024];
    ret=recv(m_pipefd[0],signals,sizeof(signals),0);
    if(-1==ret)
    {
        return false;
    }
    else if(0==ret)
    {
        return false;
    }
    else
    {
        for(int i=0;i<ret;++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout=true;
                break;
            }
            case SIGTERM:
            {
                stop_server=true;
                break;
            }
            }
        }
    }
    return true;  
}

void webserver::dealwithread(int sockfd)
{
    util_timer *timer=users_timer[sockfd].timer;

    if(1==m_actormodel)
    {   
        if(timer)
        {

        adjust_timer(timer);
        }
    
        m_pool->append(users+sockfd,0);
    
        while (true)
        {
            if(1==users[sockfd].improv)
            {
                if(1==users[sockfd].timer_flag)
                {
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag=0;
                }
                users[sockfd].improv=0;
                break;
            }
        }
    }
    else
    {
        if(users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)",inet_ntoa(users[sockfd].get_address()->sin_addr));
            m_pool->append_p(users+sockfd);
            if(timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer,sockfd);
        }
    }
    
}

void webserver::dealwithwrite(int sockfd)
{
    util_timer *timer=users_timer[sockfd].timer;

    if(1==m_actormodel)
    {
        if(timer)
        {
            adjust_timer(timer);
        }
        m_pool->append(users+sockfd,1);
        while (true)
        {
            if(1==users[sockfd].improv)
            {
                if(1==users[sockfd].timer_flag)
                {
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag=0;
                }
                users[sockfd].improv=0;
                break;

            }
        }
        
    }
    else
    {
        if(users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)",inet_ntoa(users[sockfd].get_address()->sin_addr));
            if(timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer,sockfd);
        }
    }

}

void webserver::eventloop()
{
    bool timeout=false;
    bool stop_server=false;
    
    while (!stop_server)
    {
        int number=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if(number<0&&errno!=EINTR)
        {
            LOG_ERROR("%s","epoll failure");
            break;
        }
        for (int i = 0; i < number; ++i)
        {
            int sockfd=events[i].data.fd;
            if(sockfd==m_listenfd)
            {
                bool flag= dealclinetdata();
                if(false==flag)
                {
                    continue;
                }
            }
            else if (events[i].events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR))
            {
                util_timer *timer=users_timer[sockfd].timer;
                deal_timer(timer,sockfd);
            }
            else if ((sockfd==m_pipefd[0])&&events[i].events&EPOLLIN)
            {
                bool flag=dealwithsigal(timeout,stop_server);
                if(false==flag)
                {
                    LOG_ERROR("%s","dealclientdata failure");
                }
            }
            else if (events[i].events&EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (events[i].events&EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if(timeout)
        {
            util.timer_handler();
            LOG_INFO("%s","timer tick");
            timeout=false;
        }
        
    }
    
}