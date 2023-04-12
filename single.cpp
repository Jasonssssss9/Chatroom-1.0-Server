#include "TCPServer.hpp"
#include "ThreadPool.hpp"

TcpServer* TcpServer::pt_ = nullptr;

ThreadPool* ThreadPool::ptp_ = nullptr;