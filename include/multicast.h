#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <vector>
#include <string>
#include <cstring>

#define MULTICAST_DEFAULT_PORT 4546

struct MC_Client
{
    pthread_t thread;
    bool exit_flag;
    
    int fd;
    struct sockaddr_in addr;
    int addrlen;
    
    // frame dimensions
    // buffer size MUST equal exactly width*height*3
    uint32_t width;
    uint32_t height;
    
    // stores the most recent frame data received from the network
    // buffer[0] is for the video player, buffer[2] is currently being
    // accumulated from the network, and buffer[1] is a spare buffer
    // swapped between the two for triple buffering.
    std::vector<unsigned char> buffer[3];
    uint64_t frame_tick[3]; // for determining if the buffers are newer
    
    pthread_mutex_t mutex; // protects buffer[0], buffer[1], and frame_tick
    
    void player_poll();  // called by player to get latest buffer[0] before draw
    void network_flip(); // called by network thread to store buffer[2]
    
    void lock()
    {
        pthread_mutex_lock(&mutex);
    }
    
    void unlock()
    {
        pthread_mutex_unlock(&mutex);
    }
    
    
    MC_Client()
    {
        exit_flag = false;
        
        addrlen = sizeof(addr);
        
        fd = 0;
        width = 0;
        height = 0;
        
        mutex = PTHREAD_MUTEX_INITIALIZER;
        std::memset(&frame_tick, 0, sizeof(frame_tick));
    }
    
    void setup_data()
    {
        for(int i = 0; i < 3; i++)
        {
            buffer[i].clear();
            buffer[i].insert(buffer[i].begin(), width*height*3, '\0');
        }
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
    
    uint64_t tick;
    
    MC_Server();
    
    void setup_fd(const std::string& multicast_ip, 
        const std::string& interface_ip,
        unsigned short port,
        unsigned int ttl=1);
    
    // splits the data up based on max_chunk_size and sends it via multicast
    // to all listening clients immediately
    void send(uint64_t length, unsigned char* data);
};


