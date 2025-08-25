#ifndef TOOLS
#define TOOLS

#include <stdint.h>  // 添加缺失的类型定义

#ifdef __cplusplus
extern "C" {
#endif

void saveYuvAsJpeg(uint8_t* yuvData, int width, int height, const char* filename, const char* savePath);
void saveBgrAsJpeg(uint8_t* bgrData, int width, int height, const char* filename, const char* savePath);

#ifdef __cplusplus
}
#endif

#endif