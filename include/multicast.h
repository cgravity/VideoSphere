#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <vector>
#include <string>

struct MC_Client
{
    pthread_t thread;
    bool exit_flag;
    
    int fd;
    struct sockaddr_in addr;
    int addrlen;
    
    // frame dimensions
    // data.size() MUST equal exactly width*height*3
    uint32_t width;
    uint32_t height;
    
    // stores the most recent frame data received from the network
    std::vector<unsigned char> data;
    
    MC_Client()
    {
        exit_flag = false;
        
        addrlen = sizeof(addr);
        
        fd = 0;
        width = 0;
        height = 0;
    }
    
    void setup_data()
    {
        data.clear();
        data.insert(data.begin(), width*height*3, '\0');
    }
    
    void setup_fd(const std::string& multicast_ip, 
        const std::string& interface_ip,
        unsigned short port);
    
    void start_thread();
    void loop();
};

struct MC_Server
{
    int fd;
    struct sockaddr_in addr;
    int addrlen;
    uint16_t max_chunk_size;
    
    MC_Server();
    
    void setup_fd(const std::string& multicast_ip, 
        const std::string& interface_ip,
        unsigned short port,
        unsigned int ttl=1);
    
    // splits the data up based on max_chunk_size and sends it via multicast
    // to all listening clients immediately
    void send(uint64_t length, unsigned char* data);
};


