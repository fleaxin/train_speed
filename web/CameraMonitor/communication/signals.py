# 监听配置保存事件

from .utils import sync_config_to_json


def apply_hardware_settings(obj):
    
    try:
        sync_config_to_json(obj)
    except Exception as e:
        raise RuntimeError(f"同步配置到JSON文件失败: {str(e)}")
    
    try:
        # 应用到硬件系统

        # 向C端发送信号，从JSON文件中重新载入配置
        # sand_signal_to_hardware_system(config_data)
        pass
    except Exception as e:
        raise RuntimeError(f"应用到硬件系统失败: {str(e)}")