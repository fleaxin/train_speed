#ifndef TRAIN_SPEED
#define TRAIN_SPEED

#include "hi_type.h"
#include "hi_common.h"
#include "ot_common.h"
#include "ot_type.h"
#include "svp.h"
#include "camera_test.h"


#ifdef __cplusplus
extern "C" {
#endif


struct SegResultDetection {
    td_float bbox[4];  // center_x center_y w h
    td_float conf;  // bbox_conf * cls_conf
    td_float class_id;
    td_float mask[32];
};

struct frame_size{
    td_s32 w;
    td_s32 h;
};

struct frame_size_f{
    float w;
    float h;
};

struct Rect {
    td_s32 x;// 左上X值
    td_s32 y;// 左上Y值
    td_s32 width;
    td_s32 height;
    td_s32 center_x;
    td_s32 center_y;
};

td_s32 train_speed_load_carriage_info();
td_s32 train_speed_load_carriage_type(SetModle *set_modle);
td_s32 train_speed_reset_data();
td_s32 train_speed(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data);
td_s32 train_speed_test(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data);
td_s32 train_speed_test_match(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data);
td_s32 train_speed_match_new_count_test(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data);
td_s32 train_speed_origin(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data);

#ifdef __cplusplus
}
#endif

#endif // TRAIN_SPEED