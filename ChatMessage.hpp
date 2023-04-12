#pragma once
#include "Log.hpp"
#include "Reactor.hpp"
#include "Util.hpp"
#include "Reactor.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>


//防止头文件互相引用，ChatMessage放在这个单独的头文件，而不是Protocol.hpp
//疑问？？？能不能在Event中用模板解决这个问题
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
};



//进行应用层的管理内容
class Chatroom
{
private:

};

