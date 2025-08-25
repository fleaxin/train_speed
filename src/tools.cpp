#include <opencv2/opencv.hpp>
#include <cstdint>
#include <cstring>
#include "tools.h"

extern "C" {

void saveYuvAsJpeg(uint8_t* yuvData, int width, int height, const char* filename, const char* savePath) {
    // 假设是 NV12 格式（YUV420SP）
    cv::Mat yuv(height * 3 / 2, width, CV_8UC1, yuvData);
    cv::Mat bgr;

    // 转换 YUV -> BGR
    cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_NV21);

    // 构建完整的文件路径
    std::string fullPath(savePath);
    fullPath += "/";
    fullPath += filename;
    std::cout<<"保存图片："<<fullPath<<std::endl;
    // 保存为 JPEG
    std::vector<int> params;
    params.push_back(cv::IMWRITE_JPEG_QUALITY);
    params.push_back(95); // 设置 JPEG 质量 (0~100)

    if(cv::imwrite(fullPath, bgr, params) != true) {
        printf("保存图片失败！\n");
    }
}

void saveBgrAsJpeg(uint8_t* bgrData, int width, int height, const char* filename, const char* savePath) {
    // 创建 BGR 格式的 cv::Mat
    cv::Mat bgr(height, width, CV_8UC3, bgrData);

    // 构建完整文件路径
    std::string fullPath = std::string(savePath) + "/" + std::string(filename);

    // 设置 JPEG 质量参数
    std::vector<int> params;
    params.push_back(cv::IMWRITE_JPEG_QUALITY);
    params.push_back(95);  // 质量范围：0~100

    // 保存为 JPEG
    bool success = cv::imwrite(fullPath, bgr, params);
    if (!success) {
        // 保存失败，可能路径无效或权限不足
        std::cerr << "Failed to save image to " << fullPath << std::endl;
    }
}

}