# 使用uds 接收C/C++ 实时数据
# data_recevier.py

import json
import os
import struct
import sys
import threading
import time
import socket
from datetime import datetime

from .network_transmitter import create_forwarder
from .utils import logger



DATA_SOCKET_PATH = "/tmp/train_data.sock"

# 全局变量用于控制线程状态
data_forwarder = None
_running = False  # 控制线程运行状态
_listener_thread = None  # 保存线程引用


class DataParser:
    FIELD_TYPES = {
        'carriage_number': int,
        'is_in_carriage': int,
        'speed': float,
        'time': int, # C端为long类型
        'carriage_type': str,
        'remark': str,
        'carriage_distance_list': list,
    }

    def format_timestamp(ms: int) -> str:
        """
        将毫秒级时间戳转换为格式化字符串。

        参数:
            ms (int): 毫秒级时间戳，如 C 端返回的 long 值。

        返回:
            str: 格式为 'YYYY-MM-DD HH:MM:SS.mmm' 的时间字符串。
        """
        seconds = ms // 1000
        milliseconds = ms % 1000
        dt = datetime.fromtimestamp(seconds)
        # 使用 strftime 并手动拼接毫秒部分，确保三位数补零
        return f"{dt.strftime('%Y-%m-%d %H:%M:%S')}.{milliseconds:03d}"

    @classmethod
    def _parse_float_with_nan(cls, value):
        """处理可能的NaN值转换"""
        if value is None or isinstance(value, str) and value.lower() in ('nan', 'null'):
            return float('nan')
        try:
            return float(value)
        except (TypeError, ValueError) as e:
            msg = f"浮点数转换失败: {e}"
            if cls.raise_on_error:
                raise ValueError(msg) from e
            logger.warning(msg)
            return float('nan')
        
    @classmethod
    def get_current_time_str(cls) -> str:
        """
        获取当前系统时间并格式化为相同格式（用于对比延迟）
        """
        now_ms = int(time.time() * 1000)
        return cls.format_timestamp(now_ms)

    @classmethod
    def parse_line(cls, line, raise_on_error=False):
        try:
            j = json.loads(line)
            data = {}
            for key in j:
                if key in cls.FIELD_TYPES:
                    if key == 'time':
                        try:
                            ms = int(j[key])
                            data[key] = cls.format_timestamp(ms)
                        except ValueError as e:
                            msg = f"时间戳格式错误：{j[key]}"
                            if raise_on_error:
                                raise ValueError(msg)
                            logger.warning(msg)
                    elif key == 'carriage_distance_list':
                        try:
                            distance_list = []
                            for item in j[key]:  # 遍历数组中的每个元素
                                # 处理to_head
                                head = cls._parse_float_with_nan(item.get('head'))
                                # 处理tail
                                tail = cls._parse_float_with_nan(item.get('tail'))
                                distance_list.append((head, tail))  # 转换为元组列表
                            data[key] = distance_list
                        except (KeyError, TypeError, ValueError) as e:
                            msg = f"carriage_distance_list解析失败: {e}"
                            if raise_on_error:
                                raise ValueError(msg) from e
                            logger.warning(msg)
                    else:
                        try:
                            value = j[key]
                            # 特殊处理浮点数中的 NaN/null
                            if cls.FIELD_TYPES[key] == float:
                                if value is None or (isinstance(value, str) and value.lower() == 'nan'):
                                    data[key] = float('nan')
                                else:
                                    data[key] = float(value)
                            else:
                                data[key] = cls.FIELD_TYPES[key](value)
                        except (TypeError, ValueError) as e:
                            msg = f"字段 {key} 类型转换失败"
                            if raise_on_error:
                                raise ValueError(msg) from e
                            logger.warning(msg)
                else:
                    msg = f"未知字段 {key}"
                    if raise_on_error:
                        raise ValueError(msg)
                    print(msg)
            return data
        except json.JSONDecodeError as e:
            msg = f"JSON解析失败：{e}"
            if raise_on_error:
                raise ValueError(msg) from e
            logger.warning(msg)
            return {}


def initialize_forwarder():
    logger.info("初始化转发器...")
    """初始化转发器"""
    global data_forwarder

    # 清理旧资源
    if data_forwarder:
        try:
            data_forwarder.stop()
            data_forwarder = None  # 显式置空
        except Exception as e:
            logger.warning(f"清理旧转发器失败: {e}")
    try:
        # config = {
        #     "type": "udp",
        #     "host": "192.168.0.92",
        #     "port": 9999,
        #     "send_interval": 0.2
        # }
        # config = {
        #     'type': 'tcp',  # 改为'tcp'以启用TCP服务
        #     'host': '0.0.0.0',  # 监听所有网络接口
        #     'port': 8888,  # TCP服务端口
        # }
        # config_http_server = {
        #     'type': 'http-server',
        #     'host': '0.0.0.0',
        #     'port': 8000
        # }
        # config_http_client = {
        #     'type': 'http-client',
        #     'url': 'http://192.168.0.21:8080',
        #     'send_interval': 1
        # }
        time.sleep(2)
        from CameraSettings.models import CameraConfig
        try:
            config = CameraConfig.objects.get()
            config.refresh_from_db()
            print(config.data_mode)
            if config.data_mode == 'udp':
                forwarder_config = {
                    "type": "udp",
                    "host": config.udp_host,
                    "port": config.udp_port,
                    "send_interval": config.udp_send_rate,
                }
            elif config.data_mode == 'tcp_server':
                forwarder_config = {
                    "type": "tcp",
                    "host": config.tcp_host,
                    "port": config.tcp_port,
                }
            elif config.data_mode == 'http_server':
                forwarder_config = {
                    "type": "http_server",
                    "host": config.http_server_host,
                    "port": config.http_server_port,
                }
            elif config.data_mode == 'http_client':
                forwarder_config = {
                    "type": "http_client",
                    "url": config.http_client_url,
                    "send_interval": config.http_client_rate,
                }
            else:
                logger.error("未知的数据转发推送模式")
                return False
        except CameraConfig.DoesNotExist:
            logger.error("未找到配置")
            return False

        
        data_forwarder = create_forwarder(forwarder_config)
        data_forwarder.start()
        logger.info("数据转发器初始化成功")
        return True
    except Exception as e:
        logger.error(f"数据转发器初始化失败: {e}")
        return False

def run_socket_listener():
    global data_forwarder, _running, _listener_thread
    _running = True  # 设置运行标志为 True

    # 初始化转发器（如果未初始化）
    if not data_forwarder:
        if not initialize_forwarder():
            logger.error("数据转发器未初始化")
            return  # 初始化失败则退出

    try:
        while _running:
            # 删除旧的 socket 文件（如果存在）
            if os.path.exists(DATA_SOCKET_PATH):
                os.remove(DATA_SOCKET_PATH)

            # 创建 socket
            server_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                server_socket.bind(DATA_SOCKET_PATH)
                server_socket.listen(1)
                os.chmod(DATA_SOCKET_PATH, 0o777)  # 设置权限便于访问
                logger.info(f"等待火车数据计算端连接... 套接字路径: {DATA_SOCKET_PATH}")
                print(f"等待火车数据计算端连接... 套接字路径: {DATA_SOCKET_PATH}")

                conn, addr = server_socket.accept()
                logger.info(f"火车数据计算端已连接")
                print("火车数据计算端已连接")

                try:
                    while _running:
                        # 先读取长度信息（4字节）
                        raw_len = receive_exactly(conn, 4)
                        if not raw_len:
                            break

                        data_len = struct.unpack('!I', raw_len)[0]  # 解析长度
                        # 根据长度读取数据
                        data = receive_exactly(conn, data_len)
                        if not data:
                            break
                        line = data.decode().strip()
                        try:
                            parsed_data = DataParser.parse_line(line, raise_on_error=True)
                            current_time_seconds = time.time()
                            current_time_formatted = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(current_time_seconds))
                            milliseconds = int((current_time_seconds % 1) * 1000)
                            current_time_with_ms = f"{current_time_formatted}.{milliseconds:03d}"
                            # print(f"  当前时间: {current_time_with_ms}")
                            # print(f"  图片时间: {parsed_data['time']}")
                            # print(f"  车厢号: {parsed_data['carriage_number']}")
                            # # print(f"  离当前车头的距离: {parsed_data['to_current_to_head']}")
                            # # print(f"  离当前车尾的距离: {parsed_data['to_current_tail']}")
                            # # print(f"  离下一车头的距离: {parsed_data['to_next_to_head']}")
                            # # print(f"  离上一车尾的距离: {parsed_data['to_last_tail']}")
                            # # print(f"  time_ref: {parsed_data['time_ref']}")
                            # print(f"  是否在车厢内: {parsed_data['is_in_carriage']}")
                            # print(f"  速度: {parsed_data['speed']}")
                            # print(f"  车型: {parsed_data['carriage_type']}")
                            # print(f"  remark: {parsed_data['remark']}")
                            # print(f"  车厢距离列表：{parsed_data['carriage_distance_list']}")

                            if data_forwarder:
                                data_forwarder.update_data(line)

                        except ValueError as e:
                            print("火车数据解析失败，跳过该条数据:", e)
                            print(line)
                            logger.error(f"火车数据解析失败，跳过该条数据: {e}")
                            continue

                finally:
                    conn.close()
                    server_socket.close()
                    if os.path.exists(DATA_SOCKET_PATH):
                        os.unlink(DATA_SOCKET_PATH)
                    print("获取火车数据的连接已关闭，准备重新启动监听...")

            except Exception as e:
                print(f"获取火车数据 监听过程中发生异常: {e}")
                logger.error(f"获取火车数据 监听过程中发生异常: {e}")
                time.sleep(1)  # 防止快速失败导致 CPU 占用过高

    finally:
        if data_forwarder:
            data_forwarder.stop()
            data_forwarder = None
        _running = False  # 确保无论如何都重置运行状态
        print("获取火车数据 socket 监听线程已退出")
        logger.info("获取火车数据 socket 监听线程已退出")

# 辅助函数，确保接收指定数量的字节
def receive_exactly(conn, size):
    data = b''
    while len(data) < size:
        packet = conn.recv(size - len(data))
        if not packet:
            return None
        data += packet
    return data
def stop_socket_listener():
    """停止 socket 监听线程"""
    global data_forwarder, _running, _listener_thread
    print("正在请求停止 获取火车数据 socket 监听线程...")
    logger.info("正在请求停止 获取火车数据 socket 监听线程...")  
    data_forwarder.stop()
    print("test")
    data_forwarder = None
    _running = False

