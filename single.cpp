#include "TCPServer.hpp"
#include "ThreadPool.hpp"
#include "Protocol.hpp"

TcpServer* TcpServer::pt_ = nullptr;

ThreadPool* ThreadPool::ptp_ = nullptr;

Chatroom* Chatroom::pc_ = nullptr;