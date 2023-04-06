#pragma once
#include "Log.hpp"
#include <unistd.h>
#include <sys/epoll.h>
#include <iostream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <functional>

#define MAX_NUM 128

class Reactor;

//Event表示一个连接事件结构体
//Event就是一个socket+缓冲区+读/写/异常回调方法的集合
struct Event
{
public:
    int sock_; //标识当前事件的socket
    Reactor* pr_; //指向当前Event被加入到的Reactor模型

    //读/写/异常回调函数
    std::function<void(Event&)> recvCallback_;
    std::function<void(Event&)> sendCallback_;
    std::function<void(Event&)> errorCallback_;

    std::string inbuffer_;  //读缓冲区
    std::string outbuffer_; //写缓冲区

public:
    Event(int sock = -1, Reactor* pr = nullptr):sock_(sock), pr_(pr)
    {}

    //注册回调函数，即给该Event绑定特定的回调函数
    //每次将新的sock加入reactor时，必须设置注册特定的回调函数

    //注册读回调
    void RegisterRecv(std::function<void(Event&)> recv_tem)
    {
        recvCallback_ = recv_tem;
    }

    //注册写回调
    void RegisterSend(std::function<void(Event&)> send_tem)
    {
        sendCallback_ = send_tem;
    }

    //注册异常回调
    void RegisterError(std::function<void(Event&)> err_tem)
    {
        errorCallback_ = err_tem;
    }
};

//Reactor模型，包括两部分：(1)一个Epoll模型；(2)自己进行连接管理的eventsMap_
//Reactor模型是整个服务器的核心，完成连接管理，就绪事件监测以及对就绪事件的分派
class Reactor
{
private:
    int epfd_; //Reactor模型对应的Epoll模型
    std::unordered_map<int, Event> eventsMap_; 
    // //Reactor模型自己对连接的管理，表示一个socket到其对应的连接事件Event的映射

public:
    Reactor():epfd_(-1)
    {
        //创建一个epoll对象
        epfd_ = epoll_create(256);
        if(epfd_ < 0){
            LOG(FATAL, "epoll_create error");
            exit(1);
        }
        LOG(INFO, "Reactor is initialized successfully");

        //不在这里将listen_sock添加到Reactor模型中
        //因为这里的Reactor模型完全和网络解耦，作用仅仅是等待并分配任务而已
    }

    ~Reactor()
    {
        if(epfd_ >= 0){
            close(epfd_);
        }
    }

    //将一个事件ev加入到当前Reactor模型中，events为需要监测的事件
    //成功返回false，失败返回true
    bool AddEvent(const Event& ev, uint32_t events)
    {
        //加入Epoll模型
        epoll_event epoll_ev;
        epoll_ev.data.fd = ev.sock_;
        epoll_ev.events = events;
        if(epoll_ctl(epfd_, EPOLL_CTL_ADD, ev.sock_, &epoll_ev) < 0){
            LOG(ERROR, "epoll_ctl adding error");
            return false;
        }

        //加入eventsMap_
        eventsMap_.insert(std::make_pair(ev.sock_, ev));

        LOG(INFO, std::string("An event is added to Reactor, sock: ")+std::to_string(ev.sock_));
        return true;
    }

    //将一个Event事件从当前Reactor模型中删除
    //成功返回true，失败返回false
    bool DelEvent(int sock)
    {
        //判断是否存在
        auto it = eventsMap_.find(sock);
        if(it == eventsMap_.end()){
            LOG(WARNING, "epoll_ctl deleting error: no such socket");
            return false;
        }

        //从Epoll模型中删除
        if(epoll_ctl(epfd_, EPOLL_CTL_DEL, sock, nullptr) < 0){
            LOG(ERROR, "epoll_ctl deleting error");
            return false;
        }

        //从eventsMap_中删除
        eventsMap_.erase(sock);

        //一定要关闭socket
        close(sock);

        LOG(INFO, std::string("An event is deleted from Reactor, sock: ")+std::to_string(sock));
        return true;
    }   
    

    //用来检测当前socket是否还在reactor模型中，即连接是否还存在，存在返回true，反之false
    bool isExists(int sock)
    {
        auto it = eventsMap_.find(sock);
        if(it == eventsMap_.end()){
            return false;
        }
        return true;
    }

    //使能读写接口
    void EnableReadWrite(int sock, bool readable, bool writeable)
    {
        struct epoll_event ev;
        ev.events = (EPOLLET | (readable ? EPOLLIN : 0) | (writeable ? EPOLLOUT : 0));
        ev.data.fd = sock;

        epoll_ctl(epfd_, EPOLL_CTL_MOD, sock, &ev);
    }

    //reactor模型核心：对就绪事件进行监听和派发
    //timeout为希望从就绪队列中等待的时间间隔
    void Dispatcher(int timeout)
    {
        //从就绪队列中获取就绪事件数组
        epoll_event revents[MAX_NUM];
        int num = epoll_wait(epfd_, revents, MAX_NUM, timeout);
        if(num < 0){
            LOG(ERROR, "epoll_wait error");
            return;
        }

        //对num个就绪事件进行分派
        for(int i = 0;i < num;i++){
            //(1)先从revents中获取第i个事件的相关信息
            int sock = revents[i].data.fd;
            uint32_t events = revents[i].events;

            LOG(INFO, std::string("An event is ready, sock: ")+std::to_string(sock));

            //(2)将事件派发给该socket对应的处理事件回调函数
            //派发之后，reactor模型转到回调函数中处理读写异常等事件，之后再回来继续等待
                
            //如果出现异常，那么先交给读写处理，而读写又会将所有异常交给error_handler处理
            //-> 无论任何异常事件，最终都会交给error_handler处理
            if(events & EPOLLERR){
                events |= (EPOLLIN | EPOLLOUT);
            }
            if(events & EPOLLHUP){ //表示对端连接关闭
                events |= (EPOLLIN | EPOLLOUT);
            }

            //读写回调函数
            if(isExists(sock) && (events & EPOLLIN)){
                if(eventsMap_[sock].recvCallback_){ //如果有读回调函数
                    eventsMap_[sock].recvCallback_(eventsMap_[sock]);
                }
            }
            if(isExists(sock) && (events & EPOLLOUT)){
                if(eventsMap_[sock].sendCallback_){
                    eventsMap_[sock].sendCallback_(eventsMap_[sock]);
                }
            }
            //注意，写回调调用前一定要判断当前sock是否还存在 -> isExists(sock)
            //因为在调用读回调函数的时候可能会出现异常，之后将当前连接释放
            //写回调其实不用判断，但是为了统一还是进行判断
        }
    }   
};
