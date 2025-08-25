/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#ifndef SAMPLE_SVP_NPU_DEFINE_H
#define SAMPLE_SVP_NPU_DEFINE_H
#include "hi_type.h"
#include "svp_acl_mdl.h"

#define SAMPLE_SVP_NPU_MAX_THREAD_NUM    16
#define SAMPLE_SVP_NPU_MAX_TASK_NUM      16
#define SAMPLE_SVP_NPU_MAX_MODEL_NUM     1
#define SAMPLE_SVP_NPU_EXTRA_INPUT_NUM   2
#define SAMPLE_SVP_NPU_BYTE_BIT_NUM      8
#define SAMPLE_SVP_NPU_SHOW_TOP_NUM      5
#define SAMPLE_SVP_NPU_MAX_NAME_LEN      32
#define SAMPLE_SVP_NPU_MAX_MEM_SIZE      0xFFFFFFFF
#define SAMPLE_SVP_NPU_RECT_LEFT_TOP     0
#define SAMPLE_SVP_NPU_RECT_RIGHT_TOP    1
#define SAMPLE_SVP_NPU_RECT_RIGHT_BOTTOM 2
#define SAMPLE_SVP_NPU_RECT_LEFT_BOTTOM  3
#define SAMPLE_SVP_NPU_THRESHOLD_NUM     4

typedef struct {
    hi_u32 model_id;
    hi_bool is_load_flag;
    hi_ulong model_mem_size;
    hi_void *model_mem_ptr;
    svp_acl_mdl_desc *model_desc;
    size_t input_num;
    size_t output_num;
    size_t dynamic_batch_idx;
} sample_svp_npu_model_info;

typedef struct {
    hi_u32 max_batch_num;
    hi_u32 dynamic_batch_num;
    hi_u32 total_t;
    hi_bool is_cached;
    hi_u32 model_idx;
} sample_svp_npu_task_cfg;

typedef struct {
    sample_svp_npu_task_cfg cfg;
    svp_acl_mdl_dataset *input_dataset;
    svp_acl_mdl_dataset *output_dataset;
    hi_void *task_buf_ptr;
    size_t task_buf_size;
    size_t task_buf_stride;
    hi_void *work_buf_ptr;
    size_t work_buf_size;
    size_t work_buf_stride;
} sample_svp_npu_task_info;

typedef struct {
    hi_void *work_buf_ptr;
    size_t work_buf_size;
    size_t work_buf_stride;
} sample_svp_npu_shared_work_buf;

typedef struct {
    hi_float score;
    hi_u32 class_id;
} sample_svp_npu_top_n_result;

typedef struct {
    hi_char *num_name;
    hi_char *roi_name;
    hi_bool has_background;
    hi_u32 roi_offset;
} sample_svp_npu_detection_info;

typedef struct {
    hi_float nms_threshold;
    hi_float score_threshold;
    hi_float min_height;
    hi_float min_width;
    hi_char *name;
} sample_svp_npu_threshold;
#endif
