#include "ChatroomServer.hpp"
#include <iostream>
#include <thread>


int main()
{
    ChatroomServer* p = new ChatroomServer(8081);
    p->Loop();

    return 0;
}