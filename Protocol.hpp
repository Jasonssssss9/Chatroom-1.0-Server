#pragma once
#include "Log.hpp"
#include "Reactor.hpp"
#include "ThreadPool.hpp"

class Protocol
{
private:
    static int GetIniLine(Event& event);
    static int GetHeader(Event& event);
    static int GetBody(int len, const std::string& in, std::string& out);

public:
    static void GetPerseMessage(Event& event);
    

};