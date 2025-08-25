# 负责向其他设备发送数据
import json
import socket
import threading
import requests
from http.server import BaseHTTPRequestHandler, HTTPServer
from threading import Thread
from collections.abc import Mapping
from abc import ABCMeta, abstractmethod
import queue
import time
from .utils import logger


class DataForwarder(metaclass=ABCMeta):
    """数据转发器基类"""
    
    def __init__(self, config):
        """
        初始化转发器
        
        参数:
            config: 包含转发配置的字典，必须包含'type'字段
        """
        if not isinstance(config, dict):
            raise TypeError(f"配置参数必须为字典类型，当前类型: {type(config)}")
            
        forwarder_type = config.get('type')
        if not forwarder_type:
            raise ValueError("配置中缺少转发器类型(type)字段")
            
        self.config = config
        self.running = True
        self.current_data = None
        self.data_lock = threading.Lock()
        
        # 添加调试日志
        logger.debug(f"{self.__class__.__name__} 实例化参数:")
        logger.debug(f"  type: {type(self)}")
        logger.debug(f"  config: {config}")
    
    @abstractmethod
    def _setup(self):
        """初始化转发器"""
        pass
    
    def update_data(self, data):
        """更新待转发数据（线程安全）"""
        with self.data_lock:
            self.current_data = data
            
    def get_current_data(self):
        """获取当前数据（线程安全）"""
        with self.data_lock:
            return self.current_data
            
    def stop(self):
        """停止转发器"""
        self.running = False
        logger.info(f"{self.__class__.__name__} 已停止")


class UdpForwarder(threading.Thread, DataForwarder):
    """UDP数据转发器"""
    
    def __init__(self, config):
        """
        初始化UDP转发器
        
        参数:
            config: 配置字典，必须包含以下字段：
                - host: 目标主机IP
                - port: 目标主机端口
                - send_interval: 发送间隔（秒）
        """
        # 初始化父类
        threading.Thread.__init__(self)
        DataForwarder.__init__(self, config)  # 调用基类初始化方法
        
        self.host = config['host']
        self.port = int(config['port'])
        self.send_interval = float(config.get('send_interval', 0.2))
        self.last_send_time = 0
        self.sock = None
        
        logger.info(f"UDP转发器配置参数：{self.config}")
    
    def _setup(self):
        """初始化转发器UDP socket"""
        try:
            if hasattr(self, 'sock') and self.sock:
                self.sock.close()
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.settimeout(1)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            logger.info(f"转发器UDP socket已初始化，目标地址: {self.host}:{self.port}")
        except Exception as e:
            logger.error(f"初始化转发器UDP socket失败: {e}", exc_info=True)
            raise
    
    def send_data(self, data):
        """发送单条数据"""
        try:
            # 支持字典和原始字符串两种格式
            if isinstance(data, Mapping):
                # 将字典转换为查询字符串格式
                message = "&".join([f"{k}={v}" for k, v in data.items() if v is not None])
            else:
                message = str(data)
                
            self.sock.sendto(message.encode(), (self.host, self.port))
            logger.debug(f"UDP转发器已发送最新数据到 {self.host}:{self.port}:{message}")
        except Exception as e:
            logger.error(f"转发器数据发送失败: {e}", exc_info=True)
            self._setup()  # 发送失败后重新初始化socket
    
    def run(self):
        """线程主循环"""
        try:
            self._setup()  # 初始化socket
            while self.running:
                current_time = time.time()
                # 检查是否达到发送间隔
                if current_time - self.last_send_time >= self.send_interval:
                    with self.data_lock:
                        if self.current_data is not None:
                            self.send_data(self.current_data)
                            self.last_send_time = current_time  # 更新最后发送时间
                            self.current_data = None # 重置当前数据 避免持续发送
                time.sleep(min(0.01, self.send_interval))  # 短暂休眠避免CPU过载
        except Exception as e:
            logger.error(f"转发器UDP线程运行过程中发生异常: {e}", exc_info=True)
        finally:
            self.stop()  # 确保执行停止清理
        
        logger.info("UDP转发器已停止")
            
    def stop(self):
        """停止转发器"""
        self.running = False
        


class TcpForwarder(threading.Thread, DataForwarder):
    """转发器TCP服务端，接收客户端连接并发送当前数据"""
    
    def __init__(self, config):
        """
        初始化TCP服务端
        
        参数:
            config: 配置字典，必须包含以下字段：
                - host: 监听主机IP（通常为0.0.0.0）
                - port: 监听端口号
        """
        # 初始化父类
        threading.Thread.__init__(self)
        DataForwarder.__init__(self, config)  # 调用基类初始化方法
        
        self.host = config['host']
        self.port = int(config['port'])
        self.request_timeout = config.get('request_timeout', 5)  # 默认5秒超时
        self.server_socket = None
        
        logger.info(f"转发器TCP服务端配置参数：{self.config}")
    
    def _setup(self):
        """初始化转发器（DataForwarder接口）"""
        try:
            if hasattr(self, 'server_socket') and self.server_socket:
                self.server_socket.close()
                
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(5)  # 最多允许5个连接排队
            logger.info(f"转发器TCP服务器已启动，监听地址: {self.host}:{self.port}")
        except Exception as e:
            logger.error(f"初始化转发器TCP服务器失败: {e}", exc_info=True)
            raise


    def _handle_client(self, client_socket, addr):
        """处理转发器客户端连接（按需响应模式）"""
        logger.info(f"新转发器客户端连接: {addr}")
        try:
            # 设置客户端socket超时
            client_socket.settimeout(self.request_timeout)
            
            while self.running:
                try:
                    # 等待客户端请求
                    data = client_socket.recv(1024)
                    
                    if not data:
                        logger.info(f"转发器客户端 {addr} 主动断开连接")
                        break
                    
                    request = data.decode().strip()
                    logger.debug(f"收到转发器客户端请求: {request}")
                    
                    # 检查是否为有效请求（可根据实际需求扩展协议）
                    if request.lower() == "get_data":
                        # 获取当前数据（线程安全）
                        with self.data_lock:
                            current_data = self.current_data
                            # 创建副本以避免在发送过程中被修改
                            data_copy = current_data if current_data is not None else {}
                        
                        # 构建响应消息
                        response = f"{data_copy}\n"  # 可根据需要调整响应格式
                        
                        # 发送数据
                        client_socket.sendall(response.encode())
                        logger.info(f"已响应转发器客户端 {addr} 的请求")
                        logger.info(f"已转发数据: {data_copy}")
                    else:
                        logger.warning(f"转发器收到无效请求: {request}")
                        client_socket.sendall(b"ERROR: Invalid request\n")
                        
                except socket.timeout:
                    logger.warning(f"转发器客户端 {addr} 请求超时")
                    break
                except Exception as e:
                    logger.error(f"处理转发器客户端 {addr} 请求失败: {e}")
                    break
                    
        finally:
            client_socket.close()
            logger.info(f"转发器客户端 {addr} 连接已关闭")
    
    def run(self):
        """线程主循环"""
        try:
            self._setup()  # 初始化服务器
            
            while self.running:
                # 设置超时以便能够定期检查运行状态
                self.server_socket.settimeout(1.0)

                try:
                    client_socket, addr = self.server_socket.accept()
                    # 启动单独线程处理客户端连接
                    client_thread = threading.Thread(
                        target=self._handle_client,
                        args=(client_socket, addr),
                        daemon=True
                    )
                    client_thread.start()
                except socket.timeout:
                    # 超时属于正常情况，继续循环
                    continue
                    
        except Exception as e:
            if self.running:
                logger.error(f"TCP转发服务器运行过程中发生异常: {e}", exc_info=True)
        finally:
            self.stop()  # 确保执行停止清理
    
    def stop(self):
        """停止TCP服务"""
        self.running = False
        
        # 关闭服务器socket
        if self.server_socket:
            try:
                self.server_socket.shutdown(socket.SHUT_RDWR)  # ✅ 主动中断现有连接
            except:
                pass
            try:
                self.server_socket.close()
            except Exception as e:
                logger.error(f"关闭转发服务器socket失败: {e}")
            finally:
                self.server_socket = None
                
        logger.info("TCP转发服务已停止")

class HttpServerForwarder(threading.Thread, DataForwarder):
    """HTTP服务端转发器，接收GET请求返回当前数据"""
    
    def __init__(self, config):
        """
        初始化HTTP服务端
        
        参数:
            config: 配置字典，必须包含以下字段：
                - host: 监听主机IP（通常为0.0.0.0）
                - port: 监听端口号
        """
        threading.Thread.__init__(self)
        DataForwarder.__init__(self, config)
        
        self.host = config['host']
        self.port = int(config['port'])
        self.server = None
        logger.info(f"HTTP服务端配置参数：{self.config}")
    
    class RequestHandler(BaseHTTPRequestHandler):
        def __init__(self, *args, http_forwarder=None, **kwargs):
            self.http_forwarder = http_forwarder
            super().__init__(*args, **kwargs)

        def do_GET(self):
            with self.http_forwarder.data_lock:
                data = self.http_forwarder.current_data
                data_copy = data if data is not None else {}

            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(data_copy).encode())
            logger.info(f"HTTP服务端已返回数据: {data_copy}")
    
    def _setup(self):
        """初始化HTTP服务"""
        try:
            if self.server:
                self.server.shutdown()
                self.server.server_close()
            
            # 使用 lambda 动态绑定 self 到 RequestHandler
            def handler_factory(*args, **kwargs):
                return self.RequestHandler(*args, http_forwarder=self, **kwargs)
            
            server_address = (self.host, self.port)
            self.server = HTTPServer(server_address, handler_factory)
            logger.info(f"HTTP服务端已启动，监听地址: {self.host}:{self.port}")
        except Exception as e:
            logger.error(f"初始化HTTP服务端失败: {e}", exc_info=True)
            raise
    
    def run(self):
        try:
            self._setup()
            self.server.serve_forever()
        except Exception as e:
            logger.error(f"HTTP服务端运行失败: {e}", exc_info=True)
        finally:
            self.stop()
    
    def stop(self):
        self.running = False
        if self.server:
            self.server.shutdown()
            self.server.server_close()
            self.server = None
        logger.info("HTTP 服务端已停止")


class HttpClientForwarder(threading.Thread, DataForwarder):
    """HTTP客户端转发器，定期向远程URL发送POST请求"""
    
    def __init__(self, config):
        """
        初始化HTTP客户端转发器
        
        参数:
            config: 配置字典，必须包含以下字段：
                - url: 目标URL
                - send_interval: 发送间隔（秒）
        """
        threading.Thread.__init__(self)
        DataForwarder.__init__(self, config)
        
        self.url = config['url']
        self.send_interval = float(config.get('send_interval', 5))
        self.last_send_time = time.time()
        logger.info(f"HTTP客户端转发器配置参数：{self.config}")

    def _setup(self):
        """HTTP客户端转发器初始化方法（空实现）"""
        logger.info("HTTP客户端转发器已初始化")
    def send_data(self, data):
        try:
            response = requests.post(self.url, json=data, timeout=5)
            if response.status_code == 200:
                logger.info(f"HTTP客户端已发送数据到 {self.url}: {data}")
            else:
                logger.warning(f"HTTP客户端发送失败，状态码: {response.status_code}")
        except requests.exceptions.ConnectionError as e:
            logger.error(f"HTTP客户端发送失败: 目标服务不可达 - {str(e)}")
        except requests.exceptions.RequestException as e:
            logger.error(f"HTTP客户端发送失败: {e}")
        except Exception as e:
            logger.error(f"未知错误: {e}", exc_info=True)
    
    def run(self):
        try:
            while self.running:
                current_time = time.time()
                if current_time - self.last_send_time >= self.send_interval:
                    with self.data_lock:
                        if self.current_data is not None:
                            self.send_data(self.current_data)
                            self.last_send_time = current_time
                            self.current_data = None
                time.sleep(min(0.1, self.send_interval))
        except Exception as e:
            logger.error(f"HTTP客户端转发器运行失败: {e}", exc_info=True)
        finally:
            self.stop()
    
    def stop(self):
        self.running = False
        logger.info("HTTP客户端转发器已停止")

FORWARDER_MAP = {
    'udp': UdpForwarder,
    'tcp': TcpForwarder,
    'http_server': HttpServerForwarder,
    'http_client': HttpClientForwarder,
}

def create_forwarder(config):
    """
    根据配置创建转发器实例
    
    参数:
        config: 配置字典，必须包含'type'字段
    
    返回:
        DataForwarder子类的实例
    """
    forwarder_type = config.get('type')
    if not forwarder_type:
        raise ValueError("配置中缺少转发器类型(type)字段")
        
    forwarder_class = FORWARDER_MAP.get(forwarder_type.lower())
    if not forwarder_class:
        raise ValueError(f"不支持的转发器类型: {forwarder_type}")
        
    try:
        instance = forwarder_class(config)
        logger.info(f"{forwarder_type}转发器创建成功")
        return instance
    except Exception as e:
        logger.error(f"{forwarder_type}转发器初始化失败: {e}")
        raise