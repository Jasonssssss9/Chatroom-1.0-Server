#pragma once
#include <iostream>
#include <string>
#include <vector>

class Util
{
public:
    //从in中读取第一行的内容
    //读取成功返回这一行的长度，读取失败返回-1
    static int Readline(const std::string& in, std::string& out)
    {
        int pos = in.find("\r\n");
        if(pos == in.npos){
            return -1;
        }
        else{
            out = in.substr(0, pos+2);
            return out.size();
        }
    }

    //切分字符串，target为目标字符串，sep为分隔符，result_v用来保存切分结果
    static bool CutString(const std::string& target, std::vector<std::string>& result_v, std::string sep)
    {
        std::string temp(target);
        int begin = 0;
        while(true){
            int it = temp.find(sep);
            if(it == std::string::npos){
                break;
            }
            std::string t = temp.substr(begin, it-begin);
            if(t.size() > 0){
                result_v.push_back(t);
            }
            temp = temp.substr(it+sep.size());
        }
        if(temp.size() != 0){
            result_v.push_back(temp);
        }
        if(result_v.size() < 2){
            return false;
        }
        return true;
    }
};