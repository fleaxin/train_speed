#ifndef SVP_H
#define SVP_H

#ifdef __cplusplus
extern "C" {
#endif
#include "yolov5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>

#include "ot_common_gdc.h"
#include "ot_common_vgs.h"
#include "ot_common_sys.h"
#include "ot_common_svp.h"
#include "ot_common_vb.h"
#include "svp_acl.h"
#include "ss_mpi_ive.h"
#include "svp_acl_mdl.h"




#define OBJDETECTMAX 265


  td_s32 framecpy(ot_svp_dst_img* dstf,ot_video_frame_info* srcf);
  td_s32 frame_cut(ot_svp_dst_img* dstf, ot_svp_dst_img* srcf, td_float starting_point);
  td_s32 create_usr_frame_yuv(ot_svp_img* img_algo, ot_vb_blk *vb_blk_yuv, td_s32 w, td_s32 h);
  td_s32 create_usr_frame_bgr(ot_svp_img* img_algo, ot_vb_blk *vb_blk_yuv, td_s32 w, td_s32 h);
  td_s32 vgsdraw(ot_video_frame_info *pframe, stYolov5Objs *pOut);
  td_s32 create_yuv_frame_info(ot_video_frame_info* frame_info, ot_vb_blk *vb_blk_yuv,td_s32 w, td_s32 h);
  td_void merge_detection_box(stYolov5Objs *pOut, td_float *starting_point, td_s32 cut_number, td_s32 src_width, td_s32 src_height);

  #ifdef __cplusplus
}
#endif

#endif