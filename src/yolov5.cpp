#include "yolov5.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdio.h>
#include <iostream>
#include <cstring>
#include <sys/time.h>
struct Detection
{
    hi_float x, y, w, h;
    hi_s32 class_id;
    hi_float confidence;
};

#define NMS_THRESHOLD 0.1f
#define CONFIDENCE_THRESHOLD 0.8f
#define INPUT_WIDTH 640
#define INPUT_HEIGHT 640
#define IMAGE_WIDTH 640
#define IMAGE_HEIGHT 360
// // 函数声明
// hi_float sigmoid(hi_float x);
// hi_float calculate_iou(const Detection& det_a, const Detection& det_b);
// void nms_process(std::vector<Detection>& detections,
//                  std::vector<Detection>& result, hi_float nms_threshold);
// std::vector<Detection> correct_boxes(const std::vector<Detection>& detections, hi_s32 input_h, hi_s32 input_w,
//                                      hi_s32 image_h, hi_s32 image_w);
// sigmoid 实现
const hi_float sigmoid(hi_float x)
{
    return 1.0f / (1.0f + std::exp(-x));
}

long getm()
{
    struct timeval start;
    gettimeofday(&start, NULL);
    long ms = (start.tv_sec) * 1000 + (start.tv_usec) / 1000;
    return ms;
}
hi_float calculate_iou(const stObjinfo &a, const stObjinfo &b)
{
    hi_float inter_x1 = std::max(a.x, b.x);
    hi_float inter_y1 = std::max(a.y, b.y);
    hi_float inter_x2 = std::min(a.x + a.w, b.x + b.w);
    hi_float inter_y2 = std::min(a.y + a.h, b.y + b.h);

    hi_float inter_area = std::max(0.0f, inter_x2 - inter_x1) * std::max(0.0f, inter_y2 - inter_y1);
    hi_float union_area = a.w * a.h + b.w * b.h - inter_area;

    return inter_area / union_area;
}
stYolov5Objs correct_boxes(stObjinfo *objs, hi_s32 input_w, hi_s32 input_h, hi_s32 image_w, hi_s32 image_h)
{
    // 计算填充
    hi_float fill_each_pix_w = (input_h - image_h) / image_h;
    hi_float fill_each_pix_h = (input_w - image_w) / image_w;

    objs->x = (hi_s32)objs->x * (1 + fill_each_pix_w);
    objs->y = (hi_s32)objs->y * (1 + fill_each_pix_h);
    objs->w = (hi_s32)objs->w * (1 + fill_each_pix_w);
    objs->h = (hi_s32)objs->h * (1 + fill_each_pix_h);

    if (objs->x < 0)
    {
        objs->x = (hi_s32)0;
    }
    if (objs->y < 0)
    {
        objs->y = (hi_s32)0;
    }
    if (objs->x + objs->w > image_w)
    {
        objs->w = image_w - objs->x > 0 ? image_w - objs->x : 0;
    }
    if (objs->y + objs->h > image_h)
    {
        objs->h = image_h - objs->y > 0 ? image_h - objs->y : 0;
    }
}
void nms_process_in_place_and_correct_boxes(stYolov5Objs *p_out)
{
    // 按 score 降序排序对象数组
    std::sort(p_out->objs, p_out->objs + p_out->count, [](const stObjinfo &a, const stObjinfo &b)
              { return a.score > b.score; });

    bool is_suppressed[OBJDETECTMAX] = {false};
    hi_s32 write_index = 0; // 用于记录要写入的下一个位置
    for (hi_s32 i = 0; i < p_out->count; ++i)
    {
        if (is_suppressed[i])
            continue;

        // 将未被抑制的框移动到 write_index 位置
        p_out->objs[write_index++] = p_out->objs[i];
        // std::cout<< "lx:"<< p_out->objs[i].x << "  ly:" <<p_out->objs[i].y << "  w:" << p_out->objs[i].w << "  h:" << p_out->objs[i].h <<"  class:" << p_out->objs[i].class_id << "  score:" << p_out->objs[i].score << std::endl;
        for (hi_s32 j = i + 1; j < p_out->count; ++j)
        {
            if (is_suppressed[j])
                continue;

            // 仅对同类的检测框进行 IoU 计算
            if (p_out->objs[i].class_id == p_out->objs[j].class_id)
            {
                hi_float iou = calculate_iou(p_out->objs[i], p_out->objs[j]);
                if (iou > NMS_THRESHOLD)
                {
                    is_suppressed[j] = true; // 标记为抑制
                }
            }
        }
    }

    // 更新 count 为实际保留的框数
    p_out->count = write_index;
}

// NMS 处理以及去除靠近边缘的检测框
void nms_process_and_filter(stYolov5Objs *p_out)
{
    // 按 score 降序排序对象数组
    std::sort(p_out->objs, p_out->objs + p_out->count, [](const stObjinfo &a, const stObjinfo &b)
              { return a.score > b.score; });

    bool is_suppressed[OBJDETECTMAX] = {false};
    hi_s32 write_index = 0; // 用于记录要写入的下一个位置
    for (hi_s32 i = 0; i < p_out->count; ++i)
    {
        if (is_suppressed[i])
            continue;

        const stObjinfo &obj = p_out->objs[i];

        // 判断是否靠近画面边缘
        if (obj.x < 0  || (obj.x + obj.w) > (IMAGE_WIDTH - 10))
        {
            continue; // 跳过靠近边缘的框
        }

        // 将未被抑制的框移动到 write_index 位置
        p_out->objs[write_index++] = p_out->objs[i];

        for (hi_s32 j = i + 1; j < p_out->count; ++j)
        {
            if (is_suppressed[j])
                continue;

            // 仅对同类的检测框进行 IoU 计算
            if (p_out->objs[i].class_id == p_out->objs[j].class_id)
            {
                hi_float iou = calculate_iou(p_out->objs[i], p_out->objs[j]);
                if (iou > NMS_THRESHOLD)
                {
                    is_suppressed[j] = true; // 标记为抑制
                }
            }
        }
    }

    // 更新 count 为实际保留的框数
    p_out->count = write_index;
}

// 校准检测框坐标,yolo输入图像宽，高，原始图像宽，高

// YOLO 输出解析
const void parse_yolo(const hi_float *src, hi_u32 len, const std::vector<hi_float> &anchors,
                      hi_s32 grid_size, hi_s32 stride, hi_s32 height, stYolov5Objs *p_out,hi_s32 num_outputs)
{

    hi_s32 num_anchors = anchors.size() / 2;

    // 确保数据长度足够
    if (len < grid_size * grid_size * num_anchors * num_outputs)
    {
        std::cerr << "数据长度不足，无法完成 YOLO 解析。" << std::endl;
        return;
    }

    const hi_float *data = src; // 使用 data 指针，避免重复偏移计算
    for (hi_s32 i = 0; i < grid_size * grid_size * num_anchors; ++i, data += num_outputs)
    {
        if (p_out->count >= OBJDETECTMAX)
            break;

        hi_s32 anchor_idx = i / (grid_size * grid_size);
        hi_s32 grid_x = (i % (grid_size * grid_size)) % grid_size;
        hi_s32 grid_y = (i % (grid_size * grid_size)) / grid_size;

        // 直接从 data 中取值，不需要重复计算偏移
        hi_float dx = sigmoid(data[0]);
        hi_float dy = sigmoid(data[1]);
        hi_float dw = sigmoid(data[2]);
        hi_float dh = sigmoid(data[3]);
        // 这里解析中心点x,y坐标的方程和官方给出的不一样，不知道为什么
        hi_float box_x = (2.f * dx - 0.5f + grid_x) * stride;
        hi_float box_y = (2.f * dy - 0.5f + grid_y) * stride;
        hi_float box_w = std::pow(dw * 2.f, 2) * anchors[anchor_idx * 2];
        hi_float box_h = std::pow(dh * 2.f, 2) * anchors[anchor_idx * 2 + 1];
        hi_float min_x = box_x - box_w / 2;
        hi_float min_y = box_y - box_h / 2;

        hi_float objectness = sigmoid(data[4]);
        hi_s32 class_id = 0;
        hi_float max_class_score = 0;
        for (hi_s32 j = 5; j < num_outputs; ++j)
        {
            hi_float class_score = sigmoid(data[j]);
            if (class_score > max_class_score)
            {
                max_class_score = class_score;
                class_id = j - 5;
            }
        }
        hi_float confidence = objectness * max_class_score;
        //hi_float confidence = objectness * sigmoid(data[5]);
        if (confidence > CONFIDENCE_THRESHOLD)
        {
            p_out->objs[p_out->count].x = min_x;
            p_out->objs[p_out->count].y = min_y;
            p_out->objs[p_out->count].w = box_w;
            p_out->objs[p_out->count].h = box_h;
            p_out->objs[p_out->count].class_id = class_id;
            p_out->objs[p_out->count].score = confidence;
            p_out->count++;
        }
    }
}

const void parse_yolo_train(const hi_float *src, hi_u32 len, const std::vector<hi_float> &anchors,
                            hi_s32 grid_size, hi_s32 stride, hi_s32 height, stYolov5Objs *p_out, hi_s32 num_outputs)
{
    // if(grid_size == 80)return;

    hi_s32 num_anchors = anchors.size() / 2;

    // 确保数据长度足够
    if (len < grid_size * grid_size * num_anchors * num_outputs)
    {
        std::cerr << "数据长度不足，无法完成 YOLO 解析。" << std::endl;
        return;
    }

    const hi_float *data = src; // 使用 data 指针，避免重复偏移计算
    for (hi_s32 i = 0; i < grid_size * grid_size * num_anchors; ++i, data += num_outputs)
    {
        if (p_out->count >= OBJDETECTMAX)
            break;

        //
        hi_float box_x = data[0];
        hi_float box_y = data[1];
        hi_float box_w = data[2];
        hi_float box_h = data[3];
        hi_float min_x = box_x - box_w / 2;
        hi_float min_y = box_y - box_h / 2;

        hi_float objectness = data[4];
        hi_s32 class_id = 0;
        hi_float max_class_score = 0;
        for (hi_s32 j = 5; j < num_outputs; ++j)
        {
            hi_float class_score = data[j];
            if (class_score > max_class_score)
            {
                max_class_score = class_score;
                class_id = j - 5;
            }
        }

        hi_float confidence = objectness * max_class_score;
        if (confidence > CONFIDENCE_THRESHOLD)
        {
            p_out->objs[p_out->count].x = min_x;
            p_out->objs[p_out->count].y = min_y;
            p_out->objs[p_out->count].w = box_w;
            p_out->objs[p_out->count].h = box_h;
            p_out->objs[p_out->count].center_x = box_x;
            p_out->objs[p_out->count].center_y = box_y;
            p_out->objs[p_out->count].x_f = min_x;
            p_out->objs[p_out->count].y_f = min_y;
            p_out->objs[p_out->count].w_f = box_w;
            p_out->objs[p_out->count].h_f = box_h;
            p_out->objs[p_out->count].center_x_f = box_x;
            p_out->objs[p_out->count].center_y_f = box_y;
            p_out->objs[p_out->count].class_id = class_id;
            p_out->objs[p_out->count].score = confidence;
            p_out->count++;
            // std::cout << "x:" << box_x << " y:" << box_y << " w:" << box_w << " h:" << box_h << " label:" << class_id << " score:" << confidence << std::endl;
        }
    }
}

const void parse_yolo1(const hi_float *src, hi_u32 len, const std::vector<hi_float> &anchors,
                      hi_s32 grid_size, hi_s32 stride, hi_s32 height, stYolov5Objs *p_out,hi_s32 num_outputs)
{
    //if(grid_size == 80)return;

    hi_s32 num_anchors = anchors.size() / 2;

    // 确保数据长度足够
    if (len < grid_size * grid_size * num_anchors * num_outputs)
    {
        std::cerr << "数据长度不足，无法完成 YOLO 解析。" << std::endl;
        return;
    }

    const hi_float *data = src; // 使用 data 指针，避免重复偏移计算
    for (hi_s32 i = 0; i < grid_size * grid_size * num_anchors; ++i, data += num_outputs)
    {
        if (p_out->count >= OBJDETECTMAX)
            break;

        hi_s32 anchor_idx = i / (grid_size * grid_size);
        hi_s32 grid_x = (i % (grid_size * grid_size)) % grid_size;
        hi_s32 grid_y = (i % (grid_size * grid_size)) / grid_size;

        // 直接从 data 中取值，不需要重复计算偏移
        hi_float dx = sigmoid(data[0]);
        hi_float dy = sigmoid(data[1]);
        hi_float dw = data[2];
        hi_float dh = data[3];
        //
        hi_float box_x = (2.f * dx - 0.5f) + grid_x * stride;
        hi_float box_y = (2.f * dy - 0.5f) + grid_y * stride;
        hi_float box_w = dw;
        hi_float box_h = dh;;
        hi_float min_x = box_x - box_w / 2;
        hi_float min_y = box_y - box_h / 2;
        

        hi_float objectness = sigmoid(data[4]);
        hi_s32 class_id = 0;
        hi_float max_class_score = 0;
        for (hi_s32 j = 5; j < num_outputs; ++j)
        {
            hi_float class_score = sigmoid(data[j]);
            if (class_score > max_class_score)
            {
                max_class_score = class_score;
                class_id = j - 5;
            }
        }

        hi_float confidence = objectness * max_class_score;
        if (confidence > CONFIDENCE_THRESHOLD)
        {
            p_out->objs[p_out->count].x = min_x;
            p_out->objs[p_out->count].y = min_y;
            p_out->objs[p_out->count].w = box_w;
            p_out->objs[p_out->count].h = box_h;
            p_out->objs[p_out->count].class_id = class_id;
            p_out->objs[p_out->count].score = confidence;
            p_out->count++;
            //std::cout << "x:" << box_x << " y:" << box_y << " w:" << dw << " h:" << dh << " label:" << class_id << " score:" << confidence << std::endl;
        }
    }
}

// 适用原版yolo
void yolo_detect(const hi_float *src, hi_u32 len, stYolov5Objs *p_out)
{
    //std::cout<< "yolo_detect" << std::endl;
    //long start_time = getm();
    std::vector<std::vector<hi_float>> anchors = {
        {10.f, 13.f, 16.f, 30.f, 33.f, 23.f},
        {30.f, 61.f, 62.f, 45.f, 59.f, 119.f},
        {116.f, 90.f, 156.f, 198.f, 373.f, 326.f}
    };
    if (len == 3 * 80 * 80 * 85)
    { 
        //parse_yolo(src, len, anchors, 80, 8, INPUT_HEIGHT, p_out, 85);
    }
    else if (len == 3 * 40 * 40 * 85)
    {
        parse_yolo(src, len, anchors[1], 40, 16, INPUT_HEIGHT, p_out, 85);
    }
    else if (len == 3 * 20 * 20 * 85)
    {
        parse_yolo(src, len, anchors[2], 20, 32, INPUT_HEIGHT, p_out, 85);
    }
    else
    {
        std::cout << "yolo_detect unknown" << std::endl;
        return;
    }
    //long end_time = getm();
    //std::cout << "time2: " << (end_time - start_time) << "ms" << std::endl;
}

// 火车测试用nnn
// 适用于火车测试 yolo—seg版 
void yolo_detect_train(const hi_float *src, hi_u32 len, stYolov5Objs *p_out)
{

    //std::cout<< "yolo_detect" << std::endl;
    //long start_time = getm();
    td_s32 class_number = 7;
    td_s32 num_outputs = class_number + 5;
    // 检查输入长度是否符合预期
    if(len != 3 * num_outputs * (80 * 80 + 40 * 40 + 20 * 20)) {
        std::cout << "yolo_detect1 unknown" << std::endl;
        return;
    }

    // 定义 YOLO 的锚点
    std::vector<std::vector<hi_float>> anchors = {
        {10.f, 13.f, 16.f, 30.f, 33.f, 23.f},
        {30.f, 61.f, 62.f, 45.f, 59.f, 119.f},
        {116.f, 90.f, 156.f, 198.f, 373.f, 326.f}
    };

    hi_s32 grid_sizes[3] = {80, 40, 20};
    const hi_float *src_current = src;

    // 遍历每个特征层
    for(hi_s32 i = 0; i < 3; ++i) {
        hi_s32 layer_length = 3 * num_outputs * grid_sizes[i] * grid_sizes[i];
        
        // 解析每层特征图
        parse_yolo_train(src_current, layer_length, anchors[i], grid_sizes[i], 640 / grid_sizes[i], INPUT_HEIGHT, p_out, num_outputs);
        
        // 更新 src 指针，指向下一个特征层的起始位置
        src_current += layer_length;
    }

    //long end_time = getm();
    //std::cout << "time2: " << (end_time - start_time) << "ms" << std::endl;
}



// 适用shv29 使用npu
void yolo_detect1(const hi_float *src, hi_u32 len, stYolov5Objs *p_out)
{

    //std::cout<< "yolo_detect" << std::endl;
    //long start_time = getm();

    // 检查输入长度是否符合预期
    if(len != 3 * 7 * (80 * 80 + 40 * 40 + 20 * 20)) {
        return;
    }

    // 定义 YOLO 的锚点
    std::vector<std::vector<hi_float>> anchors = {
        {10.f, 13.f, 16.f, 30.f, 33.f, 23.f},
        {30.f, 61.f, 62.f, 45.f, 59.f, 119.f},
        {116.f, 90.f, 156.f, 198.f, 373.f, 326.f}
    };

    hi_s32 grid_sizes[3] = {80, 40, 20};
    const hi_float *src_current = src;

    // 遍历每个特征层
    for(hi_s32 i = 0; i < 3; ++i) {
        hi_s32 layer_length = 3 * 7 * grid_sizes[i] * grid_sizes[i];
        
        // 解析每层特征图
        parse_yolo1(src_current, layer_length, anchors[i], grid_sizes[i], 640 / grid_sizes[i], INPUT_HEIGHT, p_out, 7);
        
        // 更新 src 指针，指向下一个特征层的起始位置
        src_current += layer_length;
    }

    //long end_time = getm();
    //std::cout << "time2: " << (end_time - start_time) << "ms" << std::endl;
}

// 使用shv29 使用svp npu
void yolo_detect2(const hi_float *src, hi_u32 stride_offset, hi_s32 total_valid_num, stYolov5Objs *p_out){

    for(hi_s32 i = 0; i < total_valid_num; i++){
        hi_float l = *(src + i + 0 * stride_offset);
        hi_float t = *(src + i + 1 * stride_offset);
        hi_float r = *(src + i + 2 * stride_offset);
        hi_float b = *(src + i + 3 * stride_offset);
        hi_float conf = sigmoid(*(src + i + 4 * stride_offset));
        hi_float class_id = *(src + i + 5 * stride_offset);

        hi_float box_w = r - l;
        hi_float box_h = b - t;
        hi_float center_x = (r + l) / 2;
        hi_float center_y = (b + t) / 2;
        if(conf > 0.6f){
            p_out->objs[p_out->count].x = l;
            p_out->objs[p_out->count].y = t;
            p_out->objs[p_out->count].w = box_w;
            p_out->objs[p_out->count].h = box_h;
            p_out->objs[p_out->count].x_f = l;
            p_out->objs[p_out->count].y_f = t;
            p_out->objs[p_out->count].w_f = box_w;
            p_out->objs[p_out->count].h_f = box_h;
            p_out->objs[p_out->count].center_x = center_x;
            p_out->objs[p_out->count].center_y = center_y;
            p_out->objs[p_out->count].center_x_f = center_x;
            p_out->objs[p_out->count].center_y_f = center_y;
            p_out->objs[p_out->count].class_id = class_id;
            p_out->objs[p_out->count].score = conf;
            p_out->count++;
            // std::cout << " class_id" << class_id << " conf:" << conf << std::endl;

        }
        //nms_process_in_place_and_correct_boxes(p_out);       
    }
    // std::cout << " p_out.count:" << p_out->count << std::endl;
    
}