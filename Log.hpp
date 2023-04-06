#pragma once
#include <ctime>
#include <iostream>
#include <string>


#define LOG(level, message) Log(#level, message, __FILE__, __LINE__)

static void Log(std::string level, std::string message, std::string file, int line)
{
    std::cout << "[" << level << "]" << "[" << time(nullptr) << "]" << "[" << message << "]" << "[" << file << "]" << "[" << line << "]" << std::endl;
}