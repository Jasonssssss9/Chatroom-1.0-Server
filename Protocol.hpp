#pragma once
#include "Log.hpp"
#include "Reactor.hpp"
#include "ThreadPool.hpp"
#include <string>
#include <unordered_map>
#include <mutex>

#define LINE_END "\r\n"
#define VERSION "JCHAT/1.0"

//进行应用层的管理内容
class Chatroom
{
private:
    std::unordered_map<std::string, std::string> users_; //管理所有用户及其密码
    //增加！！！将用户的用户名和密码信息加入数据库

    std::unordered_map<std::string, int> online_; 
    //管理所有在线的用户，建立当前用户到其登陆时对应socket的映射


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

    std::unordered_map<std::string, std::string>& GetUsersInfo()
    {
        return users_;
    }

    std::unordered_map<std::string, int>& GetOnlineInfo()
    {
        return online_;
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