#include "read_config.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstring>
#include "global_logger.h"

using json = nlohmann::json;

const std::string JSON_FILE_NAME = "config.json";
const std::string JSON_CARRIAGE_INFO_FILE_NAME = "carriage_info.json";

static GlobalConfig g_config; // 静态存储配置

int init_carriage_info(const char* json_path) {
    try {
        // 构造完整路径
        std::string fullPath = json_path;
        if (fullPath.empty() || fullPath.back() != '/') {
            fullPath += '/';
        }
        fullPath += JSON_CARRIAGE_INFO_FILE_NAME;

        // 打开文件
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open carriage info file: %s", fullPath.c_str());
            return -1;
        }

        // 解析 JSON
        json info;
        file >> info;

        // 检查 carriage_info 节点
        if (!info.contains("carriage_info") || !info["carriage_info"].is_object()) {
            LOG_ERROR("Missing or invalid 'carriage_info' section in %s", fullPath.c_str());
            return -1;
        }

        // 遍历 JSON 对象
        int index = 0;
        for (auto& [carriage_key, carriage_json] : info["carriage_info"].items()) {
            // 检查数组边界
            if (index >= 256) {
                LOG_WARN("Too many carriage entries, max 256 allowed");
                break;
            }

            CarriageInfo* carriage = &g_config.carriage_info_list.carriage[index++];
            g_config.carriage_info_list.carriage_num = index;
            std::string model_name = carriage_key;
            
            // 拷贝模型名称
            strncpy(carriage->model_name, model_name.c_str(), sizeof(carriage->model_name) - 1);
            carriage->model_name[sizeof(carriage->model_name) - 1] = '\0';

            // 读取字段值
            if (carriage_json.contains("length_px")) {
                carriage->length_px = carriage_json["length_px"].get<float>();
            }

            if (carriage_json.contains("length_meter")) {
                carriage->length_meter = carriage_json["length_meter"].get<float>();
            }

            if (carriage_json.contains("px_to_meter_ratio")) {
                carriage->px_to_meter_ratio = carriage_json["px_to_meter_ratio"].get<float>();
            }

            if (carriage_json.contains("first_edge_to_head")) {
                carriage->first_edge_to_head = carriage_json["first_edge_to_head"].get<float>();
            }

            if (carriage_json.contains("second_edge_to_tail")) {
                carriage->second_edge_to_tail = carriage_json["second_edge_to_tail"].get<float>();
            }

            // 读取 scale_ratio 数组
            if (carriage_json.contains("scale_ratio") && carriage_json["scale_ratio"].is_array()
                && carriage_json["scale_ratio"].size() == 8) {
                auto ratio_array = carriage_json["scale_ratio"];
                int array_size = std::min((int)ratio_array.size(), 8);
                
                for (int i = 0; i < array_size; ++i) {
                    carriage->scale_ratio[i] = ratio_array[i].get<float>();
                }
            }
        }

        LOG_INFO("Loaded %d carriage configurations", index);
        return 0;
    } 
    catch (const std::exception& e) {
        LOG_ERROR("Parse carriage info error: %s", e.what());
        return -1;
    }
}


int init_config(const char* json_path) {
    try {
        // 初始化 g_config 结构体的默认值
        g_config.local_frame_test = 0;
        strncpy(g_config.local_frame_path, "", sizeof(g_config.local_frame_path) - 1);
        g_config.local_frame_path[sizeof(g_config.local_frame_path) - 1] = '\0'; // 确保终止
        strncpy(g_config.local_video_path, "", sizeof(g_config.local_video_path) - 1);
        g_config.local_video_path[sizeof(g_config.local_video_path) - 1] = '\0'; // 确保终止
        g_config.start_frame = 0;
        g_config.push_video = 1;
        g_config.save_file = 0;

        g_config.camera.rate = 25;
        g_config.camera.height = 1200;
        g_config.camera.width = 2448;
        g_config.camera.exposure_time = 40000;// 曝光时间最长时间 单位：us
        g_config.camera.AAROI_height = 800;// 自动曝光区域高度
        g_config.camera.AAROI_width = 2448;// 自动曝光区域宽度
        g_config.camera.AAROI_offsetx = 0;// 自动曝光区域X偏移
        g_config.camera.AAROI_offsety = 650;// 自动曝光区域Y偏移

        strncpy(g_config.push_config.push_url, "http://default.push.url", sizeof(g_config.push_config.push_url) - 1);
        g_config.push_config.push_url[sizeof(g_config.push_config.push_url) - 1] = '\0'; // 确保终止
        g_config.push_config.push_port = 544;
        g_config.push_config.push_frame_rate = 25;
        g_config.push_config.push_frame_height = 1280;
        g_config.push_config.push_frame_width = 1920;

        const int default_pmf_coef[] = {524288, 0, 0, 0, 524288, 0, 0, 0, 524288};// 不进行调整的参数
        for (size_t i = 0; i < sizeof(default_pmf_coef) / sizeof(default_pmf_coef[0]); ++i) {
            g_config.pmf_coef[i] = default_pmf_coef[i];
        }
        const int default_ldc_v2_attr_class[] = {240880, 240731, 120874, 54985, 39024};
        for (size_t i = 0; i < sizeof(default_ldc_v2_attr_class) / sizeof(default_ldc_v2_attr_class[0]); ++i) {
            g_config.ldc.ldc_v2_attr_class[i] = default_ldc_v2_attr_class[i];
        }
        const int default_ldc_v2_attr_src_calibration_ratio[] = {100000,-10097,23683,-42847,0,0,0,0,3200000};
        for (size_t i = 0; i < sizeof(default_ldc_v2_attr_src_calibration_ratio) / sizeof(default_ldc_v2_attr_src_calibration_ratio[0]); ++i) {
            g_config.ldc.ldc_v2_attr_src_calibration_ratio[i] = default_ldc_v2_attr_src_calibration_ratio[i];
        }
        const int default_ldc_v2_attr_dst_calibration_ratio[] = {0,0,0,0,100000,0,0,0,100000,0,0,0,0,25838};
        for (size_t i = 0; i < sizeof(default_ldc_v2_attr_dst_calibration_ratio) / sizeof(default_ldc_v2_attr_dst_calibration_ratio[0]); ++i) {
            g_config.ldc.ldc_v2_attr_dst_calibration_ratio[i] = default_ldc_v2_attr_dst_calibration_ratio[i];
        }

        // 构造完整路径
        std::string fullPath = json_path;
        if (fullPath.empty() || fullPath.back() != '/') {
            fullPath += '/';
        }
        fullPath += JSON_FILE_NAME;

        // 读取JSON文件
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open config file: %s", fullPath);
            return -1;
        }

        // 解析JSON
        json j;
        file >> j;

        // 检查model_path 是否存在
        if (j.contains("model_path")) {
            std::string model_path = j["model_path"];
            strncpy(g_config.model_path, model_path.c_str(), sizeof(g_config.model_path) - 1);
            g_config.model_path[sizeof(g_config.model_path) - 1] = '\0'; // 确保终止
        }else{
            LOG_ERROR("model_path not found in config file: %s", fullPath);
            return -1;
        }

        // 检查 local_frame_test 是否存在
        if (j.contains("local_frame_test")) {
            g_config.local_frame_test = j["local_frame_test"];
        }

        // 检查 local_frame_path 是否存在
        if (j.contains("local_frame_path")) {
            std::string local_frame_path = j["local_frame_path"];
            strncpy(g_config.local_frame_path, local_frame_path.c_str(), sizeof(g_config.local_frame_path) - 1);
            g_config.local_frame_path[sizeof(g_config.local_frame_path) - 1] = '\0'; // 确保终止
        }

        // 检查 local_video_path 是否存在
        if (j.contains("local_video_path")) {
            std::string local_video_path = j["local_video_path"];
            strncpy(g_config.local_video_path, local_video_path.c_str(), sizeof(g_config.local_video_path) - 1);
            g_config.local_video_path[sizeof(g_config.local_video_path) - 1] = '\0'; // 确保终止
        }

        // 检查 start_frame 是否存在
        if (j.contains("start_frame")) {
            g_config.start_frame = j["start_frame"];
        }

        // 检查 push_video 是否存在
        if (j.contains("push_video")) {
            g_config.push_video = j["push_video"];
        }

        // 检查 save_file 是否存在
        if (j.contains("save_file")) {
            g_config.save_file = j["save_file"];
        }

        // 检查 camera 节点是否存在
        if (j.contains("camera") && j["camera"].is_object()) {
        json cam = j["camera"];
        if (cam.contains("rate")) g_config.camera.rate = cam["rate"];
        if (cam.contains("height")) g_config.camera.height = cam["height"];
        if (cam.contains("width")) g_config.camera.width = cam["width"];
        if (cam.contains("offset_x")) g_config.camera.offset_x = cam["offset_x"];
        if (cam.contains("offset_y")) g_config.camera.offset_y = cam["offset_y"];
        if (cam.contains("exposure_time")) g_config.camera.exposure_time = cam["exposure_time"];
        if (cam.contains("AAROI_height")) g_config.camera.AAROI_height = cam["AAROI_height"];
        if (cam.contains("AAROI_width")) g_config.camera.AAROI_width = cam["AAROI_width"];
        if (cam.contains("AAROI_offsetx")) g_config.camera.AAROI_offsetx = cam["AAROI_offsetx"];
        if (cam.contains("AAROI_offsety")) g_config.camera.AAROI_offsety = cam["AAROI_offsety"];
        }

        if (j.contains("push_config") && j["push_config"].is_object()){
            json push_config = j["push_config"];
            
            // 检查 push_url 是否存在
            if (push_config.contains("push_url")) {
                std::string push_url_str = push_config["push_url"];
                strncpy(g_config.push_config.push_url, push_url_str.c_str(), sizeof(g_config.push_config.push_url) - 1);
                g_config.push_config.push_url[sizeof(g_config.push_config.push_url) - 1] = '\0'; // 确保终止
            }

            // 检查 push_port 是否存在
            if (push_config.contains("push_port")) {
                g_config.push_config.push_port = push_config["push_port"];
            }

            // 检查 push_frame_rate 是否存在
            if (push_config.contains("push_frame_rate")) {
                g_config.push_config.push_frame_rate = push_config["push_frame_rate"];
            }
            
            // 检查 push_frame_height 是否存在
            if (push_config.contains("push_frame_height")) {
                g_config.push_config.push_frame_height = push_config["push_frame_height"];
            }

            // 检查 push_frame_width 是否存在
            if (push_config.contains("push_frame_width")) {
                g_config.push_config.push_frame_width = push_config["push_frame_width"];
            }
        }

        // 检查 pmf_coef 是否存在
        if (j.contains("pmf_coef") && j["pmf_coef"].is_array() && j["pmf_coef"].size() == 9) {
            for (size_t i = 0; i < j["pmf_coef"].size(); ++i) {
                g_config.pmf_coef[i] = j["pmf_coef"][i];
            }
        } else {
            LOG_WARN("pmf_coef array not found or not of size 9, using default values.");
        }

        // 检查 ldc 节点是否存在
        if (j.contains("ldc") && j["ldc"].is_object()) { 
            json ldc = j["ldc"];

            // 检查 ldc_v2_attr_class 存在
            if (ldc.contains("ldc_v2_attr_class") && ldc["ldc_v2_attr_class"].is_array() && ldc["ldc_v2_attr_class"].size() == 5) {
                for (size_t i = 0; i < ldc["ldc_v2_attr_class"].size(); ++i) {
                    g_config.ldc.ldc_v2_attr_class[i] = ldc["ldc_v2_attr_class"][i];
                }
            }else {
                LOG_WARN("ldc_v2_attr_class array not found or not of size 5, using default values.");
            }

            // 检查 ldc_v2_attr_src_calibration_ratio 存在
            if (ldc.contains("ldc_v2_attr_src_calibration_ratio") && ldc["ldc_v2_attr_src_calibration_ratio"].is_array() && ldc["ldc_v2_attr_src_calibration_ratio"].size() == 9) {
                for (size_t i = 0; i < ldc["ldc_v2_attr_src_calibration_ratio"].size(); ++i){
                    g_config.ldc.ldc_v2_attr_src_calibration_ratio[i] = ldc["ldc_v2_attr_src_calibration_ratio"][i];
                }
            }else {
                LOG_WARN("ldc_v2_attr_src_calibration_ratio array not found or not of size 9, using default values.");
            }

            // 检查 ldc_v2_attr_dst_calibration_ratio 存在
            if (ldc.contains("ldc_v2_attr_dst_calibration_ratio") && ldc["ldc_v2_attr_dst_calibration_ratio"].is_array() && ldc["ldc_v2_attr_dst_calibration_ratio"].size() == 14) {
                for (size_t i = 0; i < ldc["ldc_v2_attr_dst_calibration_ratio"].size(); ++i){
                    g_config.ldc.ldc_v2_attr_dst_calibration_ratio[i] = ldc["ldc_v2_attr_dst_calibration_ratio"][i];
                }
            }else {
                LOG_WARN("ldc_v2_attr_dst_calibration_ratio array not found or not of size 14, using default values.");
            }
        }

        // 读取车型信息
        init_carriage_info("./");

        LOG_INFO("config init success");
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("get config error: %s", e.what());
        return -1; 
    }

    
}

const GlobalConfig* get_global_config() {
    return &g_config;
}