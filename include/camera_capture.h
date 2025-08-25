#ifndef CAMERA_CAPTURE_H
#define CAMERA_CAPTURE_H

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif

#include <stdlib.h>
#include <string.h>
#include "GxIAPI.h"
#include "DxImageProc.h"
#include "ot_type.h"
#include "ot_common_vb.h"
#include "camera_test.h"

#define ACQ_BUFFER_NUM 5              ///< Acquisition Buffer Qty.
#define ACQ_TRANSFER_SIZE (64 * 1024) ///< Size of data transfer block
#define ACQ_TRANSFER_NUMBER_URB 64    ///< Qty. of data transfer block
#define FILE_NAME_LEN 50              ///< Save image file name length

#define PIXFMT_CVT_FAIL -1   ///< PixelFormatConvert fail
#define PIXFMT_CVT_SUCCESS 0 ///< PixelFormatConvert success

// Show error message
#define GX_VERIFY(emStatus)            \
    if (emStatus != GX_STATUS_SUCCESS) \
    {                                  \
        GetErrorString(emStatus);      \
        return emStatus;               \
    }

// Show error message, close device and lib
#define GX_VERIFY_EXIT(emStatus)       \
    if (emStatus != GX_STATUS_SUCCESS) \
    {                                  \
        GetErrorString(emStatus);      \
        GXCloseDevice(g_hDevice);      \
        g_hDevice = NULL;              \
        GXCloseLib();                  \
        printf("<App Exit!>\n");       \
        return emStatus;               \
    }

    
    int start_camera_capture(PthreadInf *thread_info);
    int stop_camera_capture();
    
    void *get_and_send_frame(void* arg);

#ifdef __cplusplus
#if __cplusplus
}

struct vb_info
    {
        ot_vb_blk vb_blk;
        td_u64 vb_size;
        td_phys_addr_t vb_phy_addr[2];
        td_u8 *vb_virt_addr[2];
    };
struct camera_param
    {
        int width;
        int height;
        int offset_x;
        int offset_y;
        int frame_rate;
        int exposure_time;
        int AAROI_height;
        int AAROI_width;
        int AAROI_offsetx;
        int AAROI_offsety;
    };

class DaHengModule
    {
    public:
        DaHengModule();

        ~DaHengModule();

        int open_device();

        int get_frame_buffer();

        int stop_device();     

        int get_vb_buffer();

        int release_vb_buffer();

        int set_config();
        
        // 创建文件夹
        std::string create_folder_by_datetime(const char* base_path);
        // 保存图片
        int save_frame_to_file(const void* data, size_t data_length, const std::string& directory, const std::string& filename);
    public:
        GX_STATUS emStatus;
        GX_DEV_HANDLE g_hDevice; ///< Device handle
        uint32_t ui32DeviceNum;
        bool g_bColorFilter;             ///< Color filter support flag
        int64_t g_i64ColorFilter;        ///< Color filter of device
        bool g_bAcquisitionFlag;         ///< Thread running flag
        bool g_bSavePPMImage;            ///< Save raw image flag
        pthread_t g_nAcquisitonThreadID; ///< Thread ID of Acquisition thread
        int64_t g_nPayloadSize;

        unsigned char *g_pRGBImageBuf; ///< Memory for RAW8toRGB24
        unsigned char *g_pRaw8Image;   ///< Memory for RAW16toRAW8

        camera_param g_Image_param;

        ot_vb_blk vb_blk_rgb;
        ot_vb_blk vb_blk_yuv;

        vb_info rgb_vb;
        vb_info yuv_vb;

        std::string file_path;

    private:
        static void GetErrorString(GX_STATUS);

        void PreForAcquisition();

        void UnPreForAcquisition();
    };

    extern DaHengModule g_camera;
#endif
#endif
#endif