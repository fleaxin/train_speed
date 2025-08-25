import json
import socket
import os
# 添加 parse_command 方法
def parse_command(raw_command):
    """解析指令（支持JSON和字符串格式）"""
    try:
        # 尝试JSON解析
        if '|' in raw_command:
            cmd_type, payload = raw_command.split('|', 1)
            if cmd_type == "SET_MODEL":
                try:
                    data = json.loads(payload)
                    return {
                        "version": data.get("version", "1.0"),
                        "type": "SET_MODEL",
                        "payload": {
                            "count": int(data["count"]),
                            "models": data["models"]
                        }
                    }
                except:
                    # 回退到字符串解析
                    payload_parts = payload.split(':', 1)
                    if len(payload_parts) == 2:
                        return {
                            "version": "0.1",
                            "type": "SET_MODEL",
                            "payload": {
                                "count": int(payload_parts[0]),
                                "models": payload_parts[1].split(',')
                            }
                        }
            return {"type": cmd_type, "payload": payload}
        return None
    except:
        return None
# 清理已存在的socket文件
SOCKET_PATH = "/tmp/control.sock"
if os.path.exists(SOCKET_PATH):
    os.remove(SOCKET_PATH)

# 创建UDS服务器
server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(SOCKET_PATH)
server.listen(1)
print(f"UDS服务器已启动，监听 {SOCKET_PATH}")

# 接收连接
conn, _ = server.accept()
print("客户端已连接")

# 指令处理部分
while True:
    data = conn.recv(1024)
    if not data:
        break
        
    try:
        # 解析JSON
        raw_json = data.decode('utf-8').strip()
        print(f"收到JSON指令: {raw_json}")
        
        # 解析JSON
        try:
            data = json.loads(raw_json)
        except json.JSONDecodeError as e:
            print(f"JSON解析失败: {e}")
            conn.sendall(b"ERROR: JSON type err\n")
            continue
        
        # 执行指令
        cmd_type = data.get("type")
        if cmd_type == "SET_MODEL":
            payload = data.get("payload", {})
            # 处理车厢设置
            print(f"处理指令: {payload.get('count')}节车厢")
            for i, model in enumerate(payload.get("models", [])):
                print(f"  车厢{i+1}: {model}")
        elif cmd_type == "RESTART":
            payload = data.get("payload", {})
            print(f"收到重启指令: {payload.get('target', 'system')}")
        elif cmd_type == "RESET_DATA":
            payload = data.get("payload", {})
            print(f"收到重置指令: {payload.get('target', 'all')}")
        else:
            print(f"收到未知指令: {cmd_type}")
        
        # 发送响应
        conn.sendall(f"INFO: 收到 {raw_json}\n".encode('utf-8'))
        
    except Exception as e:
        print(f"处理异常: {e}")
        conn.sendall(f"ERROR: {str(e)}\n".encode('utf-8'))

# 清理资源
conn.close()
server.close()
os.remove(SOCKET_PATH)
