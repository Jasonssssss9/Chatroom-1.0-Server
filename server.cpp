#include "ChatroomServer.hpp"
#include <iostream>
#include <thread>

#include "Util.hpp"
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <string>
#include <vector>

int main()
{
    ChatroomServer* p = new ChatroomServer(8081);
    p->Loop();

    return 0;
}