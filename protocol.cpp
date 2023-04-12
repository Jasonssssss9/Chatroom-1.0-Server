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

//注册
void Protocol::SignUp(Event& event)
{   
    //确定对方希望注册的用户名
    auto it = event.recvMessage_.headerMap_.find("User");
    std::string name;
    if(it == event.recvMessage_.headerMap_.end()){
        //差错处理，返回格式错误响应
        event.sendMessage_.status_ = "401";

        LOG(WARNING, "Wrong fromatioin");
        return;
    }

    name = it->second;
    if(IsUserExist(name)){
        //差错处理，如果当前名字存在，返回用户名重复响应
        event.sendMessage_.headerMap_.at("Return") = "wrong";
        event.sendMessage_.headerMap_.insert(std::make_pair("Wrong", "dup_user"));

        LOG(WARNING, "Duplicated user name");
        return;
    }

    //用户名不重复，则加入到Chatroom中
    //先取出密码
    it = event.recvMessage_.headerMap_.find("Password");
    std::string password;
    if(it == event.recvMessage_.headerMap_.end()){
        //差错处理，返回格式错误响应
        event.sendMessage_.status_ = "401";

        LOG(WARNING, "Wrong fromatioin");
        return;
    }
    password = it->second;

    //插入到users中
    auto& users = Chatroom::GetInstance()->GetUsersInfo();
    users.insert(std::make_pair(name, password));

    LOG(INFO, std::string("Sign up, name: ")+name+std::string(", password: ")+users.at(name));
}

//登录
void Protocol::SignIn(Event& event)
{
    //确定对方输入的用户名和密码
    std::string name;
    std::string password;
    auto it1 = event.recvMessage_.headerMap_.find("User");
    auto it2 = event.recvMessage_.headerMap_.find("Password");
    if(it1 == event.recvMessage_.headerMap_.end() || it2 == event.recvMessage_.headerMap_.end()){
        //差错处理，返回格式错误响应
        event.sendMessage_.status_ = "401";

        LOG(WARNING, "Wrong fromatioin");
        return;
    }
    name = it1->second;
    password = it2->second;
    std::cout << "######: " << name;

    if(!IsUserExist(name)){
        //差错处理，返回当前用户不存在
        event.sendMessage_.headerMap_.at("Return") = "wrong";
        event.sendMessage_.headerMap_.insert(std::make_pair("Wrong", "no_user"));

        LOG(WARNING, std::string("No such user, name: ")+name);
        return;
    }

    //和服务器存储的密码进行对比
    const std::string& stored_pw = Chatroom::GetInstance()->GetUsersInfo().at(name);
    if(password != stored_pw){
        //差错处理，返回密码错误
        event.sendMessage_.headerMap_.at("Return") = "wrong";
        event.sendMessage_.headerMap_.insert(std::make_pair("Wrong", "pw"));

        LOG(WARNING, "Wrong password");
        return;
    }

    //将当前用户设置为在线状态
    auto& online = Chatroom::GetInstance()->GetOnlineInfo();
    auto it3 = online.find(name);
    if(it3 == online.end()){
        //如果还没登录
        online.insert(std::make_pair(name, event.sock_));
    }
    //如果已经登录，不用设置，依然返回正常登录

    LOG(INFO, std::string("One user is signing in, name: ")+name);
}


//当前name存在，即name对应一个user，返回true；反之返回false
bool Protocol::IsUserExist(std::string name)
{
    const auto& users = Chatroom::GetInstance()->GetUsersInfo();
    auto it = users.find(name);
    if(it != users.end()){
        return true;
    }
    return false;
}

//如果name不存在，返回make_pair(false, std::string())；反之返回make_pair(true, password);
std::pair<bool, std::string> Protocol::GetPassword(std::string name)
{
    if(!IsUserExist(name)){
        return std::make_pair(false, std::string());
    }
    const auto& users = Chatroom::GetInstance()->GetUsersInfo();
    std::string password = users.at(name);
    return std::make_pair(true, password);
}

//如果用户已经登陆，返回true；反之返回false
bool Protocol::IsSignIn(std::string name)
{
    const auto& online = Chatroom::GetInstance()->GetOnlineInfo();
    auto it3 = online.find(name);
    if(it3 == online.end()){
        return false;
    }
    return true;
}

void Protocol::BuildResMessage(Event& event)
{
    auto& ini_line = event.sendMessage_.iniLine_;
    auto& headers = event.sendMessage_.headers_;
    auto& blank = event.sendMessage_.blank_;
    blank = LINE_END;

    //构建初始行
    ini_line += event.sendMessage_.method_;
    ini_line += ' ';
    ini_line += event.sendMessage_.status_;
    ini_line += ' ';
    ini_line += event.sendMessage_.version_;
    ini_line += LINE_END;

    LOG(INFO, std::string("iniLine: ")+ini_line);

    //构建报头
    for(auto p : event.sendMessage_.headerMap_){
        std::string tmp;
        tmp += p.first;
        tmp += ": ";
        tmp += p.second;
        tmp += LINE_END;
        headers.push_back(tmp);

        LOG(INFO, std::string("Res headrs: ")+tmp);
    }
    headers.emplace_back(LINE_END);

    //body已经被设置好，此时只需要发送即可
}

void Protocol::ClearEvent(Event& event)
{
    event.inbuffer_.clear();
    event.recvMessage_.Clear();
    event.sendMessage_.Clear();
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

    //如果收到Req报文，构建ReqHandler任务；如果收到Res报文，构建ResHandler任务
    //将任务push到任务队列中，再退出
    if(event.recvMessage_.method_ == "REQ"){
        Task task([&event]{
            ReqHandler(event);
        });
        ThreadPool::GetInstance()->AddTask(task);
    }
    else if(event.recvMessage_.method_ == "RES"){
        Task task([&event]{
            ResHandler(event);
        });
        ThreadPool::GetInstance()->AddTask(task);        
    }
    else{
        LOG(ERROR, "Wrong method");
        //差错处理，返回错误报文
    }
}


//处理REQ报文
void Protocol::ReqHandler(Event& event)
{
    LOG(INFO, "Request handler");

    //在此函数中直接设置sendMessage中的信息，实际上就是告诉发送线程该怎么生成发送报文
    event.sendMessage_.version_ = VERSION;
    event.sendMessage_.headerMap_.insert(std::make_pair("Content-Length", "0"));

    auto& status = event.recvMessage_.status_;
    switch(status[0]){
        //基础管理功能
        case '0':{
            if(status == "010"){
                //申请注册
                event.sendMessage_.method_ = "RES";
                event.sendMessage_.status_ = "011";
                //先假设没有问题，设置Return为Right
                event.sendMessage_.headerMap_.insert(std::make_pair("Return", "right"));
                Protocol::SignUp(event);
            }
            else if(status == "020"){
                //用户登录
                event.sendMessage_.method_ = "RES";
                event.sendMessage_.status_ = "021";
                event.sendMessage_.headerMap_.insert(std::make_pair("Return", "right"));
                Protocol::SignIn(event);
            }
            else if(status == "030"){
                //用户退出
            }
            else{

            }
            break;
        }
        //消息相关
        case '1':{
            break;
        }
        //群聊相关
        case '2':{
            break;
        }
        //文件相关
        case '3':{
            break;
        }
        default:{
            //差错处理
            break;
        }
    }

    //建立发送Res报文任务并加入任务队列
    Task task([&event]{
        SendResponse(event);
    });
    ThreadPool::GetInstance()->AddTask(task);
}

void Protocol::ResHandler(Event& event)
{

}


void Protocol::SendResponse(Event& event)
{
    LOG(INFO, "Send response");

    //构建响应报文
    BuildResMessage(event);

    //发送响应报文，只需要将内容放入outbuffer，并且设置写使能即可
    event.outbuffer_ += event.sendMessage_.iniLine_;
    for(auto& s : event.sendMessage_.headers_){
        event.outbuffer_ += s;
    }
    event.outbuffer_ += event.sendMessage_.blank_;
    event.outbuffer_ += event.sendMessage_.body_;

    (event.pr_)->EnableReadWrite(event.sock_, true, true);

    //清除event内容，只留下outbuffer，其内容会在发送时清除
    //保证等到下一次接收时，event除了sock_和pr_，其他都是空的
    ClearEvent(event);
}

void Protocol::SendInform(Event& event)
{

}