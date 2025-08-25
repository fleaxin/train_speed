#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include "sys/time.h"
#include <sys/stat.h>

#include "ot_type.h"
#include "ot_common.h"
#include "sample_comm.h"

class CommandServer {
    int cmd_fd;
    int resp_fd;
    
public:
    CommandServer() {
        cmd_fd = open("/tmp/camera_cmd", O_RDONLY);
        resp_fd = open("/tmp/camera_resp", O_WRONLY);
    }
    
    void run() {
        uint8_t header[8];
        while(true) {
            ssize_t n = read(cmd_fd, header, 8);
            if(n == 8) {
                process_command(header);
            }
            usleep(10000); // 10ms
        }
    }
    
private:
    void process_command(uint8_t* header) {
        uint8_t magic = header[0];
        uint8_t type = header[1];
        uint16_t plen = *(uint16_t*)(header+2);
        uint32_t timestamp = *(uint32_t*)(header+4);
        
        // 读取payload
        uint8_t* payload = new uint8_t[plen];
        read(cmd_fd, payload, plen);
        
        // 生成响应
        uint8_t resp_header[4] = {0xB0, 0x00, 0, 0};
        uint8_t* resp_payload = nullptr;
        uint16_t resp_len = 0;
        
        switch(type) {
            case 0x01: { // 获取曝光
                float exp = get_current_exposure();
                resp_len = sizeof(float);
                resp_payload = (uint8_t*)&exp;
                break;
            }
            // 其他命令处理...
        }
        
        *(uint16_t*)(resp_header+2) = resp_len;
        write(resp_fd, resp_header, 4);
        if(resp_payload) {
            write(resp_fd, resp_payload, resp_len);
        }
        
        delete[] payload;
    }

    float get_current_exposure();
};