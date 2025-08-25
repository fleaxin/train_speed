import json
import socket
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.core.exceptions import ObjectDoesNotExist
from CameraSettings.models import CameraConfig
from django.contrib import admin
from django.shortcuts import render
from django.contrib.admin.views.decorators import staff_member_required


@staff_member_required
def video_view(request):
    """视频播放页面视图"""
    context = admin.site.each_context(request)
    return render(request, 'video/video.html', context=context)

@csrf_exempt
def update_camera_config(request):
    if request.method == 'POST':
        try:
            config = CameraConfig.get_solo()
            data = json.loads(request.body)

            # 更新字段
            if 'auto_exposure' in data:
                config.auto_exposure = data['auto_exposure']
            if 'exposure_value' in data:
                config.exposure_value = data['exposure_value']

            config.save()

            # 调用外部模块函数应用摄像头参数
            

            return JsonResponse({'status': 'success'})
        except Exception as e:
            return JsonResponse({'status': 'error', 'message': str(e)})
    return JsonResponse({'status': 'error', 'message': '无效请求'})

def get_camera_config(request):
    config = CameraConfig.get_solo()
    return JsonResponse({
        'auto_exposure': config.auto_exposure,
        'exposure_value': config.exposure_value,
    })



def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # 不需要真正连接
        s.connect(('10.255.255.255', 1))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip

def get_stream_url(request):
    stream_host = get_local_ip()  # 动态获取本机 IP
    stream_port = "8080"  # 可以根据实际配置从 settings 或环境变量中读取

    return JsonResponse({
        'stream_url': f'http://{stream_host}:{stream_port}/live/test.live.flv'
    })