#include "ChatroomServer.hpp"

int main()
{
    ChatroomServer* p = new ChatroomServer(8081);
    p->Loop();

    return 0;
}