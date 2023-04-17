#include "TCPServer.hpp"
#include "ThreadPool.hpp"
#include "Protocol.hpp"

TcpServer* TcpServer::pt_ = nullptr;

template<>
ThreadPool<ChatMessage, Protocol>* ThreadPool<ChatMessage, Protocol>::ptp_ = nullptr;
//注意语法，这里的定义是显示定义，<>中直接放类型，前面还要加template<>

Chatroom* Chatroom::pc_ = nullptr;