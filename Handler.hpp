#pragma once
#include "Reactor.hpp"
#include "Log.hpp"
#include "Util.hpp"
#include "ThreadPool.hpp"
#include "Protocol.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cerrno>
#include <iostream>
#include <string>
#include <sstream>


class Handler
{
private:
    //ET模式下轮询监测，完成读任务，正常返回0，出错返回-1
    static int RecvHelper(int sock, std::string& out)
    {
        while(true){
            char buffer[1024];
            ssize_t s = recv(sock, buffer, sizeof(buffer)-1, 0);
            if(s > 0){
                //当s大于0，认为还没读完，继续读
                buffer[s] = '\0';
                out += buffer;
            }
            else if(s < 0){
                if(errno == EINTR){
                    //信号中断
                    continue;
                }
                else if(errno == EAGAIN || errno == EWOULDBLOCK){
                    //保证底层数据已经读完，即此次读取完毕，返回0
                    return 0;
                }
                else{
                    //出错
                    return -1;
                }
            }
            else{
                //s==0，表示对端关闭，也需要交给异常处理
                return -1;
            }
        }
    }

    //send_string: 输入输出型参数
    //ret >  0 : 缓冲区数据全部发完
    //ret == 0 : 本轮发送完, 但是缓冲区还有数据
    //ret <  0 : 发送失败
    static int SendHelper(int sock, std::string& send_string)
    {
        //send不一定一次把数据发完，因此采用经典total+size法多次发送
        const char* start = send_string.c_str();
        ssize_t total = 0; //表示已经发了多少
        ssize_t size = 0; //send返回值
        while(true){
            size = send(sock, start+total, send_string.size()-total, 0);
            if(size > 0){
                total += size;
                if(total == send_string.size()){
                    //这次缓冲区大小足够，一次发完
                    send_string.clear();
                    return 1;
                }
                //如果send没发完，则继续循环
            }
            else{
                if(errno == EINTR){
                    //信号阻塞，继续发
                    continue;
                }
                else if(errno == EAGAIN || errno == EWOULDBLOCK){
                    //说明缓冲区大小不够，本次发送已经发完，但是下次还要继续发
                    //需要将send_string前面已经发送的清除
                    send_string.erase(0, total);
                    return 0;
                }
                else{
                    //出错
                    return -1;
                }
            }
        }
    }

public:
    //event对应读事件
    static void Receiver(Event& event)
    {
        //读任务直接交给RecvHelper处理，结果直接读到inbuffer里
        //如果返回值为-1说明读出错，交给异常处理回调，之后退出
        if(RecvHelper(event.sock_, event.inbuffer_) == -1){
            if(event.errorCallback_){
                event.errorCallback_(event);
            }
            return;
        }
        LOG(INFO, std::string("Receive successfully, sock: ")+std::to_string(event.sock_));

        //for test
        // event.outbuffer_ = event.inbuffer_;
        // event.inbuffer_.clear();
        // event.pr_->EnableReadWrite(event.sock_, true, true);

        Task task([&]{
            Protocol::GetPerseMessage(event);
        });
        ThreadPool::GetInstance()->AddTask(task);
    }

    //event对应写事件
    static void Sender(Event& event)
    {
        //写任务直接交给SendHelper处理，将outbuffer里的内容直接读走
        //如果返回值为-1说明写出错，交给异常处理回调，之后退出
        int ret = SendHelper(event.sock_, event.outbuffer_);
        if(ret == -1){
            if(event.errorCallback_){
                event.errorCallback_(event);
            }
            return;
        }
        else if (ret == 1){ //outbuffer发送完毕，关闭写
            (event.pr_)->EnableReadWrite(event.sock_, true, false);
            LOG(INFO, std::string("Send successfully, sock: ")+std::to_string(event.sock_));
        }
        else if (ret == 0){ //outbuffer本轮发送完毕，等下一次写事件就绪，还要再发
            (event.pr_)->EnableReadWrite(event.sock_, true, true);
        }
        else{}
    }

    //event对应异常事件，直接关闭连接
    static void Errorer(Event& event)
    {
        event.pr_->DelEvent(event.sock_);
    }

};
