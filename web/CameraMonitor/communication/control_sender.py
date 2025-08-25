# 发送控制命令给 C/C++
from abc import ABCMeta, abstractmethod
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import json
import socket
import threading
import subprocess
import logging
import os
import time
from queue import Queue
from .utils import logger

# 协议版本常量
PROTOCOL_VERSION = "1.0"


class UDSClient:
    """UDS客户端，用于与C/C++端通信"""
    def __init__(self, socket_path="/tmp/control.sock", max_retries=3, retry_delay=1):
        self.socket_path = socket_path
        self.sock = None
        self.max_retries = max_retries
        self.retry_delay = retry_delay
        self.connect()
        
    def connect(self):
        """建立UDS连接（带重试机制）"""
        self._close_connection()  # 强制关闭现有连接
        
        retry_count = 0
        current_delay = self.retry_delay
        
        while retry_count <= self.max_retries:
            try:
                # 创建新的socket连接
                self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.sock.settimeout(2)  # 设置超时防止永久阻塞
                
                # 尝试连接前检查socket文件是否存在
                if not os.path.exists(self.socket_path):
                    logger.warning(f"socket文件不存在: {self.socket_path}")
                    if retry_count < self.max_retries:
                        retry_count += 1
                        time.sleep(current_delay)
                        current_delay = min(current_delay * 2, 10)
                        continue
                
                self.sock.connect(self.socket_path)
                logger.info(f"成功连接到C/C++控制通道 {self.socket_path}")
                return True
                
            except (socket.error, FileNotFoundError) as e:
                retry_count += 1
                if retry_count > self.max_retries:
                    logger.error(f"连接C/C++控制通道失败（最大重试次数已到）: {e}")
                    self.sock = None
                    return False
                    
                logger.warning(f"连接失败，{current_delay}秒后重试... (尝试次数: {retry_count}/{self.max_retries})")
                time.sleep(current_delay)
                current_delay = min(current_delay * 2, 10)
                
        return False

    def send_command(self, cmd_type, payload=""):
        """
        发送控制指令（带自动重连机制）
        
        返回:
            str: 响应结果
        """
        # 第一次连接状态检测
        if not self._is_connected():
            if not self.connect():
                return "ERROR: UDS连接不可用（自动重连失败）"
                
        # 构造JSON参数
        message = json.dumps({
            "version": PROTOCOL_VERSION,
            "type": cmd_type,
            "payload": payload
        }, ensure_ascii=False) + "\n"
        
        print(message)
        
        try:
            # 发送前再次检测连接状态（包括socket文件是否存在）
            if not self._is_connected() or not os.path.exists(self.socket_path):
                if not self.connect():
                    return "ERROR: UDS连接不可用（发送前重连失败）"
            
            # 将字符串编码为字节流发送
            self.sock.sendall(message.encode())
            response = self.sock.recv(1024)
            return response.decode().strip()
            
        except (socket.error, BrokenPipeError, socket.timeout) as e:
            logger.error(f"UDS通信错误: {e}")
            self._close_connection()
            
            # 即使出现错误也尝试重连
            if self.connect():
                try:
                    # 重新发送指令
                    self.sock.sendall(message)
                    response = self.sock.recv(1024)
                    return response.decode().strip()
                except:
                    return "ERROR: UDS重连后通信失败"
            
            return f"ERROR: {str(e)}"
        except Exception as e:
            logger.error(f"发送指令异常: {e}")
            self._close_connection()
            return f"ERROR: {str(e)}"
    
    def _is_connected(self):
        """增强的连接状态检测"""
        if not self.sock:
            return False
            
        try:
            # 检查socket文件是否存在
            if not os.path.exists(self.socket_path):
                return False            
            # 非阻塞模式心跳检测
            self.sock.setblocking(0)
            result = self.sock.recv(16, socket.MSG_PEEK)
            
            if result == b'':
                return False
                
            self.sock.setblocking(1)
            return True
        except BlockingIOError:
            self.sock.setblocking(1)
            return True
        except:
            return False
            
    def _close_connection(self):
        """安全关闭连接"""
        if self.sock:
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
            except:
                logger.error("shutdown error")
            try:
                self.sock.close()
            except:
                logger.error("close error")
        self.sock = None
    def force_close(self):
        """强制关闭连接并清理残留资源"""
        self._close_connection()
        # 确保删除残留socket文件
        if os.path.exists(self.socket_path):
            try:
                os.unlink(self.socket_path)  # 删除残留socket文件
                logger.info(f"残留socket文件已清理: {self.socket_path}")
            except Exception as e:
                logger.error(f"清理socket文件失败: {e}")


class BaseControlServer(threading.Thread, metaclass=ABCMeta):
    """控制服务基类，定义通用协议和接口"""
    
    def __init__(self, uds_client, host="0.0.0.0", port=8001):
        super().__init__(daemon=True)
        self.host = host
        self.port = port
        self.uds_client = uds_client
        self.running = True
        self.protocol_version = "1.0"

    def validate_protocol(self, data):
        """验证JSON协议格式"""
        if not data.get("version"):
            return False, "缺少version字段"
        
        if data["version"] != self.protocol_version:
            return False, f"协议版本不支持: {data['version']}"
        
        cmd_type = data.get("type")
        payload = data.get("payload", {})
        
        # 验证指令类型
        if cmd_type == "SET_MODEL":
            if not isinstance(payload.get("models"), list):
                return False, "models字段必须为数组"
            if not isinstance(payload.get("count"), int) or payload.get("count") <= 0:
                return False, "count字段必须为正整数"
        elif cmd_type == "RESTART":
            if not payload.get("target"):
                return False, "缺少target字段"
        elif cmd_type == "RESET_DATA":
            # if not payload.get("target"):
            #     return False, "缺少target字段"
            return True, ""
        else:
            return False, f"未知指令类型: {cmd_type}"
        
        return True, ""

    def execute_command(self, cmd_type, payload):
        """
        执行指定的控制指令
        
        参数:
            cmd_type: 指令类型
            payload: 指令负载数据
            
        返回:
            str: 执行结果
        """
        handlers = {
            "SET_MODEL": lambda: self.send_to_uds(cmd_type, payload),
            "RESET_DATA": lambda: self.send_to_uds(cmd_type, payload),
            "RESTART": lambda: self._handle_restart(payload)
        }
        
        if cmd_type in handlers:
            try:
                return handlers[cmd_type]()
            except Exception as e:
                logger.error(f"执行指令失败 {cmd_type}: {str(e)}")
                return f"ERROR: 执行{cmd_type}指令失败 - {str(e)}"
        return "ERROR: 未知指令类型"

    def send_to_uds(self, cmd_type, payload):
        """通过UDS发送指令到C/C++"""
        return self.uds_client.send_command(cmd_type, payload)

    def _handle_restart(self, payload):
        """处理重启指令"""
        target = payload.get("target", "system")
        if target == "system":
            # 通过systemd重启系统
            import subprocess
            subprocess.run(["reboot"])
        elif target == "service":
            # 重启指定服务
            subprocess.run(["systemctl", "restart", target])
            return "INFO: 服务重启指令已执行"
        else:
            return "ERROR: 未知目标"

    @abstractmethod
    def run(self):
        """启动控制服务"""
        pass

    @abstractmethod
    def stop(self):
        """停止控制服务"""
        pass


class TCPControlServer(BaseControlServer):
    """TCP控制服务器（统一JSON协议）"""
    
    def __init__(self, uds_client, host="0.0.0.0", port=8001):
        super().__init__(uds_client, host, port)
        self.server_socket = None
        self.clients = []

    def run(self):
        """启动TCP服务器"""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(5)
            logger.info(f"TCP控制服务器已启动，监听 {self.host}:{self.port}")
            
            while self.running:
                try:
                    client_socket, addr = self.server_socket.accept()
                    logger.info(f"新TCP连接来自 {addr}")
                    self.clients.append(client_socket)
                    threading.Thread(
                        target=self.handle_client,
                        args=(client_socket, addr),
                        daemon=True
                    ).start()
                except socket.timeout:
                    continue
                except Exception as e:
                    logger.error(f"接受TCP连接异常: {e}")
        except Exception as e:
            logger.error(f"启动TCP控制服务器失败: {e}")
    
    def handle_client(self, client_socket, addr):
        """处理TCP客户端连接"""
        try:
            while self.running:
                data = client_socket.recv(1024)
                if not data:
                    break
                    
                try:
                    # 解析JSON
                    raw_json = data.decode('utf-8').strip()
                    logger.info(f"收到TCP指令: {raw_json}")
                    
                    # 验证协议
                    try:
                        data = json.loads(raw_json)
                    except json.JSONDecodeError as e:
                        logger.error(f"TCP JSON解析失败: {e}")
                        logger.debug(f"原始数据: {raw_json!r}")
                        client_socket.sendall(b"ERROR: JSON type error\n")
                        continue
                    
                    is_valid, error = self.validate_protocol(data)
                    if not is_valid:
                        logger.error(f"TCP协议验证失败: {error}")
                        client_socket.sendall(f"ERROR: {error}\n".encode('utf-8'))
                        continue
                    
                    # 执行指令
                    cmd_type = data.get("type")
                    response = self.execute_command(cmd_type, data.get("payload", {}))
                    
                    # 发送响应
                    client_socket.sendall(f"{response}\n".encode('utf-8'))
                    
                except Exception as e:
                    logger.error(f"TCP处理指令异常: {e}")
                    client_socket.sendall(f"ERROR: {str(e)}\n".encode('utf-8'))
                    
        except Exception as e:
            logger.error(f"TCP客户端 {addr} 连接异常: {e}")
        finally:
            client_socket.close()
            self.clients.remove(client_socket)
            logger.info(f"TCP连接 {addr} 已关闭")
    
    def stop(self):
        """安全停止 TCP 控制服务器"""
        if not self.running:
            return
        
        self.running = False
        
        # 关闭监听 socket
        if self.server_socket:
            try:
                self.server_socket.close()
            except Exception as e:
                logger.warning(f"关闭监听 socket 失败: {e}")

        # 设置停止标志后立即关闭监听socket
        if self.server_socket:
            try:
                # 立即关闭socket中断阻塞
                self.server_socket.shutdown(socket.SHUT_RDWR)
            except:
                pass
            try:
                self.server_socket.close()
            except Exception as e:
                logger.warning(f"关闭监听 socket 失败: {e}")
        
        # 强制中断accept阻塞
        if self.server_socket:
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.connect(('127.0.0.1', self.port))
            except:
                pass

        # 关闭所有客户端连接
        for client_socket in self.clients:
            try:
                client_socket.shutdown(socket.SHUT_RDWR)
                client_socket.close()
            except Exception as e:
                logger.warning(f"关闭TCP客户端连接失败: {e}")
        
        # 关闭UDS连接
        if hasattr(self, 'uds_client') and self.uds_client:
            try:
                self.uds_client._close_connection()
                logger.info("已关闭UDS连接")
            except Exception as e:
                logger.warning(f"关闭UDS连接失败: {e}")
        
        self.clients.clear()
        logger.info("TCP控制服务器已停止")




class HTTPControlServer(BaseControlServer):
    """HTTP控制服务器（统一JSON协议）"""
    
    def __init__(self, uds_client, host="0.0.0.0", port=8001):
        super().__init__(uds_client, host, port)
        self.server = None

    class RequestHandler(BaseHTTPRequestHandler):
        def __init__(self, *args, control_server=None, **kwargs):
            self.control_server = control_server
            super().__init__(*args, **kwargs)

        def do_POST(self):
            """处理HTTP POST请求"""
            try:
                content_length = int(self.headers['Content-Length'])
                post_data = self.rfile.read(content_length)
                
                raw_json = post_data.decode('utf-8').strip()
                logger.info(f"收到HTTP指令: {raw_json}")
                
                # 验证协议
                try:
                    data = json.loads(raw_json)
                except json.JSONDecodeError as e:
                    logger.error(f"HTTP JSON解析失败: {e}")
                    self.send_error(400, "JSON解析失败")
                    return
                
                is_valid, error = self.control_server.validate_protocol(data)
                if not is_valid:
                    logger.error(f"HTTP协议验证失败: {error}")
                    self.send_response(400)
                    self.send_header("Content-Type", "application/json; charset=utf-8")
                    self.end_headers()
                    self.wfile.write(json.dumps({"error": error}, ensure_ascii=False).encode("utf-8"))
                    return
                
                # 执行指令
                cmd_type = data.get("type")
                payload = data.get("payload", {})
                response = self.control_server.execute_command(cmd_type, payload)

                print(f"指令响应: {response}")
                
                # 返回响应
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({"response": response}, ensure_ascii=False).encode("utf-8"))
                
            except Exception as e:
                logger.error(f"HTTP处理指令异常: {e}")
                self.send_error(500, str(e))

    def run(self):
        """启动HTTP服务器"""
        def handler_factory(*args, **kwargs):
            return self.RequestHandler(*args, control_server=self, **kwargs)
        
        try:
            server_address = (self.host, self.port)
            self.server = HTTPServer(server_address, handler_factory)
            logger.info(f"HTTP控制服务器已启动，监听 {self.host}:{self.port}")
            self.server.serve_forever()
        except Exception as e:
            logger.error(f"启动HTTP控制服务器失败: {e}")

    def stop(self):
        """停止HTTP服务器"""
        if self.server:
            self.server.shutdown()
            self.server.server_close()
            self.server = None
        logger.info("HTTP控制服务器已停止")

        # 关闭UDS连接        
        if hasattr(self, 'uds_client') and self.uds_client:
            try:
                self.uds_client._close_connection()
                logger.info("UDS连接已关闭")
            except Exception as e:
                logger.warning(f"关闭UDS连接失败: {e}")
        

def create_control_server(protocol, uds_client, host="0.0.0.0", port=8000):
    """
    控制服务器工厂函数
    
    参数:
        protocol: 协议类型 ('tcp' or 'http')
        uds_client: UDS客户端实例
        host: 监听主机
        port: 监听端口
    
    返回:
        BaseControlServer 实例
    """
    servers = {
        'tcp': TCPControlServer,
        'http': HTTPControlServer
    }
    
    server_class = servers.get(protocol.lower())
    if not server_class:
        raise ValueError(f"不支持的协议类型: {protocol}")
        
    return server_class(uds_client, host, port)