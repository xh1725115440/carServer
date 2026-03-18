#pragma once
#ifndef _TrConnection_H_
#define _TrConnection_H_
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/asio.hpp>
#include <openssl/md5.h>
#include <filesystem>
#include "../Scheduler/SocketsOps.h"
#include "MegParse.h"
#include "TcpConnection.h"
#include "TrDatabase.h"
namespace fs = std::filesystem;

const size_t CHUNK_SIZE = 4096;
const std::string BASE_DIR = "/database/accept"; // 确保为绝对路径或相对路径

class TrServer;
class TrDatabase;
class TrConnection : public TcpConnection
{
public:

	static TrConnection* createNew(TrServer* TrServer, TrDatabase* TrDatabase, int clientFd);

	TrConnection(TrServer* TrServer, TrDatabase* TrDatabase, int clientFd);
	virtual ~TrConnection();
protected:
	virtual void handleReadBytes(int client_socket);
	void analysis_path(struct Car_DATA*& car_data, std::string file_path);

private:
	std::string calculate_md5(const fs::path& path);
	char* getTime();
	char tmp_time[64];//时间戳储存
	std::string mPeerIp;
	TrDatabase* Trdb;
	int clientfd_cur;
	void* mArg;
	//void receive_file(tcp::socket& socket);
	//void server(boost::asio::io_service& io_service, short port);
};


#endif // !_TCPSERVER_H_



