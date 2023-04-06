#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <strings.h>
#include <iostream>


class Sock
{
private:
    static const int backlog = 5;

public:
    //socket创建监听套接字
    //参数opt为0，则默认会进入TIME_WAIT状态；opt不为0，则不进入TIME_WAIT状态
    //成功返回listen_sock，失败返回-1
    static int Socket(int opt = 0)
    {
        int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if(listen_sock < 0){
            return -1;
        }
        
        //opt不为0则不进入TIME_WAIT状态
        if (opt != 0){
            opt = 1;
            setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        }

        return listen_sock;
    }

    //bind绑定端口和IP，成功返回0，失败返回-1
    static int Bind(int listen_sock, uint16_t port)
    {
        sockaddr_in local;
        bzero(&local, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(port);
        local.sin_addr.s_addr = INADDR_ANY;

        if(bind(listen_sock, (sockaddr*)&local, sizeof(local)) < 0){
            return -1;
        }
        return 0;
    }

    //listen监听。成功返回0，失败返回-1
    static int Listen(int listen_sock)
    {
        if(listen(listen_sock, backlog) < 0){
            return -1;
        }
        return 0;
    }

    static void SetNonBlock(int sock)
    {
        int fl = fcntl(sock, F_GETFL);
        fcntl(sock, F_SETFL, fl | O_NONBLOCK);
    }
};