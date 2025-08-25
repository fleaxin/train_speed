import json
import os
from django.conf import settings


def get_model_fields_dict(instance):
    """
    动态获取模型对象的所有字段和值，返回字典
    """
    field_dict = {}
    for field in instance._meta.fields:
        field_name = field.name
        field_value = getattr(instance, field_name)
        # 转换布尔值为标准格式（避免 'True' vs True）
        if isinstance(field_value, bool):
            field_value = bool(field_value)
        field_dict[field_name] = field_value
    return field_dict


# utils.py

def check_config_consistency():
    """
    检查数据库配置与 JSON 文件是否一致（以 JSON 字段为准）
    返回: (is_consistent: bool, mismatched_fields: list)
    """
    from .models import CameraConfig

    # 获取数据库配置对象
    config_obj = CameraConfig.get_solo()

    # JSON 文件路径
    file_path = os.path.join(settings.MEDIA_ROOT, 'camera_config.json')

    # 检查文件是否存在
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"配置文件不存在: {file_path}")

    # 读取 JSON 文件
    with open(file_path, 'r') as f:
        try:
            json_config = json.load(f)
        except json.JSONDecodeError:
            raise ValueError("JSON 文件格式错误")

    # 获取数据库字段字典（用于快速访问）
    db_config = get_model_fields_dict(config_obj)

    # 比较字段
    mismatched_fields = []
    for key in json_config:
        db_val = db_config.get(key)
        json_val = json_config.get(key)

        # 对布尔值统一处理
        if isinstance(db_val, bool):
            json_val = bool(json_val)
        # 对整数字段做类型适配（如 framerate）
        elif isinstance(db_val, int):
            try:
                json_val = int(json_val)
            except (TypeError, ValueError):
                pass  # 如果不是数字则跳过转换

        if str(db_val) != str(json_val):
            mismatched_fields.append({
                'field': key,
                'database_value': db_val,
                'json_value': json_val
            })

    is_consistent = len(mismatched_fields) == 0
    return is_consistent, mismatched_fields