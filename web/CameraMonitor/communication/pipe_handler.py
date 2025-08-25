# 接收 C/C++ 实时数据
# CameraMonitor/communication/pipeline_listener.py

import os
import sys
import time

PIPE_PATH = "/tmp/train_data"

class DataParser:
    FIELD_TYPES = {
        'current_distance': float,
        'speed': float,
        'time_ref': int,
        'carriage_number': int,
    }

    @classmethod
    def parse_line(cls, line):
        data = {}
        pairs = line.strip().split('&')
        for pair in pairs:
            try:
                key, value = pair.split('=', 1)
                if key in cls.FIELD_TYPES:
                    data[key] = cls.FIELD_TYPES[key](value)
                else:
                    print(f"警告：未知字段 {key}")
            except ValueError:
                print(f"格式错误：{pair}")
        return data

def run_pipeline_listener():
    # 创建管道（如果不存在）
    if not os.path.exists(PIPE_PATH):
        os.mkfifo(PIPE_PATH)

    print(f"等待数据... 管道路径: {PIPE_PATH}")
    with open(PIPE_PATH, 'r') as pipe:
        while True:
            line = pipe.readline()
            if line:
                data = DataParser.parse_line(line)
                if data:
                    print(f"[{time.ctime()}] 收到数据:")
                    print(f"  车厢号: {data['carriage_number']}")
                    print(f"  当前距离: {data['current_distance']}")
                    print(f"  速度: {data['speed']}")
                    print(f"  time_ref: {data['time_ref']}")
            else:
                time.sleep(0.1)