#pragma once
#include "Log.hpp"
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
#define MAX_NUM 10

class ThreadPool
{
private:
    std::vector<std::thread> workers; //线程池
    std::queue<Task> taskQueue_; //任务队列
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> run_; //线程池运行为true，反之为false

public:
    ThreadPool(int num = MAX_NUM): run_(true)
    {
        //构造函数中直接启动num个线程
        for(int i = 0;i < num;i++){
            workers.emplace_back(std::thread([this]{
                //线程循环去任务队列取任务并执行，没有任务则等待，任务做完继续去
                while(run_){
                    Task task;

                    {
                        std::unique_lock<std::mutex> u_mtx(mtx_); //取任务必须加锁
                        cv_.wait(u_mtx, [this]{
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
                    }

                    //执行任务
                    task();
                }
            }));
        }
    }

    ~ThreadPool()
    {
        run_ = false;
        cv_.notify_all();
        for(auto& t : workers){
            if(t.joinable()){
                t.join();
            }
        }
    }


    template<class F, class ... Args>
    auto AddTask(F&& f, Args&&... args) ->std::future<decltype(f(args...))>
    {
        using RetType = decltype(f(args...)); //f函数返回值
        auto ptask = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...)); //task是一个智能指针
        std::future<RetType> ft = ptask->get_future();

        {
            //加入任务队列
            std::unique_lock<std::mutex> u_lock(mtx_);
            taskQueue_.emplace([ptask]() {
				(*ptask)();
			});
        }

        cv_.notify_one(); //唤醒一个线程

        return ft;
    }
};