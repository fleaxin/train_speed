# CameraSettings/admin.py
from gettext import translation
from django.contrib import admin
from solo.admin import SingletonModelAdmin
from django.db import transaction

from .views import handle_config_update
from communication.signals import apply_hardware_settings
from .models import CameraConfig
from django.utils.html import format_html
from django.contrib import messages
from .utils import check_config_consistency
from communication.utils import logger



admin.site.site_header = '管理后台'  # 设置header
admin.site.site_title = '管理后台'   # 设置title
admin.site.index_title = '管理后台'
@admin.register(CameraConfig)
class CameraConfigAdmin(SingletonModelAdmin):
    # 字段分组布局
    fieldsets = (
        ('控制命令设置',{
            'fields':(
                'COMMAND_TCP_PORT',
            ),
        }),
        ('数据推送设置', {
            'fields': ('data_mode', 
                       'http_server_host', 
                       'http_server_port', 
                       'http_client_url', 
                       'http_client_rate',
            ),
        }),
        # ('推流配置', {
        #     'fields': (
        #         'resolution', 
        #         'framerate',
        #         'stream_protocol',
        #         'stream_bitrate',
        #     ),
        #     'classes': ('grp-collapse grp-open',)  # 展开式分组
        # }),
        # ('系统设置', {
        #     'fields': (
        #         'auto_start',
        #         'log_level',
        #     ),
        #     'classes': ('grp-collapse',)  # 折叠式分组
        # }),
    )

    # 禁止新增操作
    def has_add_permission(self, request):
        return False

    # 配置保存后的操作
    def save_model(self, request, obj, form, change):
        super().save_model(request, obj, form, change)
        # 调用配置生效逻辑
        
        try:
            # 重启获取
            transaction.on_commit(lambda: handle_config_update(None, obj)) # 使用 transaction.on_commit 确保在事务提交后执行
        except Exception as e:
            self.message_user(request, f"配置应用失败: {str(e)}", level='ERROR')
        
        # try:
        #     # 调用
        #     apply_hardware_settings(obj)  
        #     self.message_user(request, "配置已成功应用到硬件", level='SUCCESS')
        # except Exception as e:
        #     self.message_user(request, f"配置应用失败: {str(e)}", level='ERROR')
    
    
    # 检查配置一致性,如果数据库中数据与JSON配置不一致，则显示警告消息
    def change_view(self, request, object_id=None, form_url='', extra_context=None):
        # try:
        #     is_consistent, mismatches = check_config_consistency()
        #     if not is_consistent:
        #         mismatch_msg = ", ".join([f"{item['field']}: DB={item['database_value']} vs JSON={item['json_value']}" for item in mismatches])
        #         messages.warning(request, f"⚠️ 数据库与 JSON 不一致：{mismatch_msg}")
        #     else:
        #         messages.success(request, "✅ 数据库与 JSON 配置一致")
        # except Exception as e:
        #     messages.error(request, f"❌ 检查配置一致性失败: {str(e)}")
        
        
        # 强制刷新单例配置
        try:
            config = CameraConfig.objects.get()
            config.refresh_from_db()
        except CameraConfig.DoesNotExist:
            pass

        return super().change_view(request, object_id, form_url, extra_context)
