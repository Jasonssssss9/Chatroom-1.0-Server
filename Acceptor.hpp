#pragma once
#include "Reactor.hpp"
#include "Log.hpp"
#include "Handler.hpp"
#include "Socket.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h> 
#include <cerrno>
#include <iostream>


//listen_sock回调函数Acceptor

class Acceptor
{
public:
    static void Accept(Event& listen_event)
    {
        //listen_sock就绪，代表可能有多个连接就绪，必须while循环保证所有连接都被accept
        sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);
        while(true){
            int sock = accept(listen_event.sock_, (sockaddr*)&peer, &peer_len);
            if(sock > 0){
                //设置sock为非阻塞读写
                Sock::SetNonBlock(sock);

                //给新的sock建立Event，绑定回调函数，并且加入Reactor模型
                Event new_event(sock, listen_event.pr_);
                new_event.RegisterRecv(Handler::Receiver);
                new_event.RegisterSend(Handler::Sender);
                new_event.RegisterError(Handler::Errorer);

                listen_event.pr_->AddEvent(new_event, EPOLLIN | EPOLLET);
                LOG(INFO, std::string("Add new socket to reactor: ")+std::to_string(sock));
            }
            else{
                if(errno == EINTR){
                    //accept被信号中断
                    continue;
                }
                else if(errno == EAGAIN || errno == EWOULDBLOCK){
                    //对非阻塞来说，出现此错误代表底层数据已经读完了，即底层已经没有连接了
                    //只有确保底层数据读完才能退出
                    break;
                }
                else{
                    //真出错
                    std::cerr << "accept error!" << errno << std::endl;
                    LOG(ERROR, std::string("Accept error: ")+std::to_string(errno));
                    continue;
                }
            }
        }
    }   
};
