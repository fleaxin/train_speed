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

#include "ss_mpi_vgs.h"
#include "ss_mpi_gdc.h"
#include "ot_common_gdc.h"
#include "ot_common_vgs.h"
#include "sample_comm.h"
#include "frame_process.h"
#include "sample_svp_npu_process.h"
#include "sample_svp_npu_model.h"
#include "sample_npu_process.h"
#include "sample_common_ive.h"

#include "train_speed.h"
#include "camera_test.h"
#include "tools.h"

typedef struct local_frame_info{
    td_s32 MAX_PATH;
    char* base_name;
    char file_path[256];
    FILE* yuv_files[200];
    td_s32 frame_count;
    td_s32 frame_width;
    td_s32 frame_height;
}local_frame_info;

td_s32 save_frame(ot_video_frame_info *frame_in, local_frame_info *local_frame_info)
{   
    td_s32 ret;
    td_s32 frame_width;
    td_s32 frame_height;
    td_s32 frame_size;
    char base_name[256];
    char file_path[256];
    const char *file_path_r = "./source_file/out_frame/";

    // 生成文件名
    snprintf(base_name, sizeof(base_name), local_frame_info->base_name, local_frame_info->frame_count-1);

    // 复制基础路径到 file_path
    snprintf(file_path, sizeof(file_path), "%s", file_path_r);

    // 将文件名追加到 file_path
    strncat(file_path, base_name, sizeof(file_path) - strlen(file_path) - 1);
    printf("file_path: %s\n", file_path);
    td_u64 blk_size = frame_in->video_frame.width * frame_in->video_frame.height * 3 / 2;
    td_u8* ptr_tmp1 = (td_u8*)ss_mpi_sys_mmap(frame_in->video_frame.phys_addr[0], blk_size);
    FILE *file = fopen(file_path, "w");
    size_t data_length = blk_size;
    size_t written = fwrite(ptr_tmp1, sizeof(td_u8), data_length, file);
    if (written < data_length) {
        fprintf(stderr, "\n\n\nFile write error\n\n\n");
    }
    fclose(file);
    chmod(file_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    ss_mpi_sys_munmap(ptr_tmp1, blk_size);
}
// 因为vdec直接绑定vi会产生跳帧的现象，所以手动冲vdec获取一帧 送入vi中
td_s32 get_vdec_frame_send_to_vi(){


    td_s32 ret;
    ot_video_frame_info frame_info;
    ot_vdec_supplement_info supplement;
    ret = ss_mpi_vdec_get_frame(0,&frame_info,&supplement,100);
    if(ret != TD_SUCCESS){
        LOG_ERROR("get frame from vdec failed, ret:0x%x", ret);
        goto release;
    }
    
    ret = ss_mpi_vi_send_pipe_yuv(4,&frame_info,100);
    if(ret != TD_SUCCESS){
        LOG_ERROR("send frame to vi failed, ret:0x%x", ret);
    }
release:
    ret = ss_mpi_vdec_release_frame(0,&frame_info);
    if(ret != TD_SUCCESS){
        LOG_ERROR("release vdec frame failed, ret:0x%x", ret);    
    }
    return ret;
}

td_void save_unnormal_frame(stYolov5Objs* pOut1, ot_video_frame_info* frame_info)
{
    td_bool save = TD_FALSE;
    // 如果mutex和edge靠的近或者mutex过于宽，说明有可能错误识别
    for(int i = 0; i < pOut1->count; i++){
        if(pOut1->objs[i].class_id == 4){// 存在mutex
            for(int j = 0; j < pOut1->count; j++){
                if(pOut1->objs[j].class_id == 0){// 如果同时存在edge和mutex
                    // 检查距离是否接近，是的话说明识别异常，保存图片
                    if(abs(pOut1->objs[i].center_x_f - pOut1->objs[j].center_x_f) < 100 ){
                        save = TD_TRUE;
                    }
                }
            }

            // 如果mutex的宽度超出正常值，也可能是错误识别，保存图片
            if(pOut1->objs[i].w_f > 50){

                save = TD_TRUE;
            }
        }
    }
    for(int i = 0; i < pOut1->count; i++){
        if(pOut1->objs[i].class_id == 3){// 存在handel
            // 检查和edge的距离，如果过于近，说明可能错误识别
            for(int j = 0; j < pOut1->count; j++){
                if(pOut1->objs[j].class_id == 0){// 如果同时存在edge和handel
                    // 检查距离是否接近，是的话说明识别异常，保存图片
                    if(abs(pOut1->objs[i].center_x_f - pOut1->objs[j].center_x_f) < 100){
                        save = TD_TRUE;
                    }
                }
            }
        
        }
    }
    for(int i = 0; i < pOut1->count; i++){
        if(pOut1->objs[i].class_id == 5){// 存在head
            // 检查是否存在handel或者mutexs，如果过于近，说明可能错误识别
            for(int j = 0; j < pOut1->count; j++){
                if(pOut1->objs[j].class_id == 3 || pOut1->objs[j].class_id == 4){// 存在handel或者mutex
                    if(abs(pOut1->objs[i].center_x_f - pOut1->objs[j].center_x_f) < 200){
                        save = TD_TRUE;
                    }
                }
            }
        
        }
    }
    if(save == TD_TRUE){
        td_void *vir_addr = (td_u8 *)ss_mpi_sys_mmap(frame_info->video_frame.phys_addr[0], 2448 * 1200 * 3 / 2);
        td_u8* ptr_tmp1 = (td_u8 *)vir_addr;
        char filename[256];
        snprintf(filename, sizeof(filename), "image_%04d.jpg", frame_info->video_frame.time_ref);
        saveYuvAsJpeg(ptr_tmp1,2448,1200,filename,"./image/");
        ss_mpi_sys_mmap(frame_info->video_frame.phys_addr[0], 2448*1200*3/2);
    } 
}
td_s32 get_local_yuv_frame(ot_video_frame_info *local_frame, local_frame_info *local_frame_info){

    td_s32 ret;
    td_s32 frame_width;
    td_s32 frame_height;
    td_s32 frame_size;
    char base_name[256];
    char file_path[256];

    // 生成文件名
    snprintf(base_name, sizeof(base_name), local_frame_info->base_name, local_frame_info->frame_count);

    // 复制基础路径到 file_path
    strncpy(file_path, local_frame_info->file_path, sizeof(file_path) - 1);
    file_path[sizeof(file_path) - 1] = '\0'; // 确保字符串以 null 结尾

    // 将文件名追加到 file_path
    strncat(file_path, base_name, sizeof(file_path) - strlen(file_path) - 1);

    // 尝试以二进制模式打开文件
    FILE* fp = fopen(file_path, "rb");
    if (fp == NULL) {
        printf("无法打开文件: %s (可能已读完所有帧)\n", file_path);
        local_frame_info->frame_count = 0;
        printf("重置rame_count");
        return TD_FAILURE;
    }

    printf("成功打开第 %d 帧: %s\n", local_frame_info->frame_count, file_path);
    
    frame_width = local_frame_info->frame_width;
    frame_height = local_frame_info->frame_height;
    frame_size = frame_width * frame_height * 3 / 2;
    
    td_u8 *buffer_ = (td_u8*)local_frame->video_frame.virt_addr[0];
    size_t size = fread(buffer_, sizeof(td_u8), frame_size, fp);
    if(size != frame_size){
        LOG_ERROR("get_local_yuv_frame: fread failed with error code %d", size);
        return TD_FAILURE;
    }
    fclose(fp);

    local_frame->video_frame.time_ref = local_frame_info->frame_count * 2;
    // 发送数据块到vpss模块中 阻塞式发送
    ret = ss_mpi_vpss_send_frame(0,local_frame, 100);
    if (ret != TD_SUCCESS) {
        err_print("ss_mpi_vpss_send_frame fail, ret:0x%x\n", ret);
        LOG_ERROR("ss_mpi_vpss_send_frame fail, ret:0x%x", ret);
    }
    local_frame_info->frame_count++;
    return TD_SUCCESS;
}
static td_s32 pmf_set_vb_s(ot_video_frame_info *img, ot_vb_blk *vb_out_blk, td_u64 *buf_size)
{
    td_u64 *out_phys_addr = img->video_frame.phys_addr;
    td_u8 **out_virt_addr = img->video_frame.virt_addr;
    td_char *mmz_name = TD_NULL;
    *vb_out_blk = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, *buf_size, mmz_name);

    if (*vb_out_blk == OT_VB_INVALID_HANDLE) {
        err_print("Info:mpi_vb_get_blk(size:%u) fail\n", *buf_size);
        LOG_ERROR("Info:mpi_vb_get_blk(size:%u) fail", *buf_size);
        return TD_FAILURE;
    }

    *out_phys_addr = ss_mpi_vb_handle_to_phys_addr(*vb_out_blk);
    if (*out_phys_addr == 0) {
        err_print("Info:mpi_vb_handle_to_phys_addr fail, u32OutPhyAddr:0x%llx\n", *out_phys_addr);
        LOG_ERROR("Info:mpi_vb_handle_to_phys_addr fail, u32OutPhyAddr:0x%llx", *out_phys_addr);
        ss_mpi_vb_release_blk(*vb_out_blk);
        return TD_FAILURE;
    }
    
    return TD_SUCCESS;
}
static td_s32 pmf_set_vb(ot_gdc_task_attr *task, ot_vb_blk *vb_out_blk, td_u64 *buf_size)
{
    td_u64 *out_phys_addr = task->img_out.video_frame.phys_addr;
    td_u8 **out_virt_addr = &task->img_out.video_frame.virt_addr;
    td_char *mmz_name = TD_NULL;
    *vb_out_blk = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, *buf_size, mmz_name);

    if (*vb_out_blk == OT_VB_INVALID_HANDLE) {
        err_print("Info:mpi_vb_get_blk(size:%u) fail\n", *buf_size);
        LOG_ERROR("Info:mpi_vb_get_blk(size:%u) fail", *buf_size);
        return TD_FAILURE;
    }

    *out_phys_addr = ss_mpi_vb_handle_to_phys_addr(*vb_out_blk);
    if (*out_phys_addr == 0) {
        err_print("Info:mpi_vb_handle_to_phys_addr fail, u32OutPhyAddr:0x%llx\n", *out_phys_addr);
        LOG_ERROR("Info:mpi_vb_handle_to_phys_addr fail, u32OutPhyAddr:0x%llx", *out_phys_addr);
        ss_mpi_vb_release_blk(*vb_out_blk);
        return TD_FAILURE;
    }
    
    return TD_SUCCESS;
}

static td_void pmf_set_task(ot_gdc_task_attr *task, const ot_vb_blk *vb_out_blk ,td_u32 out_width,td_u32 out_height)
{
    td_u32 out_stride;
    td_s32 ret;
    out_stride = OT_ALIGN_UP(out_width, 16);

    td_u64 out_phys_addr = *task->img_out.video_frame.phys_addr;
    td_u8 *out_virt_addr =  (td_u8*)task->img_out.video_frame.virt_addr[0];
    // printf("out_virt_addr:0x%llx\n", (td_u8*)task->img_out.video_frame.virt_addr[0]);
    // printf("out_virt_addr:0x%llx\n",out_virt_addr);
    ret = memcpy_s(&task->img_out, sizeof(ot_video_frame_info), &task->img_in, sizeof(ot_video_frame_info));
    if(ret != EOK){
        LOG_ERROR("memcpy_s failed with error code %d", ret);
        return TD_FAILURE;
    }
    task->img_out.pool_id = ss_mpi_vb_handle_to_pool_id(*vb_out_blk);
    task->img_out.video_frame.phys_addr[0] = out_phys_addr;
    task->img_out.video_frame.phys_addr[1] = out_phys_addr + out_stride * out_height;
    task->img_out.video_frame.virt_addr[0] = (td_void *)out_virt_addr;
    task->img_out.video_frame.virt_addr[1] = (td_void *)out_virt_addr + out_stride * out_height;
    task->img_out.video_frame.stride[0] = out_stride;
    task->img_out.video_frame.stride[1] = out_stride;
    task->img_out.video_frame.width = out_width;
    task->img_out.video_frame.height = out_height;
}

    td_s32 frame_apply_pmf(ot_gdc_task_attr *pmf_task, ot_vb_blk *vb_pmf_frame_out, td_s32 pmf_width,td_s32 pmf_height,ot_gdc_pmf_attr *gdc_pmf_attr){
        td_s32 ret;
        ot_gdc_handle pmf_handle;
        td_u64 blk_size = pmf_width * pmf_height * 3 / 2;
        // 分配缓存池给pmf输出图像 放在循环外初始化的时候分配内存会导致VGS画框闪烁，原因未知
        ret = pmf_set_vb(pmf_task, vb_pmf_frame_out, &blk_size);
        if (ret != TD_SUCCESS)
        {
            ss_mpi_gdc_cancel_job(pmf_handle);
            err_print("Err, pmf_set_vb filed ret:0x%x\n", ret);
            LOG_ERROR("Err, pmf_set_vb filed ret:0x%x", ret);
            return ret;
        }
        // goto release;
        // 启动PMF job
        ret = ss_mpi_gdc_begin_job(&pmf_handle);
        if (ret != TD_SUCCESS)
        {
            err_print("Err, start pmf job faill, ret:0x%x\n", ret);
            LOG_ERROR("Err, start pmf job faill, ret:0x%x", ret);
            return ret;
        }
        
        //设置task属性
        pmf_set_task(pmf_task, vb_pmf_frame_out,pmf_width,pmf_height);

        // 添加PMF task
        ret = ss_mpi_gdc_add_pmf_task(pmf_handle, pmf_task, gdc_pmf_attr);
        if (ret != TD_SUCCESS) {
            ss_mpi_gdc_cancel_job(pmf_handle);
            err_print("ss_mpi_gdc_add_pmf_task fail, ret:0x%x\n", ret);
            LOG_ERROR("ss_mpi_gdc_add_pmf_task fail, ret:0x%x", ret);
            return ret;
        }
        // 执行pmf job
        ret = ss_mpi_gdc_end_job(pmf_handle);
        if (ret != TD_SUCCESS) {
            ss_mpi_gdc_cancel_job(pmf_handle);
            err_print("ss_mpi_gdc_end_job fail, ret:0x%x\n", ret);
            LOG_ERROR("ss_mpi_gdc_end_job fail, ret:0x%x", ret);
            return ret;
        }

        return TD_SUCCESS;
    }
td_s32 image_scale(ot_video_frame_info *frame_in, ot_video_frame_info *frame_out, ot_vb_blk *vb_scale, td_s32 width, td_s32 height){
    
    td_s32 ret;

    // 给frame_out分配内存
    td_u64 blk_size = width*height*3/2;
    td_char *mmz_name = TD_NULL;
    td_phys_addr_t phy_addr;
    td_void *vir_addr;

    *vb_scale = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, blk_size, mmz_name);
    if (*vb_scale == OT_VB_INVALID_HANDLE) {
        err_print("Info:mpi_vb_get_blk(size:%u) fail\n", blk_size);
        LOG_ERROR("Info:mpi_vb_get_blk(size:%u) fail", blk_size);
        return TD_FAILURE;
    }

    phy_addr = ss_mpi_vb_handle_to_phys_addr(*vb_scale);
    if (phy_addr == 0) {
        err_print("Info:mpi_vb_handle_to_phys_addr fail, u32OutPhyAddr:0x%llx\n", phy_addr);
        LOG_ERROR("Info:mpi_vb_handle_to_phys_addr fail, u32OutPhyAddr:0x%llx", phy_addr);
        // ss_mpi_vb_release_blk(*vb_scale);
        return TD_FAILURE;
    }
    
    // 设置缩放后图像参数
    td_u32 stride;
    stride = OT_ALIGN_UP(width, 16);
    ret = memcpy_s(frame_out, sizeof(ot_video_frame_info), frame_in, sizeof(ot_video_frame_info));
    if(ret != EOK){
        LOG_ERROR("memcpy_s failed with error code %d", ret);
        return TD_FAILURE;
    }
    frame_out->pool_id = ss_mpi_vb_handle_to_pool_id(*vb_scale);
    frame_out->video_frame.phys_addr[0] = phy_addr;
    frame_out->video_frame.phys_addr[1] = phy_addr + stride * height;   
    frame_out->video_frame.stride[0] = stride;
    frame_out->video_frame.stride[1] = stride;
    frame_out->video_frame.width = width;
    frame_out->video_frame.height = height;
    
    
    // 创建缩放job
    ot_vgs_handle scale_handle;
    ret = ss_mpi_vgs_begin_job(&scale_handle);
    if (ret != HI_SUCCESS)
    {
        err_print("hi_mpi_vgs_begin_job failed with %#x\n", ret);
        LOG_ERROR("hi_mpi_vgs_begin_job failed with %#x", ret);
        ss_mpi_vgs_cancel_job(scale_handle);
        return HI_FAILURE;
    }

    
    ot_vgs_task_attr vgs_scale_task_attr;
    vgs_scale_task_attr.img_in = *frame_in;
    vgs_scale_task_attr.img_out = *frame_out;
    // 添加缩放任务
    ret = ss_mpi_vgs_add_scale_task(scale_handle, &vgs_scale_task_attr, OT_VGS_SCALE_COEF_NORM);
    if (ret != HI_SUCCESS)
    {
        err_print("ss_mpi_vgs_add_scale_task failed with %#x\n", ret);
        LOG_ERROR("ss_mpi_vgs_add_scale_task failed with %#x", ret);
        ss_mpi_vgs_cancel_job(scale_handle);
        return HI_FAILURE;
    }
    // 执行缩放job
    ret = ss_mpi_vgs_end_job(scale_handle);
    if (ret != HI_SUCCESS)
    {
        err_print("ss_mpi_vgs_end_job failed with %#x\n", ret);
        LOG_ERROR("ss_mpi_vgs_end_job failed with %#x", ret);
        ss_mpi_vgs_cancel_job(scale_handle);
        return HI_FAILURE;
    }
    

    // td_u8* ptr_tmp1 = (td_u8*)ss_mpi_sys_mmap(frame_in->video_frame.phys_addr[0], 2448*1200*3/2);
    // FILE *file = fopen("pmf_out.yuv", "w");
    // size_t data_length = frame_in->video_frame.width * frame_in->video_frame.height * 3 / 2;
    // printf("width: %d, height: %d, data_length: %zu\n",frame_in->video_frame.width, frame_in->video_frame.height, data_length);
    // size_t written = fwrite(ptr_tmp1, sizeof(td_u8), data_length, file);
    // if (written < data_length) {
    //     fprintf(stderr, "\n\n\nFile write error\n\n\n");
    // }
    // fclose(file);
    // chmod("pmf_out.yuv", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    // printf("save yuv file success\n");

    // td_u8* ptr_tmp1 = (td_u8*)ss_mpi_sys_mmap(frame_in->video_frame.phys_addr[0], 2448*1200*3/2);
    // char filename[256];
    // snprintf(filename, sizeof(filename), "image_%04d.jpg", frame_in->video_frame.time_ref/2);
    // saveYuvAsJpeg(ptr_tmp1,frame_in->video_frame.width,frame_in->video_frame.height,filename,"/root/ldc_pmf");    
    // ss_mpi_sys_mmap(frame_in->video_frame.phys_addr[0], 2448*1200*3/2);
    return HI_SUCCESS;

}




td_void *frame_process(td_void *arg)
{
    PthreadInf *inf = (PthreadInf*)arg;
    td_bool start_signal = inf->start_singal;
    SpeedData *speed_data = (SpeedData *)inf->data_struct;
    // 心跳信号
    td_slong *last_active_time = &inf->last_active_time;
    prctl(PR_SET_NAME, "Frame_process", 0, 0, 0);
    LOG_INFO("frame process thread started");
    
    // 获取配置文件中的配置信息
    const GlobalConfig* config = get_global_config();

    // 如果从本地读取测试图像，
    ot_vb_blk vb_local_frame;
    ot_video_frame_info local_frame;
    local_frame_info local_frame_info;
    
    local_frame_info.base_name = "%04d.yuv";
    local_frame_info.frame_count = config->start_frame;
    local_frame_info.MAX_PATH =256;
    local_frame_info.frame_width = config->camera.width;
    local_frame_info.frame_height = config->camera.height;
    if (config->local_frame_path != NULL) {
        strncpy(local_frame_info.file_path, config->local_frame_path, sizeof(local_frame_info.file_path) - 1);
        local_frame_info.file_path[sizeof(local_frame_info.file_path) - 1] = '\0'; // 确保字符串以null结尾
    }else{
        LOG_ERROR("local_frame_path is NULL");
    }
    printf("config->local_frame_path: %s\n",config->local_frame_path);
    printf("local_frame_info.file_path: %s\n", local_frame_info.file_path);
    if(config->local_frame_test==1){
        create_yuv_frame_info(&local_frame, &vb_local_frame, config->camera.width, config->camera.height);
    }
    td_s32  ret;
    // vpss相关参数
    ot_vpss_grp grp = 0;
    ot_vpss_chn chn = 2;// 
    ot_video_frame_info frame_info;// 
    td_s32 milli_sec;
    if(config->local_frame_test==1){
        milli_sec = 200;// 延迟时间拉长
    }else{
        milli_sec = 50;// 设置获取vpss图像超时时间  
    }

    // pmf相关参数
    ot_gdc_handle pmf_handle;
    ot_gdc_task_attr pmf_task; //创建pmf task
    ot_vb_blk vb_pmf_frame_out;// 创建pmf输出缓存
    td_u32 pmf_width = 2448;
    td_u32 pmf_height = 1200;
    td_u64 blk_size = pmf_width*pmf_height*3/2; //YUV格式的图像大小
    ot_video_frame_info pmf_out_frame;// pmf输出的图像
    
    // 设置pmf参数
    ot_gdc_pmf_attr gdc_pmf_attr;
    for (td_s32 i = 0; i < 9; i++) {
        gdc_pmf_attr.pmf_coef[i] = config->pmf_coef[i];// 从配置文件中读取pmf参数
    }

    // svp相关参数
    td_s32 yolo_width = 640;
    td_s32 yolo_height = 640;

    ot_vb_blk vb_scaled;// 创建原尺寸缩放后的缓存
    ot_video_frame_info frame_scaled;// 缩放后输出的图像
    ot_vb_blk vb_yolo_640_640;// 创建640x236->640x640 yolo输入缓存  
    ot_video_frame_info yolo_frame_640_640;// yolo输入的图像
    
    // 创建输出结果
    stYolov5Objs* pOut = (stYolov5Objs*)malloc(sizeof(stYolov5Objs));
    ot_vb_blk vb_yolo_yuv_out;
    ot_svp_img img_algo;

    create_usr_frame_yuv(&img_algo,&vb_yolo_yuv_out,yolo_width,yolo_height);
    
    // 创建svp任务，输入图像地址为img_algo.virt_addr[0]
    ret = svp_init(img_algo.virt_addr[0]);
    if(ret != TD_SUCCESS){
        err_print("Error, svp_init fail\n");
        LOG_ERROR("Error, svp_init fail");
    }

    // nnn初始化
    // if(!nnn_init()){
    //     printf("nnn_init failed\n");
    // };

    td_s32 fps = 0;
    td_slong prems = get_time_ms();
    td_slong start_time = get_time_ms();
    td_slong now_time;
    td_slong last_time;
    while (start_signal) {

        start_signal = inf->start_singal;
        *last_active_time = get_time_ms();
        td_slong start = get_time_ms();
        if (config->local_frame_test == 1)// 读取本地YUV图像
        {   
            // 从本地读取一帧YUV图像发送到vpss(0)
            ret = get_local_yuv_frame(&local_frame, &local_frame_info);
            if (ret != TD_SUCCESS)
            {
                err_print("Error, get_local_yuv_frame fail\n");
                LOG_ERROR("Error, get_local_yuv_frame fail");
                continue;
            }
            printf("get one local frame success\n");
        }

        // 从vpss获取一帧图像
        ret = ss_mpi_vpss_get_chn_frame(grp, chn, &pmf_task.img_in, milli_sec);
        if (ret != TD_SUCCESS)
        {
            err_print("ss_mpi_vpss_get_chn_frame:0x%x\n", ret);
            LOG_ERROR("ss_mpi_vpss_get_chn_frame:0x%x", ret);
            continue;
        }

        td_slong pmf_start = get_time_ms();
        /*------------------------对图像进行pmf透视矫正------------------------*/
        
        // 分配缓存池给pmf输出图像
        ret = pmf_set_vb(&pmf_task, &vb_pmf_frame_out, &blk_size);
        if (ret != TD_SUCCESS)
        {
            ss_mpi_gdc_cancel_job(pmf_handle);
            err_print("Err, pmf_set_vb filed ret:0x%x\n", ret);
            LOG_ERROR("Err, pmf_set_vb filed ret:0x%x", ret);
            continue;
        }
        // goto release;
        // 启动PMF job
        ret = ss_mpi_gdc_begin_job(&pmf_handle);
        if (ret != TD_SUCCESS)
        {
            err_print("Err, start pmf job faill, ret:0x%x\n", ret);
            LOG_ERROR("Err, start pmf job faill, ret:0x%x", ret);
            return TD_FALSE;
        }
        
        //设置task属性
        pmf_set_task(&pmf_task, &vb_pmf_frame_out,pmf_width,pmf_height);

        // 添加PMF task
        ret = ss_mpi_gdc_add_pmf_task(pmf_handle, &pmf_task, &gdc_pmf_attr);
        if (ret != TD_SUCCESS) {
            ss_mpi_gdc_cancel_job(pmf_handle);
            err_print("ss_mpi_gdc_add_pmf_task fail, ret:0x%x\n", ret);
            LOG_ERROR("ss_mpi_gdc_add_pmf_task fail, ret:0x%x", ret);
            ss_mpi_vpss_release_chn_frame(grp, chn, &frame_info);
            break;
        }
        // 执行pmf job
        ret = ss_mpi_gdc_end_job(pmf_handle);
        if (ret != TD_SUCCESS) {
            ss_mpi_gdc_cancel_job(pmf_handle);
            err_print("ss_mpi_gdc_end_job fail, ret:0x%x\n", ret);
            LOG_ERROR("ss_mpi_gdc_end_job fail, ret:0x%x", ret);
            ss_mpi_vpss_release_chn_frame(grp, chn, &frame_info);
            break;
        }

        td_slong pmf_end = get_time_ms();

        
        /*------------------------对pmf透视矫正后的图像进行缩放和填充------------------------*/

        // 对pmf矫正后的图像进行缩放
        ret = image_scale(&pmf_task.img_out, &frame_scaled, &vb_scaled, 640, 314);
        
        // 将缩放后640*236的图像复制并填充为640*640的图像
        framecpy(&img_algo, &frame_scaled);

        td_slong scale_end = get_time_ms();
        
        
        /*-------------------------------执行推理模型和画框---------------------------------*/
        // 模型输出计数器清零
        pOut->count = 0;
        // 运行模型执行推理
        svp_execute(img_algo.virt_addr[0],pOut);

        // 运行nnn模型执行推理
        // hi_s32 w = img_algo.width;
        // hi_s32 h = img_algo.height;
        // hi_s32 framelen = w * h * 3 /2;
        // nnn_execute(img_algo.virt_addr[0], framelen, pOut);

        td_slong execute_end = get_time_ms();



        // 计算速度
        train_speed_origin(pOut, &pmf_task.img_out, speed_data);
        td_slong caculate_end = get_time_ms();
      
        // 根据推理结果画框
        vgsdraw(&pmf_task.img_out, pOut);            
        td_slong draw_end = get_time_ms();


        if (config->local_frame_test == 1)
        {
            // save_frame(&pmf_task.img_out, &local_frame_info);
        }
        
        /*-------------------------------画框后的图像送至---------------------------------*/
        // 输出到VO-HDMI
        ret = ss_mpi_vo_send_frame(0, 1, &pmf_task.img_out, 0);
        if (ret != TD_SUCCESS) {
            err_print("ss_mpi_vo_send_frame fail, ret:0x%x\n", ret);
            LOG_ERROR("ss_mpi_vo_send_frame fail, ret:0x%x", ret);
        }

        // 发送到vpps group[1]
        // 再把码流输出到vpss grup[1]是为了方便编码输出，vpss直接绑定venc做编码较为简单
        ret = ss_mpi_vpss_send_frame(1, &pmf_task.img_out, 0);
        if (ret != TD_SUCCESS) {
            err_print("ss_mpi_vo_send_frame fail, ret:0x%x\n", ret);
            LOG_ERROR("ss_mpi_vo_send_frame fail, ret:0x%x", ret);
        }

        td_slong send_frame = get_time_ms();
        
        // // 计算速度
        // train_speed_origin(pOut, &pmf_task.img_out, speed_data);
        // td_slong caculate_end = get_time_ms();
release:
        // 释放资源
        ss_mpi_vb_release_blk(vb_scaled);
        ss_mpi_vb_release_blk(vb_pmf_frame_out);
        ss_mpi_vpss_release_chn_frame(grp, chn, &pmf_task.img_in);
        if (ret != TD_SUCCESS) {
            err_print("ss_mpi_vpss_release_chn_frame fail, ret:0x%x\n", ret);
            LOG_ERROR("ss_mpi_vpss_release_chn_frame fail, ret:0x%x", ret);
        }

        if (config->local_frame_test == 0)
        {
            printf("finish one frame\n");
            printf("pmf cost time:                  %d\n", pmf_end - pmf_start);
            printf("scale cost time:                %d\n", scale_end - pmf_end);
            printf("mode execute cost time:         %d\n", execute_end - scale_end);
            printf("caculate cost time:             %d\n", caculate_end - execute_end);
            printf("draw cost time:                 %d\n", draw_end - caculate_end);
            printf("send frame cost time:           %d\n", send_frame - draw_end);
            printf("process one frame cost time:    %d\n\n", get_time_ms() - start);
        }
        fps++;
        if (start - prems >= 1000)
        {
            prems = start;
            now_time = (prems - start_time) / 1000;
            printf("===========train speed========= vedio time:%d s     fps:%d\n", now_time, fps);
            fps = 0;
        }
    }

    
    // 去初始化svp 释放资源
    svp_uninit();
    ss_mpi_vb_release_blk(vb_yolo_yuv_out);
    if(config->local_frame_test==1){
        ss_mpi_vb_release_blk(vb_local_frame);
    }
    LOG_INFO("frame process thread exit");
    return TD_NULL;
}

static td_s32 frame_yuv2rgb_package(ot_svp_dst_img *dst_img,
    ot_video_frame_info* srcf) {
    td_s32 ret = OT_ERR_IVE_NULL_PTR;
    hi_ive_handle handle;
    ot_svp_src_img src_img;
    ot_ive_csc_ctrl csc_ctrl;
    csc_ctrl.mode = OT_IVE_CSC_MODE_PIC_BT709_YUV_TO_RGB;// vpss输出为BT709
    hi_bool is_finish = HI_FALSE;
    hi_bool is_block = HI_TRUE;
    
    src_img.width = srcf->video_frame.width;   // w;
    src_img.height = srcf->video_frame.height;  // h;
    
    src_img.phys_addr[0] = srcf->video_frame.phys_addr[0];
    src_img.phys_addr[1] = srcf->video_frame.phys_addr[1];
    src_img.stride[0] = srcf->video_frame.stride[0];
    src_img.stride[1] = srcf->video_frame.stride[1];
    src_img.type = OT_SVP_IMG_TYPE_YUV420SP;



    ret = ss_mpi_ive_csc(&handle, &src_img, dst_img, &csc_ctrl, HI_TRUE);
    if (ret != HI_SUCCESS) {
        LOG_ERROR("ss_mpi_ive_csc failed with %#x", ret);
        return TD_FALSE;
    }
    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }

    // td_void *vir_addr = (td_u8 *)ss_mpi_sys_mmap(dst_img->phys_addr[0], 1280 * 640 * 3);
    // td_u8 *ptr_tmp1 = (td_u8 *)vir_addr;
    // FILE *file = fopen("img_bgr_1280*640.bgr", "w");
    // size_t data_length = 1280 * 640 * 3;
    // size_t written = fwrite(ptr_tmp1, sizeof(td_u8), data_length, file);
    // if (written < data_length)
    // {
    //     fprintf(stderr, "\n\n\nFile write error\n\n\n");
    // }
    // fclose(file);
    // chmod("img_bgr_1280*640.bgr", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    // printf("write yuv file success\n");

    return TD_SUCCESS;
}

td_void *frame_process_cut(td_void *arg){
    PthreadInf *inf = (PthreadInf*)arg;
    td_bool start_signal = inf->start_singal;
    SpeedData *speed_data = (SpeedData *)inf->data_struct;
    ControlCommand *control_command = (ControlCommand *)inf->command_struct;
    // 心跳信号
    td_slong *last_active_time = &inf->last_active_time;
    prctl(PR_SET_NAME, "Frame_process", 0, 0, 0);
    LOG_INFO("frame process thread started");
    
    // 获取配置文件中的配置信息
    const GlobalConfig* config = get_global_config();

    // 如果从本地读取测试图像，
    ot_vb_blk vb_local_frame;
    ot_video_frame_info local_frame;
    local_frame_info local_frame_info;
    
    local_frame_info.base_name = "%04d.yuv";
    local_frame_info.frame_count = config->start_frame;
    local_frame_info.MAX_PATH =256;
    local_frame_info.frame_width = config->camera.width;
    local_frame_info.frame_height = config->camera.height;
    if (config->local_frame_path != NULL) {
        strncpy(local_frame_info.file_path, config->local_frame_path, sizeof(local_frame_info.file_path) - 1);
        local_frame_info.file_path[sizeof(local_frame_info.file_path) - 1] = '\0'; // 确保字符串以null结尾
    }else{
        LOG_ERROR("local_frame_path is NULL");
    }
    printf("config->local_frame_path: %s\n",config->local_frame_path);
    printf("local_frame_info.file_path: %s\n", local_frame_info.file_path);
    if(config->local_frame_test==1){
        create_yuv_frame_info(&local_frame, &vb_local_frame, config->camera.width, config->camera.height);
    }

    td_s32  ret;
    // vpss相关参数
    ot_vpss_grp grp = 0;
    ot_vpss_chn chn = 1;// 
    ot_video_frame_info frame_info;// 
    td_s32 milli_sec;
    if(config->local_frame_test!=0){// 本地视频或图像
        milli_sec = 200;// 增加延时，防止图像获取失败
    }else{// 摄像头
        milli_sec = 80;// 设置获取vpss图像超时时间  
    }

    // pmf相关参数
    ot_gdc_task_attr pmf_task; //创建pmf task
    ot_vb_blk vb_pmf_frame_out;// 创建pmf输出缓存
    td_u32 pmf_width = 2448;
    td_u32 pmf_height = 1200;
    // 设置pmf参数
    ot_gdc_pmf_attr gdc_pmf_attr;
    for (td_s32 i = 0; i < 9; i++) {
        gdc_pmf_attr.pmf_coef[i] = config->pmf_coef[i];// 从配置文件中读取pmf参数
    }

    // svp相关参数
    td_s32 yolo_width = 640;
    td_s32 yolo_height = 640;

    ot_vb_blk vb_scaled;// 创建原尺寸缩放后的缓存
    ot_video_frame_info frame_scaled;// 缩放后输出的图像

    ot_vb_blk vb_scale_bgr_out;
    ot_svp_img frame_scale_bgr;// 创建原尺寸缩放为1280*640后的BGR图像
    create_usr_frame_bgr(&frame_scale_bgr,&vb_scale_bgr_out,1280,640);
    

    // 创建输出结果 多个检测框的输出
    stYolov5Objs* pOut = (stYolov5Objs*)malloc(4 * sizeof(stYolov5Objs));
    ot_vb_blk vb_yolo_bgr_out;
    ot_svp_img img_algo;
    create_usr_frame_bgr(&img_algo,&vb_yolo_bgr_out,yolo_width,yolo_height);
    
    // 创建svp任务，输入图像地址为img_algo.virt_addr[0]
    ret = svp_init(img_algo.virt_addr[0]);
    if(ret != TD_SUCCESS){
        err_print("Error, svp_init fail\n");
        LOG_ERROR("Error, svp_init fail");
    }

    // nnn初始化
    // if(!nnn_init()){
    //     printf("nnn_init failed\n");
    // };


    // 读取车厢参数信息
    train_speed_load_carriage_info();

    
    td_s32 fps = 0;
    td_slong prems = get_time_ms();
    td_slong start_time = get_time_ms();
    td_slong now_time;
    td_slong last_time;
    while (start_signal) {

        start_signal = inf->start_singal;

        pthread_mutex_lock(&inf->lock);
        *last_active_time = get_time_ms();
        if(control_command->reset_data){
            train_speed_reset_data();
            control_command->reset_data = TD_FALSE;
            LOG_INFO("reset data");
        }
        if(control_command->set_model.set_model){
            train_speed_load_carriage_type(&control_command->set_model);
            control_command->set_model.set_model = TD_FALSE;
            LOG_INFO("set model");
        }
        pthread_mutex_unlock(&inf->lock);
        
        
        td_slong start = get_time_ms();
        if (config->local_frame_test == 1)// 读取本地YUV图像
        {   
            
            // 从本地读取一帧YUV图像发送到vpss(0)
            ret = get_local_yuv_frame(&local_frame, &local_frame_info);
            if (ret != TD_SUCCESS)
            {
                err_print("Error, get_local_yuv_frame fail\n");
                LOG_ERROR("Error, get_local_yuv_frame fail");
                goto release;
            }
            printf("get one local frame success\n");
        }
        else if (config->local_frame_test == 2) // 读取本地视频
        {
            get_vdec_frame_send_to_vi();
        }
        

        // // 从vpss获取一帧图像
        // ret = ss_mpi_vpss_get_chn_frame(grp, chn, &pmf_task.img_in, milli_sec);
        // if (ret != TD_SUCCESS)
        // {
        //     err_print("ss_mpi_vpss_get_chn_frame:0x%x\n", ret);
        //     LOG_ERROR("ss_mpi_vpss_get_chn_frame:0x%x", ret);
        //     goto release;
        // }

        // 从vi获取一帧图像
        ret = ss_mpi_vi_get_chn_frame(4, 0, &pmf_task.img_in, milli_sec);
        if (ret != TD_SUCCESS)
        {
            err_print("ss_mpi_vi_get_chn_frame:0x%x\n", ret);
            LOG_ERROR("ss_mpi_vi_get_chn_frame:0x%x", ret);
            goto release;
        }
        // if(pmf_task.img_in.video_frame.time_ref < 104160){
        //     printf("time_ref: %d\n",pmf_task.img_in.video_frame.time_ref);
        //     goto release;
        // }

        td_slong pmf_start = get_time_ms();

        /*------------------------对图像进行pmf透视矫正------------------------*/
        
        ret = frame_apply_pmf(&pmf_task, &vb_pmf_frame_out, pmf_width,pmf_height,&gdc_pmf_attr);
        if(ret != TD_SUCCESS){
            LOG_ERROR("Error, frame_apply_pmf failed,ret:0x%x",ret);
            goto release;
        }

        

        td_slong pmf_end = get_time_ms();

        /*------------------------对pmf透视矫正后的图像进行缩放和填充------------------------*/

        // 对pmf矫正后的图像进行缩放
        ret = image_scale(&pmf_task.img_out, &frame_scaled, &vb_scaled, 1280, 640);
        
        td_slong scale_end = get_time_ms();
        /*-------------------------------执行推理模型和画框---------------------------------*/
        
        // IVE色域转换 YUV-BGR
        ret = frame_yuv2rgb_package(&frame_scale_bgr, &frame_scaled);
        if(ret != TD_SUCCESS){
            LOG_ERROR("Error, frame_yuv2rgb_package fail");
            goto release;
        }
        

        // 截取图像分别推理
        td_float starting_point[3] = {0.0f,0.25f,0.5f};
        pOut[0].count = 0;// 模型输出计数器清零
        pOut[1].count = 0;// 模型输出计数器清零
        pOut[2].count = 0;// 模型输出计数器清零
        pOut[3].count = 0;// 模型输出计数器清零
        frame_cut(&img_algo,&frame_scale_bgr,starting_point[0]);// IVE DMA操作截取图像
        svp_execute(img_algo.virt_addr[0],&pOut[0]);// 运行模型执行推理
        frame_cut(&img_algo,&frame_scale_bgr,starting_point[1]);// IVE DMA操作截取图像
        svp_execute(img_algo.virt_addr[0],&pOut[1]);// 运行模型执行推理
        frame_cut(&img_algo,&frame_scale_bgr,starting_point[2]);// IVE DMA操作截取图像
        svp_execute(img_algo.virt_addr[0],&pOut[2]);// 运行模型执行推理

        // 运行nnn模型执行推理
        // hi_s32 w = img_algo.width;
        // hi_s32 h = img_algo.height;
        // hi_s32 framelen = w * h * 3;
        // td_float starting_point[3] = {0.0f,0.25f,0.5f};
        // pOut[0].count = 0;// 模型输出计数器清零
        // pOut[1].count = 0;// 模型输出计数器清零
        // pOut[2].count = 0;// 模型输出计数器清零
        // pOut[3].count = 0;// 模型输出计数器清零
        // frame_cut(&img_algo,&frame_scale_bgr,starting_point[0]);// IVE DMA操作截取图像
        // nnn_execute(img_algo.virt_addr[0], framelen, &pOut[0]);// 运行模型执行推理
        // frame_cut(&img_algo,&frame_scale_bgr,starting_point[1]);// IVE DMA操作截取图像
        // nnn_execute(img_algo.virt_addr[0], framelen, &pOut[1]);// 运行模型执行推理
        // frame_cut(&img_algo,&frame_scale_bgr,starting_point[2]);// IVE DMA操作截取图像
        // nnn_execute(img_algo.virt_addr[0], framelen, &pOut[2]);// 运行模型执行推理


        td_slong execute_end = get_time_ms();

        // 三次推理结果合并，去重
        merge_detection_box(pOut, starting_point, 3, 1280,1300);
        
        // 保存推理异常图片
        // save_unnormal_frame(&pOut[3],&pmf_task.img_out);
        
        
        // // 计算速度
        train_speed_match_new_count_test(&pOut[3], &pmf_task.img_out, speed_data);
        td_slong caculate_end = get_time_ms();
      
        // 根据推理结果画框
        vgsdraw(&pmf_task.img_out, &pOut[3]);            
        td_slong draw_end = get_time_ms();


        if (config->local_frame_test == 1)
        {
            // save_frame(&pmf_task.img_out, &local_frame_info);
        }
        
        /*-------------------------------画框后的图像送至---------------------------------*/
        // 输出到VO-HDMI 2
        ret = ss_mpi_vo_send_frame(0, 2, &pmf_task.img_out, 0);
        if (ret != TD_SUCCESS) {
            err_print("ss_mpi_vo_send_frame fail, ret:0x%x\n", ret);
            LOG_ERROR("ss_mpi_vo_send_frame fail, ret:0x%x", ret);
        }

        // 发送到vpps group[1]
        // 再把码流输出到vpss grup[1]是为了方便编码输出，vpss直接绑定venc做编码较为简单
        ret = ss_mpi_vpss_send_frame(1, &pmf_task.img_out, 0);
        if (ret != TD_SUCCESS) {
            err_print("ss_mpi_vo_send_frame fail, ret:0x%x\n", ret);
            LOG_ERROR("ss_mpi_vo_send_frame fail, ret:0x%x", ret);
        }

        td_slong send_frame = get_time_ms();

        
        // 计算速度(仅测试使用，会提高延迟，但可以在画面更新后输出数据)
        // train_speed_match_new_count_test(&pOut[3], &pmf_task.img_out, speed_data);
        // td_slong caculate_end = get_time_ms();

release:
        // 释放资源
        ss_mpi_vb_release_blk(vb_scaled);
        ss_mpi_vb_release_blk(vb_pmf_frame_out);
        
        // ss_mpi_vpss_release_chn_frame(grp, chn, &pmf_task.img_in);
        // if (ret != TD_SUCCESS) {
        //     err_print("ss_mpi_vpss_release_chn_frame fail, ret:0x%x\n", ret);
        //     LOG_ERROR("ss_mpi_vpss_release_chn_frame fail, ret:0x%x", ret);
        // }

        ss_mpi_vi_release_chn_frame(4, 2, &pmf_task.img_in);
        if (ret != TD_SUCCESS) {
            err_print("ss_mpi_vi_release_chn_frame fail, ret:0x%x\n", ret);
            LOG_ERROR("ss_mpi_vi_release_chn_frame fail, ret:0x%x", ret);
        }


        printf("finish one frame\n");
        printf("waiting for next frame time:    %d\n",pmf_start - start);
        printf("pmf cost time:                  %d\n", pmf_end - pmf_start);
        printf("scale cost time:                %d\n", scale_end - pmf_end);
        printf("mode execute cost time:         %d\n", execute_end - scale_end);
        printf("caculate cost time:             %d\n", caculate_end - execute_end);
        printf("draw cost time:                 %d\n", draw_end - caculate_end);
        printf("send frame cost time:           %d\n", send_frame - draw_end);
        printf("process one frame cost time:    %d\n\n", get_time_ms() - start);
    
        fps++;
        if (start - prems >= 1000)
        {
            prems = start;
            now_time = (prems - start_time) / 1000;
            printf("===========train speed========= vedio time:%d s     fps:%d\n", now_time, fps);
            fps = 0;
        }
    }

    
    // 去初始化svp 释放资源
    svp_uninit();
    ss_mpi_vb_release_blk(vb_scale_bgr_out);
    ss_mpi_vb_release_blk(vb_yolo_bgr_out);
    if(config->local_frame_test==1){
        ss_mpi_vb_release_blk(vb_local_frame);
    }
    LOG_INFO("frame process thread exit");
    return TD_NULL;
}