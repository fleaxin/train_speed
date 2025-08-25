import json
import requests

# HTTP服务端地址配置
HOST = "http://192.168.0.122"
PORT = 8001  # 替换为你的HTTP服务端口
BASE_URL = f"{HOST}:{PORT}"

def send_command(message):
    """
    发送JSON指令到HTTP服务端
    返回响应内容
    """
    url = BASE_URL
    headers = {'Content-Type': 'application/json'}
    
    try:
        response = requests.post(url, json=message, headers=headers, timeout=10)
        print(f"状态码: {response.status_code}")
        print(f"响应内容: {response.text.strip()}")
        return response.text
    except requests.exceptions.ConnectionError:
        print("错误：无法连接到服务端（可能服务未启动）")
    except requests.exceptions.Timeout:
        print("错误：请求超时")
    except requests.exceptions.RequestException as e:
        print(f"请求失败: {e}")

if __name__ == "__main__":
    print("HTTP指令测试工具（JSON格式）")
    print("可用指令: set_model, restart_system, restart_service, reset_data")
    
    while True:
        cmd_type = input("输入指令类型 (q退出): ").strip()
        if cmd_type == "q":
            break
            
        if cmd_type not in ["set_model", "restart_system", "restart_service", "reset_data"]:
            print("无效指令类型")
            continue
        
        # 指令参数
        message = None

        # 设置车型
        if cmd_type == "set_model":
            # 完整json样例:
            {
                "version": "1.0",
                "type": "SET_MODEL",
                "payload": {
                            "count": 4,
                            "models":[  "C70",
                                        "C70E",
                                        "C70H",
                                        "C64K"
                                        ]
                            }
            }

            # count:车厢数
            # models:车型名称列表
            # 构造数据格式
            command_type = "SET_MODEL"
            count = 4
            models = "C70,C70E,C70H,C64K"
            payload = {
                "count": int(count),
                "models": models.split(',') if models else []
            }
            # 构造JSON
            message = {
                "version": "1.0",
                "type": command_type,
                "payload": payload
            }

        # 发送重启设备命令
        elif cmd_type == "restart_system":
            payload = {"target": "system"}
            message = {
                "version": "1.0",
                "type": "RESTART",
                "payload": payload
            }
        
        # 发送重启服务软件命令
        elif cmd_type == "restart_service":
            payload = {"target": "service"}
            message = {
                "version": "1.0",
                "type": "RESTART",
                "payload": payload
            }

        # 发送重置计数命令
        elif cmd_type == "reset_data":
            message = {
                "version": "1.0",
                "type": "RESET_DATA",
                "payload": {"target": ""}
            }

        # 发送指令
        print(f"\n发送指令: {json.dumps(message, indent=2, ensure_ascii=False)}")
        try:
            send_command(message)
        except Exception as e:
            print(f"通信失败: {e}")
        
        print("-" * 50)