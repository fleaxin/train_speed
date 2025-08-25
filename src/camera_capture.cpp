
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include "sys/time.h"
#include <sys/stat.h>

#include "camera_capture.h"
#include "GxIAPI.h"
#include "DxImageProc.h"
#include "ot_type.h"
#include "ot_common.h"
#include "sample_comm.h"
#include "ot_common_vb.h"
#include "ss_mpi_ive.h"
#include "camera_test.h"
#include <sys/prctl.h>
#include "read_config.h"
#include "global_logger.h"

// 定义全局对象
DaHengModule g_camera;
// 定义线程名称
pthread_t capture_tid;// 采集线程
pthread_t command_tid;// 命令控制线程

static unsigned long getCurTime() {
    struct timeval start;
    gettimeofday(&start, NULL);
    unsigned long ms = (start.tv_sec) * 1000 + (start.tv_usec) / 1000;
    return ms;
}


/// 大恒相机
DaHengModule::DaHengModule() {
    
    // stop_device();// 启动前先尝试关闭

    emStatus = GX_STATUS_SUCCESS;
    g_hDevice = nullptr;
    ui32DeviceNum = 0;
    g_bColorFilter = false;
    g_i64ColorFilter = GX_COLOR_FILTER_NONE;
    g_bAcquisitionFlag = false;
    g_bSavePPMImage = false;
    g_nAcquisitonThreadID = 0;

    g_nPayloadSize = 5;
    g_pRGBImageBuf = nullptr;
    g_pRaw8Image = nullptr;

    g_Image_param.height = 1200;
    g_Image_param.width = 2448;
    g_Image_param.offset_x = 0;
    g_Image_param.offset_y = 500;
    g_Image_param.frame_rate = 25;
    g_Image_param.exposure_time = 40000;
}

DaHengModule::~DaHengModule() {
    stop_device();
}

int DaHengModule::open_device() {
    emStatus = GXInitLib();
    if (emStatus != GX_STATUS_SUCCESS) {
        GetErrorString(emStatus);
        LOG_ERROR("Camera init error");
        return emStatus;
    }

    //Get device enumerated number
    emStatus = GXUpdateDeviceList(&ui32DeviceNum, 1000);
    if (emStatus != GX_STATUS_SUCCESS) {
        GetErrorString(emStatus);
        GXCloseLib();
        LOG_ERROR("Camera update device list error");
        return emStatus;
    }

    //If no device found, app exit
    if (ui32DeviceNum <= 0) {
        LOG_ERROR("Camera no device found");
        GXCloseLib();
        return emStatus;
    }
    //Open first device enumerated
    emStatus = GXOpenDeviceByIndex(1, &g_hDevice);
    if (emStatus != GX_STATUS_SUCCESS) {
        GetErrorString(emStatus);
        GXCloseLib();
        LOG_ERROR("Camera open device failed");
        return emStatus;
    }

    g_bColorFilter = false;
    //Get the type of Bayer conversion. whether is a color camera.
    emStatus = GXIsImplemented(g_hDevice, GX_ENUM_PIXEL_COLOR_FILTER, &g_bColorFilter);
    GX_VERIFY_EXIT(emStatus);

    emStatus = GXGetInt(g_hDevice, GX_INT_PAYLOAD_SIZE, &g_nPayloadSize);
    GX_VERIFY(emStatus);

    //This app only support color cameras
    if (!g_bColorFilter) {
        LOG_ERROR("<This app only support color cameras! App Exit!>");
        GXCloseDevice(g_hDevice);
        g_hDevice = nullptr;
        GXCloseLib();
        return 0;
    } else {
        emStatus = GXGetEnum(g_hDevice, GX_ENUM_PIXEL_COLOR_FILTER, &g_i64ColorFilter);
        GX_VERIFY_EXIT(emStatus);
    }

    emStatus = GXGetInt(g_hDevice, GX_INT_PAYLOAD_SIZE, &g_nPayloadSize);
    GX_VERIFY(emStatus);

    //Set acquisition mode
    emStatus = GXSetEnum(g_hDevice, GX_ENUM_ACQUISITION_MODE, GX_ACQ_MODE_CONTINUOUS);
    GX_VERIFY_EXIT(emStatus);

    //Set trigger mode
    emStatus = GXSetEnum(g_hDevice, GX_ENUM_TRIGGER_MODE, GX_TRIGGER_MODE_OFF);
    GX_VERIFY_EXIT(emStatus);

    //Set buffer quantity of acquisition queue
    uint64_t nBufferNum = ACQ_BUFFER_NUM;
    emStatus = GXSetAcqusitionBufferNumber(g_hDevice, nBufferNum);
    GX_VERIFY_EXIT(emStatus);

    bool bStreamTransferSize = false;
    emStatus = GXIsImplemented(g_hDevice, GX_DS_INT_STREAM_TRANSFER_SIZE, &bStreamTransferSize);
    GX_VERIFY_EXIT(emStatus);

    if (bStreamTransferSize) {
        //Set size of data transfer block
        emStatus = GXSetInt(g_hDevice, GX_DS_INT_STREAM_TRANSFER_SIZE, ACQ_TRANSFER_SIZE);
        GX_VERIFY_EXIT(emStatus);
    }

    bool bStreamTransferNumberUrb = false;
    emStatus = GXIsImplemented(g_hDevice, GX_DS_INT_STREAM_TRANSFER_NUMBER_URB, &bStreamTransferNumberUrb);
    GX_VERIFY_EXIT(emStatus);

    if (bStreamTransferNumberUrb) {
        //Set qty. of data transfer block
        emStatus = GXSetInt(g_hDevice, GX_DS_INT_STREAM_TRANSFER_NUMBER_URB, ACQ_TRANSFER_NUMBER_URB);
        GX_VERIFY_EXIT(emStatus);
    }

    //Set Balance White Mode : Continuous
    emStatus = GXSetEnum(g_hDevice, GX_ENUM_BALANCE_WHITE_AUTO, GX_BALANCE_WHITE_AUTO_CONTINUOUS);
    GX_VERIFY_EXIT(emStatus);

    double framerate_;
    emStatus = GXGetFloat(g_hDevice, GX_FLOAT_ACQUISITION_FRAME_RATE, &framerate_);
    emStatus = GXSetInt(g_hDevice, GX_INT_WIDTH, g_Image_param.width);
    emStatus = GXSetInt(g_hDevice, GX_INT_HEIGHT, g_Image_param.height);
    emStatus = GXSetInt(g_hDevice, GX_INT_OFFSET_Y, g_Image_param.offset_y);
    emStatus = GXSetInt(g_hDevice, GX_INT_OFFSET_X, g_Image_param.offset_x);

    //使能采集帧率调节模式
    emStatus = GXSetEnum(g_hDevice, GX_ENUM_ACQUISITION_FRAME_RATE_MODE, GX_ACQUISITION_FRAME_RATE_MODE_ON);
    
    //设置采集帧率
    emStatus = GXSetFloat(g_hDevice, GX_FLOAT_ACQUISITION_FRAME_RATE, g_Image_param.frame_rate);

    //设置自动曝光模式
    emStatus = GXSetEnum(g_hDevice, GX_ENUM_EXPOSURE_AUTO, GX_EXPOSURE_AUTO_CONTINUOUS);

    // //设置曝光模式为Timed
    // emStatus = GXSetEnum(g_hDevice, GX_ENUM_EXPOSURE_MODE, GX_EXPOSURE_MODE_TIMED);

    // //设置曝光时间
    // emStatus = GXSetFloat(g_hDevice, GX_FLOAT_EXPOSURE_TIME, g_Image_param.exposure_time);

    //设置连续自动增益
    emStatus = GXSetEnum(g_hDevice, GX_ENUM_GAIN_AUTO, GX_GAIN_AUTO_CONTINUOUS);

    //获取自动增益和自动曝光范围
    GX_FLOAT_RANGE autoGainMinRange;
    GX_FLOAT_RANGE autoGainMaxRange;
    GX_FLOAT_RANGE autoExposureMinRange;
    GX_FLOAT_RANGE autoExposureMaxRange;
    emStatus = GXGetFloatRange(g_hDevice, GX_FLOAT_AUTO_GAIN_MIN, &autoGainMinRange);
    emStatus = GXGetFloatRange(g_hDevice, GX_FLOAT_AUTO_GAIN_MAX,&autoGainMaxRange);
    // printf("gainmin:%f, gainmax:%f\n",autoGainMinRange.dMin,autoGainMaxRange.dMax);
    emStatus = GXGetFloatRange(g_hDevice, GX_FLOAT_AUTO_EXPOSURE_TIME_MIN,&autoExposureMinRange);
    emStatus = GXGetFloatRange(g_hDevice, GX_FLOAT_AUTO_EXPOSURE_TIME_MAX,&autoExposureMaxRange);
    // printf("exposuremin:%f, exposuremax:%f\n",autoExposureMinRange.dMin,autoExposureMaxRange.dMax);

    //设置自动增益最大值
    autoGainMaxRange.dMax = 24.0;
    emStatus = GXSetFloat(g_hDevice, GX_FLOAT_AUTO_GAIN_MAX, autoGainMaxRange.dMax);
    //设置自动增益最小值
    autoGainMinRange.dMin;
    emStatus = GXSetFloat(g_hDevice, GX_FLOAT_AUTO_GAIN_MIN, autoGainMinRange.dMin);

    //设置自动曝光最大值
    autoExposureMaxRange.dMax = g_Image_param.exposure_time;
    emStatus = GXSetFloat(g_hDevice, GX_FLOAT_AUTO_EXPOSURE_TIME_MAX, autoExposureMaxRange.dMax);
    //设置自动曝光最小值
    autoExposureMinRange.dMin;
    emStatus = GXSetFloat(g_hDevice, GX_FLOAT_AUTO_EXPOSURE_TIME_MIN, autoExposureMinRange.dMin);

    //设置自动曝光统计区域
    //获取调节范围
    GX_INT_RANGE stROIWidthRange;
    GX_INT_RANGE stROIHeightRange;
    GX_INT_RANGE stROIXRange;
    GX_INT_RANGE stROIYRange;
    emStatus = GXGetIntRange(g_hDevice, GX_INT_AAROI_WIDTH, &stROIWidthRange);
    emStatus = GXGetIntRange(g_hDevice, GX_INT_AAROI_HEIGHT, &stROIHeightRange);
    emStatus = GXGetIntRange(g_hDevice, GX_INT_AAROI_OFFSETX, &stROIXRange);
    emStatus = GXGetIntRange(g_hDevice, GX_INT_AAROI_OFFSETY,&stROIYRange);
    

    //设置统计区域为整幅图像
    emStatus = GXSetInt(g_hDevice, GX_INT_AAROI_WIDTH, g_Image_param.AAROI_width);
    emStatus = GXSetInt(g_hDevice, GX_INT_AAROI_HEIGHT, g_Image_param.AAROI_height);
    emStatus = GXSetInt(g_hDevice, GX_INT_AAROI_OFFSETX, g_Image_param.AAROI_offsetx);
    emStatus = GXSetInt(g_hDevice, GX_INT_AAROI_OFFSETY, g_Image_param.AAROI_offsety);


    //设置为获取最新一帧图像
    emStatus = GXSetEnum(g_hDevice, GX_DS_ENUM_STREAM_BUFFER_HANDLING_MODE, GX_DS_STREAM_BUFFER_HANDLING_MODE_NEWEST_ONLY);
    
    //Allocate the memory for pixel format transform
    PreForAcquisition();

    //Device start acquisition
    emStatus = GXStreamOn(g_hDevice);
    if (emStatus != GX_STATUS_SUCCESS) {
        //Release the memory allocated
        UnPreForAcquisition();
        GX_VERIFY_EXIT(emStatus);
    }

    LOG_INFO("Device start acquisition");

    return emStatus;
}

int DaHengModule::release_vb_buffer(){

    int ret;

    ret = ss_mpi_vb_release_blk(rgb_vb.vb_blk);
    if(ret != TD_SUCCESS){
        err_print("ss_mpi_vb_release_blk fail, ret:0x%x\n", ret);
        LOG_ERROR("ss_mpi_vb_release_blk fail, ret:0x%x", ret);
    }
    ret = ss_mpi_vb_release_blk(yuv_vb.vb_blk);
    if(ret != TD_SUCCESS){
        err_print("ss_mpi_vb_release_blk fail, ret:0x%x\n", ret);
        LOG_ERROR("ss_mpi_vb_release_blk fail, ret:0x%x", ret);
    }
    
    return TD_SUCCESS;

}

int DaHengModule::get_vb_buffer(){

    // 获取一个存储RGB888的VB块   
    rgb_vb.vb_size = g_Image_param.width * g_Image_param.height * 3;
    rgb_vb.vb_blk = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, rgb_vb.vb_size, TD_NULL);
    if(rgb_vb.vb_blk == OT_VB_INVALID_HANDLE){
        LOG_ERROR("get vb blk failed");
        return TD_FAILURE;
    }
    rgb_vb.vb_phy_addr[0] = ss_mpi_vb_handle_to_phys_addr(rgb_vb.vb_blk);
    rgb_vb.vb_virt_addr[0] = (td_u8*)ss_mpi_sys_mmap(rgb_vb.vb_phy_addr[0], rgb_vb.vb_size);

    // 获取一个存储yuv420sp的VB块
    yuv_vb.vb_size = g_Image_param.width * g_Image_param.height * 3 / 2;
    yuv_vb.vb_blk = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, yuv_vb.vb_size, TD_NULL);
    if(yuv_vb.vb_blk == OT_VB_INVALID_HANDLE){
        LOG_ERROR("get vb blk failed");
        return TD_FAILURE;
    }
    td_phys_addr_t yuv_phy_addr[2];
    yuv_vb.vb_phy_addr[0] = ss_mpi_vb_handle_to_phys_addr(yuv_vb.vb_blk);
    yuv_vb.vb_phy_addr[1] = yuv_vb.vb_phy_addr[0] + g_Image_param.width * g_Image_param.height;
    yuv_vb.vb_virt_addr[0] = (td_u8*)ss_mpi_sys_mmap(yuv_vb.vb_phy_addr[0], yuv_vb.vb_size);
    yuv_vb.vb_virt_addr[1] = yuv_vb.vb_virt_addr[0] + g_Image_param.width * g_Image_param.height;
    
    return TD_SUCCESS;

}
std::string DaHengModule::create_folder_by_datetime(const char* base_path) {
    // 获取当前时间
    time_t now = time(nullptr);
    struct tm* local_time = localtime(&now);

    // 格式化时间字符串为 日-时-分
    char folder_name[20];
    strftime(folder_name, sizeof(folder_name), "%d-%H-%M", local_time);

    // 构建完整路径
    std::string full_path = std::string(base_path) + "/" + folder_name;

    // 创建文件夹
    if (mkdir(full_path.c_str(), 0777) == -1) {
        if (errno == EEXIST) {
            std::cout << "Folder already exists: " << full_path << std::endl;
        } else {
            perror("mkdir failed");
        }
        return ""; // 返回空字符串表示失败
    }
    std::cout << "Folder created successfully: " << full_path << std::endl;
    return full_path; // 返回文件夹路径
}


int DaHengModule::save_frame_to_file(const void* data, size_t data_length, const std::string& directory, const std::string& filename) {
    if (data == nullptr || data_length == 0 || directory.empty() || filename.empty()) {
        fprintf(stderr, "Invalid input parameters\n");
        return -1;
    }

    // 构建完整文件路径
    std::string full_path = directory + "/" + filename;

    // 打开文件
    FILE *file = fopen(full_path.c_str(), "wb");
    if (file == nullptr) {
        perror("fopen failed");
        return -1;
    }

    // 写入数据
    size_t written = fwrite(data, sizeof(td_u8), data_length, file);
    if (written < data_length) {
        fprintf(stderr, "File write error\n");
        fclose(file);
        return -1;
    }

    // 关闭文件
    fclose(file);

    // 设置文件权限
    // if (chmod(full_path.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) != 0) {
    //     perror("chmod failed");
    //     return -1;
    // }
    return 0;
}


int DaHengModule::get_frame_buffer() {
    td_s32 ret;
    PGX_FRAME_BUFFER pFrameBuffer = nullptr;
    auto t1 = getCurTime();
    emStatus = GXDQBuf(g_hDevice, &pFrameBuffer, 1000);
    if (emStatus != GX_STATUS_SUCCESS) {
        if (emStatus == GX_STATUS_TIMEOUT) {
            return -1;
        } else {
            GetErrorString(emStatus);
            return -2;
        }
    }

    auto t2 = getCurTime();
    long pic_time = get_time_ms();
    if (pFrameBuffer->nStatus != GX_FRAME_STATUS_SUCCESS) {
        LOG_ERROR("<Abnormal Acquisition: Exception code: %d>", pFrameBuffer->nStatus);
    } else {
        VxInt32 emDXStatus = DX_OK;

        emDXStatus = DxRaw8toRGB24Ex((unsigned char *) pFrameBuffer->pImgBuf,
                                   g_pRGBImageBuf, pFrameBuffer->nWidth,
                                   pFrameBuffer->nHeight,
                                   RAW2RGB_NEIGHBOUR,
                                   DX_PIXEL_COLOR_FILTER(g_i64ColorFilter),
                                   false,
                                   DX_ORDER_BGR);// 海思图像中为BGR格式
        if (emDXStatus != DX_OK) {
            LOG_ERROR("DxRaw8toRGB24 Failed, Error Code: %d", emDXStatus);
            return -1;
        }
        auto t3 = getCurTime();
        // std::cout << "GXDQBuf cost:    " << t2 - t1 << std::endl;
        // std::cout << "Raw8toRGB24 cost:    " << t3 - t2 << std::endl;
    }
    
    auto t4 = getCurTime();

    auto t5 = getCurTime();
    // 复制图像数据到RGB888 vb块中
    ret = memcpy_s((void *)rgb_vb.vb_virt_addr[0], rgb_vb.vb_size, (void *)g_pRGBImageBuf, rgb_vb.vb_size);
    if (ret != 0) {
        LOG_ERROR("memcpy_s failed, ret: %d", ret);
        return -1;
    }
    // std::cout << "copy RGB888 cost:    " <<getCurTime() - t5 << std::endl;

    // 使用IVE做颜色转换 RGB888->YUV420sp
    ot_svp_img src_img;
    ot_svp_img dst_img;
    ot_ive_csc_ctrl csc_ctrl;
    csc_ctrl.mode = OT_IVE_CSC_MODE_VIDEO_BT709_RGB_TO_YUV;
    
    src_img.width = pFrameBuffer->nWidth;
    src_img.height = pFrameBuffer->nHeight;
    src_img.type = OT_SVP_IMG_TYPE_U8C3_PACKAGE;
    src_img.stride[0] = pFrameBuffer->nWidth;
    src_img.phys_addr[0] = rgb_vb.vb_phy_addr[0];

    dst_img.width = pFrameBuffer->nWidth;
    dst_img.height = pFrameBuffer->nHeight;
    dst_img.type = OT_SVP_IMG_TYPE_YUV420SP;
    dst_img.stride[0] = pFrameBuffer->nWidth;
    dst_img.stride[1] = dst_img.stride[0];
    dst_img.phys_addr[0] = yuv_vb.vb_phy_addr[0];
    dst_img.phys_addr[1] = yuv_vb.vb_phy_addr[1];


    ot_ive_handle handle;
    td_bool is_finish = TD_FALSE;
    td_bool is_block = TD_TRUE;
    auto t6 = getCurTime();
    ret = ss_mpi_ive_csc(&handle, &src_img, &dst_img, &csc_ctrl, TD_TRUE);
    if (ret != TD_SUCCESS) {
        LOG_ERROR("ss_mpi_ive_csc failed with %#x\n", ret);
    }

    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }
    // std::cout << "IVE cost:    " << getCurTime() - t6 << std::endl;


    // 创建海思视频格式 ot_video_frame_info
    ot_video_frame_info frame_info;
    frame_info.pool_id = ss_mpi_vb_handle_to_pool_id(yuv_vb.vb_blk);
    frame_info.mod_id = OT_ID_VI;
    frame_info.video_frame.width = pFrameBuffer->nWidth;
    frame_info.video_frame.height = pFrameBuffer->nHeight;
    frame_info.video_frame.stride[0] = pFrameBuffer->nWidth;
    frame_info.video_frame.stride[1] = pFrameBuffer->nWidth;
    frame_info.video_frame.field = OT_VIDEO_FIELD_FRAME;
    frame_info.video_frame.pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    frame_info.video_frame.video_format = OT_VIDEO_FORMAT_LINEAR;
    frame_info.video_frame.compress_mode = OT_COMPRESS_MODE_NONE;
    frame_info.video_frame.dynamic_range = OT_DYNAMIC_RANGE_SDR8;
    frame_info.video_frame.color_gamut = OT_COLOR_GAMUT_BT709;
    frame_info.video_frame.phys_addr[0] = yuv_vb.vb_phy_addr[0];
    frame_info.video_frame.phys_addr[1] = yuv_vb.vb_phy_addr[1];
    frame_info.video_frame.virt_addr[0] = (td_void*)yuv_vb.vb_virt_addr[0];
    frame_info.video_frame.virt_addr[1] = (td_void*)yuv_vb.vb_virt_addr[0];
    frame_info.video_frame.time_ref = (td_u32)pFrameBuffer->nFrameID * 2;// 图像序列号要求为偶数
    // frame_info.video_frame.time_ref = 0;
    // frame_info.video_frame.pts = (td_u64)pFrameBuffer->nTimestamp;
    frame_info.video_frame.pts = (td_u64)pic_time;
    frame_info.video_frame.frame_flag = 0;// 如果送给VPSS的帧是用户自己构建，不是从某个模块获取而来，需要将ot_video_frame中的frame_flag初始化为0。


    // 发送数据块到vi模块中 使用vi模块的LDC畸变矫正功能
    ret = ss_mpi_vi_send_pipe_yuv(4,&frame_info, 100);
    if (ret != TD_SUCCESS) {
        err_print("ss_mpi_vi_send_pipe_yuv fail, ret:0x%x\n", ret);
        LOG_ERROR("ss_mpi_vi_send_pipe_yuv fail, ret:0x%x", ret);
    }

    // 发送数据块到vpss[2],然后绑定venc编码保存原始视频图像
    ret = ss_mpi_vpss_send_frame(2,&frame_info, 100);
    if (ret != TD_SUCCESS) {
        err_print("ss_mpi_vpss_send_frame fail, ret:0x%x\n", ret);
        LOG_ERROR("ss_mpi_vpss_send_frame fail, ret:0x%x", ret);
    }
    
    // std::cout << "send frame to vpss cost:    " << getCurTime() - t4 << std::endl;
    emStatus = GXQBuf(g_hDevice, pFrameBuffer);
    if (emStatus != GX_STATUS_SUCCESS) {
        GetErrorString(emStatus);
        return -2;
    }
    return 0;
}

void DaHengModule::GetErrorString(GX_STATUS emErrorStatus) {
    char *error_info = nullptr;
    size_t size = 0;
    GX_STATUS emStatus = GX_STATUS_SUCCESS;

    // Get length of error description
    emStatus = GXGetLastError(&emErrorStatus, nullptr, &size);
    if (emStatus != GX_STATUS_SUCCESS) {
        LOG_ERROR("<Error when calling GXGetLastError>");
        return;
    }

    // Alloc error resources
    error_info = new char[size];
    if (error_info == nullptr) {
        LOG_ERROR("<Failed to allocate memory>");
        return;
    }

    // Get error description
    emStatus = GXGetLastError(&emErrorStatus, error_info, &size);
    if (emStatus != GX_STATUS_SUCCESS) {
        LOG_ERROR("<Error when calling GXGetLastError>");
    } else {
        printf("%s\n", error_info);
    }

    // Realease error resources
    if (error_info != nullptr) {
        delete[]error_info;
        error_info = nullptr;
    }
}

void DaHengModule::PreForAcquisition() {
    g_pRGBImageBuf = new unsigned char[g_nPayloadSize * 3];
    g_pRaw8Image = new unsigned char[g_nPayloadSize];
}

void DaHengModule::UnPreForAcquisition() {
    if (g_pRaw8Image != nullptr) {
        delete[] g_pRaw8Image;
        g_pRaw8Image = nullptr;
    }
    if (g_pRGBImageBuf != nullptr) {
        delete[] g_pRGBImageBuf;
        g_pRGBImageBuf = nullptr;
    }
}

int DaHengModule::stop_device() {
    emStatus = GXStreamOff(g_hDevice);
    if (emStatus != GX_STATUS_SUCCESS) {
        //Release the memory allocated
        UnPreForAcquisition();
        GX_VERIFY_EXIT(emStatus);
    }

    //Release the resources and stop acquisition thread
    UnPreForAcquisition();

    //Close device
    emStatus = GXCloseDevice(g_hDevice);
    if (emStatus != GX_STATUS_SUCCESS) {
        GetErrorString(emStatus);
        g_hDevice = nullptr;
        GXCloseLib();
        return emStatus;
    }

    //Release libary
    emStatus = GXCloseLib();
    if (emStatus != GX_STATUS_SUCCESS) {
        GetErrorString(emStatus);
        return emStatus;
    }
    return 0;
}

int DaHengModule::set_config(){
    const GlobalConfig* cfg = get_global_config();
    g_Image_param.frame_rate    = cfg->camera.rate;
    g_Image_param.width         = cfg->camera.width;
    g_Image_param.height        = cfg->camera.height;
    g_Image_param.offset_x      = cfg->camera.offset_x;
    g_Image_param.offset_y      = cfg->camera.offset_y;
    g_Image_param.exposure_time = cfg->camera.exposure_time;
    g_Image_param.AAROI_height  = cfg->camera.AAROI_height;
    g_Image_param.AAROI_width   = cfg->camera.AAROI_width;
    g_Image_param.AAROI_offsetx      = cfg->camera.AAROI_offsetx;
    g_Image_param.AAROI_offsety      = cfg->camera.AAROI_offsety;
}
// void *get_and_send_frame(void* arg){

//     PthreadInf *inf = (PthreadInf*)arg;
//     td_bool start_signal = inf->start_singal;
//     // 心跳信号
//     volatile long *last_active_time = &inf->last_active_time;
//     prctl(PR_SET_NAME, "get_and_send_frame", 0, 0, 0);
//     LOG_INFO("get frame thread started!");
//     DaHengModule daheng;
//     daheng.set_config();// 读取配置文件中camera参数
//     daheng.open_device();
//     daheng.get_vb_buffer();
//     auto t1 = getCurTime();
//     int count = 0; 

//     while(start_signal){
        
//         start_signal = inf->start_singal;
//         *last_active_time = get_time_ms();
//         daheng.get_frame_buffer();
//         std::cout << "========================= capture one frame cost time " << getCurTime() - t1 << "=================" <<"\n" <<std::endl;
//         t1 = getCurTime();
//         count ++;
//     }

//     daheng.release_vb_buffer();

//     LOG_INFO("stop get and send frame");
// }
void* get_and_send_frame(void* arg) {
    PthreadInf *inf = (PthreadInf*)arg;
    volatile long *last_active_time = &inf->last_active_time;

    prctl(PR_SET_NAME, "camera_capture", 0, 0, 0);
    LOG_INFO("Camera capture thread started.");
    
    int ret;
    // 初始化摄像头
    g_camera.set_config();

    g_camera.emStatus = g_camera.open_device();
    if(g_camera.emStatus != GX_STATUS_SUCCESS){
        LOG_ERROR("Open camera device failed.");
        return nullptr;
    }

    ret = g_camera.get_vb_buffer();
    if(ret != TD_SUCCESS){
        LOG_ERROR("Get video buffer failed.");
        return nullptr;
    }

    auto t1 = getCurTime();
    int count = 0; 
    while (inf->start_singal) {

        // 写入时间
        pthread_mutex_lock(&inf->lock);
        *last_active_time = get_time_ms();
        pthread_mutex_unlock(&inf->lock);

        g_camera.get_frame_buffer();  // 只做图像采集和推送
        std::cout << "========================= capture one frame cost time " << getCurTime() - t1 << "=================" <<"\n" <<std::endl;
        t1 = getCurTime();
        count ++;
    }
    g_camera.release_vb_buffer();
    LOG_INFO("Camera capture thread stopped.");
    return nullptr;
}

void* command_handler_thread(void* arg) {
    PthreadInf *inf = (PthreadInf*)arg;
    prctl(PR_SET_NAME, "command_handler", 0, 0, 0);
    LOG_INFO("Command handler thread started.");

    while (inf->start_singal) {
        
    }

    LOG_INFO("Command handler thread stopped.");
    return nullptr;
}

int start_camera_capture(PthreadInf *thread_info)
{
    
    // 初始化互斥锁
    pthread_mutex_init(&thread_info->lock, NULL);

    // 启动图像采集线程
    pthread_create(&capture_tid, TD_NULL, get_and_send_frame, (td_void*)thread_info);

    // 启动命令处理线程
    // pthread_create(&command_tid, TD_NULL, command_handler_thread, (td_void*)thread_info);

    return 0;
}

int stop_camera_capture()
{ 
    pthread_join(capture_tid,TD_NULL);
    
    pthread_join(command_tid,TD_NULL);
}
