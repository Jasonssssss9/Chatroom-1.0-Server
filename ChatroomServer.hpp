#pragma once
#include "Reactor.hpp"
#include "TCPServer.hpp"
#include "Acceptor.hpp"
#include "Log.hpp"
#include <iostream>

#define PORT 8081

class ChatroomServer
{
private:
    uint16_t port_;
    Reactor* pr_;
public:
    ChatroomServer(uint16_t port = PORT):port_(port)
    {
        pr_ = new Reactor();
    }

    void Loop()
    {
        //创建listen_sock并加入Reactor模型
        int listen_sock = TcpServer::GetInstance(port_)->GetLinstenSocket();
        LOG(INFO, std::string("Listen_sock is set: ")+std::to_string(listen_sock));

        //创建Event对象
        Event ev(listen_sock, pr_);
        //listen_sock只需要监测读就绪事件，并且回调函数为Acceptor
        ev.RegisterRecv(Acceptor::Accept);

        //将ev注册到reactor模型中
        pr_->AddEvent(ev, EPOLLIN | EPOLLET); //监测读以及工作在ET模式下


        //进入事件派发逻辑，服务器启动
        int timeout = 1000;
        while(true){
            pr_->Dispatcher(timeout);
        }
    }
};