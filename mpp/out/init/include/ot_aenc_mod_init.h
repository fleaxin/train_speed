/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#ifndef OT_AENC_MOD_INIT_H
#define OT_AENC_MOD_INIT_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include "ot_type.h"

#ifdef __cplusplus
extern "C" {
#endif /* end of #ifdef __cplusplus */

int aenc_module_init(void);
void aenc_module_exit(void);

#ifdef __cplusplus
}
#endif /* end of #ifdef __cplusplus */

#endif /* end of #ifndef OT_AENC_MOD_INIT_H */

