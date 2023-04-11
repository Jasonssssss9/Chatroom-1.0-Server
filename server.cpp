// #include "ChatroomServer.hpp"
// #include <iostream>
// #include <thread>


// int main()
// {
//     ChatroomServer* p = new ChatroomServer(8081);
//     p->Loop();

//     return 0;
// }


#include "ThreadPool.hpp"
#include <future>
#include <iostream>

int fun1(int a, int b)
{
    std::cout << "fun1: " << std::this_thread::get_id() << ": " << a + b << std::endl;
    return a + b;
}

class A
{
private:
    double fun2(double a, double b)
    {
        std::cout << "fun2: " << std::this_thread::get_id() << ": " << a + b << std::endl;
        return a + b;
    }
public:
    double operator()(double a, double b)
    {
        return fun2(a, b);
    }

    static void fun3(int a, int b)
    {
        std::cout << "fun3: " << std::this_thread::get_id() << ": " << a + b << std::endl;
    }
};


int main()
{
    ThreadPool* tp = new ThreadPool(10);
    std::future<int> f1 = tp->AddTask(fun1, 1, 2);
    std::future<double> f2 = tp->AddTask(A(), 1.1, 2.2);
    std::future<void> f3 = tp->AddTask(&A::fun3, 1, 2);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    int a = f1.get();
    double b = f2.get();
    f3.get();

    std::cout << a << std::endl;
    std::cout << b << std::endl;
    
    return 0;
}
