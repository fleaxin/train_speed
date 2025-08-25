/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <math.h>


#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "sample_npu_model.h"
#include "hi_common_svp.h"
#include "sample_common_svp.h"
#include "sample_npu_model.h"
#include "sample_common_ive.h"
#include "hi_common_ive.h"
#include "yolov5.h"

static npu_acl_model_t g_npu_acl_model[MAX_THREAD_NUM] = {0};

hi_s32 sample_npu_load_model_with_mem(const char *model_path, hi_u32 model_index)
{
    if (g_npu_acl_model[model_index].is_load_flag) {
        sample_svp_trace_err("has already loaded a model\n");
        return HI_FAILURE;
    }

    hi_s32 ret = aclmdlQuerySize(model_path, &g_npu_acl_model[model_index].model_mem_size,
        &g_npu_acl_model[model_index].model_weight_size);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("query model failed, model file is %s\n", model_path);
        return HI_FAILURE;
    }

    ret = aclrtMalloc(&g_npu_acl_model[model_index].model_mem_ptr, g_npu_acl_model[model_index].model_mem_size,
        ACL_MEM_MALLOC_HUGE_FIRST);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("malloc buffer for mem failed, require size is %lu\n",
            g_npu_acl_model[model_index].model_mem_size);
        return HI_FAILURE;
    }

    ret = aclrtMalloc(&g_npu_acl_model[model_index].model_weight_ptr, g_npu_acl_model[model_index].model_weight_size,
        ACL_MEM_MALLOC_HUGE_FIRST);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("malloc buffer for weight fail, require size is %lu\n",
            g_npu_acl_model[model_index].model_weight_size);
        return HI_FAILURE;
    }

    ret = aclmdlLoadFromFileWithMem(model_path, &g_npu_acl_model[model_index].model_id,
        g_npu_acl_model[model_index].model_mem_ptr, g_npu_acl_model[model_index].model_mem_size,
        g_npu_acl_model[model_index].model_weight_ptr, g_npu_acl_model[model_index].model_weight_size);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("load model from file failed, model file is %s\n", model_path);
        return HI_FAILURE;
    }

    sample_svp_trace_info("load mem_size:%lu weight_size:%lu id:%d\n", g_npu_acl_model[model_index].model_mem_size,
        g_npu_acl_model[model_index].model_weight_size, g_npu_acl_model[model_index].model_id);

    g_npu_acl_model[model_index].is_load_flag = HI_TRUE;
    sample_svp_trace_info("load model %s success\n", model_path);

    return HI_SUCCESS;
}

hi_s32 sample_npu_load_model_with_mem_cached(const char *model_path, hi_u32 model_index)
{
    if (g_npu_acl_model[model_index].is_load_flag) {
        sample_svp_trace_err("has already loaded a model\n");
        return HI_FAILURE;
    }

    hi_s32 ret = aclmdlQuerySize(model_path, &g_npu_acl_model[model_index].model_mem_size,
        &g_npu_acl_model[model_index].model_weight_size);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("query model failed, model file is %s\n", model_path);
        return HI_FAILURE;
    }

    ret = ss_mpi_sys_mmz_alloc_cached(&g_npu_acl_model[model_index].model_mem_phy_addr,
        &g_npu_acl_model[model_index].model_mem_ptr, "model_mem", NULL, g_npu_acl_model[model_index].model_mem_size);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("malloc buffer for mem failed\n");
        return HI_FAILURE;
    }
    memset_s(g_npu_acl_model[model_index].model_mem_ptr, g_npu_acl_model[model_index].model_mem_size, 0,
        g_npu_acl_model[model_index].model_mem_size);
    ss_mpi_sys_flush_cache(g_npu_acl_model[model_index].model_mem_phy_addr, g_npu_acl_model[model_index].model_mem_ptr,
        g_npu_acl_model[model_index].model_mem_size);

    ret = ss_mpi_sys_mmz_alloc_cached(&g_npu_acl_model[model_index].model_weight_phy_addr,
        &g_npu_acl_model[model_index].model_weight_ptr, "model_weight",
        NULL, g_npu_acl_model[model_index].model_weight_size);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("malloc buffer for weight fail\n");
        return HI_FAILURE;
    }
    memset_s(g_npu_acl_model[model_index].model_weight_ptr, g_npu_acl_model[model_index].model_weight_size, 0,
        g_npu_acl_model[model_index].model_weight_size);
    ss_mpi_sys_flush_cache(g_npu_acl_model[model_index].model_weight_phy_addr,
        g_npu_acl_model[model_index].model_weight_ptr, g_npu_acl_model[model_index].model_weight_size);

    ret = aclmdlLoadFromFileWithMem(model_path, &g_npu_acl_model[model_index].model_id,
        g_npu_acl_model[model_index].model_mem_ptr, g_npu_acl_model[model_index].model_mem_size,
        g_npu_acl_model[model_index].model_weight_ptr, g_npu_acl_model[model_index].model_weight_size);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("load model from file failed, model file is %s\n", model_path);
        return HI_FAILURE;
    }

    sample_svp_trace_info("load mem_size:%lu weight_size:%lu id:%d\n", g_npu_acl_model[model_index].model_mem_size,
        g_npu_acl_model[model_index].model_weight_size, g_npu_acl_model[model_index].model_id);

    g_npu_acl_model[model_index].is_load_flag = HI_TRUE;
    sample_svp_trace_info("load model %s success\n", model_path);

    return HI_SUCCESS;
}

hi_s32 sample_npu_create_desc(hi_u32 model_index)
{
    hi_s32 ret;

    g_npu_acl_model[model_index].model_desc = aclmdlCreateDesc();
    if (g_npu_acl_model[model_index].model_desc == HI_NULL) {
        sample_svp_trace_err("create model description failed\n");
        return HI_FAILURE;
    }

    ret = aclmdlGetDesc(g_npu_acl_model[model_index].model_desc, g_npu_acl_model[model_index].model_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("get model description failed\n");
        return HI_FAILURE;
    }

    sample_svp_trace_info("create model description success\n");

    return HI_SUCCESS;
}

hi_void sample_npu_destroy_desc(hi_u32 model_index)
{
    if (g_npu_acl_model[model_index].model_desc != HI_NULL) {
        (hi_void)aclmdlDestroyDesc(g_npu_acl_model[model_index].model_desc);
        g_npu_acl_model[model_index].model_desc = HI_NULL;
    }

    sample_svp_trace_info("destroy model description success\n");
}

hi_s32 sample_npu_get_input_size_by_index(const hi_u32 index, size_t *input_size, hi_u32 model_index)
{
    if (g_npu_acl_model[model_index].model_desc == HI_NULL) {
        sample_svp_trace_err("no model description, create input failed\n");
        return HI_FAILURE;
    }

    *input_size = aclmdlGetInputSizeByIndex(g_npu_acl_model[model_index].model_desc, index);

    return HI_SUCCESS;
}

hi_s32 sample_npu_create_input_dataset(hi_u32 model_index)
{
    /* om used in this sample has only one input */
    if (g_npu_acl_model[model_index].model_desc == HI_NULL) {
        // sample_svp_trace_err("no model description, create input failed\n");
        return HI_FAILURE;
    }

    g_npu_acl_model[model_index].input_dataset = aclmdlCreateDataset();
    if (g_npu_acl_model[model_index].input_dataset == HI_NULL) {
        sample_svp_trace_err("can't create dataset, create input failed\n");
        return HI_FAILURE;
    }

    // sample_svp_trace_info("create model input dataset success\n");
    return HI_SUCCESS;
}

hi_void sample_npu_destroy_input_dataset(hi_u32 model_index)
{
    if (g_npu_acl_model[model_index].input_dataset == HI_NULL) {
        return;
    }

    aclmdlDestroyDataset(g_npu_acl_model[model_index].input_dataset);
    g_npu_acl_model[model_index].input_dataset = HI_NULL;

    sample_svp_trace_info("destroy model input dataset success\n");
}

hi_s32 sample_npu_create_input_databuf(hi_void *data_buf, size_t data_len, hi_u32 model_index)
{
    /* om used in this sample has only one input */
    if (g_npu_acl_model[model_index].model_desc == HI_NULL) {
        sample_svp_trace_err("no model description, create input failed\n");
        return HI_FAILURE;
    }

    size_t input_size = aclmdlGetInputSizeByIndex(g_npu_acl_model[model_index].model_desc, 0);
    if (data_len != input_size) {
        sample_svp_trace_err("input image size[%zu] != model input size[%zu]\n", data_len, input_size);
        return HI_FAILURE;
    }

    aclDataBuffer *input_data = aclCreateDataBuffer(data_buf, data_len);
    if (input_data == HI_NULL) {
        sample_svp_trace_err("can't create data buffer, create input failed\n");
        return HI_FAILURE;
    }

    aclError ret = aclmdlAddDatasetBuffer(g_npu_acl_model[model_index].input_dataset, input_data);
    if (ret != ACL_SUCCESS) {
        sample_svp_trace_err("add input dataset buffer failed, ret is %d\n", ret);
        (void)aclDestroyDataBuffer(input_data);
        input_data = HI_NULL;
        return HI_FAILURE;
    }
    // sample_svp_trace_info("create model input success\n");

    return HI_SUCCESS;
}

hi_void sample_npu_destroy_input_databuf(hi_u32 model_index)
{
    hi_u32 i;

    if (g_npu_acl_model[model_index].input_dataset == HI_NULL) {
        return;
    }

    for (i = 0; i < aclmdlGetDatasetNumBuffers(g_npu_acl_model[model_index].input_dataset); ++i) {
        aclDataBuffer *data_buffer = aclmdlGetDatasetBuffer(g_npu_acl_model[model_index].input_dataset, i);
        aclDestroyDataBuffer(data_buffer);
    }

    sample_svp_trace_info("destroy model input data buf success\n");
}

hi_s32 sample_npu_create_cached_input(hi_u32 model_index)
{
    hi_u32 input_size;
    if (g_npu_acl_model[model_index].model_desc == HI_NULL) {
        sample_svp_trace_err("no model description, create input failed\n");
        return HI_FAILURE;
    }

    g_npu_acl_model[model_index].input_dataset = aclmdlCreateDataset();
    if (g_npu_acl_model[model_index].input_dataset == HI_NULL) {
        sample_svp_trace_err("can't create dataset, create input failed\n");
        return HI_FAILURE;
    }

    input_size = aclmdlGetNumInputs(g_npu_acl_model[model_index].model_desc);
    for (hi_u32 i = 0; i < input_size; i++) {
        hi_u32 buffer_size = aclmdlGetInputSizeByIndex(g_npu_acl_model[model_index].model_desc, i);
        hi_void *input_buffer = HI_NULL;

        hi_s32 ret = ss_mpi_sys_mmz_alloc_cached(&g_npu_acl_model[model_index].input_phy_addr[i],
            &input_buffer, "input", NULL, buffer_size);
        if (ret != ACL_ERROR_NONE) {
            sample_svp_trace_err("can't malloc buffer, size is %u, create input failed\n", buffer_size);
            return HI_FAILURE;
        }
        memset_s(input_buffer, buffer_size, 0, buffer_size);
        ss_mpi_sys_flush_cache(g_npu_acl_model[model_index].input_phy_addr[i], input_buffer, buffer_size);

        aclDataBuffer *input_data = aclCreateDataBuffer(input_buffer, buffer_size);
        if (input_data == HI_NULL) {
            sample_svp_trace_err("can't create data buffer, create input failed\n");
            aclrtFree(input_buffer);
            return HI_FAILURE;
        }

        ret = aclmdlAddDatasetBuffer(g_npu_acl_model[model_index].input_dataset, input_data);
        if (ret != ACL_ERROR_NONE) {
            sample_svp_trace_err("can't add data buffer, create input failed\n");
            aclrtFree(input_buffer);
            aclDestroyDataBuffer(input_data);
            return HI_FAILURE;
        }
    }

    sample_svp_trace_info("create model input cached HI_SUCCESS\n");
    return HI_SUCCESS;
}

hi_s32 sample_npu_create_cached_output(hi_u32 model_index)
{
    hi_u32 output_size;

    if (g_npu_acl_model[model_index].model_desc == HI_NULL) {
        sample_svp_trace_err("no model description, create output failed\n");
        return HI_FAILURE;
    }

    g_npu_acl_model[model_index].output_dataset = aclmdlCreateDataset();
    if (g_npu_acl_model[model_index].output_dataset == HI_NULL) {
        sample_svp_trace_err("can't create dataset, create output failed\n");
        return HI_FAILURE;
    }

    output_size = aclmdlGetNumOutputs(g_npu_acl_model[model_index].model_desc);
    for (hi_u32 i = 0; i < output_size; ++i) {
        hi_u32 buffer_size = aclmdlGetOutputSizeByIndex(g_npu_acl_model[model_index].model_desc, i);
        hi_void *output_buffer = HI_NULL;

        hi_s32 ret = ss_mpi_sys_mmz_alloc_cached(&g_npu_acl_model[model_index].output_phy_addr[i],
            &output_buffer, "output", NULL, buffer_size);
        if (ret != ACL_ERROR_NONE) {
            sample_svp_trace_err("can't malloc buffer, size is %u, create output failed\n", buffer_size);
            return HI_FAILURE;
        }
        memset_s(output_buffer, buffer_size, 0, buffer_size);
        ss_mpi_sys_flush_cache(g_npu_acl_model[model_index].output_phy_addr[i], output_buffer, buffer_size);

        aclDataBuffer *output_data = aclCreateDataBuffer(output_buffer, buffer_size);
        if (output_data == HI_NULL) {
            sample_svp_trace_err("can't create data buffer, create output failed\n");
            aclrtFree(output_buffer);
            return HI_FAILURE;
        }

        ret = aclmdlAddDatasetBuffer(g_npu_acl_model[model_index].output_dataset, output_data);
        if (ret != ACL_ERROR_NONE) {
            sample_svp_trace_err("can't add data buffer, create output failed\n");
            aclrtFree(output_buffer);
            aclDestroyDataBuffer(output_data);
            return HI_FAILURE;
        }
    }

    sample_svp_trace_info("create model output cached HI_SUCCESS\n");
    return HI_SUCCESS;
}

hi_void sample_npu_destroy_cached_input(hi_u32 model_index)
{
    if (g_npu_acl_model[model_index].input_dataset == HI_NULL) {
        return;
    }

    for (hi_u32 i = 0; i < aclmdlGetDatasetNumBuffers(g_npu_acl_model[model_index].input_dataset); ++i) {
        aclDataBuffer *data_buffer = aclmdlGetDatasetBuffer(g_npu_acl_model[model_index].input_dataset, i);
        hi_void *data = aclGetDataBufferAddr(data_buffer);
        ss_mpi_sys_mmz_free(g_npu_acl_model[model_index].input_phy_addr[i], data);
        (hi_void)aclDestroyDataBuffer(data_buffer);
    }

    (hi_void)aclmdlDestroyDataset(g_npu_acl_model[model_index].input_dataset);
    g_npu_acl_model[model_index].input_dataset = HI_NULL;
}

hi_void sample_npu_destroy_cached_output(hi_u32 model_index)
{
    if (g_npu_acl_model[model_index].output_dataset == HI_NULL) {
        return;
    }

    for (hi_u32 i = 0; i < aclmdlGetDatasetNumBuffers(g_npu_acl_model[model_index].output_dataset); ++i) {
        aclDataBuffer *data_buffer = aclmdlGetDatasetBuffer(g_npu_acl_model[model_index].output_dataset, i);
        hi_void *data = aclGetDataBufferAddr(data_buffer);
        ss_mpi_sys_mmz_free(g_npu_acl_model[model_index].output_phy_addr[i], data);
        (hi_void)aclDestroyDataBuffer(data_buffer);
    }

    (hi_void)aclmdlDestroyDataset(g_npu_acl_model[model_index].output_dataset);
    g_npu_acl_model[model_index].output_dataset = HI_NULL;
}

hi_s32 sample_npu_create_output(hi_u32 model_index)
{
    hi_u32 output_size;

    if (g_npu_acl_model[model_index].model_desc == HI_NULL) {
        sample_svp_trace_err("no model description, create output failed\n");
        return HI_FAILURE;
    }

    g_npu_acl_model[model_index].output_dataset = aclmdlCreateDataset();
    if (g_npu_acl_model[model_index].output_dataset == HI_NULL) {
        sample_svp_trace_err("can't create dataset, create output failed\n");
        return HI_FAILURE;
    }

    output_size = aclmdlGetNumOutputs(g_npu_acl_model[model_index].model_desc);
    for (hi_u32 i = 0; i < output_size; ++i) {
        hi_u32 buffer_size = aclmdlGetOutputSizeByIndex(g_npu_acl_model[model_index].model_desc, i);

        hi_void *output_buffer = HI_NULL;
        hi_s32 ret = aclrtMalloc(&output_buffer, buffer_size, ACL_MEM_MALLOC_NORMAL_ONLY);
        if (ret != ACL_ERROR_NONE) {
            sample_svp_trace_err("can't malloc buffer, size is %u, create output failed\n", buffer_size);
            return HI_FAILURE;
        }

        aclDataBuffer *output_data = aclCreateDataBuffer(output_buffer, buffer_size);
        if (output_data == HI_NULL) {
            sample_svp_trace_err("can't create data buffer, create output failed\n");
            aclrtFree(output_buffer);
            return HI_FAILURE;
        }

        ret = aclmdlAddDatasetBuffer(g_npu_acl_model[model_index].output_dataset, output_data);
        if (ret != ACL_ERROR_NONE) {
            sample_svp_trace_err("can't add data buffer, create output failed\n");
            aclrtFree(output_buffer);
            aclDestroyDataBuffer(output_data);
            return HI_FAILURE;
        }
    }

    // sample_svp_trace_info("create model output HI_SUCCESS\n");
    return HI_SUCCESS;
}

/* print the top 5 confidence values with indexes */
#define SHOW_TOP_NUM    5
static hi_void ssample_npu_sort_output_result(const hi_float *src, hi_u32 src_len,
    hi_float *dst, hi_u32 dst_len)
{
    hi_u32 i, j;

    if (src == HI_NULL || dst == HI_NULL || src_len == 0 || dst_len == 0) {
        return;
    }

    for (i = 0; i < src_len; i++) {
        hi_bool charge = HI_FALSE;
        hi_u32 index;

        for (j = 0; j < dst_len; j++) {
            if (src[i] > dst[j]) {
                index = j;
                charge = HI_TRUE;
                break;
            }
        }

        if (charge == HI_TRUE) {
            for (j = dst_len - 1; j > index; j--) {
                dst[j] = dst[j - 1];
            }
            dst[index] = src[i];
        }
    }
}

hi_void sample_npu_output_model_result1(hi_u32 model_index, stYolov5Objs* pOut)
{
    aclDataBuffer *data_buffer = HI_NULL;
    hi_void *data = HI_NULL;
    hi_u32 len;
    hi_u32 i, j;
    hi_float top[SHOW_TOP_NUM] = { 0.0 };
    
    for (i = 0; i < aclmdlGetDatasetNumBuffers(g_npu_acl_model[model_index].output_dataset); ++i) {
        data_buffer = aclmdlGetDatasetBuffer(g_npu_acl_model[model_index].output_dataset, i);
        if (data_buffer == HI_NULL) {
            sample_svp_trace_err("get data buffer null.\n");
            continue;
        }

        data = aclGetDataBufferAddr(data_buffer);
        len = aclGetDataBufferSizeV2(data_buffer);
        if (data == HI_NULL || len == 0) {
            sample_svp_trace_err("get data null.\n");
            continue;
        }
        printf("len=%d\n",len/sizeof(float));
        yolo_detect_train(data, len/sizeof(float), pOut);
        
    }

    nms_process_in_place_and_correct_boxes(pOut);
    sample_svp_trace_info("output data success\n");
    return;
}

// 转换单个 float32 到 float16 的函数
uint16_t float32_to_float16(float input) {
    uint32_t in_bits = *((uint32_t*)&input);
    uint32_t sign = (in_bits >> 31) & 0x1;
    uint32_t exponent = (in_bits >> 23) & 0xFF;
    uint32_t mantissa = in_bits & 0x7FFFFF;

    uint16_t fp16 = 0;

    // 特殊值处理（例如 NaN 和无穷大）
    if (exponent == 255) {
        fp16 = (sign << 15) | (0x1F << 10) | (mantissa ? 0x200 : 0);
    } else if (exponent > 112) {  // 正常范围
        exponent = exponent - 127 + 15;
        if (exponent >= 31) {  // 超过 float16 的范围，设为无穷大
            fp16 = (sign << 15) | (0x1F << 10);
        } else {  // 正常转换
            fp16 = (sign << 15) | (exponent << 10) | (mantissa >> 13);
        }
    } else if (exponent >= 103) {  // 次正规数
        mantissa |= 1 << 23;
        mantissa = mantissa >> (113 - exponent);
        fp16 = (sign << 15) | (mantissa >> 13);
    } else {  // 太小，转为零
        fp16 = (sign << 15);
    }

    return fp16;
}

// 将 float32 数据转换为 float16 并保存到文件
void save_as_fp16(const float *data, size_t len, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("文件打开失败");
        return;
    }

    for (size_t i = 0; i < len; i++) {
        uint16_t fp16 = float32_to_float16(data[i]);
        fwrite(&fp16, sizeof(uint16_t), 1, file);
    }

    fclose(file);
    printf("数据已保存为 FP16 格式到文件: %s\n", filename);
}

// 双线性插值上采样函数
void bilinear_upsample(hi_float *input, hi_s32 in_h, hi_s32 in_w, hi_float *output, hi_s32 out_h, hi_s32 out_w, hi_s32 channels) {
    // 比例因子
    hi_float scale_h = (out_h > 1) ? (hi_float)(in_h - 1) / (out_h - 1) : 0.0f;
    hi_float scale_w = (out_w > 1) ? (hi_float)(in_w - 1) / (out_w - 1) : 0.0f;

    // 遍历每个通道
    for (hi_s32 c = 0; c < channels; ++c) {
        hi_float *index = input + c * in_h * in_w;
        hi_float *output_channel = output + c * out_h * out_w;

        // 遍历输出图像的每个像素
        for (hi_s32 y = 0; y < out_h; ++y) {
            for (hi_s32 x = 0; x < out_w; ++x) {
                // 映射到输入图像的坐标
                hi_float src_y = y * scale_h;
                hi_float src_x = x * scale_w;

                // 计算输入图像中最近的四个像素的索引
                hi_s32 y0 = (hi_s32)src_y;                 // 向下取整
                hi_s32 x0 = (hi_s32)src_x;                 // 向下取整
                hi_s32 y1 = (y0 + 1 < in_h) ? y0 + 1 : y0; // 防止越界
                hi_s32 x1 = (x0 + 1 < in_w) ? x0 + 1 : x0; // 防止越界

                // 获取插值权重
                hi_float dy = src_y - y0; // y 方向上的偏移量
                hi_float dx = src_x - x0; // x 方向上的偏移量

                // 双线性插值公式
                hi_float value =
                    index[y0 * in_w + x0] * (1 - dy) * (1 - dx) +
                    index[y0 * in_w + x1] * (1 - dy) * dx +
                    index[y1 * in_w + x0] * dy * (1 - dx) +
                    index[y1 * in_w + x1] * dy * dx;

                // 写入输出图像
                output_channel[y * out_w + x] = value;
            }
        }
    }
}


hi_void sample_npu_output_model_result(hi_u32 model_index)
{
    aclDataBuffer *data_buffer = HI_NULL;
    hi_void *data = HI_NULL;
    hi_u32 len;
    hi_u32 i, j;
    hi_float top[SHOW_TOP_NUM] = { 0.0 };
    
    for (i = 0; i < aclmdlGetDatasetNumBuffers(g_npu_acl_model[model_index].output_dataset); ++i) {
        data_buffer = aclmdlGetDatasetBuffer(g_npu_acl_model[model_index].output_dataset, i);
        if (data_buffer == HI_NULL) {
            sample_svp_trace_err("get data buffer null.\n");
            continue;
        }

        data = aclGetDataBufferAddr(data_buffer);
        len = aclGetDataBufferSizeV2(data_buffer);
        if (data == HI_NULL || len == 0) {
            sample_svp_trace_err("get data null.\n");
            continue;
        }
        // printf("\n\n\nlen/sizeof(uint8_t) = %d,len = %d\n\n\n\n",len/sizeof(hi_u8),len);
        
        // FILE *fp = fopen("output.bin","wb");
        // uint8_t *byte_data = (uint8_t *)data;
        // uint8_t one = 0b11111111;
        // uint8_t zero = 0b00000000;
        // for (size_t i = 0; i < len; i++)
        // {
        //     // 判断当前字节的最低位
        //     if (byte_data[i] & 0x01)
        //     {
        //         fwrite(&one, sizeof(uint8_t), 1, fp);
        //     }
        //     else
        //     {
        //         fwrite(&zero, sizeof(uint8_t), 1, fp);
        //     }
        // }

        printf("\n\n\nlen/sizeof(uint32_t) = %d,len = %d\n\n\n\n",len/sizeof(uint32_t),len);
        
        // FILE *fp = fopen("output.bin","wb");
        // uint32_t *byte_data = (uint32_t *)data;
        // uint8_t one = 0b11111111;
        // uint8_t zero = 0b00000000;
        // for (size_t i = 0; i < len/sizeof(uint32_t); i++)
        // {
        //     // 判断当前字节的最低位
        //     if (byte_data[i] & 0x01)
        //     {
        //         fwrite(&one, sizeof(uint8_t), 1, fp);
        //     }
        //     else
        //     {
        //         fwrite(&zero, sizeof(uint8_t), 1, fp);
        //     }
        // }
        //fwrite(data,len,1,fp);
        // fclose(fp);
        // chmod("output.bin", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    }
    sample_svp_trace_info("output data success\n");
    return;
}

hi_void sample_npu_destroy_output(hi_u32 model_index)
{
    if (g_npu_acl_model[model_index].output_dataset == HI_NULL) {
        return;
    }

    for (hi_u32 i = 0; i < aclmdlGetDatasetNumBuffers(g_npu_acl_model[model_index].output_dataset); ++i) {
        aclDataBuffer *data_buffer = aclmdlGetDatasetBuffer(g_npu_acl_model[model_index].output_dataset, i);
        hi_void *data = aclGetDataBufferAddr(data_buffer);
        (hi_void)aclrtFree(data);
        (hi_void)aclDestroyDataBuffer(data_buffer);
    }

    (hi_void)aclmdlDestroyDataset(g_npu_acl_model[model_index].output_dataset);
    g_npu_acl_model[model_index].output_dataset = HI_NULL;
}

hi_s32 sample_npu_model_execute(hi_u32 model_index)
{
    hi_s32 ret;
    ret = aclmdlExecute(g_npu_acl_model[model_index].model_id, g_npu_acl_model[model_index].input_dataset,
        g_npu_acl_model[model_index].output_dataset);
    // sample_svp_trace_info("end aclmdlExecute, modelId is %u\n", g_npu_acl_model[model_index].model_id);
    return ret;
}

hi_void sample_npu_unload_model(hi_u32 model_index)
{
    if (!g_npu_acl_model[model_index].is_load_flag) {
        sample_svp_trace_info("no model had been loaded.\n");
        return;
    }

    hi_s32 ret = aclmdlUnload(g_npu_acl_model[model_index].model_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("unload model failed, modelId is %u\n", g_npu_acl_model[model_index].model_id);
    }

    if (g_npu_acl_model[model_index].model_desc != HI_NULL) {
        (hi_void)aclmdlDestroyDesc(g_npu_acl_model[model_index].model_desc);
        g_npu_acl_model[model_index].model_desc = HI_NULL;
    }

    if (g_npu_acl_model[model_index].model_mem_ptr != HI_NULL) {
        aclrtFree(g_npu_acl_model[model_index].model_mem_ptr);
        g_npu_acl_model[model_index].model_mem_ptr = HI_NULL;
        g_npu_acl_model[model_index].model_mem_size = 0;
    }

    if (g_npu_acl_model[model_index].model_weight_ptr != HI_NULL) {
        aclrtFree(g_npu_acl_model[model_index].model_weight_ptr);
        g_npu_acl_model[model_index].model_weight_ptr = HI_NULL;
        g_npu_acl_model[model_index].model_weight_size = 0;
    }

    g_npu_acl_model[model_index].is_load_flag = HI_FALSE;
    sample_svp_trace_info("unload model SUCCESS, modelId is %u\n", g_npu_acl_model[model_index].model_id);
}

hi_void sample_npu_unload_model_cached(hi_u32 model_index)
{
    if (!g_npu_acl_model[model_index].is_load_flag) {
        sample_svp_trace_info("no model had been loaded.\n");
        return;
    }

    hi_s32 ret = aclmdlUnload(g_npu_acl_model[model_index].model_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("unload model failed, modelId is %u\n", g_npu_acl_model[model_index].model_id);
    }

    if (g_npu_acl_model[model_index].model_desc != HI_NULL) {
        (hi_void)aclmdlDestroyDesc(g_npu_acl_model[model_index].model_desc);
        g_npu_acl_model[model_index].model_desc = HI_NULL;
    }

    if (g_npu_acl_model[model_index].model_mem_ptr != HI_NULL) {
        ss_mpi_sys_mmz_free(g_npu_acl_model[model_index].model_mem_phy_addr,
            g_npu_acl_model[model_index].model_mem_ptr);
        g_npu_acl_model[model_index].model_mem_ptr = HI_NULL;
        g_npu_acl_model[model_index].model_mem_size = 0;
    }

    if (g_npu_acl_model[model_index].model_weight_ptr != HI_NULL) {
        ss_mpi_sys_mmz_free(g_npu_acl_model[model_index].model_weight_phy_addr,
            g_npu_acl_model[model_index].model_weight_ptr);
        g_npu_acl_model[model_index].model_weight_ptr = HI_NULL;
        g_npu_acl_model[model_index].model_weight_size = 0;
    }

    g_npu_acl_model[model_index].is_load_flag = HI_FALSE;
    sample_svp_trace_info("unload model SUCCESS, modelId is %u\n", g_npu_acl_model[model_index].model_id);
}

#define MAX_BATCH_COUNT 100
static hi_s32 sample_nnn_npu_get_dynamicbatch(hi_u32 model_index, hi_u32 *batchs, hi_u32 *count)
{
    aclmdlBatch batch;
    if (g_npu_acl_model[model_index].model_desc == HI_NULL) {
        sample_svp_trace_err("no model description, create input failed\n");
        return HI_FAILURE;
    }
    aclError ret = aclmdlGetDynamicBatch(g_npu_acl_model[model_index].model_desc, &batch);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("aclmdlGetDynamicBatch failed, modelId is %u, errorCode is %d\n",
            g_npu_acl_model[model_index].model_id, (int32_t)ret);
        return HI_FAILURE;
    }
    sample_svp_trace_info("aclmdlGetDynamicBatch batch count = %d\n", (int32_t)(batch.batchCount));
    *count = batch.batchCount;
    if (*count >= MAX_BATCH_COUNT) {
        sample_svp_trace_err("dynamic batch count[%u] is larger than max count[%u]\n", *count, MAX_BATCH_COUNT);
        return HI_FAILURE;
    }
    for (hi_u32 i = 0; i < batch.batchCount; i++) {
        sample_svp_trace_info("aclmdlGetDynamicBatch index = %d batch = %lu\n", i, batch.batch[i]);
        batchs[i] = batch.batch[i];
    }
    return HI_SUCCESS;
}

static hi_void sample_npu_set_input_data(hi_s32 model_index, hi_u32 value)
{
    aclDataBuffer *data_buffer = aclmdlGetDatasetBuffer(g_npu_acl_model[model_index].input_dataset, 0);
    hi_void *data = aclGetDataBufferAddr(data_buffer);
    hi_u32 buffer_size = aclmdlGetInputSizeByIndex(g_npu_acl_model[model_index].model_desc, 0);
    memset_s(data, buffer_size, value, buffer_size);
    ss_mpi_sys_flush_cache(g_npu_acl_model[model_index].input_phy_addr[0], data, buffer_size);
}

hi_s32 sample_nnn_npu_loop_execute_dynamicbatch(hi_u32 model_index)
{
    hi_u32 batchs[MAX_BATCH_COUNT];
    hi_u32 count = 0;
    hi_s32 ret = sample_nnn_npu_get_dynamicbatch(model_index, batchs, &count);
    if (ret != HI_SUCCESS) {
        sample_svp_trace_err("sample_nnn_npu_get_dynamicbatch fail\n");
        return HI_FAILURE;
    }

    /* memset_s input as 1 */
    sample_npu_set_input_data(model_index, 1);

    /* loop execute with every batch num */
    size_t index;
    for (hi_u32 i = 0; i < count; i++) {
        ret = aclmdlGetInputIndexByName(g_npu_acl_model[model_index].model_desc, ACL_DYNAMIC_TENSOR_NAME, &index);
        if (ret != ACL_ERROR_NONE) {
            sample_svp_trace_err("aclmdlGetInputIndexByName failed, modelId is %u, errorCode is %d\n",
                g_npu_acl_model[model_index].model_id, (int32_t)(ret));
            return HI_FAILURE;
        }
        sample_svp_trace_info("aclmdlSetDynamicBatchSize , batchSize is %u\n", batchs[i]);
        ret = aclmdlSetDynamicBatchSize(g_npu_acl_model[model_index].model_id,
            g_npu_acl_model[model_index].input_dataset, index,  batchs[i]);
        if (ret != ACL_ERROR_NONE) {
            sample_svp_trace_err("aclmdlSetDynamicBatchSize failed, modelId is %u, errorCode is %d\n",
                g_npu_acl_model[model_index].model_id, (int32_t)(ret));
            return HI_FAILURE;
        }

        /* ater set dynamic batch size, flush the mem */
        aclDataBuffer *input_data_buffer = aclmdlGetDatasetBuffer(g_npu_acl_model[model_index].input_dataset, index);
        hi_void *data = aclGetDataBufferAddr(input_data_buffer);
        hi_u32 buffer_size = aclmdlGetInputSizeByIndex(g_npu_acl_model[model_index].model_desc, index);

        ss_mpi_sys_flush_cache(g_npu_acl_model[model_index].input_phy_addr[index], data, buffer_size);

        ret = aclmdlExecute(g_npu_acl_model[model_index].model_id, g_npu_acl_model[model_index].input_dataset,
            g_npu_acl_model[model_index].output_dataset);
        if (ret != ACL_ERROR_NONE) {
            sample_svp_trace_err("execute model failed, modelId is %u, errorCode is %d\n",
                g_npu_acl_model[model_index].model_id, (int32_t)(ret));
            return HI_FAILURE;
        }
        aclDataBuffer *output_data_buffer = aclmdlGetDatasetBuffer(g_npu_acl_model[model_index].output_dataset, 0);
        data = aclGetDataBufferAddr(output_data_buffer);
        buffer_size = aclmdlGetOutputSizeByIndex(g_npu_acl_model[model_index].model_desc, 0);
        ss_mpi_sys_flush_cache(g_npu_acl_model[model_index].output_phy_addr[0], data, buffer_size);
    }
    sample_svp_trace_info("loop execute dynamicbatch success\n");
    return HI_SUCCESS;
}
