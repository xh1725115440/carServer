#pragma once

#include "../Scheduler/UsageEnvironment.h"
#include "../Scheduler/Event.h"
#include"../Scheduler/ThreadPool.h"
#include "InetAddress.h"
#include "TrDatabase.h"
#include <map>
#include <boost/asio.hpp>
using boost::asio::ip::tcp;

class TrConnection;
class TrServer {

public:
    static TrServer* createNew(UsageEnvironment* env, Ipv4Address& addr, TrDatabase* trdb);

    TrServer(UsageEnvironment* env, Ipv4Address& addr, TrDatabase* trdb);
    ~TrServer();

public:
    void start();
    UsageEnvironment* env() const {//const（常量成员函数）：不会修改类的成员变量的值
        return mEnv;
    }
private:
    static void readCallback(void*);
    void handleRead();
    static void cbDisConnect(void* arg, int clientFd);
    void handleDisConnect(int clientFd);
    static void cbCloseConnect(void* arg);
    void handleCloseConnect();
    static void TaskCallback(void*);

private:
    void thread_process_send_message();
    void remove_by_value(std::map<std::string, int>& map, const int& value);
    UsageEnvironment* mEnv;
    int  mFd;
    Ipv4Address mAddr;
    bool mListen;
    IOEvent* mAcceptIOEvent;
    TrDatabase* Trdb;
    std::map<int, TrConnection*> mConnMap; // <clientFd,conn> 维护所有被创建的连接
    std::vector<int> mDisConnList;//所有被取消的连接 clientFd
    TriggerEvent* mCloseTriggerEvent;// 关闭连接的触发事件
    ThreadPool* mThreadPool;
    Task* ReadTask;
    std::mutex mMtx;
};

