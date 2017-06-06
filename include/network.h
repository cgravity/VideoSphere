#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <stdint.h>

#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <pthread.h>
#include <list>

struct Connection;

enum IncomingMsgState
{
    MSG_STATE_NO_DATA,
    MSG_STATE_WAIT_FOR_SIZE,
    MSG_STATE_READING_DATA
};


struct Message
{
    int fd; // message came from this socket
    std::vector<unsigned char> bytes;
    
    std::string as_string() const
    {
        std::string s((const char*)&bytes[0], bytes.size());
        return s;
    }
    
    size_t size()
    {
        return bytes.size();
    }
    
    void reply(const std::string& data)
    {
        uint32_t size = (uint32_t)data.size();
        size = htonl(size);
        ::send(fd, &size, sizeof(size), 0);
        ::send(fd, &data[0], data.size(), 0);
    }
    
    void reply(const std::vector<unsigned char>& data)
    {
        uint32_t size = (uint32_t)data.size();
        size = htonl(size);
        ::send(fd, &size, sizeof(size), 0);
        ::send(fd, &data[0], data.size(), 0);
    }
};

struct Connection
{
    int fd;
    
    // recv info
    IncomingMsgState state;
    uint32_t expect_bytes;
    std::vector<unsigned char> bytes;
    
    pthread_mutex_t* msg_mutex;
    std::vector<Message>* msg_queue;
    
    Connection(int FD = 0) : fd(FD), state(MSG_STATE_NO_DATA), 
        expect_bytes(0), msg_mutex(NULL), msg_queue(NULL) {}
    
    void send(const std::string& data)
    {
        uint32_t size = (uint32_t)data.size();
        size = htonl(size);
        ::send(fd, &size, sizeof(size), 0);
        ::send(fd, &data[0], data.size(), 0);
    }
    
    void send(const std::vector<unsigned char>& data)
    {
        uint32_t size = (uint32_t)data.size();
        size = htonl(size);
        ::send(fd, &size, sizeof(size), 0);
        ::send(fd, &data[0], data.size(), 0);
    }
    
    void on_data_byte(unsigned char byte)
    {
        switch(state)
        {
            case MSG_STATE_NO_DATA:
                state = MSG_STATE_WAIT_FOR_SIZE;
                // fallthrough
                
            case MSG_STATE_WAIT_FOR_SIZE:
                bytes.push_back(byte);
                
                if(bytes.size() == sizeof(uint32_t))
                {
                    expect_bytes = ntohl(*(uint32_t*)&bytes[0]);
                    bytes.clear();
                    state = MSG_STATE_READING_DATA;
                }
                break;
            
            case MSG_STATE_READING_DATA:
                bytes.push_back(byte);
                if(bytes.size() == expect_bytes)
                {
                    Message msg;
                    msg.bytes.swap(bytes);
                    msg.fd = fd;
                    
                    pthread_mutex_lock(msg_mutex);
                    msg_queue->push_back(msg);
                    pthread_mutex_unlock(msg_mutex);
                    
                    state = MSG_STATE_NO_DATA;
                    bytes.clear();
                }
                break;
        }
    }
    
    void on_data(ssize_t size, unsigned char* data)
    {
        for(ssize_t i = 0; i < size; i++)
        {
            on_data_byte(*data);
            data++;
        }
    }
};

class NetworkThread
{

  protected:
    pthread_mutex_t mutex;
    std::vector<Message> messages;
    pthread_t thread;
    
    bool quit_flag;
  
  public:
    NetworkThread()
    {
        pthread_mutex_init(&mutex, NULL);
        quit_flag = false;
    }
    
    virtual ~NetworkThread();
    
    void get_messages(std::vector<Message>& into_buffer)
    {       
        pthread_mutex_lock(&mutex);
        
        into_buffer.swap(messages);
        messages.clear();
        
        pthread_mutex_unlock(&mutex);
    }
    
    virtual void start_thread() = 0;
    virtual void send(const std::vector<unsigned char>& data) = 0;
    virtual void send(const std::string& data) = 0;
    
    virtual void set_quit()
    {
        pthread_mutex_lock(&mutex);
        quit_flag = true;
    }
    
    virtual void join()
    {
        void* return_value; // unused
        pthread_join(thread, &return_value);
    }
    
    virtual void loop() = 0;
};

class Server : public NetworkThread
{
    std::vector<pollfd> pollfds;
    std::vector<Connection> conns;
    
    unsigned short port;
    
  public:
    Server(unsigned short port = 2345) : NetworkThread(), port(port) {}
    
    virtual void send(const std::vector<unsigned char>& data)
    {
        // starts from 1 since 0 is the listening socket
        for(size_t i = 1; i < conns.size(); i++)
            conns[i].send(data);
    }
    
    virtual void send(const std::string& data)
    {
        for(size_t i = 1; i < conns.size(); i++)
            conns[i].send(data);
    }
    
    virtual void start_thread();
    
    // don't call this directly; use start_thread()
    virtual void loop();
};

class Client : public NetworkThread
{
    Connection server_connection;
    
    std::string host;
    unsigned short port;
    
  public:    
    Client(std::string host, unsigned short port);
    
    virtual void send(const std::vector<unsigned char>& data)
    {
        server_connection.send(data);
    }
    
    virtual void send(const std::string& data)
    {
        server_connection.send(data);
    }
    
    virtual void start_thread();
    
    // don't call this directly; use start_thread()
    void loop();
};

