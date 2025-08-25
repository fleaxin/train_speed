#ifndef TRAIN_SPEED_ORIGIN
#define TRAIN_SPEED_ORIGIN


#ifdef __cplusplus
extern "C" {
#endif
#include "hi_type.h"
#include "hi_common.h"
#include "ot_common.h"
#include "ot_type.h"
#include "svp.h"
#include "camera_test.h"





#ifdef __cplusplus
}
#endif
td_s32 train_speed_origin(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data);
#endif // TRAIN_SPEED_ORIGIN