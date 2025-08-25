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
#include "sys/time.h"

#include "ss_mpi_vgs.h"
#include "ss_mpi_ive.h"
#include "ot_common_vgs.h"
#include "sample_comm.h"
#include "svp.h"
#include "frame_process.h"
#include "ot_common_svp.h"
#include "svp_acl.h"
#include "yolov5.h"
#include "hi_type.h"
#include "tools.h"
  

td_s32 create_yuv_frame_info(ot_video_frame_info *frame_info, ot_vb_blk *vb_blk_yuv, td_s32 w, td_s32 h)
{
    td_s32 vbsize = w * h * 3 / 2;
    *vb_blk_yuv = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, vbsize, TD_NULL);
    if(*vb_blk_yuv == OT_VB_INVALID_HANDLE){
        LOG_ERROR("ss_mpi_vb_get_blk fail");
    }
    frame_info->pool_id = ss_mpi_vb_handle_to_pool_id(*vb_blk_yuv);
    frame_info->mod_id = OT_ID_VI;
    frame_info->video_frame.width = w;
    frame_info->video_frame.height = h;
    frame_info->video_frame.stride[0] = w;
    frame_info->video_frame.stride[1] = w;
    frame_info->video_frame.field = OT_VIDEO_FIELD_FRAME;
    frame_info->video_frame.pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    frame_info->video_frame.video_format = OT_VIDEO_FORMAT_LINEAR;
    frame_info->video_frame.compress_mode = OT_COMPRESS_MODE_NONE;
    frame_info->video_frame.dynamic_range = OT_DYNAMIC_RANGE_SDR8;
    frame_info->video_frame.color_gamut = OT_COLOR_GAMUT_BT709;
    frame_info->video_frame.phys_addr[0] = ss_mpi_vb_handle_to_phys_addr(*vb_blk_yuv);
    if(frame_info->video_frame.phys_addr[0] == 0){
        LOG_ERROR("ss_mpi_vb_handle_to_phys_addr fail");
    }
    frame_info->video_frame.phys_addr[1] = frame_info->video_frame.phys_addr[0] + w * h;
    frame_info->video_frame.virt_addr[0] = (td_u8*)ss_mpi_sys_mmap(frame_info->video_frame.phys_addr[0], vbsize);
    frame_info->video_frame.virt_addr[1] = frame_info->video_frame.virt_addr[0] + w * h;
    frame_info->video_frame.frame_flag = 0;// 如果送给VPSS的帧是用户自己构建，不是从某个模块获取而来，需要将ot_video_frame中的frame_flag初始化为0。
}
td_s32 create_usr_frame_yuv(ot_svp_img* img_algo, ot_vb_blk *vb_blk_yuv, td_s32 w, td_s32 h)
{
    td_s32 vbsize = w * h * 3 / 2;
    *vb_blk_yuv = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, vbsize, TD_NULL);
    img_algo->phys_addr[0] = ss_mpi_vb_handle_to_phys_addr(*vb_blk_yuv);
    img_algo->phys_addr[1] = img_algo->phys_addr[0] + w * h;
    img_algo->virt_addr[0] = (td_u8*)ss_mpi_sys_mmap(img_algo->phys_addr[0], vbsize);
    img_algo->virt_addr[1] = img_algo->virt_addr[0] + w * h;
    img_algo->stride[0] = w;
    img_algo->stride[1] = w;
    img_algo->width = w;
    img_algo->height = h;
    img_algo->type = OT_SVP_IMG_TYPE_YUV420SP;
}
td_s32 create_usr_frame_bgr(ot_svp_img* img_algo, ot_vb_blk *vb_blk_yuv,td_s32 w, td_s32 h)
{
    td_s32 vbsize = w * h * 3;
    *vb_blk_yuv = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, vbsize, TD_NULL);
    img_algo->phys_addr[0] = ss_mpi_vb_handle_to_phys_addr(*vb_blk_yuv);
    img_algo->virt_addr[0] = (td_u8*)ss_mpi_sys_mmap(img_algo->phys_addr[0], vbsize);
    img_algo->stride[0] = w;
    img_algo->width = w;
    img_algo->height = h;
    img_algo->type = OT_SVP_IMG_TYPE_U8C3_PACKAGE;
}
td_s32 framecpy(ot_svp_dst_img* dstf,ot_video_frame_info* srcf) {
    td_s32 ret = OT_ERR_IVE_NULL_PTR;
    ot_ive_handle handle;
    ot_svp_src_data src_data;
    ot_svp_dst_data dst_data;
    ot_ive_dma_ctrl ctrl = { OT_IVE_DMA_MODE_DIRECT_COPY, 0, 0, 0, 0 };
    ot_ive_dma_ctrl ctrl_set = { OT_IVE_DMA_MODE_SET_8BYTE, 0x00, 0, 0, 0 };
    td_bool is_finish = TD_FALSE;
    td_bool is_block = TD_TRUE;

    td_s32 dst_w = dstf->width;
    td_s32 dst_h = dstf->height;
    td_s32 src_w = srcf->video_frame.width;
    td_s32 src_h = srcf->video_frame.height;
    // printf("dst_w: %d, dst_h: %d, src_w: %d, src_h: %d\n", dst_w, dst_h, src_w, src_h);
    // 拷贝 Y  
    src_data.phys_addr = srcf->video_frame.phys_addr[0];
    src_data.width = src_w;   // w;
    src_data.height = src_h;  // h;
    src_data.stride = dstf->stride[0];

    dst_data.phys_addr = dstf->phys_addr[0];
    dst_data.width = dst_w;   // w;
    dst_data.height = src_h;  // 由于需要填充，所以拷贝的时候需要设置高度为src_h，否则会拷贝到下一行
    dst_data.stride = dstf->stride[0];

    ret = ss_mpi_ive_dma(&handle, &src_data, &dst_data, &ctrl, TD_TRUE);
    if (ret != TD_SUCCESS) {
        err_print("ss_mpi_ive_dma fail, ret:0x%x\n", ret);
        LOG_ERROR("ss_mpi_ive_dma fail, ret:0x%x", ret);
        return TD_FAILURE;
    }
    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }
    
    // 填充 Y
    dst_data.phys_addr = dstf->phys_addr[0] + src_w * src_h;
    dst_data.width = dst_w;   // w;
    dst_data.height = dst_h - src_h;  // 由于需要填充，所以拷贝的时候需要设置高度为src_h，否则会拷贝到下一行
    dst_data.stride = dstf->stride[0];

    ret = ss_mpi_ive_dma(&handle, &dst_data, &dst_data, &ctrl_set, TD_TRUE);
    if (ret != TD_SUCCESS) {
        err_print("ss_mpi_ive_dma fail, ret:0x%x\n", ret);
        LOG_ERROR("ss_mpi_ive_dma fail, ret:0x%x", ret);
        return TD_FAILURE;
    }
    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }

    // 拷贝 UV
    src_data.phys_addr = srcf->video_frame.phys_addr[1];
    src_data.width = src_w;   // w;
    src_data.height = src_h / 2;  // h;
    src_data.stride = dstf->stride[0];

    dst_data.phys_addr = dstf->phys_addr[1];
    dst_data.width = dst_w;   // w;
    dst_data.height = src_h / 2;  // 由于需要填充，所以拷贝的时候需要设置高度为src_h，否则会拷贝到下一行
    dst_data.stride = dstf->stride[0];;
    ret = ss_mpi_ive_dma(&handle, &src_data, &dst_data, &ctrl, TD_TRUE);
    if (ret != TD_SUCCESS) {
        err_print("ss_mpi_ive_dma fail, ret:0x%x\n", ret);
        LOG_ERROR("ss_mpi_ive_dma fail, ret:0x%x", ret);
        return TD_FAILURE;
    }
    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }
    
    // 填充 UV
    dst_data.phys_addr = dstf->phys_addr[1] + (src_w * src_h)/2;
    dst_data.width = dst_w;   // w;
    dst_data.height = (dst_h - src_h)/2;  // 
    dst_data.stride = dstf->stride[0];

    ret = ss_mpi_ive_dma(&handle, &dst_data, &dst_data, &ctrl_set, TD_TRUE);
    if (ret != TD_SUCCESS) {
        err_print("ss_mpi_ive_dma fail, ret:0x%x\n", ret);
        LOG_ERROR("ss_mpi_ive_dma fail, ret:0x%x", ret);
        return TD_FAILURE;
    }
    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }



    // td_void *vir_addr = (td_u8 *)ss_mpi_sys_mmap(dstf->phys_addr[0], dst_w * dst_h * 3 / 2);
    // printf("data size: %d\n",dst_w * dst_h * 3 / 2);
    // td_u8* ptr_tmp1 = (td_u8 *)vir_addr;
    // FILE *file = fopen("img_yuv_640*640.yuv", "w");
    // size_t data_length = dst_w * dst_h * 3 / 2;
    // size_t written = fwrite(ptr_tmp1, sizeof(td_u8), data_length, file);
    // if (written < data_length) {
    //     fprintf(stderr, "\n\n\nFile write error\n\n\n");
    // }
    // fclose(file);
    // chmod("img_yuv_640*640.yuv", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    // printf("write yuv file success\n");

    return TD_SUCCESS;
}

// 使用IVE DMA操作，直接从BGR图像内存块中扣取出指定起点、宽高的数据，实现截取图像
td_s32 frame_cut(ot_svp_dst_img* dstf, ot_svp_dst_img* srcf, td_float starting_point) {
    td_s32 ret;
    ot_ive_handle handle;
    ot_svp_src_data src_data;
    ot_svp_dst_data dst_data;
    ot_ive_dma_ctrl ctrl = { OT_IVE_DMA_MODE_DIRECT_COPY, 0, 0, 0, 0 };
    td_bool is_finish = TD_FALSE;
    td_bool is_block = TD_TRUE;
    
    // 设置src 数据的宽高为要截取的宽高
    td_s32 src_w = dstf->width;
    td_s32 src_h = dstf->height;
    td_s32 dst_w = dstf->width;
    td_s32 dst_h = dstf->height;
    
    // 设置src 源数据的物理地址为要截取的物理地址起点
    td_s32 offset = starting_point * srcf->width * 3;
    src_data.phys_addr = srcf->phys_addr[0] + offset;

    // 乘以3是因为图像格式为BGR_PACKEG
    src_data.width = src_w * 3;   // 设置为要截取的宽
    src_data.height = src_h * 3;  // 设置为要截取的高
    src_data.stride = srcf->stride[0] * 3;// 需要设置为源数据的stride

    dst_data.phys_addr = dstf->phys_addr[0];
    dst_data.width = dst_w * 3;   // w;
    dst_data.height = dst_h * 3;  // h;
    dst_data.stride = dstf->stride[0] * 3;

    ret = ss_mpi_ive_dma(&handle, &src_data, &dst_data, &ctrl, TD_TRUE);
    if (ret != TD_SUCCESS) {
        LOG_ERROR("ss_mpi_ive_dma fail, ret:0x%x", ret);
        return TD_FAILURE;
    }
    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }

    // td_void *vir_addr = (td_u8 *)ss_mpi_sys_mmap(dst_data.phys_addr, dst_w * dst_h * 3);
    // td_u8* ptr_tmp1 = (td_u8 *)vir_addr;
    // FILE *file = fopen("img_bgr_640*640.bgr", "w");
    // size_t data_length = dst_w * dst_h * 3;
    // size_t written = fwrite(ptr_tmp1, sizeof(td_u8), data_length, file);
    // if (written < data_length) {
    //     fprintf(stderr, "\n\n\nFile write error\n\n\n");
    // }
    // fclose(file);
    // chmod("img_bgr_640*640.bgr", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    // printf("write yuv file success\n");

    // saveBgrAsJpeg(ptr_tmp1, 640, 640,"img_bgr_640*640.jpg","./");

    return TD_SUCCESS;
}

// 合并图像裁切分段后的检测框
td_void merge_detection_box(stYolov5Objs *pOut, td_float *starting_point,td_s32 cut_number, td_s32 src_width, td_s32 src_height) {
    td_float rate_w = 640.0 / src_width;
    td_float rate_h = 640.0 / src_height;
    for(td_s32 i = 0; i < cut_number; i++){
        stYolov5Objs pOut_tmp = pOut[i];
        for(td_s32 j = 0; j < pOut_tmp.count; j++){
            if(pOut_tmp.objs[j].x < 10 || pOut_tmp.objs[j].x + pOut_tmp.objs[j].w > 630){// 过滤掉两边靠近图像边缘的检测框
                continue;
            }
            stObjinfo *obj = &pOut[cut_number].objs[pOut[cut_number].count];
            td_float tmp = starting_point[i];
            *obj = pOut[i].objs[j];
            obj->x = (obj->x + starting_point[i] * 1280) * rate_w;
            obj->y = obj->y * rate_h;
            obj->w = obj->w * rate_w;
            obj->h = obj->h * rate_h;
            obj->x_f = (obj->x_f + starting_point[i] * 1280) * rate_w;
            obj->y_f = obj->y_f * rate_h;
            obj->w_f = obj->w_f * rate_w;
            obj->h_f = obj->h_f * rate_h;
            obj->center_x = (obj->center_x + starting_point[i] * 1280) * rate_w;
            obj->center_y = obj->center_y * rate_h;
            obj->center_x_f = (obj->center_x_f + starting_point[i] * 1280) * rate_w;
            obj->center_y_f = obj->center_y_f * rate_h;
            pOut[cut_number].count++;
        }
    }
    nms_process_in_place_and_correct_boxes(&pOut[cut_number]);
}
hi_s32 vgsdraw(ot_video_frame_info *pframe, stYolov5Objs *pOut)
{
    hi_s32 ret;

    td_u32 width;
    td_u32 height;
    width = pframe->video_frame.width;
    height = pframe->video_frame.height;

    ot_vgs_handle h_handle = -1;
    ot_vgs_task_attr vgs_task_attr = {0};
    static ot_vgs_line stLines[OBJDETECTMAX];
    hi_float ratio = (td_float)width / 640;

    hi_s32 thick = (hi_s32)(2 * ratio) & 0xFFFE; // 线条宽度

    hi_u32 color;
    hi_u32 color_lib[8] = {
        0xFF0000, // 0-红色
        0x00FF00, // 1-绿色
        0x0000FF, // 2-蓝色
        0xFFFF00, // 3-黄色
        0x00FFFF, // 4-青色
        0xFF00FF, // 5-洋红
        0xFF8500, // 6-橙色
        0x800080  // 7-紫色
    };

    hi_s32 linecount = 0;
    if (pOut && pOut->count > 0)
    {
        for (hi_s32 i = 0; i < pOut->count; i++)
        {
            color = color_lib[pOut->objs[i].class_id];
            hi_float xs1 = pOut->objs[i].x_f>0? pOut->objs[i].x_f : 0;
            hi_float ys1 = pOut->objs[i].y_f>0? pOut->objs[i].y_f : 0;
            hi_float xe1 = pOut->objs[i].x_f + pOut->objs[i].w_f > width?  width  : pOut->objs[i].x_f + pOut->objs[i].w_f;
            hi_float ye1 = pOut->objs[i].y_f + pOut->objs[i].h_f > height? height : pOut->objs[i].y_f + pOut->objs[i].h_f;
            hi_float center_x1 = pOut->objs[i].center_x_f;
            hi_float center_y1 = pOut->objs[i].center_y_f;

            // 做二对齐
            hi_s32 xs = (hi_s32)(xs1 * ratio ) & 0xFFFE;
            hi_s32 ys = (hi_s32)(ys1 * ratio ) & 0xFFFE;
            hi_s32 xe = (hi_s32)(xe1 * ratio ) & 0xFFFE;
            hi_s32 ye = (hi_s32)(ye1 * ratio ) & 0xFFFE;
            hi_s32 center_x = (hi_s32)(center_x1 * ratio ) & 0xFFFE;
            hi_s32 center_y = (hi_s32)(center_y1 * ratio ) & 0xFFFE;
            
            // printf("xs:%d,ys:%d,xe:%d,ye:%d,center_x:%d,center_y:%d\n",xs,ys,xe,ye,center_x,center_y);
            
            
            if (pOut->objs[i].w < 0 || pOut->objs[i].w > pframe->video_frame.width)
            {
                continue;
            }
            if (pOut->objs[i].h < 0 || pOut->objs[i].h > pframe->video_frame.height)
            {
                continue;
            }

            // 上方的线条
            stLines[linecount].color = color;
            stLines[linecount].thick = thick;
            stLines[linecount].start_point.x = xs;
            stLines[linecount].start_point.y = ys;
            stLines[linecount].end_point.x = xe;
            stLines[linecount].end_point.y = ys;
            linecount++;

            // 左边的线条
            stLines[linecount].color = color;
            stLines[linecount].thick = thick;
            stLines[linecount].start_point.x = xs;
            stLines[linecount].start_point.y = ys;
            stLines[linecount].end_point.x = xs;
            stLines[linecount].end_point.y = ye;
            linecount++;

            // 右边线条
            stLines[linecount].color = color;
            stLines[linecount].thick = thick;
            stLines[linecount].start_point.x = xe;
            stLines[linecount].start_point.y = ys;
            stLines[linecount].end_point.x = xe;
            stLines[linecount].end_point.y = ye;
            linecount++;

            // 下边线条
            stLines[linecount].color = color;
            stLines[linecount].thick = thick;
            stLines[linecount].start_point.x = xs;
            stLines[linecount].start_point.y = ye;
            stLines[linecount].end_point.x = xe;
            stLines[linecount].end_point.y = ye;
            linecount++;

            // 中心点
            stLines[linecount].color = color;
            stLines[linecount].thick = thick;
            stLines[linecount].start_point.x = center_x - 2;
            stLines[linecount].start_point.y = center_y;
            stLines[linecount].end_point.x = center_x + 2;
            stLines[linecount].end_point.y = center_y;
            linecount++;
        }
    }

    // 画中线
    stLines[linecount].color = 0xFF0000;
    stLines[linecount].thick = thick;
    stLines[linecount].start_point.x = (width / 2) & 0xFFFE ;
    stLines[linecount].start_point.y = 0;
    stLines[linecount].end_point.x = (width / 2) & 0xFFFE;
    stLines[linecount].end_point.y = height;
    linecount++;



    ret = ss_mpi_vgs_begin_job(&h_handle);
    if (ret != HI_SUCCESS)
    {
        printf("hi_mpi_vgs_begin_job failed with %#x\n", ret);
        return HI_FAILURE;
    }
    if (memcpy_s(&vgs_task_attr.img_in, sizeof(ot_video_frame_info), pframe,
                 sizeof(ot_video_frame_info)) != EOK)
    {
        return HI_FAILURE;
    }
    if (memcpy_s(&vgs_task_attr.img_out, sizeof(ot_video_frame_info), pframe,
                 sizeof(ot_video_frame_info)) != EOK)
    {
        return HI_FAILURE;
    }


    if (pOut && (pOut->count > 0 || linecount))
    {
        ret = ss_mpi_vgs_add_draw_line_task(h_handle, &vgs_task_attr, stLines,
                                            linecount);
        if (ret != HI_SUCCESS)
        {
            err_print("ss_mpi_vgs_add_draw_line_task box failed with %#x\n", ret);
            LOG_ERROR("ss_mpi_vgs_add_draw_line_task box failed with %#x", ret);
            ss_mpi_vgs_cancel_job(h_handle);
            return HI_FAILURE;
        }
    }

    

    /* step5: start VGS work */
    ret = ss_mpi_vgs_end_job(h_handle);
    if (ret != HI_SUCCESS)
    {
        err_print("ss_mpi_vgs_end_job failed with %#x\n", ret);
        LOG_ERROR("ss_mpi_vgs_end_job failed with %#x", ret);
        ss_mpi_vgs_cancel_job(h_handle);
        return HI_FAILURE;
    }

    return ret;
}
