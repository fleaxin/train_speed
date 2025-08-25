# 测试客户端脚本（TCP模式）
# 向设备发送请求，并接收数据
from datetime import datetime
import socket
import sys
import time
import json
from venv import logger

HOST = "192.168.0.122"      # 设备IP
PORT = 8888                 # 数据请求TCP端口

TIMEOUT = 10               # 超时时间
REQUEST = "GET_DATA"       # 请求命令


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
def tcp_client(host="192.168.0.122", port=8888):
    """TCP客户端测试"""
    try:
        # 创建TCP连接
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.settimeout(TIMEOUT)
        
        # 连接服务器
        print(f"正在连接服务器 {host}:{port}...")
        client_socket.connect((host, port))
        print("连接建立成功")
        
        # 发送请求
        print(f"发送请求: {REQUEST}")
        client_socket.sendall(REQUEST.encode() + b"\n")
        
        # 接收响应
        response = b""
        while True:
            data = client_socket.recv(4096)
            if not data:
                print("服务器关闭连接")
                break
            response += data
            
            # 尝试解析JSON，成功则停止接收
            try:
                json_data = json.loads(response.decode())
                print("完整JSON数据接收成功")
                break
            except json.JSONDecodeError:
                # JSON不完整或有错误，继续接收
                continue
                
        # 显示响应内容
        print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] 收到响应:")
        print(response.decode().strip())
        
        # 解析JSON数据
        try:
            parsed_data = DataParser.parse_line(response.decode(), raise_on_error=True)  # 使用统一解析
            logger.info("解析后的JSON数据:")
            current_time_seconds = time.time()
            current_time_formatted = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(current_time_seconds))
            milliseconds = int((current_time_seconds % 1) * 1000)
            current_time_with_ms = f"{current_time_formatted}.{milliseconds:03d}"
            print(f"  当前时间: {current_time_with_ms}")
            print(f"  图片时间: {parsed_data['time']}")
            print(f"  车厢号: {parsed_data['carriage_number']}")
            print(f"  是否在车厢内: {parsed_data['is_in_carriage']}")
            print(f"  速度: {parsed_data['speed']} m/s")
            print(f"  车型: {parsed_data['carriage_type']}")
            print(f"  remark: {parsed_data['remark']}")
            print(f"  车厢距离列表：{parsed_data['carriage_distance_list']}")
        except json.JSONDecodeError as e:
            print(f"JSON解析失败: {e}")
            
    except socket.timeout:
        print(f"连接超时({TIMEOUT}秒)，请检查服务器是否运行")
    except ConnectionRefusedError:
        print("连接被拒绝，请检查服务器是否启动且防火墙设置")
    except Exception as e:
        print(f"发生异常: {e}")
    finally:
        client_socket.close()
        print("连接已关闭")

if __name__ == "__main__":
    tcp_client(host=HOST, port= PORT)