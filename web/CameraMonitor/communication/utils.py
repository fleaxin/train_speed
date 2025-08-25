# 工具函数

import json
import os
import logging
import time

from django.conf import settings


# 与 C/C++ zlog 的日志路径保持一致
LOG_FILE_PATH = "./log/train_system.log"

def setup_logger():
    # 创建日志目录（如果不存在）
    log_dir = os.path.dirname(LOG_FILE_PATH)
    os.makedirs(log_dir, exist_ok=True)

    # 自定义 formatter 以匹配 zlog 格式：
    # "06-19 18:37:49 INFO  [2907:camera_capture.cpp:580] message"
    class ZLogStyleFormatter(logging.Formatter):
        def formatTime(self, record, datefmt=None):
            # 匹配 mm-dd hh:mm:ss 格式
            return time.strftime("%m-%d %H:%M:%S", self.converter(record.created))

        def formatMessage(self, record):
            # 获取 PID（原 processName 是线程名，改用 process）
            pid = record.process
            # 获取文件名（保留 .cpp 后缀）
            filename = os.path.basename(record.pathname)
            # 拼接 [PID:filename:lineno]
            location = f"{pid}:{filename}:{record.lineno}"
            # 统一 5 字符宽度日志级别（INFO 为 4 字符，加 1 空格）
            level = f"{record.levelname} "
            # 拼接最终格式
            return f"{self.formatTime(record)} {level} [{location}] {record.getMessage()}"

    # 文件日志处理器（匹配 zlog 的轮转规则）
    file_handler = logging.handlers.TimedRotatingFileHandler(
        LOG_FILE_PATH,
        when='midnight',
        backupCount=5,
        encoding='utf-8'
    )
    file_handler.setLevel(logging.DEBUG)
    file_formatter = ZLogStyleFormatter()
    file_handler.setFormatter(file_formatter)

    # 控制台日志处理器（模拟 zlog 的 stdout 输出）
    stream_handler = logging.StreamHandler()
    stream_handler.setLevel(logging.DEBUG)
    stream_formatter = logging.Formatter(fmt="[%(levelname).1s]  %(message)s")
    stream_handler.setFormatter(stream_formatter)

    # 创建 logger
    logger = logging.getLogger('TrainSystem')
    logger.setLevel(logging.DEBUG)
    logger.addHandler(file_handler)
    logger.addHandler(stream_handler)

    return logger

# 全局 logger 实例
logger = setup_logger()

def sync_config_to_json(obj):
    """
    将摄像头配置保存为 JSON 文件
    """
    config_data = {
        'ip_address': obj.ip_address,
        'resolution': obj.resolution,
        'framerate': obj.framerate,
        'stream_protocol': obj.stream_protocol,
        'stream_bitrate': obj.stream_bitrate,
        'auto_start': obj.auto_start,
        'log_level': obj.log_level,
    }
    
    file_path = os.path.join(settings.MEDIA_ROOT, 'camera_config.json')
    os.makedirs(os.path.dirname(file_path), exist_ok=True)

    try:
        with open(file_path, 'w') as f:
            json.dump(config_data, f, indent=4)
    except Exception as e:
        raise RuntimeError(f"写入配置文件失败: {str(e)}")
