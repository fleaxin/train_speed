// config.h (C/C++兼容头文件)
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int rate;
    int height;
    int width;
    int offset_x;
    int offset_y;
    int exposure_time;
    int AAROI_height;
    int AAROI_width;
    int AAROI_offsetx;
    int AAROI_offsety;
} CameraConfig;

typedef struct {
    char  push_url[256];
    int   push_port;
    int   push_frame_rate;
    int   push_frame_height;
    int   push_frame_width;
}PushConfig;

typedef struct {
    int   ldc_v2_attr_class[5];
    int   ldc_v2_attr_src_calibration_ratio[9];
    int   ldc_v2_attr_dst_calibration_ratio[14];
}LdcConfig;

typedef struct {
    char model_name[256];
    float length_px;
    float length_meter;
    float px_to_meter_ratio;
    float first_edge_to_head;
    float second_edge_to_tail;
    float scale_ratio[8];
}CarriageInfo;

typedef struct { 
    int   carriage_num;
    CarriageInfo carriage[256];
}CarriageInfoList;

typedef struct {
    int   local_frame_test;
    char  local_frame_path[256];
    char  local_video_path[256];
    int   start_frame;
    int   push_video;
    int   save_file;
    CameraConfig camera;
    PushConfig push_config;
    long long  pmf_coef[9];
    LdcConfig ldc;
    CarriageInfoList carriage_info_list;
} GlobalConfig;



// 初始化配置（返回0成功，-1失败）
int init_config(const char* json_path);

// 获取全局配置（只读）
const GlobalConfig* get_global_config();

#ifdef __cplusplus
}
#endif