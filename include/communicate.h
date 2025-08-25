#ifndef COMMUNICATION
#define COMMUNICATION



#ifdef __cplusplus
extern "C" {
#endif

#include "ot_type.h"
#include "camera_test.h"

typedef struct {
    td_bool set_model;// 设置车型
    td_s32 carriage_number;;
    td_char carriage_mode[256][32];
}SetModle;

typedef struct 
{
    td_bool reset_data;// 重置车辆记录的数据
    SetModle set_model;
    
} ControlCommand;


// 速度数据结构体
typedef struct {
    td_s32 carriage_number;
    td_float carriage_distance_list[256][2];
    td_float to_current_head_distance;
    td_float to_current_tail_distance;
    td_float to_last_tail_distance;
    td_float to_next_head_distance;
    td_bool is_in_carriage;
    td_float speed;
    td_slong time;  // 单位：毫秒(ms)
    td_char carriage_type[64]; // 车型
    td_char remark[256];
    td_u32 time_ref;
    sem_t sem;  // 信号量（初始为0，表示无数据可读）
} SpeedData;



td_void *data_output_uds(td_void *arg);

td_void *control_command_handler(td_void* arg);

#ifdef __cplusplus
}
#endif

#endif