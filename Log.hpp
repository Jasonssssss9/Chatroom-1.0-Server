#pragma once
#include <ctime>
#include <iostream>
#include <string>


#define LOG(level, message) Log(#level, message, __FILE__, __LINE__)

static void Log(std::string level, std::string message, std::string file, int line)
{
    if(message[message.size()-2] == '\r' && message[message.size()-1] == '\n'){
        message.resize(message.size()-2);
    }
    else if(message[message.size()-1] == '\n'){
        message.resize(message.size()-1);
    }
    
    std::cout << "[" << level << "]" << "[" << time(nullptr) << "]" << "[" << message << "]" << "[" << file << "]" << "[" << line << "]" << std::endl;
}