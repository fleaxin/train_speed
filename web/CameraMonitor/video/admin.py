from django.contrib import admin
from django.urls import path
from django.shortcuts import render
from .views import video_view

# def video_view(request):
#     context = admin.site.each_context(request)
#     return render(request, 'video/video.html', context=context)

# 保留原始get_urls方法
original_get_urls = admin.site.get_urls

# 创建新的URL配置方法
def video_get_urls():
    # 先添加自定义URL，再包含原有URL
    video_urls = [
        path('video/video/', admin.site.admin_view(video_view), name='video')
    ]
    return video_urls + original_get_urls()

# 替换get_urls方法
admin.site.get_urls = video_get_urls