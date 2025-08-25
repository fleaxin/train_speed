# http_client_test_server.py
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import threading
import time
from datetime import datetime
from venv import logger  # 根据项目实际日志配置调整

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
            # 如果解析后得到的是字符串类型，说明存在双重JSON编码
            if isinstance(j, str):
                try:
                    # 对解析出的字符串进行二次解析
                    j = json.loads(j)
                except json.JSONDecodeError as e:
                    msg = f"双层JSON解析失败：{e}"
                    if raise_on_error:
                        raise ValueError(msg) from e
                    logger.warning(msg)
                    return {}

            # 如果二次解析后仍然不是字典，则报错
            if not isinstance(j, dict):
                msg = f"解析结果非字典类型: {type(j)}"
                if raise_on_error:
                    raise ValueError(msg)
                logger.warning(msg)
                return {}
            
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
                    # msg = f"未知字段 {key}"
                    # if raise_on_error:
                    #     raise ValueError(msg)
                    # print(msg)
                    data[key] = j[key]
            return data
        except json.JSONDecodeError as e:
            msg = f"JSON解析失败：{e}"
            if raise_on_error:
                raise ValueError(msg) from e
            logger.warning(msg)
            return {}
        

class DataReceiverHandler(BaseHTTPRequestHandler):
    """处理 HTTP 客户端转发器发送的 POST 请求"""
    received_data = []

    def do_POST(self):
        """处理 POST 请求"""
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        
        try:
            data = json.loads(post_data.decode())
            self.received_data.append(data)
            print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] 接收到数据:")
            # print(json.dumps(data, indent=2))
            try:
                parsed_data = DataParser.parse_line(data, raise_on_error=True)
                print("解析后的数据:")
                print(f"  当前时间: {parsed_data.get('time', 'N/A')}")
                print(f"  车厢号: {parsed_data.get('carriage_number', 'N/A')}")
                print(f"  是否在车厢内: {parsed_data.get('is_in_carriage', 'N/A')}")
                print(f"  速度: {parsed_data.get('speed', 'N/A')} m/s")
                print(f"  车型: {parsed_data.get('carriage_type', 'N/A')}")
                print(f"  remark: {parsed_data.get('remark', 'N/A')}")
                print(f"  车厢距离列表：{parsed_data.get('carriage_distance_list', 'N/A')}")
            except Exception as e:
                print(f"解析数据失败: {e}")
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(b'{"status": "OK"}')
        except json.JSONDecodeError as e:
            print(f"JSON 解析失败: {e}")
            self.send_response(400)
            self.end_headers()

    @classmethod
    def start_server(cls, host="0.0.0.0", port=8080):
        """启动测试 HTTP 服务端"""
        server_address = (host, port)
        httpd = HTTPServer(server_address, cls)
        print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] HTTP 测试服务端启动，监听 {host}:{port}")
        httpd.serve_forever()

def start_test_http_server(host="0.0.0.0", port=8080):
    """启动 HTTP 测试服务端线程"""
    server_thread = threading.Thread(target=DataReceiverHandler.start_server, args=(host, port), daemon=True)
    server_thread.start()
    print(f"HTTP 测试服务端已启动，监听 {host}:{port}")

if __name__ == "__main__":
    start_test_http_server()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("关闭测试 HTTP 服务端...")