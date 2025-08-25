# opencv-cross.cmake

# 目标架构
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 基础配置
set(CMAKE_BUILD_TYPE Release)
set(BUILD_ZLIB ON)
set(BUILD_opencv_world ON)  # 合并为单个库（根据需求调整）

# 安装路径（与项目其他库统一）
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../build/opencv" CACHE PATH "Installation Directory" FORCE)

# 依赖项配置
set(OPENCV_EXTRA_MODULES_PATH ${CMAKE_CURRENT_LIST_DIR}/../opencv_contrib-3.4.3/modules)
set(ZLIB_INCLUDE_DIR ./3rdparty/zlib)



