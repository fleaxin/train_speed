# http_server_tester.py
import requests
import time
import json
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

def test_http_get(url):
    """
    测试 HTTP 服务端 GET 请求响应
    """
    try:
        print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] 发送 GET 请求至 {url}")
        response = requests.get(url, timeout=5)
        
        if response.status_code == 200:
            print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] 接收到数据:")
            # print(response.text)
            
            try:
                parsed_data = DataParser.parse_line(response.text, raise_on_error=True)
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
        else:
            print(f"请求失败，状态码: {response.status_code}")
    except requests.exceptions.RequestException as e:
        print(f"网络请求异常: {e}")

if __name__ == "__main__":
    url = "http://192.168.0.122:8000"
    test_http_get(url)