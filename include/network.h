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
#include <stdexcept>

struct Connection;

enum IncomingMsgState
{
    MSG_STATE_NO_DATA,
    MSG_STATE_WAIT_FOR_SIZE,
    MSG_STATE_READING_DATA
};

struct ParseError : public std::runtime_error 
{
    ParseError(const std::string& what_arg) : std::runtime_error(what_arg) {};
    ParseError(const char* what_arg) : std::runtime_error(what_arg) {};
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
    
    void reply(const Message& m)
    {
        reply(m.bytes);
    }
    
    // message parsing
    int parse_pos;
    
    void rewind()
    {
        parse_pos = 0;
    }
    
    int32_t read_int32()
    {
        if(parse_pos+4 > bytes.size())
            throw ParseError("Not enough bytes in message to read int32");
        
        int32_t result = 0;
        
        result |= bytes[parse_pos++] << 24;
        result |= bytes[parse_pos++] << 16;
        result |= bytes[parse_pos++] <<  8;
        result |= bytes[parse_pos++];
        
        return result;
    }
    
    uint32_t read_uint32()
    {
        if(parse_pos+4 > bytes.size())
            throw ParseError("Not enough bytes in message to read uint32");
        
        uint32_t result = 0;
        
        result |= bytes[parse_pos++] << 24;
        result |= bytes[parse_pos++] << 16;
        result |= bytes[parse_pos++] <<  8;
        result |= bytes[parse_pos++];
        
        return result;
    }
    
    int64_t read_int64()
    {
        if(parse_pos+8 > bytes.size())
            throw ParseError("Not enough bytes in message to read int64");
        
        int64_t result = 0;
        result |= bytes[parse_pos++] << 56;
        result |= bytes[parse_pos++] << 48;
        result |= bytes[parse_pos++] << 40;
        result |= bytes[parse_pos++] << 32;
        result |= bytes[parse_pos++] << 24;
        result |= bytes[parse_pos++] << 16;
        result |= bytes[parse_pos++] <<  8;
        result |= bytes[parse_pos++];
        
        return result;
    }
    
    uint64_t read_uint64()
    {
        if(parse_pos+8 > bytes.size())
            throw ParseError("Not enough bytes in message to read uint64");
        
        uint64_t result = 0;
        result |= bytes[parse_pos++] << 56;
        result |= bytes[parse_pos++] << 48;
        result |= bytes[parse_pos++] << 40;
        result |= bytes[parse_pos++] << 32;
        result |= bytes[parse_pos++] << 24;
        result |= bytes[parse_pos++] << 16;
        result |= bytes[parse_pos++] <<  8;
        result |= bytes[parse_pos++];
        
        return result;
    }
    
    float read_float()
    {
        if(parse_pos+4 > bytes.size())
            throw ParseError("Not enough bytes in message to read float");
        
        uint32_t result = 0;
        
        result |= bytes[parse_pos++] << 24;
        result |= bytes[parse_pos++] << 16;
        result |= bytes[parse_pos++] <<  8;
        result |= bytes[parse_pos++];
        
        return *(float*)&result;
    }
    
    double read_double()
    {
        if(parse_pos+8 > bytes.size())
            throw ParseError("Not enough bytes in message to read double");
        
        uint64_t result = 0;
        result |= bytes[parse_pos++] << 56;
        result |= bytes[parse_pos++] << 48;
        result |= bytes[parse_pos++] << 40;
        result |= bytes[parse_pos++] << 32;
        result |= bytes[parse_pos++] << 24;
        result |= bytes[parse_pos++] << 16;
        result |= bytes[parse_pos++] <<  8;
        result |= bytes[parse_pos++];
        
        return *(double*)&result;
    }
    
    char read_char()
    {
        if(parse_pos+1 > bytes.size())
            throw ParseError("Not enough bytes in message to read char");
        
        return (char)bytes[parse_pos++];
    }
    
    unsigned char read_byte()
    {
        if(parse_pos+1 > bytes.size())
            throw ParseError("Not enough bytes in message to read byte");
        
        return bytes[parse_pos++];
    }
    
    unsigned char read_uchar()
    {
        if(parse_pos+1 > bytes.size())
            throw ParseError("Not enough bytes in message to read uchar");
        
        return bytes[parse_pos++];
    }
    
    std::string read_string()
    {
        try 
        {
            uint32_t size = read_uint32();
            std::string result;
        
            for(uint32_t i = 0; i < size; i++)
            {
                result.push_back(read_char());
            }
            
            return result;
        } 
        catch(ParseError p)
        {
            throw ParseError("Failed to extract string from message");
        }
    }
    
    // message serialization
    void write_byte(unsigned char value)
    {
        bytes.push_back(value);
    }
    
    void write_uchar(unsigned char value)
    {
        write_byte(value);
    }
    
    void write_char(char value)
    {
        write_byte((unsigned char)value);
    }
    
    void write_uint32(uint32_t value)
    {
        write_byte((unsigned char)((value & 0xFF000000) >> 24));
        write_byte((unsigned char)((value & 0x00FF0000) >> 16));
        write_byte((unsigned char)((value & 0x0000FF00) >>  8));
        write_byte((unsigned char)((value & 0x000000FF)));
    }
    
    void write_int32(int32_t value)
    {
        write_uint32((uint32_t)value);
    }
    
    void write_uint64(uint64_t value)
    {
        write_byte((unsigned char)((value & 0xFF00000000000000) >> 56));
        write_byte((unsigned char)((value & 0x00FF000000000000) >> 48));
        write_byte((unsigned char)((value & 0x0000FF0000000000) >> 40));
        write_byte((unsigned char)((value & 0x000000FF00000000) >> 32));
        write_byte((unsigned char)((value & 0x00000000FF000000) >> 24));
        write_byte((unsigned char)((value & 0x0000000000FF0000) >> 16));
        write_byte((unsigned char)((value & 0x000000000000FF00) >>  8));
        write_byte((unsigned char)((value & 0x00000000000000FF)));
    }
    
    void write_int64(int64_t value)
    {
        write_uint64((uint64_t)value);
    }
    
    void write_float(float value)
    {
        uint32_t value2 = *(uint32_t*)&value;
        write_uint32(value2);
    }
    
    void write_double(double value)
    {
        uint64_t value2 = *(uint64_t*)&value;
        write_uint64(value2);
    }
    
    void write_string(const std::string& value)
    {
        if(value.size() > 0xFFFFFFFF)
            throw std::runtime_error("string is too big to serialize");
        
        uint32_t size = (uint32_t)value.size();
        write_uint32(size);
        
        for(size_t i = 0; i < value.size(); i++)
            write_byte(value[i]);
    }
    
    void write_string(const char* value)
    {
        write_string(std::string(value));
    }
    
    void write_string(const char* value, uint32_t size)
    {
        write_string(std::string(value, size));
    }
    
    Message() : fd(0), parse_pos(0) {}
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

