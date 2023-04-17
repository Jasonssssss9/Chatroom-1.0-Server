#pragma once
#include "Log.hpp"
#include "Reactor.hpp"
#include <iostream>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <queue>

using Task = std::function<void()>;
#define THREAD_NUM 8
#define MESSAGE_THREAD_NUM 2

//InformMsg为一种消息类型，对应的线程池不仅可以构建任务队列，也可以构建消息队列
template<class T> //这里的T就是ChatMessage
struct InformMsg
{
public:
    int sock_; //需要发送通知消息的socket
    Reactor<T>* pr_;
    T message_; //通知消息

    InformMsg(int sock = -1, Reactor<T>* pr = nullptr):sock_(sock), pr_(pr)
    {}

    ~InformMsg() = default;
    InformMsg(const InformMsg&) = default;
    InformMsg(InformMsg&&) = default;
    InformMsg& operator=(const InformMsg&) = default;
    InformMsg& operator=(InformMsg&&) = default;
    //关键是显示指定自动生成移动构造函数和移动赋值函数
    //这里的message_.body_中内容可能很大，必须要防止不必要的拷贝，而是使用移动语义
    //实际上没有析构，拷贝构造和拷贝复制，移动构造和移动赋值会自动生成
    //  但是这里为了突出移动语言的重要性，还是显示指定了自动生成的移动语义
};


template<class T, class P>
class ThreadPool
{
private:
    std::atomic<bool> run_; //线程池运行为true，反之为false

    //任务队列相关
    std::vector<std::thread> workers; //线程池
    std::queue<Task> taskQueue_; //任务队列
    std::mutex taskMtx_;
    std::condition_variable taskCv_;

    //通知消息队列相关
    //消息队列的意义在于，确保给同一个连接发送消息时不会出现同时发送导致混乱的问题
    std::vector<std::thread> msgWorkers;
    std::queue<InformMsg<T>> msgQueue_;
    std::mutex msgMtx_;
    std::condition_variable msgCv_;
    std::unordered_map<int, bool> isOccupied_;
    //key为一个连接socket，value为该连接目前是否被占用

    static ThreadPool<T, P>* ptp_;

    ThreadPool(int num = THREAD_NUM): run_(true)
    {
        //构造函数中直接启动num个线程
        for(int i = 0;i < num;i++){
            workers.emplace_back(std::thread([this]{
                //线程循环去任务队列取任务并执行，没有任务则等待，任务做完继续去
                while(run_){
                    Task task;

                    {
                        std::unique_lock<std::mutex> u_mtx(taskMtx_); //取任务必须加锁
                        taskCv_.wait(u_mtx, [this]{
                            //只有当任务队列为空才等待，当线程池运行停止，则停止等待
                            //意义在于防止线程池析构而还有线程在等待
                            return !run_ || !taskQueue_.empty();
                        });
                        if(!run_ && taskQueue_.empty()){
                            //停止运行且保证任务处理完，则直接返回
                            return;
                        }

                        task = move(taskQueue_.front());
                        taskQueue_.pop();
                        LOG(INFO, "Pop a task from task_queue");
                    }

                    //执行任务
                    task();
                }
            }));
        }

        //构造MESSAGE_THREAD_NUM个线程专门用来管理通知消息队列
        for(int i = 0;i < MESSAGE_THREAD_NUM;i++){
            msgWorkers.emplace_back([this]{
                while(run_){
                    //从消息队列中取消息，并且使用下面消息调度算法(提高性能需要改进算法！！！)
                    {
                        //这里逻辑和task_queue类似，从队列中取即可
                        std::unique_lock<std::mutex> u_mtx(msgMtx_);
                        msgCv_.wait(u_mtx, [this]{
                            return !run_ || !msgQueue_.empty(); 
                        });
                        if(!run_ && taskQueue_.empty()){
                            //停止运行且保证任务处理完，则直接返回
                            return;
                        }

                        InformMsg<T> im = std::move(msgQueue_.front());
                        msgQueue_.pop();

                        //判断当前socket是否正在被占用
                        int sock = im.sock_;
                        auto it = isOccupied_.find(sock);
                        if(it == isOccupied_.end()){
                            //此时还没有socket在isOccupied中管理，也没有被占用
                            isOccupied_.insert(std::make_pair(sock, false));
                        }

                        it = isOccupied_.find(sock); //此时一定能找到
                        if(it->second){
                            //socket被占用，当前消息调度算法设置为: 
                            //(1)如果当前队列长度为0(说明没有其他消息)，
                            //   那么消息插入队列尾部，当前线程休眠1毫秒，继续循环
                            //   休眠的意义是防止当前线程一直循环处理这一个被占用的连接的消息，浪费系统资源
                            if(msgQueue_.size() == 0){
                                msgQueue_.push(std::move(im));
                                //注意等待一毫秒的时候还要释放当前锁，其他消息处理线程还能进来
                                //因为这一毫秒时间内原来被占用的长连接可能就空闲了，这时其他线程还能处理
                                //或者这一毫秒时间其他消息任务进来，其他线程就会执行(2)逻辑
                                msgCv_.wait_for(u_mtx, std::chrono::milliseconds(1));
                            }
                            //(2)如果当前队列长度大于0(说明还有其他消息)，
                            //   那么消息插入队列尾部，当前线程继续执行，不休眠
                            else{
                                msgQueue_.push(std::move(im));
                            }
                            
                        }
                        else{
                            //socket没被占用
                            //直接构建发送任务，交给线程池任务队列
                            it->second = true; //设置其被占用

                            Event<T>& send_ev = im.pr_->GetEvent(sock);
                            send_ev.sendMessage_ = std::move(im.message_);
                            Task task([&send_ev]{
                                P::SendHandler(send_ev);
                            });
                            ThreadPool<T, P>::GetInstance()->AddTask(task);
                        }
                    }
                }
            });
        }
        
    }

public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    static ThreadPool* GetInstance(int num = THREAD_NUM)
    {
        static std::mutex mtx;
        if(ptp_ == nullptr){
            {
                std::unique_lock<std::mutex> u_mtx(mtx);
                if(ptp_ == nullptr){
                    ptp_ = new ThreadPool(THREAD_NUM);
                }
            }
        }
        return ptp_;
    }

    ~ThreadPool()
    {
        run_ = false;
        taskCv_.notify_all();
        for(auto& t : workers){
            if(t.joinable()){
                t.join();
            }
        }
    }

    void SetCurrSockFalse(int sock)
    {
        std::unique_lock<std::mutex> u_mtx(msgMtx_);
        auto it = isOccupied_.find(sock);
        if(it != isOccupied_.end()){
            it->second = false;
        }
        //如果没找到，就不用管
    }

    template<class F, class ... Args>
    auto AddTask(F&& f, Args&&... args) ->std::future<decltype(f(args...))>
    {
        using RetType = decltype(f(args...)); //f函数返回值
        auto ptask = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...)); //task是一个智能指针
        std::future<RetType> ft = ptask->get_future();

        {
            //加入任务队列
            std::unique_lock<std::mutex> u_lock(taskMtx_);
            taskQueue_.emplace([ptask]() {
				(*ptask)();
			});
        }

        taskCv_.notify_one(); //唤醒一个线程
        LOG(INFO, "Push a task to task_queue");

        return ft;
    }

    void AddMessage(InformMsg<T>&& t)
    {   
        {
            std::unique_lock<std::mutex> u_lock(msgMtx_);
            msgQueue_.push(std::move(t)); //直接移动
        }
        msgCv_.notify_one();
        LOG(INFO, "Push a informing message to message_queue");
    }
};