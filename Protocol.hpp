#pragma once
#include "Log.hpp"
#include "Reactor.hpp"
#include "ThreadPool.hpp"
#include "Util.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <fstream>

#define LINE_END "\r\n"
#define VERSION "JCHAT/1.0"
#define SIGN_UP_NAME "#####"

struct ChatMessage
{
public:
    //一个报文中的四个部分
    std::string iniLine_;  //初始行
    std::vector<std::string> headers_; //报头
    std::string blank_; //空行
    std::string body_;  //正文

    //解析初始行
    std::string method_;
    std::string status_;
    std::string version_;

    //解析报头
    std::unordered_map<std::string, std::string> headerMap_;

    ChatMessage() = default;
    ~ChatMessage() = default;
    ChatMessage(const ChatMessage&) = default;
    ChatMessage(ChatMessage&&) = default;
    ChatMessage& operator=(const ChatMessage&) = default;
    ChatMessage& operator=(ChatMessage&&) = default;
    //关键是指定自动生成移动构造和移动拷贝

public:
    //解析初始行，从iniLine_得到method_，status_和version_
    void ParseIniLine()
    {
        std::stringstream ss(iniLine_);
        ss >> method_ >> status_ >> version_;

        //for test
        LOG(INFO, std::string("method: ")+method_);
        LOG(INFO, std::string("status: ")+status_);
        LOG(INFO, std::string("version: ")+version_);
    }

    //解析报头，从headers_得到headerMap_
    int ParseHeader()
    {
        for(auto e : headers_){
            std::vector<std::string> tv;
            if(!Util::CutString(e, tv, ": ")){
                return -1;
            }
            headerMap_.insert(std::make_pair(tv[0], tv[1]));
            LOG(INFO, tv[0]+std::string(": ")+tv[1]);
        }
        return 0;
    }

    void Clear()
    {
        method_.clear();
        status_.clear();
        version_.clear();
        headers_.clear();
        iniLine_.clear();
        body_.clear();
        blank_.clear();
        headerMap_.clear();
    }
};


struct PairHash
{
public:
    size_t operator()(const std::pair<std::string, std::string>& p) const{ //注意一定是const的
        std::hash<std::string> h;
        return h(p.first);
    }
};

//进行应用层的管理内容
class Chatroom
{
private:
    std::mutex usersMtx_;
    std::mutex onlineMtx_;
    std::mutex shortSockMtx_;
    std::mutex longSockMtx_;
    std::mutex offlineMtx_;
    std::mutex groupsMtx_;
    std::mutex oflgroupMtx_;
    std::mutex oflfileMtx_;

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
    //如果是群聊信息，那么first为群聊名称
    
    std::unordered_map<std::string, std::unordered_set<std::string>> groups_;
    //key为群聊名称，value为这个群聊中的所有人

    std::unordered_map<std::string, std::unordered_set<std::string>> offlineGroups_;
    //key为用户名，value为该用户离线时创建的包含该用户的群聊名称，需要由此通知该用户

    std::unordered_map<std::string, std::unordered_set<std::pair<std::string, std::string>, PairHash>> offlineFiles_;
    //key为用户名，value为该用户离线时创建的发送个该用户的文件，first为文件名，second为发送者&时间
    //注意，这里的value一次存了两个信息，即sender_name和time，中间用$分隔
    
    //注意细节：unordered_set为哈希表实现，并且默认第二个模板参数为std::hash<T>
    //std::hash<T>是一个类，其中有仿函数方法std::hash<T>(T t)，用来对参数t进行哈希，返回值为size_t类型
    //但是std::hash默认实现的T类型只有常见的例如int，char，std::string等，没有实现std::pair
    //因此这里如果要将std::unordered_set的类型设置为std::pair，必须手动传入第二个哈希函数参数
    //这里直接实现一个struct PairHash，其中有仿函数，直接将pair.first作为std::hash<std::string>的参数即可
    
    //除此之外，还有一个非常容易忽略的问题，那就是自己写的仿函数必须是const的，不然就会编译出错
    //疑问？？？为什么？这里的理解对不对？
    //不加const，用该类型unordered_set创建一个对象set，以及一个pair，当调用set.insert(pair)时就会报错
    //因为insert加入pair时一定会调用哈希函数，但是这里的哈希函数是const的，而内层自己写的PairHash仿函数又不是const的
    //这样就会报错
    //参考博客：http://www.360doc.com/content/11/1102/15/7828500_161101031.shtml


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

    //直接从offline中清除当前用户
    void OfflineClear(std::string name)
    {
        std::unique_lock<std::mutex> u_mtx(offlineMtx_);
        offline_.erase(name);
    }

    const std::unordered_map<std::string, std::unordered_set<std::string>>& GetGroups()
    {
        return groups_;
    }

    void GroupsInsert(const std::string& group, const std::unordered_set<std::string>& others)
    {
        std::unique_lock<std::mutex> u_mtx(groupsMtx_);
        groups_.insert(std::make_pair(group, others));
    }

    void GroupsErase(const std::string& group)
    {
        std::unique_lock<std::mutex> u_mtx(groupsMtx_);
        groups_.erase(group);
    }
    
    const std::unordered_map<std::string, std::unordered_set<std::string>>& GetOfflineGroup()
    {
        return offlineGroups_;
    }

    void OfflineGroupInsert(std::string name, std::string group)
    {
        std::unique_lock<std::mutex> u_mtx(oflgroupMtx_);
        auto it = offlineGroups_.find(name);
        if(it == offlineGroups_.end()){
            std::unordered_set<std::string> add;
            add.insert(group);
            offlineGroups_.insert(std::make_pair(name, add));
        }
        else{
            it->second.insert(group);
        }
    }

    void OfflineGroupClear(std::string name)
    {
        std::unique_lock<std::mutex> u_mtx(oflgroupMtx_);
        offlineGroups_.erase(name);
    }

    const std::unordered_map<std::string ,std::unordered_set<std::pair<std::string, std::string>, PairHash>>& GetOfflineFiles()
    {
        return offlineFiles_;
    }

    void OfflineFilesInsert(std::string name, std::string file_name, std::string sender_name, std::string time)
    {
        std::string sender_time = sender_name;
        sender_time += "$";
        sender_time += time;
        const std::pair<std::string, std::string> pair = std::make_pair(file_name, sender_time);

        std::unique_lock<std::mutex> u_mtx(oflfileMtx_);
        auto it = offlineFiles_.find(name);
        if(it == offlineFiles_.end()){
            //当前还没有该name
            std::unordered_set<std::pair<std::string, std::string>, PairHash> set;
            set.insert(pair);
            offlineFiles_.insert(std::make_pair(name, set));
        }
        else{
            //当前已经有该name
            it->second.insert(pair);
        }
    }

    void OfflineFilesErase(std::string name)
    {
        std::unique_lock<std::mutex> u_mtx(oflfileMtx_);
        offlineFiles_.erase(name);
    }
};

class Protocol
{
private:
    static int GetIniLine(Event<ChatMessage>& event);
    static int GetHeader(Event<ChatMessage>& event);
    static int GetBody(int len, const std::string& in, std::string& out);

    static void SignUp(Event<ChatMessage>& event);
    static void SignIn(Event<ChatMessage>& event);
    static void SignOut(Event<ChatMessage>& event);

    static bool IsUserExist(std::string name);
    static std::pair<bool, std::string> GetPassword(std::string name);
    static bool IsSignIn(std::string name);

    static void BuildMessage(Event<ChatMessage>& event);
    static void ClearEvent(Event<ChatMessage>& event);

    static bool IsFileExist(const std::string&);
    static bool ReadFile(const std::string& path, std::vector<std::vector<std::string>>& out, int n);
    static void AppendFile(const std::string& path, const std::vector<std::string>& in);
    static void ClearFile(const std::string& path);

    static int MessageHandler(Event<ChatMessage>& event, std::vector<std::string>& v_peers);
    static InformMsg<ChatMessage> SendMessage(Event<ChatMessage>& event, std::string peer_name, int& is_offline);

    static void CreateGroup(Event<ChatMessage>& event);
    static int GroupMessageHandler(Event<ChatMessage>& event, std::vector<std::string>& v_peers);
    static InformMsg<ChatMessage> SendGroupMessage(Event<ChatMessage>& event, std::string member, int& is_offline);

    static void UploadFile(Event<ChatMessage>& event);
    static void DownloadFile(Event<ChatMessage>& event);

    static void ReqHandler(Event<ChatMessage>& event);
    static void ResHandler(Event<ChatMessage>& events);

public:
    static void GetPerseMessage(Event<ChatMessage>& event);

    static void SendHandler(Event<ChatMessage>& event);
};