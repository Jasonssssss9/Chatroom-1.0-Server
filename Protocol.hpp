#pragma once
#include "Log.hpp"
#include "Reactor.hpp"
#include "ThreadPool.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

#define LINE_END "\r\n"
#define VERSION "JCHAT/1.0"
#define SIGN_UP_NAME "#####"

//进行应用层的管理内容
class Chatroom
{
private:
    std::mutex usersMtx_;
    std::mutex onlineMtx_;
    std::mutex shortSockMtx_;
    std::mutex longSockMtx_;

    //易错，注意对一下所有对象操作时必须加锁，因为stl容器不能保证线程安全

    std::unordered_map<std::string, std::string> users_; //管理所有用户及其密码
    //增加！！！将用户的用户名和密码信息加入数据库

    std::unordered_map<std::string, int> online_; 
    //管理所有在线的用户，建立当前用户的名称到登陆时对应的socket的映射，这个socke为一个长连接

    std::unordered_map<int, std::string> shortSock_;
    //管理所有的短连接，即不包括登陆时对应的长连接

    std::unordered_map<int, std::string> longSock_;
    //管理所有的长连接，即登陆时对应的连接

    Chatroom()
    {}

    static Chatroom* pc_;

public:
    Chatroom(const Chatroom&) = delete;
    Chatroom& operator=(const Chatroom&) = delete;

    static Chatroom* GetInstance()
    {
        static std::mutex mtx;
        if(pc_ == nullptr){
            {
                std::unique_lock<std::mutex> u_mtx(mtx);
                if(pc_ == nullptr){
                    pc_ = new Chatroom;
                    pc_->UsersInsert(SIGN_UP_NAME, SIGN_UP_NAME); //为登录状态准备一个特殊用户名
                }
            }
        }
        return pc_;
    }

    static void DelInstance()
    {
        static std::mutex mtx;
        if(pc_ != nullptr){
            {
                std::unique_lock<std::mutex> u_mtx(mtx);
                if(pc_ != nullptr){
                    delete pc_;
                }
            }
        }
    }

    const std::unordered_map<std::string, std::string>& GetUsers()
    {
        return users_;
    }

    void UsersInsert(const std::string& name, const std::string& password)
    {
        std::unique_lock<std::mutex> u_mtx(usersMtx_);
        users_.insert(std::make_pair(name, password));
    }

    void UsersErase(const std::string& name)
    {
        std::unique_lock<std::mutex> u_mtx(usersMtx_);
        users_.erase(name);
    }

    const std::unordered_map<std::string, int>& GetOnline()
    {
        return online_;
    }

    void OnlineInsert(const std::string& name, int sock)
    {
        std::unique_lock<std::mutex> u_mtx(onlineMtx_);
        online_.insert(std::make_pair(name, sock));
    }

    void OnlineErase(const std::string& name)
    {
        std::unique_lock<std::mutex> u_mtx(onlineMtx_);
        online_.erase(name);
    }

    const std::unordered_map<int, std::string>& GetShortSock()
    {
        return shortSock_;
    }

    void ShortSockInsert(int sock, const std::string& name)
    {
        std::unique_lock<std::mutex> u_mtx(shortSockMtx_);
        shortSock_.insert(std::make_pair(sock, name));
    }

    void ShortSockErase(int sock)
    {
        std::unique_lock<std::mutex> u_mtx(shortSockMtx_);
        shortSock_.erase(sock);
    }

    const std::unordered_map<int, std::string>& GetLongSock()
    {
        return  longSock_;
    }

    void LongSockInsert(int sock, const std::string name)
    {
        std::unique_lock<std::mutex> u_mtx(longSockMtx_);
        longSock_.insert(std::make_pair(sock, name));
    }

    void LongSockErase(int sock)
    {
        std::unique_lock<std::mutex> u_mtx(longSockMtx_);
        longSock_.erase(sock);
    }
    
};

class Protocol
{
private:
    static int GetIniLine(Event& event);
    static int GetHeader(Event& event);
    static int GetBody(int len, const std::string& in, std::string& out);

    static void SignUp(Event& event);
    static void SignIn(Event& event);
    static void SignOut(Event& event);

    static bool IsUserExist(std::string name);
    static std::pair<bool, std::string> GetPassword(std::string name);
    static bool IsSignIn(std::string name);

    static void BuildResMessage(Event& event);
    static void ClearEvent(Event& event);
public:
    static void GetPerseMessage(Event& event);
    
    static void ReqHandler(Event& event);
    static void ResHandler(Event& events);

    static void SendResponse(Event& event);
    static void SendInform(Event& event);

};