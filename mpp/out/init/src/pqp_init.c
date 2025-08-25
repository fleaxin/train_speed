/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <linux/of_platform.h>
#include <asm/io.h>
#include "ot_common.h"
#include "ot_osal.h"
#include "mod_ext.h"
#include "securec.h"
#include "ot_pqp_mod_init.h"

#define OT_PQP_DEV_NAME_LENGTH 8
#define OT_PQP_DEV_NUM 1

static td_u8 g_pqp_high_profile_en = 0;
static td_u32 *g_pqp_init_reg[OT_PQP_DEV_NUM] = { TD_NULL };
static td_u32 g_pqp_init_irq[OT_PQP_DEV_NUM] = { 0 };

module_param_named(pqp_high_profile_en, g_pqp_high_profile_en, byte, S_IRUGO);
static int ot_pqp_probe(struct platform_device *pf_dev)
{
    td_s32 irq, ret;
    td_char pqp_dev_name[OT_PQP_DEV_NAME_LENGTH] = { '\0' };
    struct resource *mem = TD_NULL;
    td_void *reg = TD_NULL;
    ot_pqp_mod_param pqp_mod_param;

    ret = snprintf_s(pqp_dev_name, OT_PQP_DEV_NAME_LENGTH, OT_PQP_DEV_NAME_LENGTH - 1, "pqp");
    if ((ret < 0) || (ret > (OT_PQP_DEV_NAME_LENGTH - 1))) {
        printk("snprintf_s pqp name failed\n");
        return TD_FAILURE;
    }
    mem = osal_platform_get_resource_byname(pf_dev, IORESOURCE_MEM, pqp_dev_name);
    reg = devm_ioremap_resource(&pf_dev->dev, mem);
    if (IS_ERR(reg)) {
        printk("devm_ioremap_resource failed\n");
        return PTR_ERR(reg);
    }

    irq = osal_platform_get_irq_byname(pf_dev, "pqp_ns");
    if (irq <= 0) {
        dev_err(&pf_dev->dev, "can't find pqp IRQ\n");
        return TD_FAILURE;
    }
    g_pqp_init_irq[0] = (td_u32)irq;
    g_pqp_init_reg[0] = (td_u32 *)reg;

    pqp_mod_param.pqp_high_profile_en = g_pqp_high_profile_en;

    if (pqp_set_init_reg(g_pqp_init_reg, OT_PQP_DEV_NUM) != TD_SUCCESS) {
        return TD_FAILURE;
    }
    if (pqp_set_init_irq(g_pqp_init_irq, OT_PQP_DEV_NUM) != TD_SUCCESS) {
        return TD_FAILURE;
    }
    if (pqp_set_mod_param(&pqp_mod_param) != TD_SUCCESS) {
        return TD_FAILURE;
    }
    return pqp_mod_init();
}

static int ot_pqp_remove(struct platform_device *pf_dev)
{
    ot_unused(pf_dev);
    pqp_mod_exit();
    g_pqp_init_reg[0] = TD_NULL;
    return 0;
}

static const struct of_device_id g_ot_pqp_match[] = {
    { .compatible = "vendor,pqp" },
    {},
};
MODULE_DEVICE_TABLE(of, g_ot_pqp_match);

static struct platform_driver g_ot_pqp_driver = {
    .probe          = ot_pqp_probe,
    .remove         = ot_pqp_remove,
    .driver         = {
        .name   = "ot_pqp",
        .of_match_table = g_ot_pqp_match,
    },
};

static int __init ot_pqp_driver_init(void)
{
    if (cmpi_get_module_name(OT_ID_SVP_NPU) != TD_NULL) {
        printk("Attention: pqp cannot run because the svp_npu.ko has been loaded!\n");
        return TD_FAILURE;
    }
    return osal_platform_driver_register(&g_ot_pqp_driver);
}
module_init(ot_pqp_driver_init);

static void __exit ot_pqp_driver_exit(void)
{
    return osal_platform_driver_unregister(&g_ot_pqp_driver);
}
module_exit(ot_pqp_driver_exit);

MODULE_LICENSE("Proprietary");
