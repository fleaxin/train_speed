#include <vector>
#include <cstdio>
#include <cstdlib>

#include "yolov5.h"
#include "ot_common_video.h"
#include "train_speed.h"
#include "train_speed_origin.h"
#include "camera_test.h"
#include <iostream>

long get_time()
{
    struct timeval start;
    gettimeofday(&start, NULL);
    long ms = (start.tv_sec) * 1000 + (start.tv_usec) / 1000;
    return ms;
}
// 从CSV加载结构体数组
std::vector<stYolov5Objs> load_yolo_objs(const char* filename) {
    std::vector<stYolov5Objs> result;
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Failed to open file: %s\n", filename);
        return result;
    }
    char buffer[1024];
    stYolov5Objs current = {0};
    hi_s32 last_frame_idx = -1;
    
    // 跳过头部
    fgets(buffer, sizeof(buffer), fp);
    
    while (fgets(buffer, sizeof(buffer), fp)) {
        hi_s32 frame_idx, obj_index, obj_count;
        stObjinfo obj;
        
        // 解析CSV行
        int parsed = sscanf(buffer,
            "%d,%d,%d,%d,%d,%d,%d,%d,%d,%f,%f,%f,%f,%f,%f,%d,%f",
            &frame_idx,
            &obj_index,
            &obj_count,
            &obj.x, &obj.y,
            &obj.w, &obj.h,
            &obj.center_x, &obj.center_y,
            &obj.x_f, &obj.y_f,
            &obj.w_f, &obj.h_f,
            &obj.center_x_f, &obj.center_y_f,
            &obj.class_id, 
            &obj.score);
            
        if (parsed != 17) {
            printf("Error parsing line: %s", buffer);
            continue;
        }
        
        // 新结构体开始
        if (frame_idx != last_frame_idx) {
            if (last_frame_idx != -1) {
                result.push_back(current); // 自动扩展容量
            }
            
            // 初始化新结构体
            memset(&current, 0, sizeof(stYolov5Objs));
            current.count = obj_count;
            last_frame_idx = frame_idx;
        }
        
        // 添加对象到当前结构体
        if (obj_index < OBJDETECTMAX) {
            current.objs[obj_index] = obj;
        } else {
            printf("Object index overflow: %d\n", obj_index);
        }
    }
    
    // 保存最后一个结构体
    if (last_frame_idx != -1) {
        result.push_back(current);
    }
    
    fclose(fp);
    return result;
}

int main() {
    const char* filename = "yolov5_objs_new.csv";

    ot_video_frame_info frame_info;
    frame_info.video_frame.width = 2448;
    frame_info.video_frame.height = 1200;
    frame_info.video_frame.time_ref = 2;
    SpeedData speed_data;
    // 加载数据
    std::vector<stYolov5Objs> loaded = load_yolo_objs(filename);
    long time_relative = get_time();
    long time_current = get_time();
    train_speed_load_carriage_info();
    // printf("\nLoaded %zu structures:\n", loaded.size());
    for (size_t i = 0; i < loaded.size(); i++) {
        frame_info.video_frame.time_ref = i * 2;
        // printf("Structure %zu has %d objects:\n", i, loaded[i].count);
        train_speed_match_new_count_test(&loaded[i], &frame_info, &speed_data);
        time_current = get_time();
        std::cout << "Time cost: " << time_current - time_relative << "ms" << std::endl;
        time_relative = time_current;
    }
    
    return 0;
}