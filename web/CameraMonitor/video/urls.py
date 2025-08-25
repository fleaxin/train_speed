# video/urls.py
from django.urls import path
from . import views

urlpatterns = [
    # 曝光控制相关接口
    path('api/camera_config/', views.get_camera_config, name='get_camera_config'),
    path('api/update_camera_config/', views.update_camera_config, name='update_camera_config'),
    path('api/stream-url/', views.get_stream_url, name='get_stream_url'),
]