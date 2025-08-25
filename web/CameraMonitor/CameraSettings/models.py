# CameraSettings/models.py
from django.db import models
from solo.models import SingletonModel
from django.core.validators import MaxValueValidator

class CameraConfig(SingletonModel):
    # 摄像头基础设置
    ip_address = models.CharField(
        '摄像头IP地址', max_length=20,
        default='192.168.0.121', help_text='默认: 192.168.1.121',
    )

    # 命令设置
    COMMAND_TCP_PORT = models.PositiveIntegerField(
        '控制命令端口', default=8001,
        help_text='默认: 8001'
    )

    # 数据推送设置
    DATA_MODE_CHOICES = [
        ('http_client', 'HTTP 主动发送模式'),
        ('http_server', 'HTTP 请求响应模式'),
    ]
    data_mode = models.CharField(
        '数据推送模式', max_length=20,
        choices=DATA_MODE_CHOICES, default='http_server',
        help_text='选择数据推送方式：HTTP主动发送模式 或 HTTP请求响应模式'
    )

    http_server_host = models.CharField(
        'HTTP请求响应监听地址', max_length=50, default='0.0.0.0',
        help_text='仅当选择HTTP请求响应模式时生效'
    )
    http_server_port = models.PositiveIntegerField(
        'HTTP请求响应监听端口', default=8000,
        help_text='仅当选择HTTP请求响应模式时生效'
    )
    http_client_url = models.CharField(
        'HTTP主动发发送地址', max_length=50, default='',
        help_text='仅当选择HTTP主动发送模式时生效'
    )
    http_client_rate = models.FloatField(
        'HTTP主动发送间隔', default=0.2,
        help_text='仅当选择HTTP主动发送模式时生效,单位:秒'
    )

    # # 数据推送设置
    # DATA_MODE_CHOICES = [
    #     ('udp', 'UDP 输出'),
    #     ('tcp_server', 'TCP 请求响应模式'),
    # ]
    # data_mode = models.CharField(
    #     '数据推送模式', max_length=20,
    #     choices=DATA_MODE_CHOICES, default='udp',
    #     help_text='选择数据推送方式：UDP广播 或 TCP请求-响应模式'
    # )

    # tcp_host = models.CharField(
    #     'TCP监听地址', max_length=50, default='0.0.0.0',
    #     help_text='仅当选择TCP模式时生效'
    # )
    # tcp_port = models.PositiveIntegerField(
    #     'TCP监听端口', default=8888,
    #     help_text='仅当选择TCP模式时生效'
    # )
    # udp_host = models.CharField(
    #     'UDP发送地址', max_length=50, default='192.168.0.92',
    #     help_text='仅当选择UDP模式时生效'
    # )
    # udp_port = models.PositiveIntegerField(
    #     'UDP发送端口', default=9999
    # )
    # udp_send_rate = models.FloatField(
    #     'UDP发送间隔', default=0.2,
    #     help_text='仅当选择UDP模式时生效,单位:秒'
    # )

    # 推流设置
    RESOLUTION = [
        ('1920x1080', '1920x1080'),
        ('1280x720', '1280x720'),
        ('640x480', '640x480'),
    ]
    STREAM_PROTOCOLS = [
        ('rtmp', 'RTMP'),
        ('hls', 'HLS'),
        ('webrtc', 'WebRTC')
    ]
    resolution = models.CharField(
        '分辨率', max_length=20,
        choices = RESOLUTION, default='1920x1080', 
    )
    framerate = models.PositiveIntegerField(
        '帧率 (FPS)', default=30,
        validators=[MaxValueValidator(60)]
    )
    stream_protocol = models.CharField(
        '推流协议', max_length=10,
        choices=STREAM_PROTOCOLS, default='rtmp'
    )
    stream_bitrate = models.CharField(
        '视频码率', max_length=20,
        default='4000k', help_text='例如: 2000k, 4M'
    )
    
    # 系统级配置
    auto_start = models.BooleanField(
        '开机自启', default=True,
        help_text='系统启动时自动运行摄像头服务'
    )
    log_level = models.CharField(
        '日志级别', max_length=10,
        choices=[('debug', 'DEBUG'), ('info', 'INFO'), ('error', 'ERROR')],
        default='info'
    )

    # 摄像头曝光设置
    auto_exposure = models.BooleanField(
        '自动曝光', default=True,
        help_text='启用自动曝光后将隐藏手动曝光调节'
    )
    exposure_value = models.PositiveIntegerField(
        '曝光值', default=50,
        validators=[MaxValueValidator(100)]
    )

    class Meta:
        verbose_name = "摄像头全局配置"
        verbose_name_plural = "摄像头全局配置"

    def __str__(self):
        return "摄像头参数配置"