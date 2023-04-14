#pragma once
#include "Log.hpp"
#include "Reactor.hpp"
#include "ThreadPool.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <fstream>

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
    std::mutex offlineMtx_;

    //易错，注意对一下所有对象操作时必须加锁，因为stl容器不能保证线程安全

    std::unordered_map<std::string, std::string> users_; //管理所有用户及其密码
    //增加！！！将用户的用户名和密码信息加入数据库

    std::unordered_map<std::string, int> online_; 
    //管理所有在线的用户，建立当前用户的名称到登陆时对应的socket的映射，这个socke为一个长连接

    std::unordered_map<int, std::string> shortSock_;
    //管理所有的短连接，即不包括登陆时对应的长连接

    std::unordered_map<int, std::string> longSock_;
    //管理所有的长连接，即登陆时对应的连接

    std::unordered_map<std::string, std::unordered_map<std::string, int>> offline_;
    //key为用户名，建立该用户到其所有离线消息的映射，value也为一个映射，first为发送者名，second为与first对应的离线消息个数
    

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
        return longSock_;
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

    const std::unordered_map<std::string, std::unordered_map<std::string, int>>& GetOffline()
    {
        return offline_;
    }

    //name为离线接收者用户名，sender为对应发送者用户名
    void OfflineInsert(std::string name, std::string sender)
    {
        std::unique_lock<std::mutex> u_mtx(offlineMtx_);
        auto it1 = offline_.find(name);
        if(it1 == offline_.end()){
            std::unordered_map<std::string, int> tmp;
            tmp.insert(std::make_pair(sender, 1));
            offline_.insert(std::make_pair(name, tmp));
        }
        else{
            auto it2= offline_.at(name).find(sender);
            if(it2 == offline_.at(name).end()){
                offline_.at(name).insert(std::make_pair(sender, 1));
            }
            else{
                offline_.at(name).at(sender)++;
            }
        }
    }

    void OfflineErase(std::string name, std::string sender)
    {
        std::unique_lock<std::mutex> u_mtx(offlineMtx_);
        auto it1 = offline_.find(name);
        if(it1 != offline_.end()){
            auto it2= offline_.at(name).find(sender);
            if(it2 != offline_.at(name).end()){
                if(offline_.at(name).at(sender) > 1){
                    offline_.at(name).at(sender)--;
                }
                else{
                    offline_.at(name).erase(sender);
                }
            }
        }    
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

    static void BuildMessage(Event& event);
    static void ClearEvent(Event& event);

    static bool IsFileExist(const std::string&);
    static bool ReadFile(const std::string& path, std::vector<std::vector<std::string>>& out, int n);
    static void AppendFile(const std::string& path, const std::vector<std::string>& in);
    static void ClearFile(const std::string& path);

    static Event& SingleMessage(Event& event, int& ret);
    static int GroupMessage(Event& event);

    static void ReqHandler(Event& event);
    static void ResHandler(Event& events);

    static void SendHandler(Event& event);

public:
    static void GetPerseMessage(Event& event);
};