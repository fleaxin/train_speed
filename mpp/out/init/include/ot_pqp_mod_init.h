/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */
#ifndef OT_PQP_MOD_INIT_H
#define OT_PQP_MOD_INIT_H
#include "ot_type.h"

typedef struct {
    td_u8 pqp_high_profile_en;
} ot_pqp_mod_param;

td_s32 pqp_set_init_reg(td_u32 *pqp_reg[], td_u32 num);
td_s32 pqp_set_init_irq(const td_u32 pqp_irq[], td_u32 num);
td_s32 pqp_set_mod_param(const ot_pqp_mod_param *param);
td_s32 pqp_mod_init(td_void);
td_void pqp_mod_exit(td_void);
#endif
