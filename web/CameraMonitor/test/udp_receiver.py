# udp_receiver.py
from datetime import datetime
import socket
import logging
import json
import time

# 配置日志
logging.basicConfig(
    level=logging.DEBUG,
    format="%(asctime)s [%(levelname)s] %(message)s"
)
logger = logging.getLogger(__name__)

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

def start_udp_server(host='0.0.0.0', port=9999, buffer_size=65535):
    """
    启动UDP服务器监听指定端口
    
    参数:
        host: 监听地址
        port: 监听端口
        buffer_size: 缓冲区大小
    """
    try:
        # 创建UDP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
        # 设置SO_REUSEADDR选项
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        # 绑定地址和端口
        sock.bind((host, port))
        logger.info(f"UDP服务已启动，监听 {host}:{port}")
        
        while True:
            # 接收数据
            data, addr = sock.recvfrom(buffer_size)
            logger.info(f"收到数据来自 {addr} (长度: {len(data)}):")
            
            try:
                # 解码并解析JSON
                json_data = data.decode('utf-8')
                parsed_data = DataParser.parse_line(json_data, raise_on_error=True)  # 使用统一解析
                
                # 格式化输出
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
                logger.error(f"JSON解析失败: {e}")
                logger.info(f"原始数据: {data.decode('utf-8', errors='replace')}")
            except Exception as e:
                logger.error(f"处理数据时发生错误: {e}")
            
    except socket.error as e:
        logger.error(f"Socket错误: {e}")
    except KeyboardInterrupt:
        logger.info("正在关闭UDP服务...")
    finally:
        sock.close()

if __name__ == "__main__":
    # 使用本机所有IP监听
    start_udp_server(host='0.0.0.0', port=9999)