import os
import time
from django.apps import AppConfig
import threading
import signal
import sys

from django.db import connections


from .network_transmitter import DataForwarder
from .utils import logger

class CommunicationConfig(AppConfig):
    default_auto_field = 'django.db.models.BigAutoField'
    name = 'communication'
    
    _listener_thread = None

    def ready(self):
        # 确保只在主进程中运行
        if os.environ.get('RUN_MAIN') == 'true':
            # 延迟启动线程，确保 Django 完全初始化
            self._delayed_start()
    
    def _delayed_start(self):
        """延迟启动线程，确保 Django 完全初始化"""
        # 检查数据库连接是否就绪
        try:
            connections['default'].ensure_connection()

            # 启动数据发送模块
            if not hasattr(self, '_listener_thread') or self._listener_thread is None:
                from .data_recevier import run_socket_listener
                self._listener_thread = threading.Thread(target=run_socket_listener, daemon=True)
                self._listener_thread.start()
                logger.info("数据发送模块已启动")
                print("启动socket监听作为后台线程")
            else:
                logger.info("检测到现有监听线程，避免重复启动")

            # 启动控制服务
            if not hasattr(self, '_control_server_thread') or self._control_server_thread is None:
                # 创建UDS客户端和控制服务器实例
                from .control_sender import UDSClient, create_control_server
                try:
                    # 使用默认参数，让UDSClient自己处理重试
                    uds_client = UDSClient()
                    self._control_server = create_control_server('http', uds_client, port=8001)  # 保存实例引用
                    self._control_server_thread = threading.Thread(
                        target=self._control_server.run, 
                        daemon=True
                    )
                    self._control_server_thread.start()
                    logger.info("控制服务已启动")
                    print("启动控制服务作为后台线程")
                except Exception as e:
                    logger.error(f"控制服务初始化失败: {e}")
            else:
                logger.info("检测到现有控制服务线程，避免重复启动")
        except Exception as e:
            # 如果数据库未就绪，延迟重试
            logger.warning(f"数据库未就绪，延迟启动线程: {e}")
            threading.Timer(2.0, self._delayed_start).start()

    def reload(self):
        from .data_recevier import run_socket_listener, stop_socket_listener
        """重新加载模块，释放旧资源并重启"""
        logger.info("开始重新加载 CameraSettings 模块...")

        logger.info("正在停止数据推送线程...")
        # 这里应实现优雅停止逻辑，比如设置标志位让线程退出
        stop_socket_listener()
        self._listener_thread.join(timeout=5)  # 等待最多5秒
        if self._listener_thread.is_alive():
            print("警告：socket 监听线程未能及时退出")
            logger.warning("socket 监听线程未能及时退出")
        else:
            print("socket 监听线程已安全退出")
            logger.info("socket 监听线程已安全退出")
        self._listener_thread = None

        # 清理其他资源...
        
        # 重新启动数据发送模块
        from .control_sender import UDSClient, create_control_server
        self._listener_thread = threading.Thread(target=run_socket_listener, daemon=True)
        self._listener_thread.start()
        
        logger.info("数据发送模块已启动")

        # 重启控制服务
        logger.info("正在停止控制服务...")
        if hasattr(self, '_control_server'):
            self._control_server.stop()  # 调用停止方法
        
        if self._control_server_thread and self._control_server_thread.is_alive():
            self._control_server_thread.join(timeout=5)
            if self._control_server_thread.is_alive():
                print("警告：控制服务线程未能及时退出")
                logger.warning("控制服务线程未能及时退出")

        logger.info("控制服务已停止，正在重启...")
        
        # 重建实例
        uds_client = UDSClient()            
        self._control_server = create_control_server('http', uds_client, port=8001)
        self._control_server_thread = threading.Thread(
            target=self._control_server.run, 
            daemon=True
        )
        self._control_server_thread.start()
        logger.info("控制服务已重启")