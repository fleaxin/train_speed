#ifndef YOLOV5_H
#define YOLOV5_H


#ifdef __cplusplus
extern "C" {
#endif
#include "hi_type.h"

#define OBJDETECTMAX 265

typedef struct {
  hi_s32 x;// 左上点x
  hi_s32 y;// 左上点y
  hi_s32 w;
  hi_s32 h;
  hi_s32 center_x;
  hi_s32 center_y;
  hi_float x_f;// 左上点x
  hi_float y_f;// 左上点y
  hi_float w_f;
  hi_float h_f;
  hi_float center_x_f;
  hi_float center_y_f;
  hi_s32 class_id;
  hi_float score;
} stObjinfo;

typedef struct {
  hi_s32 count;
  stObjinfo objs[OBJDETECTMAX];
} stYolov5Objs;



void yolo_detect(const float *src, unsigned int len, stYolov5Objs* pOut);
void yolo_detect1(const float *src, unsigned int len, stYolov5Objs* pOut);
void yolo_detect2(const hi_float *src, hi_u32 stride_offset, hi_s32 total_valid_num, stYolov5Objs *p_out);
void yolo_detect_train(const hi_float *src, hi_u32 len, stYolov5Objs *p_out);
void nms_process_in_place_and_correct_boxes(stYolov5Objs* p_out);
void nms_process_and_filter(stYolov5Objs *p_out);


#ifdef __cplusplus
}
#endif

#endif // YOLOV5_H