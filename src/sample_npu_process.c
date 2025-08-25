/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#include "sample_npu_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <limits.h>


#include "hi_common_svp.h"
#include "sample_common_svp.h"
#include "sample_npu_model.h"
#include "yolov5.h"

static hi_u32 g_npu_dev_id = 0;
static hi_char *g_model_path = "./data/model/resnet50.om";
static hi_char *g_acl_config_path = "";
static hi_s32 sample_nnn_npu_fill_input_data(hi_void *dev_buf, size_t buf_size)
{
    hi_s32 ret;
    hi_char path[PATH_MAX] = { 0 };
    size_t file_size;

    if (realpath(g_model_path, path) == HI_NULL) {
        sample_svp_trace_err("Invalid file!.\n");  
        return HI_FAILURE;
    }

    FILE *fp = fopen(path, "rb");
    sample_svp_check_exps_return(fp == HI_NULL, HI_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "open image file failed!\n");

    ret = fseek(fp, 0L, SEEK_END);
    sample_svp_check_exps_goto(ret == -1, end, SAMPLE_SVP_ERR_LEVEL_ERROR, "Fseek failed!\n");
    file_size = ftell(fp);
    sample_svp_check_exps_goto(file_size == 0, end, SAMPLE_SVP_ERR_LEVEL_ERROR, "Ftell failed!\n");
    ret = fseek(fp, 0L, SEEK_SET);
    sample_svp_check_exps_goto(ret == -1, end, SAMPLE_SVP_ERR_LEVEL_ERROR, "Fseek failed!\n");

    file_size = (file_size > buf_size) ? buf_size : file_size;
    ret = fread(dev_buf, file_size, 1, fp);
    sample_svp_check_exps_goto(ret != 1, end, SAMPLE_SVP_ERR_LEVEL_ERROR, "Read file failed!\n");

    if (fp != HI_NULL) {
        fclose(fp);
    }
    return HI_SUCCESS;

end:
    if (fp != HI_NULL) {
        fclose(fp);
    }
    return HI_FAILURE;
}

static hi_void sample_nnn_npu_destroy_resource(hi_void)
{
    aclError ret;

    ret = aclrtResetDevice(g_npu_dev_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("reset device fail\n");
    }
    sample_svp_trace_info("end to reset device is %d\n", g_npu_dev_id);

    ret = aclFinalize();
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("finalize acl fail\n");
    }
    sample_svp_trace_info("end to finalize acl\n");
}

static hi_s32 sample_nnn_npu_init_resource(hi_void)
{
    /* ACL init */
    const char *acl_config_path = g_acl_config_path;
    aclrtRunMode run_mode;
    hi_s32 ret;

    ret = aclInit(acl_config_path);
    printf("%s\n",acl_config_path);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("acl init fail.\n");
        return HI_FAILURE;
    }
    sample_svp_trace_info("acl init success.\n");

    /* open device */
    ret = aclrtSetDevice(g_npu_dev_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("acl open device %d fail.\n", g_npu_dev_id);
        return HI_FAILURE;
    }
    sample_svp_trace_info("open device %d success.\n", g_npu_dev_id);

    /* get run mode */
    ret = aclrtGetRunMode(&run_mode);
    if ((ret != ACL_ERROR_NONE) || (run_mode != ACL_DEVICE)) {
        sample_svp_trace_err("acl get run mode fail.\n");
        return HI_FAILURE;
    }
    sample_svp_trace_info("get run mode success\n");

    return HI_SUCCESS;
}

static hi_void sample_nnn_npu_acl_resnet50_stop(hi_void)
{
    sample_nnn_npu_destroy_resource();
    printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
}

static hi_s32 sample_nnn_npu_acl_prepare_init()
{
    hi_s32 ret;

    ret = sample_nnn_npu_init_resource();
    if (ret != HI_SUCCESS) {
        sample_nnn_npu_destroy_resource();
    }

    return ret;
}

static hi_void sample_nnn_npu_acl_prepare_exit(hi_u32 thread_num)
{
    for (hi_u32 model_index = 0; model_index < thread_num; model_index++) {
        sample_npu_destroy_desc(model_index);
        sample_npu_unload_model(model_index);
    }
    sample_nnn_npu_destroy_resource();
}

static hi_s32 sample_nnn_npu_load_model(const char* om_model_path, hi_u32 model_index, hi_bool is_cached)
{
    hi_char path[PATH_MAX] = { 0 };
    hi_s32 ret;

    if (sizeof(om_model_path) > PATH_MAX) {
        sample_svp_trace_err("pathname too long!.\n");
        return HI_NULL;
    }
    if (realpath(om_model_path, path) == HI_NULL) {
        sample_svp_trace_err("invalid file!.\n");
        return HI_NULL;
    }

    if (is_cached == HI_TRUE) {
        ret = sample_npu_load_model_with_mem_cached(path, model_index);
    } else {
        ret = sample_npu_load_model_with_mem(path, model_index);
    }

    if (ret != HI_SUCCESS) {
        sample_svp_trace_err("execute load model fail, model_index is:%d.\n", model_index);
        goto acl_prepare_end1;
    }
    ret = sample_npu_create_desc(model_index);
    if (ret != HI_SUCCESS) {
        sample_svp_trace_err("execute create desc fail.\n");
        goto acl_prepare_end2;
    }

    return HI_SUCCESS;

acl_prepare_end2:
    sample_npu_destroy_desc(model_index);

acl_prepare_end1:
    sample_npu_unload_model(model_index);
    return ret;
}
static hi_s32 sample_nnn_npu_dataset_prepare_init(hi_u32 model_index)
{
    hi_s32 ret;

    ret = sample_npu_create_input_dataset(model_index);
    if (ret != HI_SUCCESS) {
        sample_svp_trace_err("execute create input fail.\n");
        return HI_FAILURE;
    }
    ret = sample_npu_create_output(model_index);
    if (ret != HI_SUCCESS) {
        sample_npu_destroy_input_dataset(model_index);
        sample_svp_trace_err("execute create output fail.\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

static hi_s32 sample_nnn_npu_create_cached_input_output(hi_u32 model_index)
{
    hi_s32 ret;

    ret = sample_npu_create_cached_input(model_index);
    if (ret != HI_SUCCESS) {
        sample_svp_trace_err("execute create input fail.\n");
        return HI_FAILURE;
    }
    ret = sample_npu_create_cached_output(model_index);
    if (ret != HI_SUCCESS) {
        sample_npu_destroy_cached_input(model_index);
        sample_svp_trace_err("execute create output fail.\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

static hi_void sample_nnn_npu_dataset_prepare_exit(hi_u32 thread_num)
{
    for (hi_u32 model_index = 0; model_index < thread_num; model_index++) {
        sample_npu_destroy_output(model_index);
        sample_npu_destroy_input_dataset(model_index);
    }
}

static hi_void sample_nnn_npu_release_input_data(hi_void **data_buf, size_t *data_len, hi_u32 thread_num)
{
    for (hi_u32 model_index = 0; model_index < thread_num; model_index++) {
        hi_unused(data_len[model_index]);
        (hi_void)aclrtFree(data_buf[model_index]);
    }
}

static hi_s32 sample_nnn_npu_get_input_data(hi_void **data_buf, size_t *data_len, hi_u32 model_index)
{
    size_t buf_size;
    hi_s32 ret;

    ret = sample_npu_get_input_size_by_index(0, &buf_size, model_index);
    if (ret != HI_SUCCESS) {
        sample_svp_trace_err("execute get input size fail.\n");
        return HI_FAILURE;
    }

    ret = aclrtMalloc(data_buf, buf_size, ACL_MEM_MALLOC_NORMAL_ONLY);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("malloc device buffer fail. size is %zu, errorCode is %d.\n", buf_size, ret);
        return HI_FAILURE;
    }

    ret = sample_nnn_npu_fill_input_data(*data_buf, buf_size);
    if (ret != HI_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        (hi_void)aclrtFree(data_buf);
        return HI_FAILURE;
    }

    *data_len = buf_size;

    sample_svp_trace_info("get input data success\n");

    return HI_SUCCESS;
}

static hi_s32 sample_nnn_npu_create_input_databuf(hi_void *data_buf, size_t data_len, hi_u32 model_index)
{
    return sample_npu_create_input_databuf(data_buf, data_len, model_index);
}

static hi_void sample_nnn_npu_destroy_input_databuf(hi_u32 thread_num)
{
    for (hi_u32 model_index = 0; model_index < thread_num; model_index++) {
        sample_npu_destroy_input_databuf(model_index);
    }
}

void *sample_svp_execute_func_contious(void *args)
{
    hi_s32 ret;
    hi_u32 model_index = *(hi_u32 *)args;

    ret = aclrtSetDevice(g_npu_dev_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("acl open device %d fail.\n", g_npu_dev_id);
        return NULL;
    }

    sample_svp_trace_info("open device %d success.\n", g_npu_dev_id);

    ret = sample_npu_model_execute(model_index);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("execute inference failed of thread[%d].\n", model_index);
    }

    ret = aclrtResetDevice(g_npu_dev_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("model[%d]reset device failed\n", model_index);
    }
    return NULL;
}

static hi_void sample_nnn_npu_model_execute_multithread()
{
    pthread_t execute_threads[MAX_THREAD_NUM] = {0};
    hi_u32 index[MAX_THREAD_NUM];
    hi_u32 model_index;

    for (model_index = 0; model_index < MAX_THREAD_NUM; model_index++) {
        index[model_index] = model_index;
        pthread_create(&execute_threads[model_index], NULL, sample_svp_execute_func_contious,
            (void *)&index[model_index]);
    }

    void *waitret[MAX_THREAD_NUM];
    for (model_index = 0; model_index < MAX_THREAD_NUM; model_index++) {
        pthread_join(execute_threads[model_index], &waitret[model_index]);
    }

    for (model_index = 0; model_index < MAX_THREAD_NUM; model_index++) {
        //sample_npu_output_model_result(model_index);
    }
}

/* function : show the sample of npu resnet50_multithread */
hi_void sample_nnn_npu_acl_resnet50_multithread(hi_void)
{
    hi_void *data_buf[MAX_THREAD_NUM] = {HI_NULL};
    size_t buf_size[MAX_THREAD_NUM];
    hi_u32 model_index;
    hi_s32 ret;

    const char *om_model_path = "./data/model/resnet50.om";
    ret = sample_nnn_npu_acl_prepare_init();
    if (ret != HI_SUCCESS) {
        return;
    }

    for (model_index = 0; model_index < MAX_THREAD_NUM; model_index++) {
        ret = sample_nnn_npu_load_model(om_model_path, model_index, HI_FALSE);
        if (ret != HI_SUCCESS) {
            goto acl_process_end0;
        }

        ret = sample_nnn_npu_dataset_prepare_init(model_index);
        if (ret != HI_SUCCESS) {
            goto acl_process_end1;
        }

        ret = sample_nnn_npu_get_input_data(&data_buf[model_index], &buf_size[model_index], model_index);
        if (ret != HI_SUCCESS) {
            sample_svp_trace_err("execute create input fail.\n");
            goto acl_process_end2;
        }

        ret = sample_nnn_npu_create_input_databuf(data_buf[model_index], buf_size[model_index], model_index);
        if (ret != HI_SUCCESS) {
            sample_svp_trace_err("memcpy_s device buffer fail.\n");
            goto acl_process_end3;
        }
    }

    sample_nnn_npu_model_execute_multithread();

acl_process_end3:
    sample_nnn_npu_destroy_input_databuf(MAX_THREAD_NUM);
acl_process_end2:
    sample_nnn_npu_release_input_data(data_buf, buf_size, MAX_THREAD_NUM);
acl_process_end1:
    sample_nnn_npu_dataset_prepare_exit(MAX_THREAD_NUM);
acl_process_end0:
    sample_nnn_npu_acl_prepare_exit(MAX_THREAD_NUM);
}

hi_void sample_nnn_npu_acl_mobilenet_v3_dynamicbatch(hi_void)
{
    hi_s32 ret;
    const char *om_model_path = "./data/model/mobilenet_v3_dynamic_batch.om";
    ret = sample_nnn_npu_acl_prepare_init();
    if (ret != HI_SUCCESS) {
        return;
    }

    ret = sample_nnn_npu_load_model(om_model_path, 0, HI_TRUE);
    if (ret != HI_SUCCESS) {
        goto acl_process_end0;
    }

    ret = sample_nnn_npu_create_cached_input_output(0);
    if (ret != HI_SUCCESS) {
        goto acl_process_end0;
    }

    sample_nnn_npu_loop_execute_dynamicbatch(0);

    sample_npu_destroy_cached_input(0);
    sample_npu_destroy_cached_output(0);
acl_process_end0:
    sample_npu_destroy_desc(0);
    sample_npu_unload_model_cached(0);
}

/* function : show the sample of npu resnet50 */
hi_void sample_nnn_npu_acl_resnet50(hi_void)
{
    hi_void *data_buf = HI_NULL;
    size_t buf_size;
    hi_s32 ret;

    const char *om_model_path = "./data/model/resnet50.om";
    ret = sample_nnn_npu_acl_prepare_init(om_model_path);
    if (ret != HI_SUCCESS) {
        return;
    }

    ret = sample_nnn_npu_load_model(om_model_path, 0, HI_FALSE);
    if (ret != HI_SUCCESS) {
        goto acl_process_end0;
    }

    ret = sample_nnn_npu_dataset_prepare_init(0);
    if (ret != HI_SUCCESS) {
        goto acl_process_end0;
    }

    ret = sample_nnn_npu_get_input_data(&data_buf, &buf_size, 0);
    if (ret != HI_SUCCESS) {
        sample_svp_trace_err("execute create input fail.\n");
        goto acl_process_end1;
    }

    ret = sample_nnn_npu_create_input_databuf(data_buf, buf_size, 0);
    if (ret != HI_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        goto acl_process_end2;
    }

    ret = sample_npu_model_execute(0);
    if (ret != HI_SUCCESS) {
        sample_svp_trace_err("execute inference fail.\n");
        goto acl_process_end3;
    }

    sample_npu_output_model_result(0);

acl_process_end3:
    sample_nnn_npu_destroy_input_databuf(1);
acl_process_end2:
    sample_nnn_npu_release_input_data(&data_buf, &buf_size, 1);
acl_process_end1:
    sample_nnn_npu_dataset_prepare_exit(1);
acl_process_end0:
    sample_nnn_npu_acl_prepare_exit(1);
}

/* function : npu resnet50 sample signal handle */
hi_void sample_nnn_npu_acl_resnet50_handle_sig(hi_void)
{
    sample_nnn_npu_acl_resnet50_stop();
}


hi_bool nnn_init(hi_void)
{
    hi_s32 ret;

    const char *om_model_path = "./source_file/mode/train_seg_73_nnn_bgr.om";    
    ret = sample_nnn_npu_acl_prepare_init();
    if (ret != HI_SUCCESS) {
        return HI_FALSE;
    }

    ret = sample_nnn_npu_load_model(om_model_path, 0, TD_FALSE);
    if (ret != HI_SUCCESS) {
        printf("load model failed!\n");
        goto acl_process_end0;
        return HI_FALSE;
    }

    return HI_TRUE;
acl_process_end0:
    sample_nnn_npu_acl_prepare_exit(1);
}


hi_s32 nnn_execute(hi_void* data_buf, size_t data_len,stYolov5Objs* pOut)
{
    hi_s32 ret;  
    ret = sample_nnn_npu_dataset_prepare_init(0);
    ret = sample_nnn_npu_create_input_databuf(data_buf, data_len, 0);

    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        return -1;
    }
    long start_time = get_time_ms();
    ret = sample_npu_model_execute(0);
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("execute inference fail.\n");
        return -1;
    }
    long start_time1 = get_time_ms();
    printf("execute time: %ld ms\n",start_time1 - start_time);

    sample_npu_output_model_result1(0, pOut);// 
    printf("back execute time: %ld ms\n",get_time_ms() - start_time1);
    sample_npu_destroy_input_databuf(0);
    sample_npu_destroy_output(0);
    sample_npu_destroy_input_dataset(0);
    return 0;
}

hi_s32 nnn_execute_test(hi_void* data_buf, long time, hi_s32 numbs)
{
    hi_s32 ret;  
    ret = sample_nnn_npu_dataset_prepare_init(0);
    ret = sample_nnn_npu_create_input_databuf(data_buf, 640*640*3/2, 0);

    stYolov5Objs* pOut = (stYolov5Objs*)malloc(sizeof(stYolov5Objs));
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        return -1;
    }
    // 等待同步
    while(HI_TRUE){
        long now_time = get_time_us();
        if(now_time > time + 1000*1000*3){
            break;
        }
    }
    printf("\nnnn_execute_test start\n");
    for (int i = 0; i < numbs; i++)
    {
        long start_time = get_time_us();
        ret = sample_npu_model_execute(0);
        long end_tme = get_time_us();
        printf("nnn_execute_test end\n");
        printf("nnn start time: %ld us\n", start_time);
        printf("nnn execute time: %ld us\n\n", end_tme - start_time);

    }

    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("execute inference fail.\n");
        return -1;
    }


    sample_npu_destroy_input_databuf(0);
    sample_npu_destroy_output(0);
    sample_npu_destroy_input_dataset(0);
    return 0;
}

