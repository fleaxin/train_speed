from django.shortcuts import render
from communication.utils import logger
from django.apps import apps
# Create your views here.

def handle_config_update(sender, instance, **kwargs):
    """
    配置模型保存后触发 reload()
    """
    logger.info("检测到相机配置更新，重新加载通信模块...")
    print("检测到相机配置更新，重新加载通信模块...")
    try:
        communication_config = apps.get_app_config('communication')
        if hasattr(communication_config, 'reload'):
            communication_config.reload()
            logger.info("通信模块重载成功")
        else:
            logger.error("目标模块未定义 reload() 方法")
    except LookupError:
        logger.error("找不到名为 'communication' 的应用")