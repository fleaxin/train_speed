/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#ifndef SAMPLE_SVP_NPU_PROCESS_H
#define SAMPLE_SVP_NPU_PROCESS_H

#include "hi_type.h"
#include "yolov5.h"
#include "sample_svp_npu_define.h"

/* function : show the sample of acl resnet50 */
hi_void sample_svp_npu_acl_resnet50(hi_void);

/* function : show the sample of acl resnet50_multithread */
hi_void sample_svp_npu_acl_resnet50_multi_thread(hi_void);

/* function : show the sample of resnet50 dyanamic batch with mem cached */
hi_void sample_svp_npu_acl_resnet50_dynamic_batch(hi_void);

/* function : show the sample of lstm */
hi_void sample_svp_npu_acl_lstm(hi_void);

/* function : show the sample of rfcn */
hi_void sample_svp_npu_acl_rfcn(hi_void);

/* function : show the sample of sign handle */
hi_void sample_svp_npu_acl_handle_sig(hi_void);

hi_bool svp_init(hi_void *data_buf);
hi_void svp_uninit();
hi_s32 svp_execute(hi_void* data_buf,stYolov5Objs* pOut);
hi_s32 svp_execute_test(hi_void* data_buf, long time ,hi_s32 numbs);

hi_void sample_svp_npu_acl_netv8(hi_void);

#endif
