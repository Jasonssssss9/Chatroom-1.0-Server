#pragma once
#include "Socket.hpp"
#include "pthread.h"
#include "Log.hpp"
#include <mutex>

#define PORT 8081

class TcpServer
{
private:
    uint16_t port_;
    static TcpServer* pt_;

    TcpServer(uint16_t port = PORT):port_(port){}
public: 
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // static TcpServer* GetInstance(uint16_t port)
    // {
    //     static pthread_mutex_t pt_mutex = PTHREAD_MUTEX_INITIALIZER;
    //     if(pt_  == nullptr){
    //         pthread_mutex_lock(&pt_mutex);
    //         if(pt_ == nullptr){
    //             pt_ = new TcpServer(port);
    //         }
    //         pthread_mutex_unlock(&pt_mutex);
    //     }
    //     return pt_;
    // }

    static TcpServer* GetInstance(uint16_t  port)
    {
        static std::mutex mtx;
        if(pt_ == nullptr){
            {
                std::unique_lock<std::mutex> u_lock(mtx);
                if(pt_ == nullptr){
                    pt_ = new TcpServer(port);
                }
            }
        }
        return pt_;
    }

    // static void DelInstance()
    // {
    //     static pthread_mutex_t pt_mutex = PTHREAD_MUTEX_INITIALIZER;
    //     if(pt_ != nullptr){
    //         pthread_mutex_lock(&pt_mutex);
    //         if(pt_ != nullptr){
    //             delete pt_;
    //         }
    //         pthread_mutex_unlock(&pt_mutex);
    //     }
    // }

    static void DelInstance()
    {
        static std::mutex mtx;
        if(pt_ != nullptr){
            {
                std::unique_lock<std::mutex> u_lock(mtx);
                if(pt_ != nullptr){
                    delete pt_;
                }
            }
        }
    }

    //进行网络连接，先创建listen socket并bind，listen
    //使用reactor模型，首先需将listen socket注册到reactor模型中
    int GetLinstenSocket()
    {
        int listen_sock = Sock::Socket(1);
        Sock::SetNonBlock(listen_sock);   //使用ET模式，需要设置非阻塞模式
        Sock::Bind(listen_sock, port_);
        Sock::Listen(listen_sock);

        return listen_sock;
    }
};
