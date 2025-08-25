/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#include "sample_svp_npu_process.h"
#include <pthread.h>
#include <sys/prctl.h>
#include "svp_acl_rt.h"
#include "svp_acl.h"
#include "svp_acl_ext.h"
#include "sample_svp_npu_define.h"
#include "sample_common_svp.h"
#include "sample_svp_npu_model.h"
#include "sample_common_ive.h"

#include "yolov5.h"
#include "svp.h"
#include "read_config.h"

#define SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM 1
#define SAMPLE_SVP_NPU_LSTM_INPUT_FILE_NUM     4
#define SAMPLE_SVP_NPU_SHAERD_WORK_BUF_NUM     1
#define SAMPLE_SVP_NPU_RFCN_THRESHOLD_NUM      2
#define SAMPLE_SVP_NPU_AICPU_WAIT_TIME         1000
#define SAMPLE_SVP_NPU_RECT_COLOR              0x0000FF00
#define SAMPLE_SVP_NPU_MILLIC_SEC              20000
#define SAMPLE_SVP_NPU_IMG_THREE_CHN           3
#define SAMPLE_SVP_NPU_DOUBLE                  2

static hi_bool g_svp_npu_terminate_signal = HI_FALSE;
static hi_bool g_svp_npu_aicpu_process_signal = HI_FALSE;
static pthread_t g_svp_npu_aicpu_thread = 0;
static hi_s32 g_svp_npu_dev_id = 0;
static sample_svp_npu_task_info g_svp_npu_task[SAMPLE_SVP_NPU_MAX_TASK_NUM] = {0};
static sample_svp_npu_shared_work_buf g_svp_npu_shared_work_buf[SAMPLE_SVP_NPU_SHAERD_WORK_BUF_NUM] = {0};


#ifdef SS928_SAMPLE
static sample_svp_npu_threshold g_svp_npu_rfcn_threshold[SAMPLE_SVP_NPU_RFCN_THRESHOLD_NUM] = {
    {0.7, 0.0, 16.0, 16.0, "rpn_data"}, {0.3, 0.9, 16.0, 16.0, "rpn_data1"} };

static hi_sample_svp_rect_info g_svp_npu_rect_info = {0};
static hi_bool g_svp_npu_thread_stop = HI_FALSE;
static pthread_t g_svp_npu_thread = 0;
static sample_vo_cfg g_svp_npu_vo_cfg = { 0 };
static pthread_t g_svp_npu_vdec_thread = 0;
static hi_vb_pool_info g_svp_npu_vb_pool_info;
static hi_void *g_svp_npu_vb_virt_addr = HI_NULL;

static hi_sample_svp_media_cfg g_svp_npu_media_cfg = {
    .svp_switch = {HI_FALSE, HI_TRUE},
    .pic_type = {PIC_1080P, PIC_CIF},
    .chn_num = HI_SVP_MAX_VPSS_CHN_NUM,
};

static sample_vdec_attr g_svp_npu_vdec_cfg = {
    .type = HI_PT_H264,
    .mode = HI_VDEC_SEND_MODE_FRAME,
    .width = _4K_WIDTH,
    .height = _4K_HEIGHT,
    .sample_vdec_video.dec_mode = HI_VIDEO_DEC_MODE_IP,
    .sample_vdec_video.bit_width = HI_DATA_BIT_WIDTH_8,
    .sample_vdec_video.ref_frame_num = 2, /* 2:ref_frame_num */
    .display_frame_num = 2,               /* 2:display_frame_num */
    .frame_buf_cnt = 5,                   /* 5:2+2+1 */
};

static vdec_thread_param g_svp_npu_vdec_param = {
    .chn_id = 0,
    .type = HI_PT_H264,
    .stream_mode = HI_VDEC_SEND_MODE_FRAME,
    .interval_time = 1000, /* 1000:interval_time */
    .pts_init = 0,
    .pts_increase = 0,
    .e_thread_ctrl = THREAD_CTRL_START,
    .circle_send = HI_TRUE,
    .milli_sec = 0,
    .min_buf_size = (_4K_WIDTH * _4K_HEIGHT * 3) >> 1, /* 3:chn_size */
    .c_file_path = "./data/image/",
    .c_file_name = "dolls_video.h264",
};
#endif

static hi_void sample_svp_npu_acl_terminate(hi_void)
{
    if (g_svp_npu_terminate_signal == HI_TRUE) {
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
}

/* function : svp npu signal handle */
hi_void sample_svp_npu_acl_handle_sig(hi_void)
{
    g_svp_npu_terminate_signal = HI_TRUE;
}

static hi_void sample_svp_npu_acl_deinit(hi_void)
{
    svp_acl_error ret;

    ret = svp_acl_rt_reset_device(g_svp_npu_dev_id);
    if (ret != SVP_ACL_SUCCESS) {
        sample_svp_trace_err("reset device fail\n");
    }
    sample_svp_trace_info("end to reset device is %d\n", g_svp_npu_dev_id);

    ret = svp_acl_finalize();
    if (ret != SVP_ACL_SUCCESS) {
        sample_svp_trace_err("finalize acl fail\n");
    }
    sample_svp_trace_info("end to finalize acl\n");
}

// 删除初始化system资源的部分
static hi_s32 sample_svp_npu_acl_init()
{
    /* svp acl init */
    const hi_char *acl_config_path = "";
    svp_acl_rt_run_mode run_mode;
    svp_acl_error ret;

    // svp初始化
    ret = svp_acl_init(acl_config_path);
    sample_svp_check_exps_return(ret != SVP_ACL_SUCCESS, HI_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR, "acl init failed!\n");

    sample_svp_trace_info("svp acl init success!\n");

    /* open device */
    ret = svp_acl_rt_set_device(g_svp_npu_dev_id);
    if (ret != SVP_ACL_SUCCESS) {
        (hi_void)svp_acl_finalize();
        sample_svp_trace_err("svp acl open device %d failed!\n", g_svp_npu_dev_id);
        LOG_ERROR("svp acl open device %d failed!", g_svp_npu_dev_id);
        return HI_FAILURE;
    }
    sample_svp_trace_info("open device %d success!\n", g_svp_npu_dev_id);

    /* get run mode */
    ret = svp_acl_rt_get_run_mode(&run_mode);
    if ((ret != SVP_ACL_SUCCESS) || (run_mode != SVP_ACL_DEVICE)) {
        (hi_void)svp_acl_rt_reset_device(g_svp_npu_dev_id);
        (hi_void)svp_acl_finalize();
        sample_svp_trace_err("acl get run mode failed!\n");
        return HI_FAILURE;
    }
    sample_svp_trace_info("get run mode success!\n");

    return HI_SUCCESS;
}
static hi_s32 sample_svp_npu_acl_dataset_init(hi_u32 task_idx, hi_void *input_buffer)
{

    hi_s32 ret = sample_svp_npu_create_input1(&g_svp_npu_task[task_idx], input_buffer);
    sample_svp_check_exps_return(ret != HI_SUCCESS, HI_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR, "create input failed!\n");

    ret = sample_svp_npu_create_output(&g_svp_npu_task[task_idx]);
    if (ret != HI_SUCCESS) {
        sample_svp_npu_destroy_input(&g_svp_npu_task[task_idx]);
        sample_svp_trace_err("execute create output fail.\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}


static hi_void sample_svp_npu_acl_dataset_deinit(hi_u32 task_idx)
{

    (hi_void)sample_svp_npu_destroy_input(&g_svp_npu_task[task_idx]);
    (hi_void)sample_svp_npu_destroy_output(&g_svp_npu_task[task_idx]);

}

static hi_void *sample_svp_npu_acl_thread_execute(hi_void *args)
{
    hi_s32 ret;
    hi_u32 task_idx = *(hi_u32 *)args;

    ret = svp_acl_rt_set_device(g_svp_npu_dev_id);
    sample_svp_check_exps_return(ret != HI_SUCCESS, HI_NULL, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "open device %d failed!\n", g_svp_npu_dev_id);

    ret = sample_svp_npu_model_execute(&g_svp_npu_task[task_idx]);
    if (ret != SVP_ACL_SUCCESS) {
        sample_svp_trace_err("execute inference failed of task[%u]!\n", task_idx);
        LOG_ERROR("execute inference failed of task[%u]!", task_idx);
    }

    ret = svp_acl_rt_reset_device(g_svp_npu_dev_id);
    if (ret != SVP_ACL_SUCCESS) {
        sample_svp_trace_err("task[%u] reset device failed!\n", task_idx);
        LOG_ERROR("task[%u] reset device failed!", task_idx);
    }
    return HI_NULL;
}

static hi_void sample_svp_npu_acl_model_execute_multithread()
{
    pthread_t execute_threads[SAMPLE_SVP_NPU_MAX_THREAD_NUM] = {0};
    hi_u32 idx[SAMPLE_SVP_NPU_MAX_THREAD_NUM] = {0};
    hi_u32 task_idx;

    for (task_idx = 0; task_idx < SAMPLE_SVP_NPU_MAX_THREAD_NUM; task_idx++) {
        idx[task_idx] = task_idx;
        pthread_create(&execute_threads[task_idx], NULL, sample_svp_npu_acl_thread_execute, &idx[task_idx]);
    }

    hi_void *waitret[SAMPLE_SVP_NPU_MAX_THREAD_NUM];
    for (task_idx = 0; task_idx < SAMPLE_SVP_NPU_MAX_THREAD_NUM; task_idx++) {
        pthread_join(execute_threads[task_idx], &waitret[task_idx]);
    }

    for (task_idx = 0; task_idx < SAMPLE_SVP_NPU_MAX_THREAD_NUM; task_idx++) {
        sample_svp_trace_info("output %u-th task data\n", task_idx);
        sample_svp_npu_output_classification_result(&g_svp_npu_task[task_idx]);
    }
}

static hi_void sample_svp_npu_acl_deinit_task(hi_u32 task_num, hi_u32 shared_work_buf_idx)
{
    hi_u32 task_idx;

    if (g_svp_npu_aicpu_process_signal == HI_TRUE) {
        g_svp_npu_aicpu_process_signal = HI_FALSE;
        pthread_join(g_svp_npu_aicpu_thread, HI_NULL);
    }
    
    for (task_idx = 0; task_idx < task_num; task_idx++) {
        (hi_void)sample_svp_npu_destroy_work_buf(&g_svp_npu_task[task_idx]);
        (hi_void)sample_svp_npu_destroy_task_buf(&g_svp_npu_task[task_idx]);
        (hi_void)sample_svp_npu_acl_dataset_deinit(task_idx);
        (hi_void)memset_s(&g_svp_npu_task[task_idx], sizeof(sample_svp_npu_task_cfg), 0,
            sizeof(sample_svp_npu_task_cfg));

    }
    if (g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_ptr != HI_NULL) {
        (hi_void)svp_acl_rt_free(g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_ptr);
        g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_ptr = HI_NULL;
        g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_size = 0;
        g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_stride = 0;
    }
}

static hi_s32 sample_svp_npu_acl_create_shared_work_buf(hi_u32 task_num, hi_u32 shared_work_buf_idx)
{
    hi_u32 task_idx, work_buf_size, work_buf_stride;
    hi_s32 ret;

    for (task_idx = 0; task_idx < task_num; task_idx++) {
        ret = sample_svp_npu_get_work_buf_info(&g_svp_npu_task[task_idx], &work_buf_size, &work_buf_stride);
        sample_svp_check_exps_return(ret != HI_SUCCESS, ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "get %u-th task work buf info failed!\n", task_idx);

        if (g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_size < work_buf_size) {
            g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_size = work_buf_size;
            g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_stride = work_buf_stride;
        }
    }
    ret = svp_acl_rt_malloc_cached(&g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_ptr,
        g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_size, SVP_ACL_MEM_MALLOC_NORMAL_ONLY);
    sample_svp_check_exps_return(ret != HI_SUCCESS, HI_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "malloc %u-th shared work buf failed!\n", shared_work_buf_idx);

    (hi_void)svp_acl_rt_mem_flush(g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_ptr,
        g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_size);
    return HI_SUCCESS;
}

static hi_void *sample_svp_npu_acl_aicpu_thread(hi_void *arg)
{
    svp_acl_error ret;

    while (g_svp_npu_aicpu_process_signal == HI_TRUE) {
        ret = svp_acl_ext_process_aicpu_task(SAMPLE_SVP_NPU_AICPU_WAIT_TIME);
        if (ret != SVP_ACL_SUCCESS && ret != SVP_ACL_ERROR_RT_REPORT_TIMEOUT) {
            sample_svp_trace_err("aicpu porcess failed\n");
            break;
        }
    }
    return HI_NULL;
}


static hi_s32 sample_svp_npu_acl_init_task(hi_u32 task_num, hi_bool is_share_work_buf, hi_u32 shared_work_buf_idx,hi_void *input_buffer)
{
    hi_u32 task_idx;
    hi_s32 ret;
    hi_bool has_aicpu_task = HI_FALSE;
    if (is_share_work_buf == HI_TRUE) {
        ret = sample_svp_npu_acl_create_shared_work_buf(task_num, shared_work_buf_idx);
        sample_svp_check_exps_return(ret != HI_SUCCESS, HI_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "create shared work buf failed!\n");
    }
    for (task_idx = 0; task_idx < task_num; task_idx++) {
        ret = sample_svp_npu_acl_dataset_init(task_idx, input_buffer);
        if (ret != HI_SUCCESS) {
            goto task_init_end_0;
        }
        ret = sample_svp_npu_create_task_buf(&g_svp_npu_task[task_idx]);       
        if (ret != HI_SUCCESS) {
            sample_svp_trace_err("create task buf failed.\n");
            goto task_init_end_0;
        }

        ret = sample_svp_npu_create_work_buf(&g_svp_npu_task[task_idx]);   
        if (ret != HI_SUCCESS) {
            sample_svp_trace_err("create work buf failed.\n");
            goto task_init_end_0;
        }
        /* create aicpu process thread */
        if (g_svp_npu_aicpu_process_signal == HI_FALSE) {
            ret = sample_svp_npu_check_has_aicpu_task(&g_svp_npu_task[task_idx], &has_aicpu_task);
            sample_svp_check_exps_goto(ret != HI_SUCCESS, task_init_end_0, SAMPLE_SVP_ERR_LEVEL_ERROR,
                "create check has aicpu task failed!\n");
            if (has_aicpu_task == HI_TRUE) {
                g_svp_npu_aicpu_process_signal = HI_TRUE;
                ret = pthread_create(&g_svp_npu_aicpu_thread, 0, sample_svp_npu_acl_aicpu_thread, HI_NULL);
                sample_svp_check_exps_goto(ret != HI_SUCCESS, task_init_end_0, SAMPLE_SVP_ERR_LEVEL_ERROR,
                    "create aicpu process thread failed!\n");
            }
        }
    }
    return HI_SUCCESS;

task_init_end_0:
    (hi_void)sample_svp_npu_acl_deinit_task(task_num, shared_work_buf_idx);
    return ret;
}

hi_bool svp_init(hi_void *data_buf){
    hi_s32 ret;
    const hi_u32 model_idx = 0;
    const hi_char *acl_config_path = "";
    g_svp_npu_terminate_signal = HI_FALSE;

    const GlobalConfig* config = get_global_config();
    hi_char model_path[256];
    strncpy(model_path, config->model_path, sizeof(model_path)-1);
    model_path[sizeof(model_path)-1] = '\0';
    const hi_char *om_model_path = model_path;

    if (g_svp_npu_terminate_signal == HI_FALSE) {
        /* init acl */
        ret = sample_svp_npu_acl_init();
        sample_svp_check_exps_return_void(ret != HI_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR, "init failed!\n");

        /* load model */
        ret = sample_svp_npu_load_model(om_model_path, model_idx, HI_FALSE);
        sample_svp_check_exps_goto(ret != HI_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR, "load model failed!\n");
        
    }

    /* set task cfg */
    g_svp_npu_task[0].cfg.max_batch_num = 1;
    g_svp_npu_task[0].cfg.dynamic_batch_num = 1;
    g_svp_npu_task[0].cfg.total_t = 0;
    g_svp_npu_task[0].cfg.is_cached = HI_FALSE;
    g_svp_npu_task[0].cfg.model_idx = model_idx;

    ret = sample_svp_npu_acl_init_task(1, HI_FALSE, 0, data_buf);
    if(ret != HI_SUCCESS){
        err_print("init task failed!\n");
        LOG_ERROR("init task failed!");
        return HI_FAILURE;
    }

    return HI_SUCCESS;


process_end0:
    (hi_void)sample_svp_npu_acl_deinit();
    (hi_void)sample_svp_npu_acl_terminate();
    
}

#include <sys/time.h>
long getime()
{
    struct timeval start;
    gettimeofday(&start, NULL);
    long ms = (start.tv_sec) * 1000 + (start.tv_usec) / 1000;
    return ms;
}
hi_s32 svp_execute(hi_void* data_buf,stYolov5Objs* pOut){
    
    hi_s32 ret;
    hi_s32 model_idx = 0;
    
    long start = get_time_ms();
    ret = sample_svp_npu_model_execute(&g_svp_npu_task[0]);
    sample_svp_check_exps_goto(ret != HI_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR, "execute failed!\n");
    long execute_end = get_time_ms();
    // printf("svp execute one frame cost time: %ld\n", execute_end - start);

    //(hi_void) sample_svp_npu_output_classification_result(&g_svp_npu_task[0]);
    yolo_svp_module_output_result(&g_svp_npu_task[0], pOut);
    long get_mode_result_end = get_time_ms();
    // printf("get mode result cost time: %ld\n", get_mode_result_end - execute_end);

process_end2:
    // (hi_void)sample_svp_npu_acl_deinit_task(1, 0);
    return 0;
process_end1:
    (hi_void)sample_svp_npu_unload_model(model_idx);
process_end0:
    (hi_void)sample_svp_npu_acl_deinit();
    (hi_void)sample_svp_npu_acl_terminate();
}

hi_s32 svp_execute_test(hi_void* data_buf, long time ,hi_s32 numbs){

    hi_s32 ret;
    hi_s32 model_idx = 0;
    
    /* set task cfg */
        g_svp_npu_task[0].cfg.max_batch_num = 1;
        g_svp_npu_task[0].cfg.dynamic_batch_num = 1;
        g_svp_npu_task[0].cfg.total_t = 0;
        g_svp_npu_task[0].cfg.is_cached = HI_FALSE;
        g_svp_npu_task[0].cfg.model_idx = model_idx;

    ret = sample_svp_npu_acl_init_task(1, HI_FALSE, 0, data_buf);
    sample_svp_check_exps_goto(ret != HI_SUCCESS, process_end1, SAMPLE_SVP_ERR_LEVEL_ERROR, "init task failed!\n");
    printf("task init success\n");

    // 等待同步
    while(HI_TRUE){
        long now_time = get_time_us();
        if(now_time > time + 1000*1000*3){
            break;
        }
    }
    printf("\nsvp execute start\n");
    for (int i = 0; i < numbs; i++)
    {
        long start_time = get_time_us();
        ret = sample_svp_npu_model_execute_test(&g_svp_npu_task[0]);
        long end_tme = get_time_us();
        sample_svp_check_exps_goto(ret != HI_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR, "execute failed!\n");
        printf("svp execute end\n");
        printf("svp start time: %ld us\n", start_time);
        printf("svp execute time: %ld us\n\n", end_tme - start_time);
        (hi_void) sample_svp_npu_output_classification_result(&g_svp_npu_task[0]);
    }

process_end2:
    (hi_void)sample_svp_npu_acl_deinit_task(1, 0);
    return;
process_end1:
    (hi_void)sample_svp_npu_unload_model(model_idx);
process_end0:
    (hi_void)sample_svp_npu_acl_deinit();
    (hi_void)sample_svp_npu_acl_terminate();
}
hi_void svp_uninit()
{
    hi_s32 model_idx = 0;

    sample_svp_npu_acl_deinit_task(1, 0);
    
    sample_svp_npu_unload_model(model_idx);

    sample_svp_npu_acl_deinit();
    sample_svp_npu_acl_terminate();
}

