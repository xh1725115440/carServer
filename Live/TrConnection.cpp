#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <openssl/md5.h>
#include "../Scheduler/SocketsOps.h"
#include "TrConnection.h"
#include "TrServer.h"
#include <stdio.h>
#include "../Base/Log.h"
#include <string.h>
#include <iostream>
#include <regex>




#include <math.h>
std::map<std::string, int> device_match;


static void getPeerIp(int fd, std::string& ip)
{
    struct sockaddr_in addr;														// 创建一个 IPv4 地址结构体
    socklen_t addrlen = sizeof(struct sockaddr_in);									// 获取地址结构体的大小
    getpeername(fd, (struct sockaddr*)&addr, &addrlen);								// 使用 getpeername 函数获取与指定文件描述符相关联的对等端地址信息
    ip = inet_ntoa(addr.sin_addr);													// 将对等端的 IP 地址转换为字符串，并存储在传入的 std::string 对象 ip 中
}

TrConnection* TrConnection::createNew(TrServer* TrServer, TrDatabase* TrDatabase, int clientFd)				// 创建一个新的 TrConnection 对象并返回指针
{
    return new TrConnection(TrServer, TrDatabase, clientFd);
}

TrConnection::TrConnection(TrServer* TrServer, TrDatabase* TrDatabase, int clientFd) :
    TcpConnection(TrServer->env(), clientFd),											// 调用 TcpConnection 的构造函数，传入 TrServer 的环境变量指针和客户端文件描述符
    Trdb(TrDatabase),																	// 初始化 Trdb 成员变量为传入的 TrDatabase 指针
    clientfd_cur(clientFd)																// 初始化 clientfd_cur 成员变量为传入的客户端文件描述符
{
    LOGI("TrConnection() mClientFd=%d", mClientFd);										// 输出日志，显示客户端文件描述符
    getPeerIp(clientFd, mPeerIp);														// 获取与客户端连接的对等端 IP 地址，并存储到 mPeerIp 成员变量中

}

TrConnection::~TrConnection()
{
    LOGI("~TrConnection() mClientFd=%d", mClientFd);
}

char* TrConnection::getTime()							//获取时间 //当地时间应该是本机电脑的时间
{
    time_t timep;																	// 声明一个 time_t 类型的变量，用于存储时间信息
    time(&timep);																	// 获取当前时间，将其保存在 timep 变量中
    strftime(tmp_time, sizeof(tmp_time), "%Y-%m-%d %H:%M:%S", localtime(&timep));	// 使用 strftime 函数将时间格式化为字符串，并将结果存储在 tmp_time 数组中
    return tmp_time;
}

std::string TrConnection::calculate_md5(const fs::path& path) {
    // MD5计算函数的实现
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file for MD5 calculation");
    }

    MD5_CTX md5Context;
    MD5_Init(&md5Context);

    std::vector<char> buffer(CHUNK_SIZE);
    while (file.good()) {
        file.read(buffer.data(), buffer.size());
        MD5_Update(&md5Context, buffer.data(), file.gcount());
    }

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5Context);

    char hex_output[MD5_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        sprintf(hex_output + i * 2, "%02x", result[i]);
    }

    return std::string(hex_output);
}

void TrConnection::handleReadBytes(int client_socket) {
        try {
            auto recv_all = [client_socket, this](void* buffer, size_t length) {
                size_t total_received = 0;
                while (total_received < length) {
                    //cout << total_received << endl;
                    //cout << length << endl;
                    ssize_t bytes_received = ::recv(client_socket, static_cast<char*>(buffer) + total_received, length - total_received, 0);
                    if (bytes_received == 0) {
                         //远程端关闭连接 (EOF)
                        LOGE("read error,fd=%d", client_socket);
                        this->handleDisConnect();                                         // 处理断开连接事件
                        throw std::runtime_error("disconnect");
                        break;
                    }
                    else if (bytes_received < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            continue; // 非阻塞模式下，可以重试
                        }
                        else {
                            throw std::runtime_error(std::strerror(errno));
                        }
                    }
                    total_received += bytes_received;
                }
                return total_received;
                };

            Car_DATA* car_data = new Car_DATA;
            for (int i = 0; i < 3; i++)
            {
                uint32_t name_length;
                recv_all(&name_length, sizeof(name_length));
                name_length = ntohl(name_length);
                snprintf(car_data->name_length, sizeof(car_data->name_length), "%u", name_length);
                cout << "name_length:" << name_length << endl;

                std::vector<char> file_name(name_length + 1);
                recv_all(file_name.data(), name_length);
                file_name[name_length] = '\0';
                std::string file_path(file_name.data(), name_length);
                //strncpy(car_data->file_path, file_name.data(), sizeof(car_data->file_path) - 1);
                //car_data->file_path[sizeof(car_data->file_path) - 1] = '\0';
                cout << "file_path:" << file_path << endl;
                //analysis_path(car_data, file_path);

                uint32_t file_size;
                recv_all(&file_size, sizeof(file_size));
                file_size = ntohl(file_size);
                snprintf(car_data->file_size, sizeof(car_data->file_size), "%u", file_size);
                cout << "file_size:" << file_size << endl;

                std::string::size_type pos = file_path.find("DataSet");
                if (pos == std::string::npos) {
                    throw std::runtime_error("Invalid file path: 'DataSet' not found");
                }

                fs::path relative_path = file_path.substr(pos);
                fs::path accept_path = fs::path(BASE_DIR) / relative_path;
                fs::create_directories(accept_path.parent_path());

                std::ofstream file(accept_path, std::ios::binary);
                if (!file) {
                    throw std::runtime_error("Failed to create file: " + accept_path.string());
                }
                std::string path_str = accept_path.string();
                //strncpy(car_data->file_path, path_str.c_str(), sizeof(car_data->file_size) - 1);
                //car_data->file_path[sizeof(car_data->file_path) - 1] = '\0';
				std::cout << "path_str:" << path_str << std::endl;
                analysis_path(car_data, path_str);

                std::vector<char> buffer(CHUNK_SIZE);
                size_t total_bytes_received = 0;

                while (total_bytes_received < file_size) {
                    size_t bytes_to_read = std::min(CHUNK_SIZE, file_size - total_bytes_received);
                    ssize_t bytes_received = recv_all(buffer.data(), bytes_to_read);

                    if (bytes_received == 0) {
                        break; // 远程端关闭连接 (EOF)
                    }

                    file.write(buffer.data(), bytes_received);
                    total_bytes_received += bytes_received;

                    std::cout << "\rReceiving: " << (100 * total_bytes_received / file_size) << "%" << std::flush;
                }
                std::cout << std::endl;

                file.close();

                std::string local_hash = calculate_md5(accept_path);
                std::cout << "Calculated MD5: " << local_hash << std::endl;

                uint32_t hash_length;
                recv_all(&hash_length, sizeof(hash_length));
                hash_length = ntohl(hash_length);

                std::vector<char> hash(hash_length);
                recv_all(hash.data(), hash_length);
                std::string received_hash(hash.data(), hash_length);
                std::cout << "Received MD5: " << received_hash << std::endl;

                if (local_hash == received_hash) {
                    std::cout << "File received successfully and MD5 hash verified: " << local_hash << std::endl;
                    std::cout << "File saved at: " << accept_path << std::endl;
                }
                else {
                    std::cerr << "MD5 hash mismatch! Local: " << local_hash << ", Received: " << received_hash << std::endl;
                }
            }
            strcpy(car_data->car_com_time, getTime());
            car_data->year[0] = car_data->car_com_time[2];
            car_data->year[1] = car_data->car_com_time[3];
            car_data->month[0] = car_data->car_com_time[5];
            car_data->month[1] = car_data->car_com_time[6];
            Trdb->handle(car_data);
            delete car_data;
        }
        catch (std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
}

void TrConnection::analysis_path(struct Car_DATA*& car_data, std::string file_path)
{
    int flag_start;
    std::string sensor_temp;
    flag_start = file_path.find("DataSet") + 8;
	sensor_temp = file_path.substr(flag_start, 2);
    strcpy(car_data->car_id, sensor_temp.c_str());
    
    //flag_start = file_path.find("DataSet") + 11;
    //sensor_temp = file_path.substr(flag_start, 10);
    //strcpy(car_data->car_get_time, sensor_temp.c_str());

    std::regex pattern(R"((\d{4}-\d{2}-\d{2})/(\d{2})-(\d{2})-(\d{2}))");

    // 搜索匹配
    std::smatch match;
    if (std::regex_search(file_path, match, pattern)) {
        // 获取日期和时间
        std::string date = match[1].str();
        std::string time = match[2].str() + ":" + match[3].str() + ":" + match[4].str();
        std::string result = date + " " + time;
        std::cout << result << std::endl;  // 输出结果：2019-08-10 11:08:01
        strcpy(car_data->car_get_time, result.c_str());
    }
    else {
        std::cout << "未找到日期和时间信息" << std::endl;
        std::string result = "not find";
        strcpy(car_data->car_get_time, result.c_str());
    }

    //std::cout << file_path.find("雷达点云") << std::endl;

	if (file_path.find("radar") != std::string::npos)
    {
        strcpy(car_data->radar_file_path, file_path.c_str());
        std::cout << "it is radar_file_path" << std::endl;
	}
	else if (file_path.find("inside") != std::string::npos)
    {
        strcpy(car_data->inside_file_path, file_path.c_str());
        std::cout << "it is inside_file_path" << std::endl;
	}
    else if (file_path.find("outside") != std::string::npos)
    {
        strcpy(car_data->outside_file_path, file_path.c_str());
        std::cout << "it is outside_file_path" << std::endl;
	}
	else
	{
        strcpy(car_data->file_path, file_path.c_str());
		std::cout << "error" << std::endl;
	}
}
