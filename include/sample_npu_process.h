/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#ifndef SAMPLE_NPU_PROCESS_H
#define SAMPLE_NPU_PROCESS_H

#include "hi_type.h"
#include "yolov5.h"
/* function : show the sample of acl resnet50 */
hi_void sample_nnn_npu_acl_resnet50(hi_void);

hi_void sample_nnn_npu_acl_netv8(hi_void);
hi_void sample_nnn_npu_acl_netv8_upsample(hi_void);

hi_void resize_test(hi_void);

/* function : show the sample of acl resnet50_multithread */
hi_void sample_nnn_npu_acl_resnet50_multithread(hi_void);

/* function : show the sample of acl resnet50 sign handle */
hi_void sample_nnn_npu_acl_resnet50_handle_sig(hi_void);

/* function : show the sample of acl mobilenet_v3 dyanamicbatch with mmz cached */
hi_void sample_nnn_npu_acl_mobilenet_v3_dynamicbatch(hi_void);

hi_bool nnn_init(hi_void);
hi_s32 nnn_execute(hi_void* data_buf, unsigned long data_len,stYolov5Objs* pOut);
hi_s32 nnn_execute_test(hi_void* data_buf, long time, hi_s32 numbs);
#endif
