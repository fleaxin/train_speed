#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <ctime>
#include <csignal>
#include <cstring>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cerrno>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <thread>
#include <sys/un.h>
#include <unistd.h>
#include <mutex>
#include <vector>
#include <utility>
#include <cmath>
#include <netinet/in.h>

#include "ot_common.h"
#include "ot_type.h"
#include "sample_comm.h"
#include "camera_test_cmd.h"
#include "camera_test_video_path.h"
#include "camera_test.h"
#include "camera_capture.h"
#include "frame_process.h"
#include "zlog.h"
#include "global_logger.h"
#include "read_config.h"
#include "communicate.h"

#define PIPE_PATH "/tmp/train_data"
#define DATA_OUT_SOCKET_PATH "/tmp/train_data.sock"
#define CONTROL_SOCKET_PATH "/tmp/control.sock"
#define MAX_RETRY 5
#define RETRY_INTERVAL 3  // 每隔 3 秒重试一次
#define DATA_BUF_SIZE 256


enum class CommandType {
    SET_MODEL,
    RESTART,
    RESET_DATA,
    UNKNOWN
};

float roundToTwoDecimal(float value) {
    return std::round(value * 100) / 100;
}

std::string toTwoDecimalString(float value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    return oss.str();
}
void create_pipe() {
    // 检查管道是否存在
    if (access(PIPE_PATH, F_OK) == -1) {
        // 管道不存在，创建新管道
        if (mkfifo(PIPE_PATH, 0666) == -1) {
            perror("mkfifo failed");
            exit(EXIT_FAILURE);
        }
    } else {
        // 管道已存在，无需创建
        std::cout << "Pipe already exists." << std::endl;
    }
}

// 数据输出线程
td_void *data_output(td_void *arg)
{
    PthreadInf *inf = (PthreadInf*)arg;
    td_bool start_signal = inf->start_singal;
    SpeedData *speed_data = (SpeedData *)inf->data_struct;

    create_pipe();
    
    int fd = open(PIPE_PATH, O_WRONLY);
    if (fd == -1) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }

    char buf[DATA_BUF_SIZE];
   
    // s

    close(fd);
    // unlink(PIPE_PATH);
}

td_void *data_output_uds(td_void *arg)
{
    signal(SIGPIPE, SIG_IGN);
    PthreadInf *inf = (PthreadInf*)arg;
    td_bool start_signal = inf->start_singal;
    SpeedData *speed_data = (SpeedData *)inf->data_struct;
    td_slong *last_active_time = (td_slong*)&inf->last_active_time;

    char buf[DATA_BUF_SIZE];

    LOG_INFO("Data output thread start");

    while (start_signal) {
        start_signal = inf->start_singal;
        if(!start_signal){
            break;
        }
        int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd == -1) {
            LOG_ERROR("Create socket failed");
            sleep(RETRY_INTERVAL);
            continue;
        }

        struct sockaddr_un server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sun_family = AF_UNIX;
        strncpy(server_addr.sun_path, DATA_OUT_SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

        int retry = 0;
        int connected = 0;

        while (retry < MAX_RETRY && start_signal) {
            start_signal = inf->start_singal;
            if(!start_signal){
                break;
            }
            if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
                connected = 1;
                break;
            }
            retry++;
            LOG_ERROR("Socket connect failed ,the %dth tary...\n", retry);
            sleep(RETRY_INTERVAL);
        }

        if (!connected && start_signal) {
            LOG_ERROR("Can not connect to socket server, try next connect \n");
            close(sockfd);
            sleep(RETRY_INTERVAL);
            continue;
        }

        LOG_INFO("Connect socket server succeed\n");

        // 正常发送数据
        while(start_signal && connected){
            start_signal = inf->start_singal;

            pthread_mutex_lock(&inf->lock);
            *last_active_time = get_time_ms();
            pthread_mutex_unlock(&inf->lock);

            struct timespec ts;
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            ts.tv_sec = tv.tv_sec + 5;  // 设置超时时间为当前时间 + 5 秒
            ts.tv_nsec = tv.tv_usec * 1000;

            int result = sem_timedwait(&speed_data->sem, &ts);
            if (result == -1 && errno == ETIMEDOUT) {
                LOG_ERROR("Get output data semaphore wait timed out\n");
                break;
            } else if (result == -1) {
                LOG_ERROR("sem_timedwait error: %s\n", strerror(errno));
                break;
            }

            int carriage_number = speed_data->carriage_number;
            // float to_current_head_distance = speed_data->to_current_head_distance;
            // float to_current_tail_distance = speed_data->to_current_tail_distance;
            // float to_next_head_distance = speed_data->to_next_head_distance;
            // float to_last_tail_distance = speed_data->to_last_tail_distance;
            // td_s32 time_ref = speed_data->time_ref;
            td_bool is_in_carriage = speed_data->is_in_carriage;
            float speed = speed_data->speed;
            long time = speed_data->time;
            std::string carriage_type = speed_data->carriage_type;
            std::string remark = speed_data->remark;
            std::vector<std::pair<float,float>> carriage_distance_list;// 保存有已通过的车厢，车头车尾的距离
            for(int i = 0; i < carriage_number + 1; i++){// 
                carriage_distance_list.emplace_back(speed_data->carriage_distance_list[i][0], speed_data->carriage_distance_list[i][1]);
            }

            // 构造协议数据
            nlohmann::json j;
            j["carriage_number"] = carriage_number;
            j["is_in_carriage"] = is_in_carriage;
            j["speed"] = toTwoDecimalString(roundToTwoDecimal(speed));
            j["time"] = time;
            j["carriage_type"] = carriage_type;
            j["remark"] = remark;
            nlohmann::json distance_list = nlohmann::json::array();
            for(auto& item : carriage_distance_list){
                nlohmann::json distance_item;
                distance_item["head"] = toTwoDecimalString(roundToTwoDecimal(item.first));
                distance_item["tail"] = toTwoDecimalString(roundToTwoDecimal(item.second));
                distance_list.push_back(distance_item);
            }
            j["carriage_distance_list"]=distance_list;

            std::string json_str = j.dump();
            uint32_t len = htonl(json_str.size()); // 转换为网络字节序

            // 先发送长度
            if (write(sockfd, &len, sizeof(len)) == -1) {
                LOG_ERROR("Train data length write to socket failed");
                break;
            }

            // 再发送数据
            if (write(sockfd, json_str.c_str(), json_str.size()) == -1) {
                LOG_ERROR("Train data write to socket failed");
                break;
            }
        }

        close(sockfd);
        if(!start_signal){
            break;
        }
        std::cout << "Connect close" << std::endl;
        sleep(RETRY_INTERVAL);
    }
    LOG_INFO("Data output thread exit");
}

CommandType getCommandType(const std::string& cmdType) {
    static const std::unordered_map<std::string, CommandType> commandMap = {
        {"SET_MODEL", CommandType::SET_MODEL},
        {"RESTART", CommandType::RESTART},
        {"RESET_DATA", CommandType::RESET_DATA}
    };

    auto it = commandMap.find(cmdType);
    return (it != commandMap.end()) ? it->second : CommandType::UNKNOWN;
}


bool parse_conmmand(const std::string& buffer, int connfd, PthreadInf* inf){

    ControlCommand *command = (ControlCommand*)inf->command_struct;

    try {
        // 解析JSON
        auto data = nlohmann::json::parse(buffer);
        
        // 解析命令类型
        std::string cmd_type = data.value("type", "");
        
        // 发送响应
        auto send_response = [&](const std::string& response) {
            write(connfd, response.c_str(), response.size());
        };

        std::cout << "Received command: " << cmd_type << std::endl;
        // 处理不同指令
        switch (getCommandType(cmd_type)) {
            case CommandType::SET_MODEL:
            {
                auto payload = data.value("payload", nlohmann::json::object());
                int count = payload.value("count", 0);
                auto models = payload.value("models", std::vector<std::string>());   
                
                if(count != models.size()){
                    LOG_ERROR("count =  %d, models.size() = %d,count != models.size(), please check",count,models.size());
                    send_response("{\"status\": \"error\", \"message\": \"count != models.size()\"}");
                    return false;
                }

                LOG_INFO("接收到处理指令: %d节车厢\n", count);

                // 处理 SET_MODEL 
                pthread_mutex_lock(&inf->lock);
                command->set_model.set_model = TD_TRUE;
                for (size_t i = 0; i < models.size(); ++i)
                {
                    strncpy(command->set_model.carriage_mode[i], models[i].c_str(), sizeof(command->set_model.carriage_mode[i]));
                    command->set_model.carriage_mode[i][sizeof(command->set_model.carriage_mode[i]) - 1] = '\0';
                }
                command->set_model.carriage_number = count;
                pthread_mutex_unlock(&inf->lock);

                for (size_t i = 0; i < models.size(); ++i)
                {
                    LOG_INFO("  车厢%zu: %s\n", i + 1, models[i].c_str());
                }
                break;
            }
            case CommandType::RESTART:
            {
                // 处理 RESTART
                auto payload = data.value("payload", nlohmann::json::object());
                std::string target = payload.value("target", "system");
                LOG_INFO("reviced restart command: %s\n", target.c_str());
                break;
            }
            case CommandType::RESET_DATA:
            {
                // 处理 RESET_DATA
                pthread_mutex_lock(&inf->lock);
                command->reset_data = TD_TRUE;
                pthread_mutex_unlock(&inf->lock);


                auto payload = data.value("payload", nlohmann::json::object());
                std::string target = payload.value("target", "all");
                LOG_INFO("reviced reset command: %s\n", target.c_str());
                break;
            }
            case CommandType::UNKNOWN:
            { // 处理 UNKNOWN
                LOG_INFO("recived unknown command: %s\n", buffer.c_str());
            }
        }
        
        // 发送通用响应
        send_response("INFO: recived " + buffer + "\n");
        return true;
        
    } catch (const nlohmann::json::parse_error& e) {
        LOG_ERROR("JSON解析失败: %s\n", e.what());
        std::string error_response = "ERROR: JSON解析失败\n";
        write(connfd, error_response.c_str(), error_response.size());
    }
    catch (const std::exception& e) {
        LOG_ERROR("处理异常: %s\n", e.what());
        std::string error_response = "ERROR: " + std::string(e.what()) + "\n";
        write(connfd, error_response.c_str(), error_response.size());
    }

    
    
    return false;
}

// 控制命令处理线程函数
td_void* control_command_handler(td_void* arg)
{
    PthreadInf* inf = static_cast<PthreadInf*>(arg);
    td_slong *last_active_time = (td_slong*)&inf->last_active_time;
    ControlCommand *command = (ControlCommand*)&inf->data_struct;

    const std::string SOCKET_PATH = CONTROL_SOCKET_PATH;
    
    LOG_INFO("Control command handler thread started\n");
    
    // 创建Unix Domain Socket
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_ERROR("Create control socket failed\n");
        return nullptr;
    }

    // 设置socket为非阻塞模式
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    // 准备socket地址结构
    struct sockaddr_un server_addr{};
    server_addr.sun_family = AF_UNIX;
    
    // 删除可能已存在的socket文件
    unlink(SOCKET_PATH.c_str());
    
    // 复制socket路径
    strncpy(server_addr.sun_path, SOCKET_PATH.c_str(), sizeof(server_addr.sun_path) - 1);
    
    // 绑定socket
    if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        LOG_ERROR("Bind control socket failed\n");
        close(sockfd);
        return nullptr;
    }
    
    // 开始监听
    if (listen(sockfd, 5) < 0) {
        LOG_ERROR("Listen control socket failed\n");
        close(sockfd);
        return nullptr;
    }
    
    LOG_INFO("Control command handler is listening on %s\n", SOCKET_PATH.c_str());
    
    // 循环接收连接和命令
    while (inf->start_singal) {
        
        // 500ms更新一次last_active_time
        static long last_update_time = get_time_ms();
        if (get_time_ms() - last_update_time > 400) {
            pthread_mutex_lock(&inf->lock);
            *last_active_time = get_time_ms();
            pthread_mutex_unlock(&inf->lock);
            last_update_time = get_time_ms();
        }
        
        struct sockaddr_un client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
        // 接受连接
        int connfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (connfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 无连接时短暂休眠，避免CPU空转
                usleep(10000);  // 10ms
                continue;
            }
            // 其他错误处理
            LOG_ERROR("Control command socket accept error: %s", strerror(errno));
            continue;
        }
        
        LOG_INFO("New control connection accepted\n");
        
        // 使用lambda表达式处理客户端通信
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = read(connfd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read <= 0) {
            if (bytes_read < 0) {
                LOG_ERROR("Read control command failed\n");
            }
            close(connfd);
            continue;
        }
        
        // 移除换行符
        if (buffer[bytes_read - 1] == '\n') {
            buffer[bytes_read - 1] = '\0';
        }
        
        // 打印接收到的命令
        LOG_INFO("Received control command: %s\n", buffer);
        
        parse_conmmand(buffer, connfd, inf);
        
        // 关闭连接
        close(connfd);
        LOG_INFO("Control connection closed\n");

    }
    // 清理资源
    close(sockfd);
    unlink(SOCKET_PATH.c_str());
    LOG_INFO("Control command handler thread exited\n");
    
    return nullptr;
}