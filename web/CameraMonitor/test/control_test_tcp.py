import json
import socket

HOST = "192.168.0.122"
PORT = 8001

def send_command(message):
    """发送JSON格式指令"""
    
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))

        # 发送指令
        s.sendall(json.dumps(message, ensure_ascii=False).encode('utf-8') + b"\n")
        
        # 接收响应
        response = s.recv(1024).decode('utf-8')
        print(f"响应: {response.strip()}")
        return response

if __name__ == "__main__":
    print("TCP指令测试工具（JSON格式）")
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
            # 完整json样例:
            {
                "version": "1.0",
                "type": "RESTART",
                "payload": {"target": "system"}
            }

            # 构造命令
            payload = {"target": "system"}
            message = {
                "version": "1.0",
                "type": "RESTART",
                "payload": payload
            }
        
        # 发送重启服务软件命令
        elif cmd_type == "restart_service":
            # 完整json样例:
            {
                "version": "1.0",
                "type": "RESTART",
                "payload": {"target": "service"}
            }

            # 构造命令
            payload = {"target": "service"}
            message = {
                "version": "1.0",
                "type": "RESTART",
                "payload": payload
            }

        # 发送重置计数命令
        elif cmd_type == "reset_data":
            # 完整json样例:target为空
            {
                "version": "1.0",
                "type": "RESTART",
                "payload": {"target": " "}
            }
            
            message = {
                "version": "1.0",
                "type": "RESET_DATA",
                "payload": {"target": " "}
            }

        # 发送指令
        try:
            send_command(message)
        except Exception as e:
            print(f"通信失败: {e}")

        
        
        