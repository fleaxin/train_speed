/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

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


#define CMD_LEN 128
#define sample_get_input_cmd(input_cmd) fgets((char *)(input_cmd), (sizeof(input_cmd) - 1), stdin)
#define BUF_SIZE 256
#define THREAD_TIMEOUT_MS   500

static td_bool g_sample_exit = TD_FALSE;


#ifndef __LITEOS__
static void hdmi_test_handle_sig(td_s32 signo)
{
    if (g_sample_exit == TD_TRUE) {
        return;
    }

    if (signo == SIGINT || signo == SIGTERM) {
        g_sample_exit = TD_TRUE;
    }
}
#endif

long get_time_ms()
{
    struct timeval start;
    gettimeofday(&start, NULL);
    long ms = (start.tv_sec) * 1000 + (start.tv_usec) / 1000;
    return ms;
}

long get_time_us(){
    struct timeval start;
    gettimeofday(&start, NULL);
    long us = (start.tv_sec) * 1000 * 1000 + (start.tv_usec);
    return us;
  }


int nonblocking_get_input_cmd(char *input_cmd, int maxlen) {
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    // 设置超时时间，例如 100ms
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 100ms

    int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

    if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        // 有输入
        if (fgets(input_cmd, maxlen, stdin)) {
            return 1; // 成功读取输入
        }
    }

    return 0; // 没有输入或出错
}

td_s32 start_venc_to_push_and_save(){
    td_s32 ret;
    const GlobalConfig* config = get_global_config();
    if (config->local_frame_test == 0)// 不是本地测试，则启动venc编码推流和保存
    {
        // 启动venc编码推流和保存
        ret = start_venc();
        if (ret != TD_SUCCESS)
        {
            LOG_ERROR("start venc fail for %#x!", ret);
            return TD_FAILURE;
        }

        /* 画框后的vpss(1,1) 绑定到venc(0) 编码推流*/
        if(config->push_video){
            ret = sample_comm_vpss_bind_venc(1, 1, 0);
            if(ret != TD_SUCCESS){
                LOG_ERROR("vpss(1,1) bind venc(0) fail for %#x!", ret);
                return TD_FAILURE;
            }
        }
        

        /* 原始画面vpss(2,1) 2448*1200绑定到venc(1) 编码保存*/
        if(config->save_file){
            ret = sample_comm_vpss_bind_venc(2, 1, 1); 
            if(ret != TD_SUCCESS){
                LOG_ERROR("vpss(0,2) bind venc(1) fail for %#x!", ret);
                return TD_FAILURE;
            }
        }
        
    }
    return TD_SUCCESS;
}
td_s32 stop_venc_to_push_and_save(){
    const GlobalConfig* config = get_global_config();

    if(config->push_video && config->local_frame_test == 0){
        stop_venc_push_and_save(0);
    }
    
    if(config->save_file && config->local_frame_test == 0){
        stop_venc_push_and_save(1);
    }
    return TD_SUCCESS;
}

td_s32 check_thread_status(PthreadList* pthread_list) { 

    long now_time = get_time_ms();

    td_s32 thread_num = pthread_list->thread_number;
    for(td_s32 i = 0; i < thread_num; i++){
        PthreadInf *thread_info = pthread_list->thread_inf[i];
        char thread_name[64];
        strncpy(thread_name, pthread_list->thread_name[i], sizeof(thread_name)-1);
        thread_name[sizeof(thread_name)-1] = '\0';  // 确保字符串终止

        now_time = get_time_ms();
        // 加锁访问线程
        pthread_mutex_lock(&thread_info->lock);
        if (thread_info->start_singal == 1 && thread_info->last_active_time < now_time - THREAD_TIMEOUT_MS) {
            LOG_ERROR("thread: %s timeout, last active time: %ld s",thread_name ,(now_time - thread_info->last_active_time)/1000);
        }
        pthread_mutex_unlock(&thread_info->lock);
    }

    if(now_time/1000%60==0){
        LOG_INFO("program run normally");
    }
    

    return TD_SUCCESS;
}


td_s32 main(td_s32 argc, td_char *argv[])
{
    
    td_s32 ret;
    td_char input_cmd[CMD_LEN] = { 0 };
    ot_vo_intf_sync format = OT_VO_OUT_1080P60;
    pthread_t get_frame_thread_id;
    pthread_t process_frame_thread_id;
    pthread_t data_output_thread_id;
    pthread_t command_handler_thread_id;
    td_u32 chn_num;
    LOG_INFO("-------------------app start!--------------------------");
    ot_unused(argc);
    ot_unused(argv);

    sample_sys_signal(hdmi_test_handle_sig);

    // 读取配置文件，创建全局变量
    if (init_config("./") != 0) {
        LOG_ERROR("Failed to load config!");
        return TD_FAILURE;
    }
    const GlobalConfig* config = get_global_config();

    // 启动vdec、vpss、vo
    ret = sample_hdmi_vdec_vpss_vo_start(format);
    sample_if_failure_return(ret, TD_FAILURE);
    if(ret!= TD_SUCCESS){
        LOG_ERROR("Failed to start vdec, vpss, vo!");
        return 0;
    }
    sample_hdmi_start(format);
    sample_if_failure_return(ret, TD_FAILURE);

    
    // 启动摄像头 获取图像
    PthreadInf camera_capture_info;
    pthread_mutex_init(&camera_capture_info.lock, NULL);
    if(config->local_frame_test == 0){
        camera_capture_info.start_singal = TD_TRUE;
        start_camera_capture(&camera_capture_info);
    }
    
    // 等待摄像头完全启动
    sleep(2);

    
    // 启动数据输出线程
    SpeedData speed_data;
    sem_init(&speed_data.sem, 0, 0);
    PthreadInf data_output_info;
    data_output_info.start_singal = TD_TRUE;
    data_output_info.data_struct = (void*)&speed_data;
    pthread_mutex_init(&data_output_info.lock, NULL);
    pthread_create(&data_output_thread_id, TD_NULL, data_output_uds, (td_void*)&data_output_info);

    // 启动外部数据和命令接收处理线程
    PthreadInf command_handler_info;
    command_handler_info.start_singal = TD_TRUE;
    ControlCommand control_command;
    control_command.reset_data = TD_FALSE;
    control_command.set_model.set_model = TD_FALSE;
    command_handler_info.command_struct = (void*)&control_command;
    pthread_mutex_init(&command_handler_info.lock, TD_NULL);
    pthread_create(&command_handler_thread_id, TD_NULL, control_command_handler, (td_void*)&command_handler_info);

    // 启动视频推理计算线程
    PthreadInf frame_process_info;
    frame_process_info.start_singal = TD_TRUE;
    frame_process_info.data_struct = (void*)&speed_data; 
    frame_process_info.command_struct = (void*)&control_command;
    pthread_mutex_init(&frame_process_info.lock, NULL);
    pthread_create(&process_frame_thread_id, TD_NULL, frame_process_cut, (td_void*)&frame_process_info);

    // 启动编码推流和保存
    start_venc_to_push_and_save();

    PthreadList pthread_list;
    pthread_list.thread_number = 4;
    PthreadInf *thread_inf[4] = {&camera_capture_info,&data_output_info,&frame_process_info,&command_handler_info};
    char thread_name[32][64] = {"camera_capture_thread","data_output_thread","frame_process_thread","command_handler_thread"};
    for(td_s32 i = 0; i < pthread_list.thread_number; i++){
        pthread_list.thread_inf[i] = thread_inf[i];
        strncpy(pthread_list.thread_name[i], thread_name[i], sizeof(pthread_list.thread_name[i])-1);
        pthread_list.thread_name[i][sizeof(pthread_list.thread_name[i])-1] = '\0';
    }

    printf("please input 'h' to get help or 'q' to quit!\n");
    printf("hdmi_cmd >");
    while (g_sample_exit == TD_FALSE) {
        
        sleep(2);
        // 检查线程状态
        check_thread_status(&pthread_list);
        
        int has_input = nonblocking_get_input_cmd(input_cmd, CMD_LEN);
        if (has_input) {
            // 处理输入命令
            if (input_cmd[0] == 'q') {
                printf("prepare to quit!\n");
                break;
            }
            hdmi_test_cmd(input_cmd, CMD_LEN);
        }
    }
    
    // 终止线程
    data_output_info.start_singal = TD_FALSE;
    frame_process_info.start_singal = TD_FALSE;
    camera_capture_info.start_singal = TD_FALSE;

    // 终止帧处理线程
    pthread_join(data_output_thread_id,TD_NULL);
    pthread_join(process_frame_thread_id,TD_NULL);

    if(config->local_frame_test==0){
        stop_camera_capture();
    }else if(config->local_frame_test==2){
        stop_vdec();
    }

    // 停止编码推流和保存
    stop_venc_to_push_and_save();

    // 停止hdmi输出 停止vdec、vpss、vo
    sample_hdmi_media_set(TD_FALSE);
    sample_hdmi_vdec_vpss_vo_venc_stop();
    LOG_INFO("-------------------app end!--------------------------\n\n\n");
    return 0;
}

