#include "TrServer.h"
#include "TrConnection.h"
#include "../Base/Log.h"
#include "../Scheduler/SocketsOps.h"
#include"../Scheduler/ThreadPool.h"
#include <thread>



TrServer* TrServer::createNew(UsageEnvironment* env, Ipv4Address& addr, TrDatabase* trdb) {

    return new TrServer(env, addr, trdb);
}
TrServer::TrServer(UsageEnvironment* env, Ipv4Address& addr, TrDatabase* trdb) :
    mEnv(env),
    mAddr(addr),
    Trdb(trdb),
    mListen(false)
{
    mFd = sockets::createTcpSock();															//非阻塞的描述符
    sockets::setReuseAddr(mFd, 1);															//设置套接字选项（重用端口）（重启时可能端口并未释放，加上后可以及时重用端口）
    if (!sockets::bind(mFd, addr.getIp(), mAddr.getPort())) {
        return;
    }

    LOGI("rtsp://%s:%d fd=%d", addr.getIp().data(), addr.getPort(), mFd);					//服务端的ip、端口当前套接字

    mAcceptIOEvent = IOEvent::createNew(mFd, this);
    mAcceptIOEvent->setReadCallback(readCallback);											//←主要数据处理//设置回调的socket可读 函数指针端口建立连接事件
    mAcceptIOEvent->enableReadHandling();													//设置为可读

    mCloseTriggerEvent = TriggerEvent::createNew(this);
    mCloseTriggerEvent->setTriggerCallback(cbCloseConnect);									//设置回调的关闭连接 函数指针

    ReadTask = Task::createNew();
    ReadTask->setTaskCallback(TaskCallback, this);

}

TrServer::~TrServer()
{
    if (mListen)
        mEnv->scheduler()->removeIOEvent(mAcceptIOEvent);

    delete mAcceptIOEvent;
    delete mCloseTriggerEvent;

    if (ReadTask) {
        delete ReadTask;
    }

    sockets::close(mFd);
}

void TrServer::thread_process_send_message() {
	try {
        boost::asio::io_context io_context;

        // 创建一个TCP服务器，监听8080端口
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 1234));

        std::cout << "Server is running on port 1234..." << std::endl;

        for (;;) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            // 读取客户端请求
            boost::asio::streambuf request;
            boost::asio::read_until(socket, request, "\r\n");

            std::istream request_stream(&request);
            std::string request_method;
            std::string request_uri;
            std::string http_version;
            request_stream >> request_method >> request_uri >> http_version;

            std::string response = Trdb->handle_request(request_uri);
			//std::cout << response << std::endl;
            // 发送响应
            boost::asio::write(socket, boost::asio::buffer(response));
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    //try {
    //    boost::asio::io_context io_context;

    //    // 创建一个TCP服务器，监听1234端口
    //    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 1234));

    //    std::cout << "Server is running on port 1234..." << std::endl;

    //    for (;;) {
    //        tcp::socket socket(io_context);
    //        acceptor.accept(socket);

    //        try {
    //            // 读取客户端请求
    //            boost::asio::streambuf request;
    //            boost::asio::read_until(socket, request, "\r\n\r\n");

    //            std::istream request_stream(&request);
    //            std::string request_method;
    //            std::string request_uri;
    //            std::string http_version;
    //            request_stream >> request_method >> request_uri >> http_version;

    //            // 处理请求并生成响应
    //            std::string response = Trdb->handle_request(request_uri);

    //            // 发送响应
    //            boost::asio::write(socket, boost::asio::buffer(response));
    //        }
    //        catch (boost::system::system_error& e) {
    //            if (e.code() == boost::asio::error::eof) {
    //                std::cerr << "Client disconnected: " << e.what() << std::endl;
    //                // 这里可以选择继续处理或关闭连接
    //            }
    //            else {
    //                throw;  // 重新抛出异常以捕获其他类型的错误
    //            }
    //        }
    //    }
    //}
    //catch (std::exception& e) {
    //    std::cerr << "Exception: " << e.what() << std::endl;
    //}

}

void TrServer::remove_by_value(std::map<std::string, int>& map, const int& value)				// 移除map中特定的值
{
    for (auto it = map.begin(); it != map.end(); )
    {
        if (it->second == value)
        {
            it = map.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void TrServer::start() {															// 启动服务器
    LOGI("Start thread");
    std::thread t1(&TrServer::thread_process_send_message, this);		// 创建一个线程执行消息发送任务
    t1.detach();														// 分离线程，使其在后台运行
    mListen = true;														// 标记服务器监听状态为true
    sockets::listen(mFd, 60);											// 开始监听客户端连接请求，backlog设置为60（最大链接个数）
    mEnv->scheduler()->addIOEvent(mAcceptIOEvent);						// 将Accept事件添加到事件调度器中
    Trdb->init_db();													// 初始化数据库
}

void TrServer::readCallback(void* arg) {								// 进行读操作的回调函数
    TrServer* trServer = (TrServer*)arg;
    trServer->handleRead();												//←主要的数据处理

}

void TrServer::handleRead() {											//处理mAcceptIOEvent// 处理读事件
    int clientFd = sockets::accept(mFd);								// 接受客户端连接，获取客户端套接字文件描述符
    if (clientFd < 0)
    {
        LOGE("handleRead error,clientFd=%d", clientFd);
        return;
    }
    TrConnection* conn = TrConnection::createNew(this, Trdb, clientFd);		// 创建新的连接对象    ←主要的数据处理
    conn->setDisConnectCallback(TrServer::cbDisConnect, this);				// 设置连接断开回调函数
    mConnMap.insert(std::make_pair(clientFd, conn));						// 将连接对象插入连接映射表中

}
void TrServer::cbDisConnect(void* arg, int clientFd) {					// 连接断开的回调函数
    TrServer* server = (TrServer*)arg;

    server->handleDisConnect(clientFd);
}

void TrServer::handleDisConnect(int clientFd) {								// 处理连接断开事件

    remove_by_value(device_match, clientFd);							// 从设备映射表中移除断开连接的设备

    mDisConnList.push_back(clientFd);									// 将断开连接的客户端套接字文件描述符添加到断开连接列表中

    mEnv->scheduler()->addTriggerEvent(mCloseTriggerEvent);				// 添加关闭连接触发事件到事件调度器中

}

void TrServer::cbCloseConnect(void* arg) {							// 关闭连接的回调函数
    TrServer* server = (TrServer*)arg;
    server->handleCloseConnect();
}

void TrServer::handleCloseConnect() {																// 处理关闭连接事件


    for (std::vector<int>::iterator it = mDisConnList.begin(); it != mDisConnList.end(); ++it) {

        int clientFd = *it;									//it指向的键值对中的键赋值给clientFd
        std::map<int, TrConnection*>::iterator _it = mConnMap.find(clientFd);
        assert(_it != mConnMap.end());						//进行断言检查检查指向是否有效，如果无效直接终止程序
        delete _it->second;																			// 删除连接对象
        mConnMap.erase(clientFd);																	// 从连接映射表中移除连接
        remove_by_value(device_match, clientFd);													// 从设备映射表中移除断开连接的设备
    }

    mDisConnList.clear();																		// 清空断开连接列表

}

void TrServer::TaskCallback(void* arg)
{
    //TrServer* trServer = (TrServer*)arg;
    //trServer->handleRead();												//←主要的数据处理
}