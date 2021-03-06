#include "multicast.h"
#include <errno.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
using namespace std;

#define MIN(a,b) ((a) < (b)? (a) : (b))

struct MC_Header
{
    uint64_t start_pos;
    uint64_t tick;
    uint16_t length;
} __attribute__((packed));

static void* mc_client_thread_main(void* arg)
{
    MC_Client* client = (MC_Client*)arg;
    client->loop();    
    
    return NULL;
}

void MC_Client::loop()
{
    // make sure that recvmsg times out so that we can periodically check
    // for the exit flag
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv))<0)
    {
        perror("setsockopt SO_RCVTIMEO");
        exit(1);
    }
    
    int nbytes;
    
    MC_Header header;
    unsigned char msgbuf[0xFFFF];
    memset(msgbuf, 0, sizeof(msgbuf));
    
    struct iovec iov[2];
    iov[0].iov_base = &header;
    iov[0].iov_len  = sizeof(header);
    iov[1].iov_base = msgbuf;
    iov[1].iov_len  = 0xFFFF;
    
    struct msghdr msg;
    msg.msg_name = &addr;
    msg.msg_namelen = addrlen;
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    
    while(!exit_flag)
    {
//        if((nbytes = recvfrom(fd, msgbuf, 0xFFFF, 0,
//            (struct sockaddr*)&addr, (socklen_t*)&addrlen)) < 0)
        if((nbytes = recvmsg(fd, &msg, 0)) < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS)
                continue; // time out
        
            perror("recvmsg");
            exit(1);
        }
        
        if(header.start_pos >= buffer[2].size())
            continue; // bad packet
        
        if(header.start_pos + header.length >= buffer[2].size())
            continue; // bad packet
        
        if(header.tick > frame_tick[2])
        {
            network_flip();
            frame_tick[2] = header.tick;
        }
        
        for(int i = 0; i < header.length; i++)
        {
            buffer[2][header.start_pos + i] = msgbuf[i];
        }
    }
}

void MC_Client::setup_fd(
    const string& multicast_ip,
    const string& interface_ip,
    unsigned short port)
{
    struct ip_mreq mreq;
    
    unsigned int yes = 1;
    
    // create socket
    if((fd = socket(AF_INET,SOCK_DGRAM,0)) < 0)
    {
        perror("socket");
        exit(1);
    }
    
    // set socket reuse option
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    {
        perror("setsockopt SO_REUSEADDR");
        exit(1);
    }
    
    // bind
    memset(&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    
    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }
    
    // join multicast group
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip.c_str());
    mreq.imr_interface.s_addr = (!interface_ip.empty()?
        inet_addr(interface_ip.c_str()) :
        htonl(INADDR_ANY));
    
    if(setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt IP_ADD_MEMBERSHIP");
        exit(1);
    }
    
    addrlen = sizeof(addr);
}

void MC_Client::start_thread()
{
    pthread_create(&thread, NULL, mc_client_thread_main, this);
}

MC_Server::MC_Server()
{
    fd = 0;
    // leave some overhead for IP headers, etc.
    //max_chunk_size = 0xFFFF - sizeof(MC_Header) - 256;
    max_chunk_size = 1024;
    addrlen = sizeof(addr);
    
    tick = 0;
}

void MC_Server::send(uint64_t length, unsigned char* data)
{
    MC_Header header;
    header.start_pos = 0;
    header.length = MIN(max_chunk_size, length);
    header.tick = tick;
    
    struct iovec iov[2];
    iov[0].iov_base = &header;
    iov[0].iov_len  = sizeof(header);
    
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.msg_name = &addr;
    msg.msg_namelen = addrlen;
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    
    while(length != 0)
    {
        iov[1].iov_base = data;
        iov[1].iov_len = header.length;
        if(sendmsg(fd, &msg, 0) < 0)
            perror("sendmsg");
        
        length -= header.length;
        header.start_pos += header.length;
        data += header.length;
        
        header.length = MIN(max_chunk_size, length);
    }
    
    tick++;
}

void MC_Server::setup_fd(
    const std::string& multicast_ip, 
    const std::string& interface_ip,
    unsigned short port,
    unsigned int ttl)
{
    struct ip_mreq mreq;
    
    // create the socket
    if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }
    
    // specify how far to transmit multicast messages (default = 1)
    if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
    {
        perror("setsockopt TTL");
    }
    
    // request which interface to send multicast packets out of
    struct ip_mreq if_addr;
    if_addr.imr_multiaddr.s_addr = inet_addr(multicast_ip.c_str());
    if_addr.imr_interface.s_addr = (!interface_ip.empty()?
        inet_addr(interface_ip.c_str()) :
        htonl(INADDR_ANY));
    
    if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, 
        &if_addr, sizeof(if_addr))<0)
    {
        perror("setsockopt IP_MULTICAST_IF");
    }
    
    // prepare destination address specification
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(multicast_ip.c_str());
    addr.sin_port = htons(port);
}

void MC_Client::player_poll()
{
    lock();
    if(frame_tick[1] > frame_tick[0])
    {
        buffer[0].swap(buffer[1]);
        swap(frame_tick[0], frame_tick[1]);
    }
    unlock();
}

void MC_Client::network_flip()
{
    lock();
        buffer[1].swap(buffer[2]);
        swap(frame_tick[1], frame_tick[2]);
    unlock();
}

