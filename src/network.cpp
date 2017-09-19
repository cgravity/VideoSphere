#include "network.h"
using namespace std;

#include <netdb.h>

NetworkThread::~NetworkThread() {}

static void* network_thread_main(void* arg)
{
    NetworkThread* nt = (NetworkThread*)arg;
    nt->loop();    
    
    return NULL;
}

void Server::start_thread()
{
    struct sockaddr_in sa;
    int listen_sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listen_sock_fd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    int flag = 1;
    
    setsockopt(
        listen_sock_fd, 
        SOL_SOCKET,
        SO_REUSEADDR,
        &flag,
        sizeof(flag));
    
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if(bind(listen_sock_fd, (struct sockaddr*)&sa, sizeof(sa)) == -1)
    {
        perror("bind");
        close(listen_sock_fd);
        exit(EXIT_FAILURE);
    }
    
    if(listen(listen_sock_fd, 255) == -1)
    {
        perror("listen");
        close(EXIT_FAILURE);
    }
    
    pollfd listen_pfd;
    memset(&listen_pfd, 0, sizeof(listen_pfd));
    listen_pfd.fd = listen_sock_fd;
    listen_pfd.events = POLLIN;
    
    pollfds.push_back(listen_pfd);
    conns.push_back(Connection(listen_sock_fd)); // placeholder
    
    // ----------------------------------------
    
    pthread_create(&thread, NULL, network_thread_main, this);
}

void Server::loop()
{
    while(true)
    {
        pthread_mutex_lock(&mutex);
        bool quit = quit_flag;
        pthread_mutex_unlock(&mutex);
        
        if(quit)
            break;
        
        int status = poll(&pollfds[0], pollfds.size(), 500);
        
        if(status == 0)
        {
            // timeout
            //cout << "Timeout\n";
            continue;
        }
        
        if(status < 0)
        {
            perror("poll");
            exit(EXIT_FAILURE);
        }
        
        // handle new connection
        if(pollfds[0].revents & POLLIN)
        {
            // listen socket always in position 0; new connection detected
            pollfd new_connection;
            memset(&new_connection, 0, sizeof(new_connection));
            
            new_connection.fd = accept(pollfds[0].fd, NULL, NULL);
            new_connection.events = POLLIN;
            
            if(new_connection.fd < 0)
            {
                perror("accept");
            }
            else
            {
                // disable Naggle's algorithm
                int flag = 1;
                setsockopt(
                    new_connection.fd,
                    IPPROTO_TCP,
                    TCP_NODELAY,
                    (char*)&flag,
                    sizeof(int));
                    
                pollfds.push_back(new_connection);
                
                Connection conn(new_connection.fd);
                conn.msg_mutex = &mutex;
                conn.msg_queue = &messages;
                
                conns.push_back(conn);
            }
        }
        
        // parse data from all connections
        for(size_t i = 1; i < pollfds.size(); i++)
        {
            if(pollfds[i].revents & POLLIN)
            {
                unsigned char bytes[4096];
                ssize_t size = recv(
                    pollfds[i].fd, 
                    &bytes[0], 
                    sizeof(bytes), 
                    MSG_DONTWAIT);
                
                if(size < 0)
                {
                    perror("recv");
                }
                
                if(size <= 0)
                {
                    close(pollfds[i].fd);
                    pollfds[i].fd = 0;
                    continue;
                }
                
                conns[i].on_data(size, bytes);
            }
        }
        
        // remove disconnected sockets
        for(size_t i = pollfds.size()-1; i > 0; i--)
        {
            if(pollfds[i].fd == 0)
            {
                pollfds.erase(pollfds.begin() + i);
                conns.erase(conns.begin() + i);
            }
        }
        
    } // while(true)
}

Client::Client(std::string host, unsigned short port) : NetworkThread()
{
    this->host = host;
    this->port = port;
}

void Client::start_thread()
{
    struct hostent* he;
    
    if((he = gethostbyname(host.c_str())) == NULL)
    {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in sa;

    int sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    memset(&sa, 0, sizeof(sa));
    
    sa.sin_family = AF_INET;
    sa.sin_port = htons(VIDEO_SPHERE_PORT);
    //int res = inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
    
    if(connect(sock_fd, (struct sockaddr*)&sa, sizeof(sa)) == -1)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    
    server_connection.fd = sock_fd;
    server_connection.msg_queue = &messages;
    server_connection.msg_mutex = &mutex;
    
    // ----------------------------------------
    
    pthread_create(&thread, NULL, network_thread_main, this);
}

void Client::loop()
{
    pollfd pfd;
    pfd.fd = server_connection.fd;
    pfd.events = POLLIN;
    
    while(true)
    {
        pthread_mutex_lock(&mutex);
        bool quit = quit_flag;
        pthread_mutex_unlock(&mutex);
        
        if(quit)
            break;
        
        int status = poll(&pfd, 1, 500);
        
        if(status < 0)
        {
            perror("poll");
            exit(EXIT_FAILURE);
        }
        
        if(status == 0)
            continue; // timeout
        
        // new data is waiting!
        unsigned char bytes[4096];
        ssize_t size = recv(
            pfd.fd, 
            &bytes[0], 
            sizeof(bytes), 
            MSG_DONTWAIT);
        
        if(size < 0)
        {
            perror("recv");
        }
        
        if(size <= 0)
        {
            close(pfd.fd);
            cout << "FIXME: Reconnect?\n";
            exit(EXIT_FAILURE);
        }
        
        server_connection.on_data(size, bytes);
    }
}

void DummyNetworkingThread::start_thread()
{
    pthread_create(&thread, NULL, network_thread_main, this);
}

