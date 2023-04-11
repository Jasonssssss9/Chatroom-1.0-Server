#pragma once
#include "Reactor.hpp"
#include "Log.hpp"
#include "Util.hpp"
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

    //获取初始行，自动去除结尾\r\n
    //粘包返回-1，正常则返回读取到的初始行大小，包括\r\n
    static int GetIniLine(Event& event)
    {
        int ret = Util::Readline(event.inbuffer_, event.recvMessage_.iniLine_);
        if(ret == -1){
            //没有读到\r\n，出现粘包问题，直接返回
            return -1;
        }
        event.recvMessage_.iniLine_.resize(event.recvMessage_.iniLine_.size()-2);
        LOG(INFO, event.recvMessage_.iniLine_);
        return ret;
    }

    //获取报头中的一行，也可能是空行，自动去除结尾\r\n
    //粘包返回-1；正常则返回读取到的报头总大小，包括\r\n与空行大小；如果读到空行；返回-2
    static int GetHeader(Event& event)
    {
        std::string tem_string;
        int ret = Util::Readline(event.inbuffer_, tem_string);
        if(ret == -1){
            //没有读到\r\n，出现粘包问题，直接返回
            return -1;
        }
        if(tem_string == "\r\n"){
            //如果读到空行，返回-2
            event.recvMessage_.blank_ = "\r\n";
            return -2;
        }
        //如果是正常的一行，那么就加入headers_中
        tem_string.resize(tem_string.size()-2);
        event.recvMessage_.headers_.push_back(tem_string);
        LOG(INFO, tem_string);

        return ret;
    }

    //获取正文数据
    //如果读完数据，返回0；没有读完数据，即in已经空了，返回-1
    static int GetBody(int len, const std::string& in, std::string& out)
    {
        if(in.size() >= len){
            //保证一定能读完数据，直接从in中读n个
            out += in.substr(0, len);
            return 0;
        }
        else{
            //不能读完数据
            out += in.substr(0);
            return -1;
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

        //在这里进行应用层业务处理的第一步：分解报文，处理粘包问题，并构建任务，交给线程池处理
        //粘包问题的解决：
        //分三部分：(1)iniline;(2)header+blank;(3)body
        //可能存在的粘包情况是，一部分只读了一部分，剩下一部分在下一个报文中
        //(1)对于iniline来说
        //   如果没读完一行，那么就直接退出，不向inline放东西，也不清理inbuffer，下次继续读剩下的
        //   如果正确读完一行，则清理inbuffer，继续读header
        //   进入判断前必须确定inline是空的，即从inline开始读，如果inline不为空，说明inline一定已经读完了
        //(2)对于header来说
        //   如果一行都没读完，那么直接退出，不向header里放东西，也不清理inbuffer
        //   while循环中，如果正确读完一行，则清理inbuffer的这一行，继续读下一行，直到读到空行再退出while循环
        //   header读完的标志是读到空行，进入header之前先判断blank，为空再进入；不为空说明header一定已经读完了
        //接下来是分析数据，必须判断inline和blank都非空，即证明(1)和(2)已经读完
        //(3)读取数据时，根据header中的Content-Length字段进行判断，没有数据就不读，有数据再读
        //   有数据时，必须保证读完数据长度个字节数据，如果没读完，则退出下次继续读，这时要清理inbuffer
        
        //读初始行，将初始行中\n去除
        int ret = 0;
        if(event.recvMessage_.iniLine_.size() == 0){
            ret = GetIniLine(event);
            if(ret == -1){
                //粘包直接退出不处理
                return;
            }
            event.inbuffer_.erase(0, ret);
        }
        
        //读报头，将报头每行的\n去除
        if((event.recvMessage_.iniLine_.size() != 0) && (event.recvMessage_.blank_.size() == 0)){
            while(true){
                ret = GetHeader(event);
                if(ret == -1){
                    //说明一行都没读完，直接返回
                    return;
                }
                else if(ret >= 0){
                    //说明只读到一行，继续循环
                    event.inbuffer_.erase(0, ret);
                }
                else{
                    //说明读到空行，报头读取完毕，继续进行逻辑
                    event.inbuffer_.erase(0, 2);
                    break;
                }
            }
        }

        //解析请求行和报头
        if((event.recvMessage_.blank_.size() != 0) && (event.recvMessage_.iniLine_.size() != 0) && (event.recvMessage_.method_.size() == 0)){
            // ParseIniLine(event.recvMessage_);
            // ParseHeader(event.recvMessage_);
            event.recvMessage_.ParseIniLine();
            event.recvMessage_.ParseHeader();   
        }

        
        //读取正文，先判断大小，再判断是否继续读
        if((event.recvMessage_.blank_.size() != 0) && (event.recvMessage_.iniLine_.size() != 0) && (event.recvMessage_.method_.size() != 0)){  
            int content_len = atoi(event.recvMessage_.headerMap_.at("Content-Length").c_str());
            if(content_len > 0 && event.recvMessage_.body_.size() < content_len){
                int len = content_len - event.recvMessage_.body_.size();
                ret = GetBody(len, event.inbuffer_, event.recvMessage_.body_);
                if(ret == 0){
                    //读完数据，清理inbuffer
                    event.inbuffer_.erase(0, len);
                }
                else{
                    //未读完数据，将inbuffer全清空
                    event.inbuffer_.erase(0);
                }
                LOG(INFO, std::string("body size: ")+std::to_string(event.recvMessage_.body_.size()));
            }
        }

        //构建任务，交给任务队列处理
        
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
