#include "Protocol.hpp"


//获取初始行，自动去除结尾\r\n
//粘包返回-1，正常则返回读取到的初始行大小，包括\r\n
int Protocol::GetIniLine(Event& event)
{
    int ret = Util::Readline(event.inbuffer_, event.recvMessage_.iniLine_);
    if(ret == -1){
        //没有读到\r\n，出现粘包问题，直接返回
        return -1;
    }
    event.recvMessage_.iniLine_.resize(event.recvMessage_.iniLine_.size()-2);
    LOG(INFO, event.recvMessage_.iniLine_);
    return ret;
}

//获取报头中的一行，也可能是空行，自动去除结尾\r\n
//粘包返回-1；正常则返回读取到的报头总大小，包括\r\n与空行大小；如果读到空行；返回-2
int Protocol::GetHeader(Event& event)
{
    std::string tem_string;
    int ret = Util::Readline(event.inbuffer_, tem_string);
    if(ret == -1){
        //没有读到\r\n，出现粘包问题，直接返回
        return -1;
    }
    if(tem_string == "\r\n"){
        //如果读到空行，返回-2
        event.recvMessage_.blank_ = "\r\n";
        return -2;
    }
    //如果是正常的一行，那么就加入headers_中
    tem_string.resize(tem_string.size()-2);
    event.recvMessage_.headers_.push_back(tem_string);
    LOG(INFO, tem_string);
    return ret;
}

//获取正文数据
//如果读完数据，返回0；没有读完数据，即in已经空了，返回-1
int Protocol::GetBody(int len, const std::string& in, std::string& out)
{
    if(in.size() >= len){
        //保证一定能读完数据，直接从in中读n个
        out += in.substr(0, len);
        return 0;
    }
    else{
        //不能读完数据
        out += in.substr(0);
        return -1;
    }
}

//获取和解析请求报文的初始行，报头并且获取正文
//之后建立新的任务，加入任务队列
void Protocol::GetPerseMessage(Event& event)
{
    //在这里进行应用层业务处理的第一步：分解报文，处理粘包问题，并构建任务，交给线程池处理
    //粘包问题的解决：
    //分三部分：(1)iniline;(2)header+blank;(3)body
    //可能存在的粘包情况是，一部分只读了一部分，剩下一部分在下一个报文中
    //(1)对于iniline来说
    //   如果没读完一行，那么就直接退出，不向inline放东西，也不清理inbuffer，下次继续读剩下的
    //   如果正确读完一行，则清理inbuffer，继续读header
    //   进入判断前必须确定inline是空的，即从inline开始读，如果inline不为空，说明inline一定已经读完了
    //(2)对于header来说
    //   如果一行都没读完，那么直接退出，不向header里放东西，也不清理inbuffer
    //   while循环中，如果正确读完一行，则清理inbuffer的这一行，继续读下一行，直到读到空行再退出while循环
    //   header读完的标志是读到空行，进入header之前先判断blank，为空再进入；不为空说明header一定已经读完了
    //接下来是分析数据，必须判断inline和blank都非空，即证明(1)和(2)已经读完
    //(3)读取数据时，根据header中的Content-Length字段进行判断，没有数据就不读，有数据再读
    //   有数据时，必须保证读完数据长度个字节数据，如果没读完，则退出下次继续读，这时要清理inbuffer
    
    //读初始行，将初始行中\n去除
    int ret = 0;
    if(event.recvMessage_.iniLine_.size() == 0){
        ret = GetIniLine(event);
        if(ret == -1){
            //粘包直接退出不处理
            return;
        }
        event.inbuffer_.erase(0, ret);
    }
    
    //读报头，将报头每行的\n去除
    if((event.recvMessage_.iniLine_.size() != 0) && (event.recvMessage_.blank_.size() == 0)){
        while(true){
            ret = GetHeader(event);
            if(ret == -1){
                //说明一行都没读完，直接返回
                return;
            }
            else if(ret >= 0){
                //说明只读到一行，继续循环
                event.inbuffer_.erase(0, ret);
            }
            else{
                //说明读到空行，报头读取完毕，继续进行逻辑
                event.inbuffer_.erase(0, 2);
                break;
            }
        }
    }
    //解析请求行和报头
    if((event.recvMessage_.blank_.size() != 0) && (event.recvMessage_.iniLine_.size() != 0) && (event.recvMessage_.method_.size() == 0)){
        // ParseIniLine(event.recvMessage_);
        // ParseHeader(event.recvMessage_);
        event.recvMessage_.ParseIniLine();
        event.recvMessage_.ParseHeader();   
    }
    
    //读取正文，先判断大小，再判断是否继续读
    if((event.recvMessage_.blank_.size() != 0) && (event.recvMessage_.iniLine_.size() != 0) && (event.recvMessage_.method_.size() != 0)){  
        int content_len = atoi(event.recvMessage_.headerMap_.at("Content-Length").c_str());
        if(content_len > 0 && event.recvMessage_.body_.size() < content_len){
            int len = content_len - event.recvMessage_.body_.size();
            ret = GetBody(len, event.inbuffer_, event.recvMessage_.body_);
            if(ret == 0){
                //读完数据，清理inbuffer
                event.inbuffer_.erase(0, len);
            }
            else{
                //未读完数据，将inbuffer全清空
                event.inbuffer_.erase(0);
            }
            LOG(INFO, std::string("body size: ")+std::to_string(event.recvMessage_.body_.size()));
        }
    }
    //构建任务，交给任务队列处理
    
}