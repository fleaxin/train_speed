/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <linux/of_platform.h>

#include "ot_ive_mod_init.h"
#include "ot_common.h"
#include "ot_osal.h"

#define OT_IVE_DEV_NAME_LENGTH 10
#define OT_IVE_DEFAULT_NODE_NUM 512

static td_u8 g_power_save_en = 1;
static td_u16 g_node_num = OT_IVE_DEFAULT_NODE_NUM;
static td_void *g_mod_reg = TD_NULL;

module_param_named(save_power, g_power_save_en, byte, S_IRUGO);
module_param_named(max_node_num, g_node_num, ushort, S_IRUGO);

static int ot_ive_probe(struct platform_device *pf_dev)
{
    struct resource *mem = TD_NULL;
    ot_ive_mod_param mod_param;
    td_char dev_name[OT_IVE_DEV_NAME_LENGTH] = { "ive" };
    td_s32 irq, ret;

    mem = osal_platform_get_resource_byname(pf_dev, IORESOURCE_MEM, dev_name);
    g_mod_reg = devm_ioremap_resource(&pf_dev->dev, mem);
    if (IS_ERR((void *)g_mod_reg)) {
        return PTR_ERR((const void *)g_mod_reg);
    }

    irq = osal_platform_get_irq_byname(pf_dev, dev_name);
    if (irq <= 0) {
        dev_err(&pf_dev->dev, "cannot find ive IRQ\n");
        printk("cannot find ive IRQ\n");
    }

    ret = ive_set_init_ive_reg((td_void *)g_mod_reg);
    if (ret != TD_SUCCESS) {
        printk("ive_set_init_ive_reg failed!\n");
        return TD_FAILURE;
    }

    ret = ive_set_init_ive_irq((td_u32)irq);
    if (ret != TD_SUCCESS) {
        printk("ive_set_init_ive_irq failed!\n");
        return TD_FAILURE;
    }

    mod_param.power_save_en = g_power_save_en;
    mod_param.mod_node_num = g_node_num;

    return ive_std_mod_init(&mod_param);
}

static int ot_ive_remove(struct platform_device *pf_dev)
{
    ot_unused(pf_dev);
    ive_std_mod_exit();
    g_mod_reg = TD_NULL;
    return 0;
}

static const struct of_device_id g_ot_ive_match[] = {
    { .compatible = "vendor,ive" },
    {},
};
MODULE_DEVICE_TABLE(of, g_ot_ive_match);

static struct platform_driver g_ot_ive_driver = {
    .probe = ot_ive_probe,
    .remove = ot_ive_remove,
    .driver = {
        .name = "ot_ive",
        .of_match_table = g_ot_ive_match,
    },
};

osal_module_platform_driver(g_ot_ive_driver);

MODULE_LICENSE("Proprietary");
