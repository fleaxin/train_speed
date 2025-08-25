#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <unordered_map>
#include <numeric>

#include "camera_test.h"
#include "train_speed.h"
#include "yolov5.h"
// #include "global_logger.h"

using namespace std;
const int TARGET_NUMBER = 8;
const int IS_TO_RIGHT = -1; // 1为向右运动 -1为向左
const int frame_rate = 7; //定义输入帧率用于计算速度  后续可改为使用视频时间戳

vector<pair<int, int>> res_latch;
vector<pair<int, int>> res_margin;
vector<pair<int, int>> res_edge;
vector<pair<int, int>> res_edge_b;
vector<pair<int, int>> res_handle;
vector<pair<int, int>> res_mutex;
vector<pair<int, int>> res_head;
vector<pair<int, int>> res_button;
vector<pair<int, int>> res_lock;

vector<vector<int>> res_latch_test;
vector<vector<int>> res_margin_test;
vector<vector<int>> res_edge_test;
vector<vector<int>> res_edge_b_test;
vector<vector<int>> res_handle_test;
vector<vector<int>> res_mutex_test;
vector<vector<int>> res_head_test;
vector<vector<int>> res_button_test;
vector<vector<int>> res_lock_test;

vector<pair<float, float>> res_latch_match;
vector<pair<float, float>> res_margin_match;
vector<pair<float, float>> res_edge_match;
vector<pair<float, float>> res_edge_b_match;
vector<pair<float, float>> res_handle_match;
vector<pair<float, float>> res_mutex_match;
vector<pair<float, float>> res_head_match;
vector<pair<float, float>> res_button_match;
vector<pair<float, float>> res_lock_match;

vector<cv::Point> detection_poly;
vector<cv::Point> train_head_poly;
vector<cv::Point> train_edge_poly;

vector<float> speed_f;
float train_speed_frame = 0; // 一帧图像中运动速度
float train_speed_s = 0;     // 一秒内累计速度
float train_speed_show = 0;
float train_speed_show_;
bool is_first = true;
int frame_count = 0;
int frame_count_end = 0;
bool is_find_edge = false;
int carriage_number = 0;
float current_distance = 0;
float move_distance = 0;
int find_edge_count = 0;
int not_edge_count = 0;
bool is_carriage_head = false;
bool is_carriage_tail = false;
bool is_head_output = false;
bool is_tail_output = false;
bool start_count = false;
int lose_count_after_start = 0;// 丢失帧数计数
vector<float> train_distance_list;      // 存储每节车厢移动像素
vector<pair<int,int>> train_frame_list;         // 存储每节车厢的起始帧号
td_bool is_in_carriage = TD_FALSE; // 当前画面是否在一节车厢内

bool is_edge_entered = false;
const float ZONE_THRESHOLD = 150.0f; // 区域范围
int is_left_entry = 0;    // edge_b是否是从画面左侧进入判定区域 1为左侧 -1为右侧 0为未进入

vector<pair<float,float>> edge_head_tail = {{NAN,NAN}};  // 记录每节车厢头和尾部 edge在总移动距离(move_distance)的位置

float to_last_tail_distance = NAN;
float to_current_head_distance = NAN;
float to_current_tail_distance = NAN;
float to_next_head_distance = NAN;

int start_frame = 0;
int end_frame = 0;
vector<int> class_count(TARGET_NUMBER,0);

pair<int,int> last_box_id={-1,-1}; // 记录上一帧的检测结果 <类别号，序号>
vector<vector<float>> class_dx_ratio(7);
vector<vector<pair<float,float>>> margin_distance;

vector<pair<float,int>> left_minus_right(TARGET_NUMBER);

vector<string> carriage_type;

struct CarriageInfo1 {
    std::string model_name;
    td_float length_px;
    td_float length_meter;
    td_float px_to_meter_ratio;
    td_float first_edge_to_head;
    td_float second_edge_to_tail;
    vector<td_float> scale_ratio;
};
// 使用哈希表存储车厢信息
std::unordered_map<std::string, CarriageInfo1> carriage_info;

CarriageInfo1 UNKONWN_CARRIAGE_INFO;// 备用的未知车厢信息结构体,避免错误

// 读取车厢信息并保存到哈希表中
td_s32 train_speed_load_carriage_info() {
    const GlobalConfig *g_config = get_global_config();
    td_s32 carriage_number = g_config->carriage_info_list.carriage_num;
    for (td_s32 i = 0; i < carriage_number; i++) { 
        const CarriageInfo* carriage = &g_config->carriage_info_list.carriage[i];
        CarriageInfo1 carriage_info1;
        carriage_info1.model_name = carriage->model_name;
        cout<<"model_name: "<<carriage_info1.model_name<<endl;
        carriage_info1.length_px = carriage->length_px;
        carriage_info1.length_meter = carriage->length_meter;
        carriage_info1.px_to_meter_ratio = carriage->px_to_meter_ratio;
        carriage_info1.first_edge_to_head = carriage->first_edge_to_head;
        carriage_info1.second_edge_to_tail = carriage->second_edge_to_tail;
        for (int i = 0; i < TARGET_NUMBER; ++i) {
            carriage_info1.scale_ratio.push_back(carriage->scale_ratio[i]);
        }
        
        // 保存数据到map中 
        carriage_info[carriage_info1.model_name] = carriage_info1;
    }

    UNKONWN_CARRIAGE_INFO.model_name = "UNKNOWN";
    UNKONWN_CARRIAGE_INFO.length_meter = 14;
    UNKONWN_CARRIAGE_INFO.length_px = 22000;
    UNKONWN_CARRIAGE_INFO.first_edge_to_head = 0;
    UNKONWN_CARRIAGE_INFO.second_edge_to_tail = 0;
    UNKONWN_CARRIAGE_INFO.px_to_meter_ratio = 0.0006099;
    
    UNKONWN_CARRIAGE_INFO.scale_ratio = {1, 0.979926328, 0.979525876, 1.005141278, 0.984851702, 0.913627385, 0.973730976, 0.980087347};
        
    return TD_SUCCESS;
    
}

td_s32 train_speed_load_carriage_type(SetModle *set_modle){
    td_s32 carriage_number = set_modle->carriage_number;
    carriage_type.clear();
    for(int i = 0; i < carriage_number; i++){
        std::string str(set_modle->carriage_mode[i]);
        carriage_type.push_back(str);
    }
}

td_s32 train_speed_reset_data(){
    res_latch_match.clear();
    res_margin_match.clear();
    res_edge_match.clear();
    res_edge_b_match.clear();
    res_handle_match.clear();
    res_mutex_match.clear();
    res_head_match.clear();
    res_button_match.clear();

    carriage_info.clear();

    speed_f.clear();
    train_speed_frame = 0;
    train_speed_s = 0;
    train_speed_show = 0;
    train_speed_show_ = 0;
    is_first = true;
    frame_count = 0;
    frame_count_end = 0;
    is_find_edge = false;
    carriage_number = 0;
    current_distance = 0;
    move_distance = 0;
    find_edge_count = 0;
    not_edge_count = 0;
    is_carriage_head = false;
    is_carriage_tail = false;
    start_count = false;
    lose_count_after_start = 0;
    train_distance_list.clear();
    train_frame_list.clear();
    is_in_carriage = TD_FALSE;
    is_edge_entered = false;

    edge_head_tail = {{NAN,NAN}};
    to_last_tail_distance = NAN;
    to_current_head_distance = NAN;
    to_current_tail_distance = NAN;
    to_next_head_distance = NAN;

    is_left_entry = 0;
    start_frame = 0;
    end_frame = 0;
    class_count = {0,0,0,0,0,0,0};
    last_box_id = {-1,-1};
    class_dx_ratio.clear();
    margin_distance.clear();
    left_minus_right.clear();

}


float speed_filter(vector<float> &speed_vec, int temp) {
    int vec_size = speed_vec.size();  // 获取速度向量的大小
    int count = vec_size / temp;      // 计算需要分组的数量
    float speed_sum{0};               // 初始化速度总和
    vector<float> vec_temp;           // 临时向量，用于存储每组的速度值

    // 遍历速度向量
    for (auto i = 0; i < vec_size; i++) {
//            cout << speed_vec[i] << " ";  // 输出当前速度值（被注释掉的部分）
        vec_temp.push_back(speed_vec[i]);  // 将当前速度值加入临时向量

        // 如果当前组已经填满
        if ((i + 1) % temp == 0) {
            std::sort(vec_temp.begin(), vec_temp.end());  // 对临时向量进行排序
            float speed_sum_{0};  // 初始化当前组的速度总和

            // 遍历临时向量，去掉最小值和最大值后求和
            for (auto j = 1; j < temp - 1; j++) {
                speed_sum_ += vec_temp[j];
            }

            // 计算当前组的平均速度，并累加到总速度中
            speed_sum += ((speed_sum_ / (temp - 2)) * temp);
            vec_temp.clear();  // 清空临时向量，准备下一组
        }
    }
    return speed_sum;  // 返回总速度
}

float simple_moving_average(float new_value) {
    static std::vector<float> history;  // 静态存储历史数据
    constexpr size_t window_size = frame_rate*2;   // 固定窗口大小
    
    // 添加新数据
    if(abs(new_value)>2){// 过滤抖动值
        history.push_back(new_value);
    }else{
        history.push_back(0);
    }
    
    // 保持固定窗口大小
    if (history.size() > window_size) {
        history.erase(history.begin());
    }
    
    // 计算平均值
    return std::accumulate(history.begin(), history.end(), 0.0f) / history.size();
}

void get_move_distance(vector<pair<int, int>> &current_res,
                       vector<pair<int, int>> &last_res,
                       vector<int> &result_vec,
                       const string &location_name,
                       int location_y
                    )
{
    /*
    检测条件：1、认为车辆从右往左移动，坐标值越小越靠近左边
             2、同一类坐标框传入之前进行了排序，序号越小，坐标轴越小，越靠近左边
    检测逻辑：
    */
    cout << "start get_move_distance" << endl;
    cout << "location name :" << location_name << endl;   
    // 如果当前检测结果数量大于1，进行去重处理
    if (current_res.size() > 1)
    {
        for (auto i = 1; i < current_res.size(); i++)
        {
            // 如果两个检测结果之间的距离小于 800，则删除这两个结果
            if (abs(current_res[i - 1].first - current_res[i].first) < 800)
            {
                current_res.erase(current_res.begin() + i);
                current_res.erase(current_res.begin() + i - 1);
            }
        }
    }
    // 如果当前检测结果为空，直接返回
    if (current_res.empty())
    {
        return;
    }
    // 如果当前检测结果数量与上一帧检测结果数量相同
    if (current_res.size() == last_res.size())
    {
        // 如果帧第一个检测框在上一帧第一个检测框的左边，且距离小于50，则认为这两个检测框相同，计算移动距离并返回
        if ((current_res[0].first - 50) <= last_res[0].first)
        {
            // 遍历所有检测结果，计算移动距离并在图像上绘制文本
            for (auto i = 0; i < current_res.size(); i++)
            {
                // 根据检测结果的位置，计算移动距离并存入结果向量
                if (current_res[i].first < 100)// 如果当前检测框左边缘小于100，即目标可能没有完全出现在视频中，
                {
                    if (last_res[i].first + last_res[i].second - current_res[i].first -
                            current_res[i].second <200)
                    {
                        result_vec.push_back(last_res[i].second - current_res[i].second);
                    }
                }
                else if (current_res[i].second > 2348)
                {
                    if (last_res[i].first - current_res[i].first < 200)
                    {
                        result_vec.push_back(last_res[i].first - current_res[i].first);
                    }
                }
                else
                {
                    result_vec.push_back(last_res[i].first - current_res[i].first);
                    result_vec.push_back(last_res[i].second - current_res[i].second);
                }
                cout << "last_width: " << last_res[i].second - last_res[i].first << endl;
                cout << "current_width: " << current_res[i].second - current_res[i].first << endl;
            }
        }
        else // 如果帧第一个检测框和上一帧第一个检测框距离大于50，则认为这两个检测框不同
        {
            if (last_res.size() == 1)// 如果上一帧检测结果数量只有一个，说明当前帧和上一帧没有匹配的框，返回
            {
                
                // cout << "111111111111111111111111111111111" << endl;
            }
            else
            {
                // 遍历计算当前第i个和上一帧第i+1个的移动距离，并将结果存入结果向量
                for (auto i = 0; i < current_res.size() - 1; i++)
                {
                    
                    // 根据检测结果的位置，计算移动距离并存入结果向量
                    if (current_res[i].first < 100)
                    {
                        if (last_res[i + 1].first + last_res[i + 1].second -
                                current_res[i].first -current_res[i].second < 200)
                        {
                            result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                        }
                    }
                    else if (current_res[i].second < 2348)
                    {
                        if (last_res[i + 1].first - current_res[i + 1].first < 200)
                        {
                            result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                        }
                    }
                    else
                    {
                        result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                        result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                    }
                }
            }
        }
    }
    else if (current_res.size() > last_res.size())// 如果当前检测结果数量大于上一帧检测结果数量
    {
        if (current_res[0].first <= last_res[0].first)// 如果当前第一个在上一帧第一个检测框的左边
        {
            for (auto i = 0; i < last_res.size(); i++)// 遍历计算当前第i个和上一帧第i个的移动距离，并将结果存入结果向量
            {
                if (current_res[i].first < 100)
                {
                    if (last_res[i].first + last_res[i].second - current_res[i].first -
                            current_res[i].second < 200)
                    {
                        result_vec.push_back(last_res[i].second - current_res[i].second);
                    }
                }
                else if (current_res[i].second < 2348)
                {
                    if (last_res[i].first - current_res[i].first < 200)
                    {
                        result_vec.push_back(last_res[i].first - current_res[i].first);
                    }
                }
                else
                {
                    result_vec.push_back(last_res[i].first - current_res[i].first);
                    result_vec.push_back(last_res[i].second - current_res[i].second);
                }
            }
        }
        else// 如果当前第一个在上一帧第一个检测框的右边，可能认为上一帧，第一个检测框已经移出画面
        {
            for (auto i = 0; i < last_res.size() - 1; i++)
            {
                if (current_res[i].first < 100)
                {
                    if (last_res[i + 1].first + last_res[i + 1].second - current_res[i].first -
                            current_res[i].second < 200)
                    {
                        result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                    }
                }
                else if (current_res[i].second < 2348)
                {
                    if (last_res[i + 1].first - current_res[i].first < 200)
                    {
                        result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                    }
                }
                else
                {
                    result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                    result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                }
            }
        }
    }
    else// 如果当前检测结果数量少于上一帧检测结果数量，可能认为上一帧中左边第一个当前已经移出画面，所以使用当前帧第i个和上一帧第i+1个计算移动距离
    {
        for (auto i = 0; i < current_res.size() - 1; i++)
        {
            if (current_res[i].first < 100)
            {
                if (last_res[i + 1].first + last_res[i + 1].second - current_res[i].first -
                        current_res[i].second < 200)
                {
                    result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                }
            }
            else if (current_res[i].second < 2348)
            {
                if (last_res[i + 1].first - current_res[i].first < 200)
                {
                    result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                }
            }
            else
            {
                result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                result_vec.push_back(last_res[i + 1].second - current_res[i].second);
            }
        }
    }
}
void get_move_distance_test(vector<vector<int>> &current_res,
                       vector<vector<int>> &last_res,
                       vector<int> &result_vec,
                       const string &location_name,
                       int location_y)
{
    /*
    检测条件：1、认为车辆从右往左移动，坐标值越小越靠近左边
    2、同一类坐标框传入之前进行了排序，序号越小，坐标轴越小，越靠近左边
    检测逻辑：
    */
    cout << "start get_move_distance" << endl;
    cout << "location name :" << location_name << endl;
    // 如果当前检测结果数量大于1，进行去重处理
    if (current_res.size() > 1)
    {
        for (auto i = 1; i < current_res.size(); i++)
        {
            // 如果两个检测结果之间的距离小于 800，则删除这两个结果
            if (abs(current_res[i - 1][0] - current_res[i][0]) < 800)
            {
                current_res.erase(current_res.begin() + i);
                current_res.erase(current_res.begin() + i - 1);
            }
        }
    }
    // 如果当前检测结果为空，直接返回
    if (current_res.empty())
    {
        return;
    }
    // 如果当前检测结果数量与上一帧检测结果数量相同
    if (current_res.size() == last_res.size())
    {
        // 如果帧第一个检测框在上一帧第一个检测框的左边，且距离小于50，则认为这两个检测框相同，计算移动距离并返回
        if ((current_res[0][0] - 50) <= last_res[0][0])
        {
            // 遍历所有检测结果，计算移动距离并在图像上绘制文本
            for (auto i = 0; i < current_res.size(); i++)
            { 
                result_vec.push_back(last_res[i][0] - current_res[i][0]);
                cout << "last_res[i]    :" << last_res[i][0] << " " << last_res[i][1] <<" "<<last_res[i][2] << endl;
                cout << "current_res[i] :" << current_res[i][0] << " " << current_res[i][1] <<" "<<current_res[i][2] << endl;
            }
        }
        else // 如果帧第一个检测框和上一帧第一个检测框距离大于50，则认为这两个检测框不同
        {
            if (last_res.size() == 1) // 如果上一帧检测结果数量只有一个，说明当前帧和上一帧没有匹配的框，返回
            {
            }
            else
            {
                // 遍历计算当前第i个和上一帧第i+1个的移动距离，并将结果存入结果向量
                for (auto i = 0; i < current_res.size() - 1; i++)
                {

                        if (last_res[i + 1][0] - current_res[i + 1][0] < 200)
                        {
                            result_vec.push_back(last_res[i + 1][0] - current_res[i][0]);
                            cout << "last_res[i + 1] :" << last_res[i + 1][0] << " " << last_res[i + 1][1] <<" "<<last_res[i + 1][2] << endl;
                            cout << "current_res[i]  :" << current_res[i][0] << " " << current_res[i][1] <<" "<<current_res[i][2] << endl;
                        }
                }
            }
        }
    }
    else if (current_res.size() > last_res.size()) // 如果当前检测结果数量大于上一帧检测结果数量
    {
        if (current_res[0][0] <= last_res[0][0]) // 如果当前第一个在上一帧第一个检测框的左边
        {
            for (auto i = 0; i < last_res.size(); i++) // 遍历计算当前第i个和上一帧第i个的移动距离，并将结果存入结果向量
            {
                result_vec.push_back(last_res[i][0] - current_res[i][0]);
                cout << "last_res[i]    :" << last_res[i][0] << " " << last_res[i][1] <<" "<<last_res[i][2] << endl;
                cout << "current_res[i] :" << current_res[i][0] << " " << current_res[i][1] <<" "<<current_res[i][2] << endl;
            }
        }
        else // 如果当前第一个在上一帧第一个检测框的右边，可能认为上一帧，第一个检测框已经移出画面
        {
            for (auto i = 0; i < last_res.size() - 1; i++)
            {
                result_vec.push_back(last_res[i + 1][0] - current_res[i][0]);
                cout << "last_res[i + 1] :" << last_res[i + 1][0] << " " << last_res[i + 1][1] <<" "<<last_res[i + 1][2] << endl;
                cout << "current_res[i]  :" << current_res[i][0] << " " << current_res[i][1] <<" "<<current_res[i][2] << endl;
            }
        }
    }
    else // 如果当前检测结果数量少于上一帧检测结果数量，可能认为上一帧中左边第一个当前已经移出画面，所以使用当前帧第i个和上一帧第i+1个计算移动距离
    {
        for (auto i = 0; i < current_res.size() - 1; i++)
        {
            result_vec.push_back(last_res[i + 1][0] - current_res[i][0]);
            cout << "last_res[i + 1] :" << last_res[i + 1][0] << " " << last_res[i + 1][1] <<" "<<last_res[i + 1][2] << endl;
            cout << "current_res[i]  :" << current_res[i][0] << " " << current_res[i][1] <<" "<<current_res[i][2] << endl;
        }
    }
}

Rect get_rect(stObjinfo obj,frame_size frame_size)
{
    Rect rect;
    td_s32 center_x;
    td_s32 center_y;
    td_s32 width = frame_size.w;
    td_s32 height = frame_size.h;
    hi_float ratio = static_cast<float>(width) / 640.0f;

    rect.x = round(obj.x_f * ratio);
    rect.y = round(obj.y_f * ratio);
    rect.width = round(obj.w_f * ratio);
    rect.height = round(obj.h_f * ratio);
    rect.center_x = (obj.x_f + obj.w_f / 2) * ratio;
    rect.center_y = (obj.y_f + obj.h_f / 2) * ratio;

    return rect;
}

td_s32 train_speed(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data){


    cout << "\n\n\n" << endl;
    vector<pair<int, int>> real_res_latch;  // 存储门闩检测结果
    vector<pair<int, int>> real_res_margin; // 存储边缘检测结果
    vector<pair<int, int>> real_res_edge;   // 存储边缘检测结果
    vector<pair<int, int>> real_res_edge_b; // 存储边缘B检测结果
    vector<pair<int, int>> real_res_handle; // 存储把手检测结果
    vector<pair<int, int>> real_res_mutex;  // 存储互斥检测结果
    vector<pair<int, int>> real_res_head;   // 存储头部检测结果
    vector<pair<int, int>> real_res_button; // 存储按钮检测结果
    vector<pair<int, int>> real_res_lock;   // 存储锁检测结果
    vector<int> result_vec_margin;          // 存储边缘检测结果的向量
    int edge_count = 0;                     // 边缘计数器

    vector<cv::Point> detection_poly;
    vector<cv::Point> train_head_poly;
    vector<cv::Point> train_edge_poly;

    frame_size f;
    f.w = frame_info->video_frame.width;
    f.h = frame_info->video_frame.height;

    // TODO: 需要把区域框改为与根据长宽得到的值，而不是固定值
    // 全屏区域，稍微向内收缩
    detection_poly = {{10, 200},
                      {2438, 200},
                      {2438, 1100},
                      {10, 1100}};

    // 画面右半区域，稍微向内收缩
    train_head_poly = {{1224, 200}, 
                       {2438, 200},
                       {2438, 1100},
                       {1224, 1100}}; /// train_edge_poly  train_head_poly

    // 全屏区域，顶部稍微外扩，其余三面稍微向内收缩
    train_edge_poly = {{10, -10},
                       {2438, -10},
                       {2438, 1100},
                       {10, 1100}};

    
    // cout << "frame_size: " << f.w << " " << f.h << endl;
    
    // 遍历检测结果
    for (int i = 0; i < pOut->count; i++)
    {
        Rect r = get_rect(pOut->objs[i], f);
        // cout << "class_id: " << pOut->objs[i].class_id << "  x: " << r.x << " y: " << r.y << " w: " << r.width 
        //      << " h: " << r.height << endl;

        switch (pOut->objs[i].class_id)
        {
        case 0:
            edge_count++; // 如果类别为0，边缘计数器自增
            break;
        case 1:
            real_res_margin.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            break;
        case 2:
            if (cv::pointPolygonTest(detection_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(detection_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_latch.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            }
            break;
        case 3:
            real_res_handle.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            break;
        case 4:
            real_res_mutex.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            break;
        case 5:
            if (cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_head.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            }
            break;
        case 6:
            if (cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_button.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            }
        case 7:
            if (cv::pointPolygonTest(train_head_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_head_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_edge.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            }
            if (cv::pointPolygonTest(detection_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(detection_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_edge_b.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            }
        }
    }

    // 对所有检测结果进行排序
    std::sort(real_res_margin.begin(), real_res_margin.end());
    std::sort(real_res_latch.begin(), real_res_latch.end());
    std::sort(real_res_edge.begin(), real_res_edge.end());
    std::sort(real_res_edge_b.begin(), real_res_edge_b.end());
    std::sort(real_res_handle.begin(), real_res_handle.end());
    std::sort(real_res_mutex.begin(), real_res_mutex.end());
    std::sort(real_res_head.begin(), real_res_head.end());
    std::sort(real_res_button.begin(), real_res_button.end());
    
    // cout << "button:" << real_res_button.size() << endl;
    // cout << "button:" << res_button.size() << endl;
    // cout << "latch:" << real_res_latch.size() << endl;
    // cout << "latch:" << res_latch.size() << endl;
    // cout << "mutex:" << real_res_mutex.size() << endl;
    // cout << "mutex:" << res_mutex.size() << endl;
    // cout << "edge:" << real_res_edge.size() << endl;
    // cout << "edge:" << res_edge.size() << endl;
    // cout << "handle:" << real_res_handle.size() << endl;
    // cout << "handle:" << res_handle.size() << endl;
    // cout << "margin:" << real_res_margin.size() << endl;
    // cout << "margin:" << res_margin.size() << endl;    
    // cout << "edge_b:" << real_res_edge_b.size() << endl;
    // cout << "edge_b:" << res_edge_b.size() << endl;   
    // cout << "head:" << real_res_head.size() << endl;
    // cout << "head:" << res_head.size() << endl;

    /// 测速部分
    int x1, x2;
    if (is_first)
    {
        is_first = false; // 如果是第一次运行，设置标志为false
        cout << "first" << endl;
    }
    else
    {
        // 根据检测到的按钮、门闩、互斥、边缘、把手等对象计算移动距离
        /*优先级：1.button > latch > mutex > edge > handle > margin*/
        if (!real_res_button.empty() and !res_button.empty())
        {
            get_move_distance(real_res_button, res_button, result_vec_margin, "button", 450);
        }
        if (result_vec_margin.empty())
        {
            if (!real_res_latch.empty() and !res_latch.empty())
            {
                get_move_distance(real_res_latch, res_latch, result_vec_margin, "latch", 200);
            }
        }
        if (result_vec_margin.empty())
        {
            if (!real_res_mutex.empty() and !res_mutex.empty())
            {
                get_move_distance(real_res_mutex, res_mutex, result_vec_margin, "mutex", 400);
            }
        }
        if (result_vec_margin.empty())
        {
            if (!real_res_edge.empty() and !res_edge.empty())
            {
                get_move_distance(real_res_edge, res_edge, result_vec_margin, "edge", 300);
            }
        }
        if (result_vec_margin.empty())
        {
            if (!real_res_handle.empty() and !res_handle.empty())
            {
                get_move_distance(real_res_handle, res_handle, result_vec_margin, "handle", 100);
            }
        }
    }
    if (result_vec_margin.empty())
    {
        if (!real_res_margin.empty() and !res_margin.empty())
        {
            get_move_distance(real_res_margin, res_margin, result_vec_margin, "margin", 150);
        }
    }
    if (!result_vec_margin.empty())
    {
        train_speed_frame = 0;                                         // 重置速度帧
        std::sort(result_vec_margin.begin(), result_vec_margin.end()); // 对结果进行排序

        // 如果结果数量大于2，舍去第一个和最后一个值，求平均值
        // TODO: 为什么要舍去第一个值?
        if (result_vec_margin.size() > 2)
        {
            for (int i = 1; i < result_vec_margin.size() - 1; i++)
            {
                train_speed_frame += result_vec_margin[i];
                cout << "result_vec_margin[i] = " << result_vec_margin[i] << endl;
            }
            train_speed_frame = train_speed_frame / (float(result_vec_margin.size() - 2));
        }
        else// 求平均值
        {
            for (const auto &res1 : result_vec_margin)
            {
                train_speed_frame += res1;
            }
            train_speed_frame = train_speed_frame / (float(result_vec_margin.size()));
        }
    }
    else// 如果结果为空，当前没有速度结果
    {
        cout << "no speed result!" << endl;
    }

    // 设置权重
        // float w1 = 0.732;
        float w1 = 0.7;
        train_speed_s += train_speed_frame;   // 累加速度帧
        cout << "train_speed_frame: " << train_speed_frame << endl;
        cout << "train_speed_s: " << train_speed_s << endl;
        speed_f.push_back(train_speed_frame); // 将速度帧加入速度向量
        frame_count++;                        // 帧计数器自增
        res_latch = real_res_latch;           // 更新门闩检测结果
        res_margin = real_res_margin;         // 更新边缘检测结果
        res_handle = real_res_handle;         // 更新把手检测结果
        res_mutex = real_res_mutex;           // 更新互斥检测结果
        res_button = real_res_button;         // 更新按钮检测结果
        if (frame_count == frame_rate)// 因为摄像头帧间隔不等，所以累加一秒后计算平均速度 TODO: 当前帧间隔较为稳定，可以考虑去除掉这部分
        {
            // cout << "test" << endl;
            // 计算并显示实时速度
            train_speed_show = float((train_speed_s * w1 * 3600) / 1000000);
            float s_one = speed_filter(speed_f, 5); // 对速度进行滤波
            if (s_one < 5)
            {
                s_one = 0;
                train_speed_frame = 0;
            }
            /// train_speed_show_ = float((s_one * w1 * 3600) / 1000000);
            train_speed_show_ = float((s_one * w1) / 10);                          // 计算显示速度
            train_speed_s = 0;                                                     // 重置速度累加器
            frame_count = 0;                                                       // 重置帧计数器
            speed_f.clear();                                                       // 清空速度向量
            // long exact_time = get_time_ms();                                       // 获取当前时间
            
        }
        cout << "speed:" << train_speed_show_ << "km/h" << endl;

        // 向共享数据结构体写入数据
        speed_data->speed = train_speed_show_;
        speed_data->time_ref = frame_info->video_frame.time_ref;
        printf("time_ref: %ld\n", frame_info->video_frame.time_ref);
        sem_post(&speed_data->sem);



        /// 定位部分
        /// ============================================================
        /// 功能：通过视频检测火车车厢侧面的标志物（按钮、插销、头部标记等），
        ///       计算车厢序号、移动距离和车厢长度。
        /// 核心逻辑：
        ///   1. 状态机切换：
        ///      - 初始状态（is_find_edge=false）：检测车厢尾部边缘（real_res_edge_b）
        ///        的稳定性（连续5帧检测到边缘后进入跟踪状态）
        ///      - 跟踪状态（is_find_edge=true）：根据标志物位置区分车厢头尾，
        ///        计算车厢长度和累计移动距离
        ///   2. 标志物优先级：
        ///      按钮（real_res_button） > 插销（real_res_latch） > 头部标记（real_res_head）
        ///   3. 距离计算：
        ///      - 通过 train_speed_frame（每帧移动像素）累加计算 current_distance
        ///      - 使用 w1/10 系数将像素距离转换为实际长度（厘米）
        /// 关键参数：
        ///   - 1224：图像参考点（可能为固定检测位置）
        ///   - 252/1191/1269：标志物到参考点的预设像素偏移量
        ///   - 5次检测阈值：确保标志物检测的稳定性
        /// ============================================================

        float distance_rate = 1; // 距离修正系数（默认为1，可扩展为缩放因子）
        string res_s;            // 调试用字符串

        // 遍历结果集（具体逻辑依赖result_vec_margin的定义，可能为边缘检测结果）
        for (auto res_res : result_vec_margin)
        {
            res_s += to_string(res_res) + "    "; // 生成调试信息
        }

        // 计算本帧移动距离（train_speed_frame可能表示每帧的基础移动像素）
        train_speed_frame = train_speed_frame * distance_rate;

        /// ================== 边缘跟踪模式 ==================
        if (is_find_edge)
        {
            current_distance += train_speed_frame; // 持续累加移动距离

            // 要求检测到且仅检测到一个边缘（real_res_edge_b.size() == 1）
            if (real_res_edge_b.size() == 1)
            {
                // 核心判断：边缘框必须覆盖参考点1224（例如图像中心线）
                if (real_res_edge_b[0].first <= 1224 && real_res_edge_b[0].second >= 1224)
                {
                    /// ----- 优先检测按钮标志物 -----
                    if (real_res_button.size() == 1)
                    {
                        // 按钮在边缘左侧：标记为车厢尾部
                        if (real_res_button[0].first <= real_res_edge_b[0].first)
                        {
                            if (!is_carriage_tail)
                            {
                                is_carriage_tail = true;
                                // 当已有车厢时，输出尾部长度（1191为按钮到车厢尾的预设偏移）
                                if (carriage_number > 0)
                                {
                                    cout << "real_res_button Carriage number " << carriage_number
                                         << "  length:  " << (current_distance - real_res_button[0].first + 1191) * w1 / 10
                                         << "  cm" << endl;
                                }
                            }
                        }
                        // 按钮在边缘右侧：标记为新车厢头部
                        else
                        {
                            if (!is_carriage_head)
                            {
                                is_carriage_head = true;
                                // 重置距离计算公式：1224到当前边缘的偏移 + 252（头部到边缘的预设值）
                                current_distance = 1224 - float(real_res_edge_b[0].first) + 252;
                                carriage_number++; // 车厢序号递增
                            }
                        }
                    }

                    /// ----- 其次检测插销标志物 -----
                    else if (real_res_latch.size() == 1)
                    {
                        // 逻辑与按钮类似，1269为插销到车厢尾的预设偏移
                        if (real_res_latch[0].first <= real_res_edge_b[0].first)
                        {
                            // ... (类似按钮处理逻辑)
                        }
                    }

                    /// ----- 最后检测头部标志物 -----
                    else if (!real_res_head.empty())
                    {
                        // 头部在边缘右侧：标记为车厢尾部
                        if (real_res_head[0].first >= real_res_edge_b[0].first)
                        {
                            // ... (输出公式使用 real_res_head[0].second - 1224 计算尾部长度)
                        }
                    }

                    /// ----- 无任何标志物时的处理 -----
                    else
                    {
                        is_carriage_tail = false;
                        is_carriage_head = false;
                        // 直接输出当前累计距离（可能为不完整检测）
                        cout << "find nothing Carriage number " << carriage_number
                             << "  length:  " << current_distance * w1 / 10 << "  cm" << endl;
                    }
                }
            }
            // 边缘检测异常时重置状态
            else
            {
                is_carriage_head = false;
                is_carriage_tail = false;
            }

            /// ----- 边缘丢失处理 -----
            if (real_res_edge_b.empty())
            {
                not_edge_count++; // 连续丢失计数器
                if (not_edge_count == 5)
                { // 连续5帧丢失则认为边缘消失
                    find_edge_count = 0;
                    is_find_edge = false; // 退出跟踪模式
                }
            }
        }

        /// ================== 初始检测模式 ==================
        else
        {
            // 重置车厢状态
            is_carriage_head = false;
            is_carriage_tail = false;

            // 无边缘检测时的处理
            if (real_res_edge_b.empty())
            {
                if (carriage_number > 0)
                {
                    current_distance += train_speed_frame; // 持续累加移动距离
                }
                else
                {
                    cout << "Not find train carriage" << endl; // 初始状态提示
                }
            }
            // 检测到边缘时的稳定性验证
            else
            {
                find_edge_count++;
                // 连续5次检测到边缘才确认有效（防止误检）
                if (find_edge_count == 5)
                {
                    is_find_edge = true; // 进入跟踪模式
                    not_edge_count = 0;
                    // 首节车厢的特殊处理
                    if (carriage_number == 0)
                    {
                        cout << "find first train carriage" << endl;
                    }
                }
                // 已有车厢时持续更新距离
                if (carriage_number > 0)
                {
                    current_distance += train_speed_frame;
                }
            }
        }

        res_edge = real_res_edge;     // 更新边缘检测结果
        res_edge_b = real_res_edge_b; // 更新边缘B检测结果
        res_head = real_res_head;     // 更新头部检测结果

        // 计算并显示当前距离
        cout << "current distance:" << current_distance * w1 / 10 << "cm" << endl;

        // 显示当前车厢编号
        cout << "current carriage number:" << carriage_number << endl;

        // 如果没有检测到任何对象，计数器自增
        if (real_res_margin.empty() and real_res_edge_b.empty() and real_res_latch.empty() and real_res_button.empty() and real_res_handle.empty() and real_res_mutex.empty() and real_res_head.empty())
        {
            frame_count_end++;
            if (frame_count_end >= frame_rate * 10)
            {
                if (carriage_number > 0)
                {

                    cout << "The train has left" << endl;

                }
                frame_count = 0;
                carriage_number = 0;
                train_speed_frame = 0;
                current_distance = 0;
                train_speed_show_ = 0;
            }
        }

        cout << "\n\n\n" << endl;
        return TD_SUCCESS;
}

td_s32 train_speed_test(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data){

    /* 
    原有代码是使用坐标框的左边和右边坐标值计算，但在3403上坐标框大小抖动较为严重，
    所以在这里改为使用中心点坐标，并且用左边和右边做辅助
    */
    cout << "\n\n\n" << endl;
    vector<vector<int>> real_res_latch;  // 存储门闩检测结果
    vector<vector<int>> real_res_margin; // 存储边缘检测结果
    vector<vector<int>> real_res_edge;   // 存储边缘检测结果
    vector<vector<int>> real_res_edge_b; // 存储边缘B检测结果
    vector<vector<int>> real_res_handle; // 存储把手检测结果
    vector<vector<int>> real_res_mutex;  // 存储互斥检测结果
    vector<vector<int>> real_res_head;   // 存储头部检测结果
    vector<vector<int>> real_res_button; // 存储按钮检测结果
    vector<int> result_vec_margin;          // 存储边缘检测结果的向量
    int edge_count = 0;                     // 边缘计数器

    vector<cv::Point> detection_poly;
    vector<cv::Point> train_head_poly;
    vector<cv::Point> train_edge_poly;

    frame_size f;
    f.w = frame_info->video_frame.width;
    f.h = frame_info->video_frame.height;

    // TODO: 需要把区域框改为与根据长宽得到的值，而不是固定值
    // 全屏区域，稍微向内收缩
    detection_poly = {{10, 200},
                      {2438, 200},
                      {2438, 1100},
                      {10, 1100}};

    // 画面右半区域，稍微向内收缩
    train_head_poly = {{1224, 200}, 
                       {2438, 200},
                       {2438, 1100},
                       {1224, 1100}}; /// train_edge_poly  train_head_poly

    // 全屏区域，顶部稍微外扩，其余三面稍微向内收缩
    train_edge_poly = {{10, -10},
                       {2438, -10},
                       {2438, 1100},
                       {10, 1100}};

    
    // cout << "frame_size: " << f.w << " " << f.h << endl;
    
    // 遍历检测结果
    for (int i = 0; i < pOut->count; i++)
    {
        Rect r = get_rect(pOut->objs[i], f);
        cout << "class_id: " << pOut->objs[i].class_id << "  x: " << r.x << " y: " << r.y << " w: " << r.width 
             << " h: " << r.height << endl;

        switch (pOut->objs[i].class_id)
        {
        case 0:
            edge_count++; // 如果类别为0，边缘计数器自增
            if (cv::pointPolygonTest(train_head_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_head_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_edge.push_back({min(max(r.center_x,0),f.w), max(r.x, 0), min(r.x + r.width, f.w)});
            }
            if (cv::pointPolygonTest(detection_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(detection_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_edge_b.push_back({min(max(r.center_x,0),f.w), max(r.x, 0), min(r.x + r.width, f.w)});
            }
            break;
        case 1:
            real_res_margin.push_back({min(max(r.center_x,0),f.w), max(r.x, 0), min(r.x + r.width, f.w)});
            break;
        case 2:
            if (cv::pointPolygonTest(detection_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(detection_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_latch.push_back({min(max(r.center_x,0),f.w), max(r.x, 0), min(r.x + r.width, f.w)});
            }
            break;
        case 3:
            real_res_handle.push_back({min(max(r.center_x,0),f.w), max(r.x, 0), min(r.x + r.width, f.w)});
            break;
        case 4:
            real_res_mutex.push_back({min(max(r.center_x,0),f.w), max(r.x, 0), min(r.x + r.width, f.w)});
            break;
        case 5:
            if (cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_head.push_back({min(max(r.center_x,0),f.w), max(r.x, 0), min(r.x + r.width, f.w)});
            }
            break;
        case 6:
            if (cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_button.push_back({min(max(r.center_x,0),f.w), max(r.x, 0), min(r.x + r.width, f.w)});
            }
            break;
        }
    }

    // 对所有检测结果按照第一个值center_x进行排序
    std::sort(real_res_margin.begin(), real_res_margin.end(), [](const vector<int>& a, const vector<int>& b) {
        return a[0] < b[0];
    });
    std::sort(real_res_latch.begin(), real_res_latch.end(), [](const vector<int>& a, const vector<int>& b) {
        return a[0] < b[0];
    });
    std::sort(real_res_edge.begin(), real_res_edge.end(), [](const vector<int>& a, const vector<int>& b) {
        return a[0] < b[0];
    });
    std::sort(real_res_edge_b.begin(), real_res_edge_b.end(), [](const vector<int>& a, const vector<int>& b) {
        return a[0] < b[0];
    });
    std::sort(real_res_handle.begin(), real_res_handle.end(), [](const vector<int>& a, const vector<int>& b) {
        return a[0] < b[0];
    });
    std::sort(real_res_mutex.begin(), real_res_mutex.end(), [](const vector<int>& a, const vector<int>& b) {
        return a[0] < b[0];
    });
    std::sort(real_res_head.begin(), real_res_head.end(), [](const vector<int>& a, const vector<int>& b) {
        return a[0] < b[0];
    });
    std::sort(real_res_button.begin(), real_res_button.end(), [](const vector<int>& a, const vector<int>& b) {
        return a[0] < b[0];
    });
    
    cout << "button:" << real_res_button.size() << endl;
    cout << "button:" << res_button.size() << endl;
    cout << "latch:" << real_res_latch.size() << endl;
    cout << "latch:" << res_latch.size() << endl;
    cout << "mutex:" << real_res_mutex.size() << endl;
    cout << "mutex:" << res_mutex.size() << endl;
    cout << "edge:" << real_res_edge.size() << endl;
    cout << "edge:" << res_edge.size() << endl;
    cout << "handle:" << real_res_handle.size() << endl;
    cout << "handle:" << res_handle.size() << endl;
    cout << "margin:" << real_res_margin.size() << endl;
    cout << "margin:" << res_margin.size() << endl;    
    cout << "edge_b:" << real_res_edge_b.size() << endl;
    cout << "edge_b:" << res_edge_b.size() << endl;   
    cout << "head:" << real_res_head.size() << endl;
    cout << "head:" << res_head.size() << endl;

    /// 测速部分
    int x1, x2;
    if (is_first)
    {
        is_first = false; // 如果是第一次运行，设置标志为false
        cout << "first" << endl;
    }
    else
    {
        // 根据检测到的按钮、门闩、互斥、边缘、把手等对象计算移动距离
        /*优先级：1.button > latch > mutex > edge > handle > margin*/
        if (!real_res_button.empty() and !res_button_test.empty())
        {
            get_move_distance_test(real_res_button, res_button_test, result_vec_margin, "button", 450);
        }
        if (result_vec_margin.empty())
        {
            if (!real_res_latch.empty() and !res_latch_test.empty())
            {
                get_move_distance_test(real_res_latch, res_latch_test, result_vec_margin, "latch", 200);
            }
        }
        if (result_vec_margin.empty())
        {
            if (!real_res_mutex.empty() and !res_mutex_test.empty())
            {
                get_move_distance_test(real_res_mutex, res_mutex_test, result_vec_margin, "mutex", 400);
            }
        }
        if (result_vec_margin.empty())
        {
            if (!real_res_edge.empty() and !res_edge_test.empty())
            {
                get_move_distance_test(real_res_edge, res_edge_test, result_vec_margin, "edge", 300);
            }
        }
        if (result_vec_margin.empty())
        {
            if (!real_res_handle.empty() and !res_handle_test.empty())
            {
                get_move_distance_test(real_res_handle, res_handle_test, result_vec_margin, "handle", 100);
            }
        }
    }
    if (result_vec_margin.empty())
    {
        if (!real_res_margin.empty() and !res_margin_test.empty())
        {
            get_move_distance_test(real_res_margin, res_margin_test, result_vec_margin, "margin", 150);
        }
    }
    if (!result_vec_margin.empty())
    {
        train_speed_frame = 0;                                         // 重置速度帧
        std::sort(result_vec_margin.begin(), result_vec_margin.end()); // 对结果进行排序

        // 如果结果数量大于2，舍去第一个值，求平均值
        // TODO: 为什么要舍去第一个值?
        if (result_vec_margin.size() > 2)
        {
            for (int i = 1; i < result_vec_margin.size() - 1; i++)
            {
                train_speed_frame += result_vec_margin[i];
                cout << "result_vec_margin[i] = " << result_vec_margin[i] << endl;
            }
            train_speed_frame = train_speed_frame / (float(result_vec_margin.size() - 2));
        }
        else// 求平均值
        {
            for (const auto &res1 : result_vec_margin)
            {
                train_speed_frame += res1;
            }
            train_speed_frame = train_speed_frame / (float(result_vec_margin.size()));
        }
    }
    else// 如果结果为空，当前没有速度结果
    {
        // train_speed_frame不置零，依然用上一帧的速度
        cout << "no speed result!" << endl;
    }

    // 设置权重
    // float w1 = 0.732;
    float w1 = 0.7;
    // 设置速度计算累计帧数
    int n = 10;
    cout << "train_speed_frame: " << train_speed_frame << endl;
    speed_f.push_back(train_speed_frame); // 将速度帧加入速度向量
    frame_count++;                        // 帧计数器自增
    res_latch_test = real_res_latch;      // 更新门闩检测结果
    res_margin_test = real_res_margin;    // 更新边缘检测结果
    res_handle_test = real_res_handle;    // 更新把手检测结果
    res_mutex_test = real_res_mutex;      // 更新互斥检测结果
    res_button_test = real_res_button;    // 更新按钮检测结果
    
    /*     未知原因会产生累计延迟   */
    // if (frame_count >= n)                 // 当帧计数器达到n，进行速度计算
    // {
    //     // cout << "test" << endl;
    //     // 计算并显示实时速度
    //     float s_one = speed_filter(speed_f, 5);       // 对速度进行滤波并累加
    //     train_speed_show_ = s_one / n;                // 计算n帧内的平均速度
    //     speed_f.erase(speed_f.begin());               // 移出第一个元素
    // }
    // cout << "speed:" << train_speed_show_ << "pixel/s" << endl;

    // 向共享数据结构体写入数据
    // speed_data->speed = train_speed_show_;
    // speed_data->time_ref = frame_info->video_frame.time_ref;
    // printf("time_ref: %ld\n", frame_info->video_frame.time_ref);
    // sem_post(&speed_data->sem);

    cout << "\n\n\n" << endl;
    return TD_SUCCESS;
}


// 匹配算法，获取上一帧和当前帧对应的坐标框
pair<vector<vector<int>>, vector<vector<int>>> match_tracks(
    const vector<vector<pair<float, float>>>& prev_boxes,
    const vector<vector<pair<float, float>>>& curr_boxes,
    float max_x_distance = 80.0f) 
{
    // 初始化匹配结果容器
    vector<vector<int>> matches(curr_boxes.size());      // 当前帧 -> 上一帧
    vector<vector<int>> reverse_matches(prev_boxes.size()); // 上一帧 -> 当前帧

    // 构建上一帧数据结构
    unordered_map<int, vector<pair<float, float>>> prev_map;  // 类别->坐标列表
    unordered_map<int, vector<bool>> matched_map;             // 类别->匹配标记

    // 初始化数据结构
    for (int cls = 0; cls < prev_boxes.size(); ++cls) {
        prev_map[cls] = prev_boxes[cls];
        matched_map[cls].resize(prev_boxes[cls].size(), false);
    }

    // 遍历当前帧所有类别
    for (int curr_cls = 0; curr_cls < curr_boxes.size(); ++curr_cls) {
        const auto& curr_coords = curr_boxes[curr_cls];
        matches[curr_cls].resize(curr_coords.size(), -1); // 默认无匹配

        // 无对应上一帧类别时跳过
        if (!prev_map.count(curr_cls)) continue;

        auto& prev_coords = prev_map[curr_cls];
        auto& prev_matched = matched_map[curr_cls];

        // 遍历当前类别所有检测框
        for (int curr_idx = 0; curr_idx < curr_coords.size(); ++curr_idx) {
            const float curr_x = curr_coords[curr_idx].first;
            float min_distance = max_x_distance;
            int best_match = -1;

            // 遍历上一帧同类别检测框
            for (int prev_idx = 0; prev_idx < prev_coords.size(); ++prev_idx) {
                if (prev_matched[prev_idx]) continue;

                const float prev_x = prev_coords[prev_idx].first;
                const float distance = abs(curr_x - prev_x);

                // 更新最佳匹配
                if (distance < min_distance) {
                    min_distance = distance;
                    best_match = prev_idx;
                }
            }

            // 记录有效匹配
            if (best_match != -1) {
                matches[curr_cls][curr_idx] = best_match;
                prev_matched[best_match] = true;

                // 更新反向匹配
                if (reverse_matches[curr_cls].size() <= best_match) {
                    reverse_matches[curr_cls].resize(best_match + 1, -1);
                }
                reverse_matches[curr_cls][best_match] = curr_idx;
            }
        }
    }

    return {matches, reverse_matches};
}

// 剔除把mutex误识别为edage的情况
td_bool check_is_edge(stYolov5Objs *pOut, td_s32 i){
    stObjinfo is_edage_suspicious = pOut->objs[i];
    for(int j = 0; j < pOut->count; j++){
        if(pOut->objs[j].class_id == 4){
            if(abs(pOut->objs[i].center_x - pOut->objs[j].center_x) < 100){
                return TD_FALSE;
            }
        }
    }
    return TD_TRUE;
}

// 保存结构体到CSV
td_void save_yolo_objs(const char* filename, const stYolov5Objs* data, hi_s32 time_ref) {
    FILE* fp = fopen(filename, "a");
    cout << "save_yolo_objs" << endl;
    // 写CSV头部（只在文件为空时写一次）
    if (ftell(fp) == 0) {
        fprintf(fp, "time_ref,obj_index,obj_count,x,y,w,h,center_x,center_y,"
                    "x_f,y_f,w_f,h_f,center_x_f,center_y_f,class_id,score\n");
    }
    
    // 遍历所有有效对象
    for (hi_s32 i = 0; i < data->count; i++) {
        const stObjinfo* obj = &data->objs[i];
        fprintf(fp, "%d,%d,%d,"
                    "%d,%d,%d,%d,%d,%d,"
                    "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f\n",
                time_ref,          // 结构体序号
                i,                  // 对象在结构体中的索引
                data->count,        // 本结构体总对象数
                obj->x, obj->y, 
                obj->w, obj->h,
                obj->center_x, obj->center_y,
                obj->x_f, obj->y_f,
                obj->w_f, obj->h_f,
                obj->center_x_f, obj->center_y_f,
                obj->class_id, 
                obj->score);
    }
    
    fclose(fp);
}

td_void save_effect_objs(const char *filename, const vector<vector<pair<float, float>>> &current_boxes,
                            const vector<vector<pair<float, float>>> &prev_boxes,
                            hi_s32 time_ref, const vector<vector<int>> &match, 
                            hi_s32 carriage_number)
{
    FILE* fp = fopen(filename, "a");
    
    // 写CSV头部（只在文件为空时写一次）
    if (ftell(fp) == 0) {
        fprintf(fp, "carriage_number,time_ref,class_id,class_name,number,last_frame_match_number,x,w,move_distance\n");
    }
    // 
    // 根据匹配结果计算每个检测框与上一帧中移动的距离
    string match_str[7] = {"button","latch ","mutex ","edge  ","handle","margin","head  "};

    // 输出匹配结果
    for (int i = 0; i < match.size(); i++)
    {
        auto it = match[i];
        for (int j = 0; j < it.size(); j++)
        {
            int jt = match[i][j];
            float dx = -1;
            if (jt != -1)
            {
                dx = abs(current_boxes[i][j].first - prev_boxes[i][jt].first);
            }
            fprintf(fp,"%d,%d,%d,%s,%d,%d,%.6f,%.6f,%.6f\n",
            carriage_number,
            time_ref/2,
            i,
            match_str[i].c_str(),
            j,
            jt,
            current_boxes[i][j].first,
            current_boxes[i][j].second,
            dx);
        }
    }
    
    fclose(fp);
}

// 保存结构体到CSV
td_void save_info(  const char* filename, 
                    td_u32 time_ref, 
                    td_s32 carriage_number, 
                    float distance, 
                    float current_distance, 
                    td_s32 target_used,
                    td_s32 start_case,
                    float start_offest,
                    td_s32 end_case,
                    float end_offest
                ) {
    FILE* fp = fopen(filename, "a");
    // 写CSV头部（只在文件为空时写一次）
    if (ftell(fp) == 0) {
        fprintf(fp, "time_ref,carriage_number,frame_distace,target_used,current_distance,start_case,start_offest,end_case,end_offest\n");
    }
    fprintf(fp, "%d,%d,%.2f,%d,%.2f,%d,%.2f,%d,%.2f\n",
                time_ref,
                carriage_number,
                distance,
                target_used,
                current_distance,
                start_case,
                start_offest,
                end_case,
                end_offest
    );
    
    fclose(fp);
}

// 保存每一帧各类标志物移动的距离，用来计算各标志物的修正系数
td_void save_move_distance(const char *filename, td_u32 time_ref, td_s32 carriage_number, vector<vector<float>> match_move_result)
{
    FILE* fp = fopen(filename, "a");
    string match_str[TARGET_NUMBER] = {"mutex","button","handle","edge","latch","margin","head","lock"};
    
    // 写CSV头部（只在文件为空时写一次）
    if (ftell(fp) == 0) {
        fprintf(fp, "time_ref,carriage_number,class_id,class_name,move_distance\n");
    }

    for (int i = 0; i < match_move_result.size(); i++)
    {
        for (int j = 0; j < match_move_result[i].size(); j++)
        {
            fprintf(fp, "%d,%d,%d,%s,%f\n",
            time_ref,
            carriage_number,
            i,
            match_str[i].c_str(),
            match_move_result[i][j]);
        }
    }
    fclose(fp);
}

bool updateTrackingTarget(
    const vector<vector<pair<float, float>>>& current_boxes,
    const vector<vector<pair<float, float>>>& prev_boxes,
    const vector<vector<int>>& backward_match,
    pair<int, int>& last_box_id,
    float& train_speed_frame,
    bool IS_TO_RIGHT)
{
    if (last_box_id.first == -1)
    { // 初始状态：寻找第一个目标
        bool found = false;
        for (int i = 0; i < current_boxes.size(); ++i)
        {
            if (IS_TO_RIGHT)
            {
                for (int j = 0; j < current_boxes[i].size(); ++j)
                {
                    if (current_boxes[i][j].first != -1)
                    {
                        last_box_id = make_pair(i, j);
                        found = true;
                        break;
                    }
                }
            }
            else
            {
                for (int j = current_boxes[i].size() - 1; j >= 0; --j)
                {
                    if (current_boxes[i][j].first != -1)
                    {
                        last_box_id = make_pair(i, j);
                        found = true;
                        break;
                    }
                }
            }
            if (found) break;
        }

        if (!found) return false; // 没有找到初始目标

    }
    else
    {
        if (last_box_id.first != 0)
        { // 不是最优先级目标，尝试切换
            bool found = false;
            for (int i = 0; i < backward_match.size(); ++i)
            {
                if (IS_TO_RIGHT)
                {
                    for (int j = 0; j < backward_match[i].size(); ++j)
                    {
                        if (backward_match[i][j] != -1)
                        {
                            last_box_id = make_pair(i, j);
                            found = true;
                            break;
                        }
                    }
                }
                else
                {
                    for (int j = backward_match[i].size() - 1; j >= 0; --j)
                    {
                        if (backward_match[i][j] != -1)
                        {
                            last_box_id = make_pair(i, j);
                            found = true;
                            break;
                        }
                    }
                }
                if (found) break;
            }

            if (found)
            {
                int curr_idx = backward_match[last_box_id.first][last_box_id.second];
                int current_x = current_boxes[last_box_id.first][curr_idx].first;
                int prev_x = prev_boxes[last_box_id.first][last_box_id.second].first;
                train_speed_frame = IS_TO_RIGHT * (current_x - prev_x);
                last_box_id = make_pair(last_box_id.first, curr_idx);
            }
            else
            { // 没有匹配目标，维持原速并选最边上的
                for (int i = 0; i < current_boxes.size(); ++i)
                {
                    if (!current_boxes[i].empty())
                    {
                        last_box_id = make_pair(i, current_boxes[i].size() - 1);
                    }
                }
            }
        }
        else
        { // 最优先级目标，直接使用匹配结果
            int curr_idx = backward_match[last_box_id.first][last_box_id.second];
            int current_x = current_boxes[last_box_id.first][curr_idx].first;
            int prev_x = prev_boxes[last_box_id.first][last_box_id.second].first;
            train_speed_frame = IS_TO_RIGHT * (current_x - prev_x);
            last_box_id = make_pair(last_box_id.first, curr_idx);
        }
    }

    return true; // 成功更新了目标
}

td_s32 train_speed_test_match(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data){

    /* 
    原有代码是使用坐标框的左边和右边坐标值计算，但在3403上坐标框大小抖动较为严重，
    所以在这里改为使用中心点坐标，并且用左边和右边做辅助
    */
    cout << "\n\n\n" << endl;
    vector<pair<float, float>> real_res_latch;  // 存储门闩检测结果
    vector<pair<float, float>> real_res_margin; // 存储边缘检测结果
    vector<pair<float, float>> real_res_edge;   // 存储边缘检测结果  只在画面右半区域内的车厢前/后边缘
    vector<pair<float, float>> real_res_edge_b; // 存储边缘B检测结果 全部画面中的车厢前/后边缘
    vector<pair<float, float>> real_res_handle; // 存储把手检测结果
    vector<pair<float, float>> real_res_mutex;  // 存储互斥检测结果
    vector<pair<float, float>> real_res_head;   // 存储头部检测结果
    vector<pair<float, float>> real_res_button; // 存储按钮检测结果
    vector<pair<float, float>> real_res_lock;   // 存储锁检测结果
    vector<vector<pair<float, float>>>  current_boxes;
    vector<vector<pair<float, float>>>  prev_boxes;
    vector<int> result_vec_margin;              // 存储边缘检测结果的向量
    int edge_count = 0;                         // 边缘计数器
    vector<vector<int>> match;                  // 存储匹配结果
    vector<vector<float>> match_move_result;    // 存储匹配后的移动结果
    vector<float> all_match_move_result;        // 储存匹配后所有的移动结果
   
    vector<cv::Point> detection_poly;
    vector<cv::Point> train_head_poly;
    vector<cv::Point> train_edge_poly;

    frame_size_f f;
    f.w = frame_info->video_frame.width;
    f.h = frame_info->video_frame.height;

    frame_size f_int;
    f_int.w = frame_info->video_frame.width;
    f_int.h = frame_info->video_frame.height;

    // TODO: 需要把区域框改为与根据长宽得到的值，而不是固定值
    // 全屏区域，稍微向内收缩
    detection_poly = {{10, 200},
                      {2438, 200},
                      {2438, 1100},
                      {10, 1100}};

    // 画面右半区域，稍微向内收缩
    train_head_poly = {{1224, 200}, 
                       {2438, 200},
                       {2438, 1100},
                       {1224, 1100}}; /// train_edge_poly  train_head_poly

    // 全屏区域，顶部稍微外扩，其余三面稍微向内收缩
    train_edge_poly = {{10, -10},
                       {2438, -10},
                       {2438, 1100},
                       {10, 1100}};

    

    // 保存检测结果到CSV文件
    // save_yolo_objs("yolov5_objs.csv", pOut, frame_info->video_frame.time_ref);

    // 遍历检测结果
    for (int i = 0; i < pOut->count; i++)
    {
        Rect r = get_rect(pOut->objs[i], f_int);

        // 靠近边界的点直接跳过
        if(r.x < 30 || r.x + r.width > f_int.w - 30){
            continue;
        }
        float ratio = static_cast<float>(frame_info->video_frame.width) / 640.0F;
        float x_f = min(max(pOut->objs[i].center_x_f * ratio,0.0f),f.w);
        float y_f = min(max(pOut->objs[i].center_y_f * ratio ,0.0f),f.h);
        float w_f = min(max(pOut->objs[i].w_f * ratio,0.0f),f.w);
        float h_f = min(max(pOut->objs[i].h_f * ratio,0.0f),f.h);
        switch (pOut->objs[i].class_id)
        {
        case 0:
            edge_count++; // 如果类别为0，边缘计数器自增
            if (check_is_edge(pOut, i))
            {
                if (cv::pointPolygonTest(train_head_poly, cv::Point(r.x, r.y), false) > 0 and
                    cv::pointPolygonTest(train_head_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
                {
                    real_res_edge.emplace_back(x_f, w_f);
                }
                if (cv::pointPolygonTest(detection_poly, cv::Point(r.x, r.y), false) > 0 and
                    cv::pointPolygonTest(detection_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
                {
                    real_res_edge_b.emplace_back(x_f, w_f);
                }
            }

            break;
        case 1:
            real_res_margin.emplace_back(x_f, w_f);
            break;
        case 2:
            if (cv::pointPolygonTest(detection_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(detection_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_latch.emplace_back(x_f, w_f);
            }
            break;
        case 3:
            real_res_handle.emplace_back(x_f, w_f);
            break;
        case 4:
            real_res_mutex.emplace_back(x_f, w_f);
            break;
        case 5:
            if (cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_head.emplace_back(x_f, w_f);
            }
            break;
        case 6:
            if (cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_button.emplace_back(x_f, w_f);
            }
            break;
        }
    }

    // 对所有检测结果按照第一个值center_x进行排序
    std::sort(real_res_margin.begin(), real_res_margin.end());
    std::sort(real_res_latch.begin(), real_res_latch.end());
    std::sort(real_res_edge.begin(), real_res_edge.end());
    std::sort(real_res_edge_b.begin(), real_res_edge_b.end());
    std::sort(real_res_handle.begin(), real_res_handle.end());
    std::sort(real_res_mutex.begin(), real_res_mutex.end());
    std::sort(real_res_head.begin(), real_res_head.end());
    std::sort(real_res_button.begin(), real_res_button.end());

    cout << "button:" << real_res_button.size() << endl;
    cout << "button:" << res_button_match.size() << endl;
    cout << "latch:" << real_res_latch.size() << endl;
    cout << "latch:" << res_latch_match.size() << endl;
    cout << "mutex:" << real_res_mutex.size() << endl;
    cout << "mutex:" << res_mutex_match.size() << endl;
    cout << "edge:" << real_res_edge.size() << endl;
    cout << "edge:" << res_edge_match.size() << endl;
    cout << "handle:" << real_res_handle.size() << endl;
    cout << "handle:" << res_handle_match.size() << endl;
    cout << "margin:" << real_res_margin.size() << endl;
    cout << "margin:" << res_margin_match.size() << endl;    
    cout << "edge_b:" << real_res_edge_b.size() << endl;
    cout << "edge_b:" << res_edge_b_match.size() << endl;   
    cout << "head:" << real_res_head.size() << endl;
    cout << "head:" << res_head_match.size() << endl;

    /// 测速部分
    int target_used = -1;
    /*优先级：1.mutex > button > handle > edge > margin > latch > head*/
    // current_boxes = {real_res_button,real_res_latch,real_res_mutex,real_res_edge,real_res_handle,real_res_margin,real_res_head};
    // prev_boxes = {res_button_match,res_latch_match,res_mutex_match,res_edge_match,res_handle_match,res_margin_match,res_head_match};


    // 创建所有检测框合集
    current_boxes = {real_res_mutex, real_res_button, real_res_handle, real_res_edge_b, real_res_latch, real_res_margin, real_res_head};
    prev_boxes =    {res_mutex_match,res_button_match,res_handle_match,res_edge_b_match,res_latch_match,res_margin_match,res_head_match};

    // 匹配检测结果 forward_match:当前帧->上一帧, backward_match:上一帧->当前帧
    auto [forward_match, backward_match] = match_tracks(prev_boxes, current_boxes,100.0F);
    
    // 根据匹配结果计算每个检测框与上一帧中移动的距离
    string match_str[7] = {"mutex ","button","handle","edge  ","latch ","margin","head  "};
    cout << "forward_match dx" << endl;
    
    // 初始化 match_move_result 的大小
    match_move_result.resize(forward_match.size());

    // 输出匹配结果
    for (int i = 0; i < forward_match.size(); i++)
    {
        cout << match_str[i] << ": ";
        for (int j = 0; j < forward_match[i].size(); j++)
        {
            auto jt = forward_match[i][j];
            float dx = -1;
            if (jt != -1)
            {
                dx = abs(current_boxes[i][j].first - prev_boxes[i][jt].first);
                cout << dx << " ";
            }
            match_move_result[i].push_back(dx);
            all_match_move_result.push_back(dx);
        }
        cout << endl;
    }

    // 计算画面中有两个相同标志物的时候，计算他们移动距离的差值；来判断透视矫正效果
    // for(int i = 0; i < match_move_result.size(); i++){
    //     if(match_move_result[i].size()>1){
    //         float dx = (match_move_result[i][0] - match_move_result[i][1]);
    //         left_minus_right[i].first += dx;
    //         left_minus_right[i].second++;
    //     }
    // }
    // for(int i = 0; i < match_move_result.size(); i++){
    //     cout << match_str[i] << "left-right: " << left_minus_right[i].first / left_minus_right[i].second << endl;
    // }
    
    // // 计算每种检测框的移动距离和mutex移动距离的比值
    // for(int number = 1;number < 7;number++){
    //     if (match_move_result[number].size()>0 && match_move_result[0].size()>0)
    //     {   
    //         float avg_x = 0;
    //         int cont = 0;
    //         for(auto i=0;i < match_move_result[number].size();i++)
    //         {
    //             if(match_move_result[number][i] != -1){
    //                 avg_x +=  match_move_result[number][i];
    //                 cont++;
    //             }
                
    //         }
    //         if(cont>0){
    //             avg_x /= cont;
    //         }
            

    //         float avg_mutex_x = 0;
    //         cont = 0;
    //         for(auto i=0;i < match_move_result[0].size();i++)
    //         {
    //             if(match_move_result[0][i] != -1){
    //                 avg_mutex_x += match_move_result[0][i];
    //                 cont++;
    //             }
                
    //         }
    //         if(cont>0){
    //             avg_mutex_x /= cont;
    //         }
            
    //         if(avg_x > 0 && avg_mutex_x > 0){
    //             float rate = avg_mutex_x/avg_x;
    //             class_dx_ratio[number].push_back(rate);
    //         }
            
    //     }
    // }
    

    
    // 输出每个匹配上的框的x值
    // for (int i = 0; i < forward_match.size(); i++)
    // {
    //     auto it = forward_match[i];
    //     cout << match_str[i] << ": ";
    //     for (int j = 0; j < it.size(); j++)
    //     {
    //         auto jt = forward_match[i][j];
    //         float dx = -1;
    //         if (jt != -1)
    //         {
    //             cout << current_boxes[i][j].first << " ";
    //         }
    //     }
    //     cout << endl;
    // }

    
    // 测试定位，移动距离暂时使用第一个
    // for(auto it : match_move_result){
    //     if(!it.empty()){
    //         train_speed_frame = it[0]==-1?train_speed_frame:it[0];
    //         break;
    //     }
    // }

    // 比例尺
    vector<float> scale_ratio = {1,0.975,0.95,1,0.99,1,1};
    scale_ratio = { 1,
                    0.986785,
                    0.968503,
                    1.00413,
                    1.00637,
                    0.972348,
                    1.032};
    scale_ratio = { 1,
                    1,
                    1,
                    1,
                    1,
                    1,
                    1};
    // 移动距离使用优先级，有多个时使用离中线最近的那个
    vector<vector<pair<float, float>>> match_move_result_with_cutern_x(forward_match.size());        // 储存匹配后所有的移动结果以及对应框的x值
    for (int i = 0; i < forward_match.size(); i++)
    {
        auto it = forward_match[i];
        for (int j = 0; j < it.size(); j++)
        {
            auto jt = forward_match[i][j];
            float dx;
            if (jt != -1)
            {
                dx = current_boxes[i][j].first - prev_boxes[i][jt].first;
                float closest_dx = abs(current_boxes[i][j].first - f.w/2);
                match_move_result_with_cutern_x[i].emplace_back(closest_dx,dx);
            }
        }
    }
    for(int i = 0; i < match_move_result_with_cutern_x.size();i++){
        auto it = match_move_result_with_cutern_x[i];
        if(!it.empty()){
            std::sort(it.begin(), it.end());
            train_speed_frame = IS_TO_RIGHT * it[0].second * scale_ratio[i];
            class_count[i]++;
            target_used = i;
            break;
        }
    }


    // 使用优先级
    // for(auto it : match_move_result){
    //     if(!it.empty()){
    //         train_speed_frame = 0;
    //         if(it.size()>2){
    //             for(int i=1;i<it.size()-1;i++){
    //                 train_speed_frame += it[i];
    //             }
    //             train_speed_frame = train_speed_frame / (float(it.size() - 2));
    //         }
    //         else if(it.size() > 0){
    //             for(auto &res : it){
    //                 train_speed_frame += res;
    //             }
    //             train_speed_frame = train_speed_frame / float(it.size());
    //         }
    //         break;
    //     }
    // }

    // 移动距离筛选：1-3个结果值选取中间值，超过三个去头去尾选取中间值
    // std::sort(all_match_move_result.begin(), all_match_move_result.end());
    // int count = all_match_move_result.size();
    // if (count == 0)
    // { // 没有检测结果保留上一帧的值
    //     train_speed_frame = train_speed_frame;
    // }
    // else if (count > 0 && count <= 3)
    // {
    //     train_speed_frame = count % 2 ? all_match_move_result[count / 2] : (all_match_move_result[count / 2 - 1] + all_match_move_result[count / 2]) / 2.0f;
    // }
    // else
    // {
    //     // 动态截断模式（保留中间50%）
    //     const size_t keep = count / 2;
    //     const size_t start = (count - keep) / 2;
    //     auto begin = all_match_move_result.begin() + start;
    //     auto end = begin + keep;

    //     // 第四步：计算截断区间的均值
    //     train_speed_frame = std::accumulate(begin, end, 0.0f) / keep;
    // }

    /*----优先级跟踪模式-------
        TODO：逻辑暂时有问题不能正常使用，
        并且当前场景下，目标最多同时出现两个，使用追踪最左边的和这个方法没有本质区别
        对一个标志物一直跟踪计算他的速度，当这个标志物消失之后
        寻找标志物进入画面一侧优先级最高的优先级标志物进行跟踪，
        以减少目标切换产生造成的误差
        如果切换的标志物不是最高优先级，则在下一帧继续寻找更高优先级的标志物进行跟踪
    */
    // cout << "1" <<endl;
    // if (last_box_id.first == -1)
    // { // 如果上一帧没有检测到任何标志物，则寻找当前帧中优先级最高且最靠近画面右侧的标志物进行跟踪
    //     cout << "2" <<endl;
    //     if (IS_TO_RIGHT)
    //     {
    //         bool found = false;
    //         for (int i = 0; i < current_boxes.size(); i++)
    //         {
    //             for (int j = 0; j < current_boxes[i].size(); j++)
    //             {
    //                 if (current_boxes[i][j].first != -1)
    //                 {
    //                     last_box_id = make_pair(i, j);
    //                     found = true;
    //                     break;
    //                 }
    //             }
    //             if(found) break;
    //         }
    //     }
    //     else
    //     {
            
    //         bool found = false;
    //         for (int i = 0; i < current_boxes.size(); i++)
    //         {
    //             for (int j = current_boxes[i].size() - 1; j >= 0; j--)
    //             {
    //                 if (current_boxes[i][j].first != -1)
    //                 {
    //                     last_box_id = make_pair(i, j);
    //                     found = true;
    //                     break;
    //                 }
    //             }
    //             if(found) break;
    //         }
    //     }
    // }
    // else
    // { 
    //     if(last_box_id.first != 0){ //如果上一个跟踪的检测框不是最高优先级 寻找最高优先级最靠边的计算
    //         cout << "3" <<endl;
    //         bool found = false;
    //         for(int i = 0; i < backward_match.size(); i++){
    //             if (IS_TO_RIGHT){
    //                 for(int j = 0; j < backward_match[i].size(); j++){
    //                     if(backward_match[i][j] != -1){
    //                         last_box_id = make_pair(i, j);
    //                         found = true;
    //                         break;
    //                     }
    //                 }
    //             }else{
    //                 for(int j = backward_match.size() - 1; j >= 0; j--){
    //                     if(backward_match[i][j] != -1){
    //                         last_box_id = make_pair(i, j);
    //                         found = true;
    //                         break;
    //                     }
    //                 }
    //             }
    //             if(found) break;
    //         }
    //         if(found){

    //             int curr_idx = backward_match[last_box_id.first][last_box_id.second];
    //             float current_x = current_boxes[last_box_id.first][curr_idx].first;
    //             float prev_x = prev_boxes[last_box_id.first][last_box_id.second].first;
    //             train_speed_frame = IS_TO_RIGHT * (current_x - prev_x);
    //             last_box_id = make_pair(last_box_id.first,curr_idx);
    //         }else{// 如果当前所有框都没有找到上一帧有对应的 维持当前移动距离 并设置最高优先级的最边一个为跟踪
    //             cout << "4" <<endl;
    //             train_speed_frame = train_speed_frame;
    //             for(int i = 0; i < current_boxes.size(); i++){
    //                 if(current_boxes[i].size()>0){
    //                     if(IS_TO_RIGHT){
    //                         last_box_id = make_pair(i,0);
    //                     }
    //                     else{
    //                         last_box_id = make_pair(i,current_boxes[i].size()-1);
    //                     }
                       
    //                 }
    //             }
    //         }
    //     }else{// 如果是最高优先级
    //         cout << "5" <<endl;
    //         for(int i = 0; i < backward_match.size(); i++){
    //             for(int j = 0; j < backward_match[i].size(); j++){
    //                 cout <<  backward_match[i][j] << " ";
    //             }
    //             cout << endl;
    //         }
    //         // 如果存在上一帧检测结果 通过匹配算法找到这个检测框在上一帧的位置
    //         if(backward_match[last_box_id.first].size()>0){
    //             cout << "6" <<endl;
    //             int curr_idx = backward_match[last_box_id.first][last_box_id.second];
    //             float current_x = current_boxes[last_box_id.first][curr_idx].first;
    //             float prev_x = prev_boxes[last_box_id.first][last_box_id.second].first;
    //             train_speed_frame = IS_TO_RIGHT * (current_x - prev_x);
    //             last_box_id = make_pair(last_box_id.first,curr_idx);
    //         }
    //         // 如果当前帧不存在上一帧跟踪的目标 则重新寻找目标跟踪
    //         else{
    //             cout << "7" <<endl;
    //             bool found = false;
    //             for(int i = 0; i < backward_match.size(); i++){
    //                 if (IS_TO_RIGHT){
    //                     for(int j = 0; j < backward_match[i].size(); j++){
    //                         if(backward_match[i][j] != -1){
    //                             last_box_id = make_pair(i, j);
    //                             found = true;
    //                             break;
    //                         }
    //                     }
    //                 }else{
    //                     for(int j = backward_match.size() - 1; j >= 0; j--){
    //                         if(backward_match[i][j] != -1){
    //                             last_box_id = make_pair(i, j);
    //                             found = true;
    //                             break;
    //                         }
    //                     }
    //                 }
    //                 if(found) break;
    //             }
    //             if(found){

    //                 int curr_idx = backward_match[last_box_id.first][last_box_id.second];
    //                 float current_x = current_boxes[last_box_id.first][curr_idx].first;
    //                 float prev_x = prev_boxes[last_box_id.first][last_box_id.second].first;
    //                 train_speed_frame = IS_TO_RIGHT * (current_x - prev_x);
    //                 last_box_id = make_pair(last_box_id.first,curr_idx);
    //             }else{// 如果当前所有框都没有找到上一帧有对应的 维持当前移动距离 并设置最高优先级的最边一个为跟踪
    //                 cout << "8" <<endl;
    //                 train_speed_frame = train_speed_frame;
    //                 for(int i = 0; i < current_boxes.size(); i++){
    //                     if(current_boxes[i].size()>0){
    //                         if(IS_TO_RIGHT){
    //                             last_box_id = make_pair(i,0);
    //                         }
    //                         else{
    //                             last_box_id = make_pair(i,current_boxes[i].size()-1);
    //                         }
    //                     }
    //                 }
    //             }
    //         }

    //     }  
    // }
    // cout << "Tracking: class=" << last_box_id.first 
    //  << ", index=" << last_box_id.second << endl;    

    // 设置权重
    // float w1 = 0.732;
    float w1 = 0.077772024;
    float px_per_s_to_m_per_s = 0.000777778;
    float m_per_s_to_km_per_h = px_per_s_to_m_per_s * 3.6;
    // 设置速度计算累计帧数
    int n = 10;
    cout << "train_speed_frame: " << train_speed_frame << endl;
    frame_count++;                        // 帧计数器自增


    // speed_f.push_back(train_speed_frame); // 将速度帧加入速度向量
    // if (frame_count >= n)                 // 当帧计数器达到n，进行速度计算
    // {
    //     // cout << "test" << endl;
    //     // 计算并显示实时速度
    //     float s_one = speed_filter(speed_f, 5);       // 对速度进行滤波并累加
    //     train_speed_show_ = s_one / n;                // 计算n帧内的平均速度
    //     speed_f.erase(speed_f.begin());               // 移出第一个元素
    // }
    train_speed_show_ = simple_moving_average(train_speed_frame);
    cout << "speed:" << train_speed_show_ * frame_rate << "pixel/s" << endl;
    cout << "speed:" << train_speed_show_ * frame_rate * px_per_s_to_m_per_s << "m/s" << endl;
    cout << "speed:" << train_speed_show_ * frame_rate * m_per_s_to_km_per_h << "km/h" << endl;
    cout << "time_ref:" << frame_info->video_frame.time_ref << endl;


    /// 定位部分
    /// ============================================================
    /// 功能：通过视频检测火车车厢侧面的标志物（按钮、插销、头部标记等），
    ///       计算车厢序号、移动距离和车厢长度。
    /// 核心逻辑：
    ///   1. 状态机切换：
    ///      - 初始状态（is_find_edge=false）：检测车厢头、尾部边缘（real_res_edge_b）
    ///        的稳定性（连续5帧检测到边缘后进入跟踪状态）
    ///      - 跟踪状态（is_find_edge=true）：根据标志物位置区分车厢头尾，
    ///        计算车厢长度和累计移动距离
    ///   2. 标志物优先级：
    ///      按钮（real_res_button） > 插销（real_res_latch） > 头部标记（real_res_head）
    ///   3. 距离计算：
    ///      - 通过 train_speed_frame（每帧移动像素）累加计算 current_distance
    ///      - 使用 w1/10 系数将像素距离转换为实际长度（厘米）
    /// 关键参数：
    ///   - 1224：图像参考点（可能为固定检测位置）
    ///   - 252/1191/1269：标志物到参考点的预设像素偏移量
    ///   - 5次检测阈值：确保标志物检测的稳定性
    /// ============================================================
    
    float distance_rate = 1; // 距离修正系数（默认为1，可扩展为缩放因子）

    int start_case = -1;
    int end_case = -1;
    float start_offset = -1;
    float end_offset = -1;

    // 计算本帧移动距离（train_speed_frame可能表示每帧的基础移动像素）
    train_speed_frame = train_speed_frame * distance_rate;

    /// ================== 边缘跟踪模式 ==================
    if (is_find_edge)
    {
        current_distance += train_speed_frame; // 持续累加移动距离
        // 要求检测到且仅检测到一个边缘（real_res_edge_b.size() == 1）
        if (real_res_edge_b.size() == 1)
        {
            // 判断逻辑，edge_b的检测框中心点在图像中心线附近
            float distance = f.w / 2 - real_res_edge_b[0].first;
            if (abs(distance) <= ZONE_THRESHOLD)
            {
                // 定义变量来存储检测到的标志物信息
                int marker_type = 0;  // 0: 无, 1: 按钮, 2: 插销, 3: 头部
                float marker_position = 0;
                float marker_width = 0;
                int case_type = 0;
                float offset = 0;



                // 检测不同类型的标志物
                if (real_res_button.size() == 1)
                {
                    marker_type = 1;
                    marker_position = real_res_button[0].first;
                    marker_width = real_res_button[0].second;
                    case_type = 1;
                }
                else if (real_res_latch.size() == 1)
                {
                    marker_type = 2;
                    marker_position = real_res_latch[0].first;
                    marker_width = real_res_latch[0].second;
                    case_type = 2;
                }
                else if (!real_res_head.empty())
                {
                    marker_type = 3;
                    marker_position = real_res_head[0].first;
                    marker_width = real_res_head[0].second;
                    case_type = 3;
                }

                // 根据标志物类型处理
                if (marker_type != 0)
                {
                    // 判断当前标志物所在的是车厢左侧边缘还是右侧边缘
                    bool is_carriage_left_side = (marker_type == 3) ? 
                        (marker_position < real_res_edge_b[0].first) : 
                        (marker_position > real_res_edge_b[0].first);

                    bool is_head = false;

                    if(IS_TO_RIGHT == -1){
                        // 车厢向左移动，左侧车厢是车头
                        is_head = is_carriage_left_side;
                    }else{
                        // 车厢向右移动，右侧车厢是车头
                        is_head = !is_carriage_left_side;
                    }
                    
                    if (!is_head)
                    {
                        // 标记为车厢尾部
                        if (!is_carriage_tail)
                        {
                            is_carriage_tail = true;
                            if (carriage_number > 0)
                            {
                                switch (marker_type)
                                {
                                    case 1: // 按钮
                                        offset = -(marker_position - f.w / 2 - marker_width / 2) * IS_TO_RIGHT + 1145;
                                        break;
                                    case 2: // 插销
                                        offset = -(marker_position - f.w / 2 - marker_width / 2) * IS_TO_RIGHT + 1247;
                                        break;
                                    case 3: // 头部
                                        offset = -1224 + (marker_position + marker_width / 2);
                                        offset = IS_TO_RIGHT * (f.w / 2 - marker_position) + marker_width / 2;
                                        break;
                                }
                                
                                train_distance_list.push_back(current_distance + offset);
                                end_frame = frame_info->video_frame.time_ref;
                                train_frame_list.emplace_back(start_frame, end_frame);
                                end_case = case_type;
                                end_offset = offset;
                            }
                        }
                    }
                    else
                    {
                        // 标记为新车厢头部
                        if (!is_carriage_head)
                        {
                            is_carriage_head = true;
                            current_distance = f.w / 2 - (real_res_edge_b[0].first - real_res_edge_b[0].second / 2) + 245;
                            carriage_number++;
                            start_frame = frame_info->video_frame.time_ref;
                            start_case = case_type;
                            start_offset = f.w / 2 - (real_res_edge_b[0].first - real_res_edge_b[0].second / 2) + 245;
                        }
                    }
                }
                else
                {
                    // 无任何标志物时的处理
                    is_carriage_tail = false;
                    is_carriage_head = false;
                    cout << "find nothing Carriage number " << carriage_number
                        << "  length:  " << current_distance * w1 / 10 << "  cm" << endl;
                }
            }else{
                // edge_b移出中线区域
                is_carriage_tail = false;
                is_carriage_head = false;
            }
        }
        // 边缘检测异常时重置状态
        else
        {
            is_carriage_head = false;
            is_carriage_tail = false;
        }

        /// ----- 边缘丢失处理 -----
        if (real_res_edge_b.empty())
        {
            not_edge_count++; // 连续丢失计数器
            if (not_edge_count == 5)
            { // 连续5帧丢失则认为边缘消失
                find_edge_count = 0;
                is_find_edge = false; // 退出跟踪模式
            }
        }
    }

    /// ================== 初始检测模式 ==================
    else
    {
        // 重置车厢状态
        is_carriage_head = false;
        is_carriage_tail = false;

        // 无边缘检测时的处理
        if (real_res_edge_b.empty())
        {
            if (carriage_number > 0)
            {
                current_distance += train_speed_frame;
            }
            else
            {
                cout << "Not find train carriage" << endl;
            }

            // 已开始计数时，累加消失计数器
            if (start_count)
            {
                lose_count_after_start++;
            }

            // 连续消失5次后清除状态
            if (lose_count_after_start >= 5)
            {
                find_edge_count = 0;        // 重置边缘检测计数器
                lose_count_after_start = 0; // 重置消失计数器
                start_count = false;        // 停止计数（防止继续累加）
            }
        }
        // 检测到边缘时的处理
        else
        {
            find_edge_count++;
            start_count = true;         // 标记开始计数
            lose_count_after_start = 0; // 重置消失计数器（重要！）

            // 连续5次检测到边缘后确认有效
            if (find_edge_count == 5)
            {
                is_find_edge = true;
                not_edge_count = 0;
                if (carriage_number == 0)
                {
                    cout << "find first train carriage" << endl;
                }
            }

            current_distance += train_speed_frame;
            // if (carriage_number > 0)
            // {
            //     current_distance += train_speed_frame;
            // }
        }
    }
    move_distance += train_speed_frame;
    res_latch_match = real_res_latch;      // 更新门闩检测结果
    res_margin_match = real_res_margin;    // 更新边缘检测结果
    res_handle_match = real_res_handle;    // 更新把手检测结果
    res_mutex_match = real_res_mutex;      // 更新互斥检测结果
    res_button_match = real_res_button;    // 更新按钮检测结果
    res_edge_match = real_res_edge;     // 更新边缘检测结果
    res_edge_b_match = real_res_edge_b; // 更新边缘B检测结果
    res_head_match = real_res_head;     // 更新头部检测结果


    // 向共享数据结构体写入数据
    speed_data->carriage_number = carriage_number;
    // speed_data->current_distance = current_distance;
    speed_data->speed = train_speed_show_;
    speed_data->time = (long)frame_info->video_frame.pts;
    speed_data->time_ref = frame_info->video_frame.time_ref;
    sem_post(&speed_data->sem);
    // printf("now time: %ld\n", get_time_ms());
    // printf("pts time: %ld\n", frame_info->video_frame.pts);
    // printf("get frame to speed_data cost time: %ld ms\n",get_time_ms() - frame_info->video_frame.pts);
    
    printf("time_ref: %ld\n", frame_info->video_frame.time_ref);
    
    // 计算并显示当前距离
    cout << "current distance:" << current_distance << "pix" << endl;
    cout << "move distabce:" << move_distance << endl;
    // 显示当前车厢编号
    cout << "current carriage number:" << carriage_number << endl;
    // 显示已经过车厢长度
    for(int i = 0; i < train_distance_list.size(); i++){
        cout << "carriage distance["<< i << "]:" << train_distance_list[i] << "  frame_number:"<< train_frame_list[i].second - train_frame_list[i].first << endl;
    }
    for(int i = 0; i < train_distance_list.size(); i++){
        cout << train_frame_list[i].first << "  " << train_frame_list[i].second << endl;
    }
    for(int i = 0; i < train_distance_list.size(); i++){
        cout << train_distance_list[i] << endl;
    }


    // 统计每个margin在火车的位置
    // if(real_res_margin.size()>0){
    //     if(margin_distance.size() < carriage_number){
    //         vector<pair<float, float>> tmp(6, make_pair(numeric_limits<float>::max(), 0.0f));
    //         margin_distance.push_back(tmp);
    //     }

    //     float distance = abs(1224 - real_res_margin[0].first);
    //     if (distance<80){
    //         int number = -1;
    //         if(current_distance>2200&&current_distance<3200){
    //             number = 0;
    //         }
    //         else if(current_distance>4500&&current_distance<6000){
    //             number = 1;
    //         }
    //         else if(current_distance>7000&&current_distance<8000){
    //             number = 2;
    //         }
    //         else if(current_distance>9000&&current_distance<11000){
    //             number = 3;
    //         }
    //         else if(current_distance>12000&&current_distance<13000){
    //             number = 4;
    //         }
    //         else if(current_distance>14600&&current_distance<15400){
    //             number = 5;
    //         }
    //         if(number>=0){
    //             if(distance < margin_distance[carriage_number-1][number].first){
    //                 margin_distance[carriage_number-1][number].first = distance;
    //                 margin_distance[carriage_number-1][number].second = current_distance + real_res_margin[0].first -1224;
    //             }
    //         }
    //     }
    // }
    
    // for(int i = 0; i < margin_distance.size(); i++){
    //     auto it = margin_distance[i];
    //     for(int j = 0; j < it.size(); j++){
    //         cout << it[j].second << ",";
    //     }
    //     cout << endl;
    // }
    

    // save_info("train_distance.csv",frame_info->video_frame.time_ref,carriage_number,train_speed_frame,current_distance,target_used,start_case,start_offset,end_case,end_offset);
    
    // 保存有效的检测框和对应上一帧的序号
    // save_effect_objs("effect_objs.csv",current_boxes,prev_boxes,frame_info->video_frame.time_ref,forward_match,carriage_number);

    // 如果没有检测到任何对象，计数器自增
    if (real_res_margin.empty() and real_res_edge_b.empty() and real_res_latch.empty() and real_res_button.empty() and real_res_handle.empty() and real_res_mutex.empty() and real_res_head.empty())
    {
        frame_count_end++;
        if (frame_count_end >= frame_rate * 10)
        {
            if (carriage_number > 0)
            {
                cout << "The train has left" << endl;
            }
            // 重置数据
            // train_speed_reset_data();
        }
    }else{
        frame_count_end = 0;
    }

    cout << "\n\n\n"<< endl;
    return TD_SUCCESS;
}

void get_move_distance_origin(vector<pair<int, int>> &current_res,
                                         vector<pair<int, int>> &last_res,
                                         vector<int> &result_vec                                         
                                         ) {
        if (current_res.size() > 1) {
            for (auto i = 1; i < current_res.size(); i++) {
                if (abs(current_res[i - 1].first - current_res[i].first) < 800) {
                    current_res.erase(current_res.begin() + i);
                    current_res.erase(current_res.begin() + i - 1);
                }
            }
        }
        if (current_res.empty()) {
            return;
        }
        if (current_res.size() == last_res.size()) {
            if ((current_res[0].first - 50) <= last_res[0].first) {
                for (auto i = 0; i < current_res.size(); i++) {
                    if (current_res[i].first < 100) {
                        if (last_res[i].first + last_res[i].second - current_res[i].first -
                            current_res[i].second < 200) {
                            result_vec.push_back(last_res[i].second - current_res[i].second);
                        }
                    } else if (current_res[i].second > 2348) {
                        if (last_res[i].first - current_res[i].first < 200) {
                            result_vec.push_back(last_res[i].first - current_res[i].first);
                        }
                    } else {
                        result_vec.push_back(last_res[i].first - current_res[i].first);
                        result_vec.push_back(last_res[i].second - current_res[i].second);
                    }
                }
            } else {
                if (last_res.size() == 1) {
//                            cout << "111111111111111111111111111111111" << endl;
                } else {
                    for (auto i = 0; i < current_res.size() - 1; i++) {
                        
                        if (current_res[i].first < 100) {
                            if (last_res[i + 1].first + last_res[i + 1].second -
                                current_res[i].first -
                                current_res[i].second < 200) {
                                result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                            }
                        } else if (current_res[i].second < 2348) {
                            if (last_res[i + 1].first - current_res[i + 1].first < 200) {
                                result_vec.push_back(
                                        last_res[i + 1].first - current_res[i].first);
                            }
                        } else {
                            result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                            result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                        }
                    }
                }
            }
        } else if (current_res.size() > last_res.size()) {
            if (current_res[0].first <= last_res[0].first) {
                for (auto i = 0; i < last_res.size(); i++) {
                    

                    if (current_res[i].first < 100) {
                        if (last_res[i].first + last_res[i].second - current_res[i].first -
                            current_res[i].second < 200) {
                            result_vec.push_back(last_res[i].second - current_res[i].second);
                        }
                    } else if (current_res[i].second < 2348) {
                        if (last_res[i].first - current_res[i].first < 200) {
                            result_vec.push_back(last_res[i].first - current_res[i].first);
                        }
                    } else {
                        result_vec.push_back(last_res[i].first - current_res[i].first);
                        result_vec.push_back(last_res[i].second - current_res[i].second);
                    }
                }
            } else {
                for (auto i = 0; i < last_res.size() - 1; i++) {
                    
                    if (current_res[i].first < 100) {
                        if (last_res[i + 1].first + last_res[i + 1].second - current_res[i].first -
                            current_res[i].second < 200) {
                            result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                        }
                    } else if (current_res[i].second < 2348) {
                        if (last_res[i + 1].first - current_res[i].first < 200) {
                            result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                        }
                    } else {
                        result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                        result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                    }
                }
            }
        } else {
            for (auto i = 0; i < current_res.size() - 1; i++) {
                

                if (current_res[i].first < 100) {
                    if (last_res[i + 1].first + last_res[i + 1].second - current_res[i].first -
                        current_res[i].second < 200) {
                        result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                    }
                } else if (current_res[i].second < 2348) {
                    if (last_res[i + 1].first - current_res[i].first < 200) {
                        result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                    }
                } else {
                    result_vec.push_back(last_res[i + 1].first - current_res[i].first);
                    result_vec.push_back(last_res[i + 1].second - current_res[i].second);
                }
            }
        }

    }

td_s32 train_speed_origin(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data){


    cout << "\n\n\n" << endl;
    vector<pair<int, int>> real_res_latch;  // 存储门闩检测结果
    vector<pair<int, int>> real_res_margin; // 存储边缘检测结果
    vector<pair<int, int>> real_res_edge;   // 存储边缘检测结果
    vector<pair<int, int>> real_res_edge_b; // 存储边缘B检测结果
    vector<pair<int, int>> real_res_handle; // 存储把手检测结果
    vector<pair<int, int>> real_res_mutex;  // 存储互斥检测结果
    vector<pair<int, int>> real_res_head;   // 存储头部检测结果
    vector<pair<int, int>> real_res_button; // 存储按钮检测结果
    vector<int> result_vec_margin;          // 存储边缘检测结果的向量
    int edge_count = 0;                     // 边缘计数器

    vector<cv::Point> detection_poly;
    vector<cv::Point> train_head_poly;
    vector<cv::Point> train_edge_poly;

    frame_size f;
    f.w = frame_info->video_frame.width;
    f.h = frame_info->video_frame.height;

    // TODO: 需要把区域框改为与根据长宽得到的值，而不是固定值
    // 全屏区域，稍微向内收缩
    detection_poly = {{10, 200},
                      {2438, 200},
                      {2438, 1100},
                      {10, 1100}};

    // 画面右半区域，稍微向内收缩
    train_head_poly = {{1224, 200}, 
                       {2438, 200},
                       {2438, 1100},
                       {1224, 1100}}; /// train_edge_poly  train_head_poly

    // 全屏区域，顶部稍微外扩，其余三面稍微向内收缩
    train_edge_poly = {{10, -10},
                       {2438, -10},
                       {2438, 1100},
                       {10, 1100}};
    
    // 遍历检测结果
    for (int i = 0; i < pOut->count; i++)
    {
        Rect r = get_rect(pOut->objs[i], f);
        // cout << "class_id: " << pOut->objs[i].class_id << "  x: " << r.x << " y: " << r.y << " w: " << r.width 
        //      << " h: " << r.height << endl;

        switch (pOut->objs[i].class_id)
        {
        case 0:
            edge_count++; // 如果类别为0，边缘计数器自增
            if (cv::pointPolygonTest(train_head_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_head_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_edge.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            }
            if (cv::pointPolygonTest(detection_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(detection_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_edge_b.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            }
            break;
        case 1:
            real_res_margin.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            break;
        case 2:
            if (cv::pointPolygonTest(detection_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(detection_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_latch.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            }
            break;
        case 3:
            real_res_handle.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            break;
        case 4:
            real_res_mutex.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            break;
        case 5:
            if (cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_head.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            }
            break;
        case 6:
            if (cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 and
                cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0)
            {
                real_res_button.emplace_back(max(r.x, 0), min(r.x + r.width, f.w));
            }
            break;
        }
    }

    // 对所有检测结果进行排序
    std::sort(real_res_margin.begin(), real_res_margin.end());
    std::sort(real_res_latch.begin(), real_res_latch.end());
    std::sort(real_res_edge.begin(), real_res_edge.end());
    std::sort(real_res_edge_b.begin(), real_res_edge_b.end());
    std::sort(real_res_handle.begin(), real_res_handle.end());
    std::sort(real_res_mutex.begin(), real_res_mutex.end());
    std::sort(real_res_head.begin(), real_res_head.end());
    std::sort(real_res_button.begin(), real_res_button.end());
    
    // cout << "button:" << real_res_button.size() << endl;
    // cout << "button:" << res_button.size() << endl;
    // cout << "latch:" << real_res_latch.size() << endl;
    // cout << "latch:" << res_latch.size() << endl;
    // cout << "mutex:" << real_res_mutex.size() << endl;
    // cout << "mutex:" << res_mutex.size() << endl;
    // cout << "edge:" << real_res_edge.size() << endl;
    // cout << "edge:" << res_edge.size() << endl;
    // cout << "handle:" << real_res_handle.size() << endl;
    // cout << "handle:" << res_handle.size() << endl;
    // cout << "margin:" << real_res_margin.size() << endl;
    // cout << "margin:" << res_margin.size() << endl;    
    // cout << "edge_b:" << real_res_edge_b.size() << endl;
    // cout << "edge_b:" << res_edge_b.size() << endl;   
    // cout << "head:" << real_res_head.size() << endl;
    // cout << "head:" << res_head.size() << endl;

    /// 测速部分
    int x1, x2;
    int target_used = -1;
        if (is_first) {
            is_first = false;
        } else {
            if (!real_res_button.empty() and !res_button.empty()) {
                get_move_distance_origin(real_res_button, res_button, result_vec_margin);
                target_used = 0;
            }
            if (result_vec_margin.empty()) {
                if (!real_res_latch.empty() and !res_latch.empty()) {
                    get_move_distance_origin(real_res_latch, res_latch, result_vec_margin);
                    target_used = 1;
                }
            }
            if (result_vec_margin.empty()) {
                if (!real_res_mutex.empty() and !res_mutex.empty()) {
                    get_move_distance_origin(real_res_mutex, res_mutex, result_vec_margin);
                    target_used = 2;
                }
            }
            if (result_vec_margin.empty()) {
                if (!real_res_edge.empty() and !res_edge.empty()) {
                    get_move_distance_origin(real_res_edge, res_edge, result_vec_margin);
                    target_used = 3;
                }
            }
            if (result_vec_margin.empty()) {
                if (!real_res_handle.empty() and !res_handle.empty()) {
                    get_move_distance_origin(real_res_handle, res_handle, result_vec_margin);
                    target_used = 4;
                }
            }

        }
        if (result_vec_margin.empty()) {
            if (!real_res_margin.empty() and !res_margin.empty()) {
                get_move_distance_origin(real_res_margin, res_margin, result_vec_margin);
                target_used = 5;
            }
        }
        if (!result_vec_margin.empty()) {
            train_speed_frame = 0;
            std::sort(result_vec_margin.begin(), result_vec_margin.end());

            if (result_vec_margin.size() > 2) {
                for (int i = 1; i < result_vec_margin.size() - 1; i++) {
                    train_speed_frame += result_vec_margin[i];
                }
                train_speed_frame = train_speed_frame / (float(result_vec_margin.size() - 2));
            } else {
                for (const auto &res1:result_vec_margin) {
                    train_speed_frame += res1;
                }
                train_speed_frame = train_speed_frame / (float(result_vec_margin.size()));
            }
        } else {

        }

//        float w1 = 0.732;
        float w1 = 0.7;
        train_speed_s += train_speed_frame;
        speed_f.push_back(train_speed_frame);
        frame_count++;
        res_latch = real_res_latch;
        res_margin = real_res_margin;
        res_handle = real_res_handle;
        res_mutex = real_res_mutex;
        res_button = real_res_button;
        if (frame_count == frame_rate) {
            train_speed_show = float((train_speed_s * w1 * 3600) / 1000000);
            float s_one = speed_filter(speed_f, 5);
            if (s_one < 5) {
                s_one = 0;
                train_speed_frame = 0;
            }
            ///train_speed_show_ = float((s_one * w1 * 3600) / 1000000);
            train_speed_show_ = float((s_one * w1) / 10);
            train_speed_s = 0;
            frame_count = 0;
            speed_f.clear();
        }
        string speed_res = to_string(train_speed_show_);
        speed_res = speed_res.substr(0, speed_res.length() - 5);

        /// 定位部分
        float distance_rate = 1;
        string res_s;

        if (is_find_edge) {
            current_distance += train_speed_frame;
            if (real_res_edge_b.size() == 1) {
                if (real_res_edge_b[0].first <= 1224 and real_res_edge_b[0].second >= 1224) {
                    if (real_res_button.size() == 1) {
                        if (real_res_button[0].first <= real_res_edge_b[0].first) {
                            if (!is_carriage_tail) {
                                is_carriage_tail = true;
//                                is_carriage_head = false;
                                if (carriage_number > 0) {
                                    cout << "real_res_button Carriage number " << carriage_number << "  length:  "
                                         << (current_distance - real_res_button[0].first + 1191) *
                                            w1 / 10 << "  cm" << endl;
                                    train_distance_list.push_back(current_distance - (1224 - real_res_button[0].first) + 1145);
                                    end_frame = frame_info->video_frame.time_ref;
                                    train_frame_list.emplace_back(start_frame, end_frame);
                                }
                            }
                        } else {
                            if (!is_carriage_head) {
                                is_carriage_head = true;
//                                is_carriage_tail = false;
                                current_distance = 1224 - float(real_res_edge_b[0].first) + 245;
                                carriage_number++;
                                start_frame = frame_info->video_frame.time_ref;
                            }
                        }
                    } else if (real_res_latch.size() == 1) {
                        if (real_res_latch[0].first <= real_res_edge_b[0].first) {
                            if (!is_carriage_tail) {
                                is_carriage_tail = true;
//                                is_carriage_head = false;
                                if (carriage_number > 0) {
                                    cout << "real_res_latch Carriage number " << carriage_number << "  length:  "
                                         << (current_distance - real_res_latch[0].first + 1269) *
                                            w1 / 10 << "  cm" << endl;
                                    // train_distance_list.push_back(current_distance - real_res_latch[0].first + 1269);
                                    train_distance_list.push_back(current_distance - (1224 - real_res_latch[0].first) + 1247);
                                    end_frame = frame_info->video_frame.time_ref;
                                    train_frame_list.emplace_back(start_frame, end_frame);
                                }
                            }
                        } else {
                            if (!is_carriage_head) {
                                is_carriage_head = true;
//                                is_carriage_tail = false;
                                current_distance = 1224 - float(real_res_edge_b[0].first) + 245;
                                carriage_number++;
                                start_frame = frame_info->video_frame.time_ref;
                            }
                        }
                    } else if (!real_res_head.empty()) {
                        if (real_res_head[0].first >= real_res_edge_b[0].first) {
                            if (!is_carriage_tail) {
                                is_carriage_tail = true;
//                                is_carriage_head = false;
                                if (carriage_number > 0) {
                                    cout << "real_res_head Carriage number " << carriage_number << "  length:  "
                                         << (current_distance + real_res_head[0].second - 1224) *
                                            w1 / 10 << "  cm" << endl;
                                    train_distance_list.push_back(current_distance - 1224 + real_res_head[0].second);
                                    end_frame = frame_info->video_frame.time_ref;
                                    train_frame_list.emplace_back(start_frame, end_frame);
                                }
                            }
                        } else {
                            if (!is_carriage_head) {
                                is_carriage_head = true;
//                                is_carriage_tail = false;
                                current_distance = 1224 - float(real_res_edge_b[0].first) + 245;
                                carriage_number++;
                                start_frame = frame_info->video_frame.time_ref;
                            }
                        }
                    } else {
                        is_carriage_tail = false;
                        is_carriage_head = false;
                        cout << "find nothing Carriage number " << carriage_number << "  length:  "
                             << current_distance * w1 / 10 << "  cm" << endl;
//                        current_distance = 1224 - float(real_res_edge_b[0].first) + 252;
                    }
                }
            } else {
                is_carriage_head = false;
                is_carriage_tail = false;
            }

            if (real_res_edge_b.empty()) {
                not_edge_count++;
                if (not_edge_count == 5) {
                    find_edge_count = 0;
                    is_find_edge = false;
                }
            }
        } /// ================== 初始检测模式 ==================
    else
    {
        // 重置车厢状态
        is_carriage_head = false;
        is_carriage_tail = false;

        // 无边缘检测时的处理
        if (real_res_edge_b.empty())
        {
            if (carriage_number > 0)
            {
                current_distance += train_speed_frame;
            }
            else
            {
                cout << "Not find train carriage" << endl;
            }

            // 已开始计数时，累加消失计数器
            if (start_count)
            {
                lose_count_after_start++;
            }

            // 连续消失5次后清除状态
            if (lose_count_after_start >= 5)
            {
                find_edge_count = 0;        // 重置边缘检测计数器
                lose_count_after_start = 0; // 重置消失计数器
                start_count = false;        // 停止计数（防止继续累加）
            }
        }
        // 检测到边缘时的处理
        else
        {
            find_edge_count++;
            start_count = true;         // 标记开始计数
            lose_count_after_start = 0; // 重置消失计数器（重要！）

            // 连续5次检测到边缘后确认有效
            if (find_edge_count == 5)
            {
                is_find_edge = true;
                not_edge_count = 0;
                if (carriage_number == 0)
                {
                    cout << "find first train carriage" << endl;
                }
            }

            if (carriage_number > 0)
            {
                current_distance += train_speed_frame;
            }
        }
    }

    res_edge = real_res_edge;
    res_edge_b = real_res_edge_b;
    res_head = real_res_head;

    string distance_res = to_string(current_distance * w1 / 10);
    distance_res = distance_res.substr(0, distance_res.length() - 3);
    ////string text_c = "current distance: " + distance_res + "  mm";

    // 计算并显示当前距离
    cout << "current distance:" << current_distance << "pix" << endl;

    // 显示当前车厢编号
    cout << "current carriage number:" << carriage_number << endl;
    // 显示已经过车厢长度
    for (int i = 0; i < train_distance_list.size(); i++)
    {
        cout << "carriage distance[" << i << "]:" << train_distance_list[i] << "  frame_number:" << train_frame_list[i].second - train_frame_list[i].first << endl;
    }

    for (int i = 0; i < train_distance_list.size(); i++)
    {
        cout << train_distance_list[i] << endl;
    }
    

    if (real_res_margin.empty() and real_res_edge_b.empty() and real_res_latch.empty() and real_res_button.empty() and real_res_handle.empty() and real_res_mutex.empty() and real_res_head.empty())
    {
        frame_count_end++;
        if (frame_count_end >= frame_rate * 10)
        {
            if (carriage_number > 0)
            {
                cout << "The train has left" << endl;
            }
            frame_count = 0;
            carriage_number = 0;
            train_speed_frame = 0;
            current_distance = 0;
            train_speed_show_ = 0;
        }
    }

    return 0;
}

// 测试新的车厢计数方式
td_s32 train_speed_match_new_count_test(stYolov5Objs* pOut, ot_video_frame_info *frame_info, SpeedData *speed_data){

    /* 
    原有代码是使用坐标框的左边和右边坐标值计算，但在3403上坐标框大小抖动较为严重，
    所以在这里改为使用中心点坐标，并且用左边和右边做辅助
    */
    cout << "\n\n\n" << endl;
    vector<pair<float, float>> real_res_latch;  // 存储门闩检测结果
    vector<pair<float, float>> real_res_margin; // 存储边缘检测结果
    vector<pair<float, float>> real_res_edge;   // 存储边缘检测结果  只在画面右半区域内的车厢前/后边缘
    vector<pair<float, float>> real_res_edge_b; // 存储边缘B检测结果 全部画面中的车厢前/后边缘
    vector<pair<float, float>> real_res_handle; // 存储把手检测结果
    vector<pair<float, float>> real_res_mutex;  // 存储互斥检测结果
    vector<pair<float, float>> real_res_head;   // 存储头部检测结果
    vector<pair<float, float>> real_res_button; // 存储按钮检测结果
    vector<pair<float, float>> real_res_lock;   // 存储锁检测结果
    vector<vector<pair<float, float>>>  current_boxes;
    vector<vector<pair<float, float>>>  prev_boxes;
    vector<int> result_vec_margin;              // 存储边缘检测结果的向量
    int edge_count = 0;                         // 边缘计数器
    vector<vector<int>> match;                  // 存储匹配结果
    vector<vector<float>> match_move_result;    // 存储匹配后的移动结果
    vector<float> all_match_move_result;        // 储存匹配后所有的移动结果
   
    vector<cv::Point> detection_poly;
    vector<cv::Point> train_head_poly;
    vector<cv::Point> train_edge_poly;

    frame_size_f f;
    f.w = frame_info->video_frame.width;
    f.h = frame_info->video_frame.height;

    frame_size f_int;
    f_int.w = frame_info->video_frame.width;
    f_int.h = frame_info->video_frame.height;

    // 根据当前车型获取车型参数
    const CarriageInfo1* carriage_info2 = NULL;
    
    if(carriage_number > 0 && carriage_type.size() >= carriage_number){// 如果传入的车型型号列表大于当前的车厢数(初始为0),说明接受到了外部传入的车型型号列表,则寻找JSON文件中对应型号的参数
        std::string carriage_type_name;
        carriage_type_name = carriage_type[carriage_number - 1];
        auto carriage_info1 = carriage_info.find(carriage_type_name);
        if(carriage_info1 != carriage_info.end()){
            carriage_info2 = &carriage_info1->second;
        }else{
            carriage_info1 = carriage_info.find("UNKNOWN");
            if(carriage_info1 != carriage_info.end()){
                carriage_info2 = &carriage_info1->second;
            }else{
                //LOG_WARN();
            }
        }
    }
    else{
        auto carriage_info1 = carriage_info.find("UNKNOWN");
        if(carriage_info1 != carriage_info.end()){
            carriage_info2 = &carriage_info1->second;
        }
    }
    if(carriage_info2 == NULL){// 没有传入车型列表,或者模型列表中没有对应型号,或者查询JSON的模型列表失败 使用代码内部的预设型号
        carriage_info2 = &UNKONWN_CARRIAGE_INFO;
        // LOG_WARN();
    }

    // TODO: 需要把区域框改为与根据长宽得到的值，而不是固定值
    // 全屏区域，稍微向内收缩
    detection_poly = {{10, 200},
                      {2438, 200},
                      {2438, 1100},
                      {10, 1100}};

    // 画面右半区域，稍微向内收缩
    train_head_poly = {{1224, 200}, 
                       {2438, 200},
                       {2438, 1100},
                       {1224, 1100}}; /// train_edge_poly  train_head_poly

    // 全屏区域，顶部稍微外扩，其余三面稍微向内收缩
    train_edge_poly = {{10, -10},
                       {2438, -10},
                       {2438, 1100},
                       {10, 1100}};

    

    // 保存检测结果到CSV文件
    // save_yolo_objs("yolov5_objs.csv", pOut, frame_info->video_frame.time_ref);
    
    // 假设 effective_pOut 数组已正确初始化
    int effective_pOut[pOut->count] = {0};  // 用于记录每个元素是否有效

    // 第一步：遍历检测结果并记录有效性
    for (int i = 0; i < pOut->count; i++) {
        Rect r = get_rect(pOut->objs[i], f_int);
        bool isEffective = false;  // 临时变量记录当前元素有效性

        // 靠近边界的点直接跳过
        if (r.x < 30 || r.x + r.width > f_int.w - 30) {
            effective_pOut[i] = 0;  // 标记为无效
            continue;
        }

        // 计算缩放后的坐标
        float ratio = static_cast<float>(frame_info->video_frame.width) / 640.0F;
        float x_f = min(max(pOut->objs[i].center_x_f * ratio, 0.0f), f.w);
        float y_f = min(max(pOut->objs[i].center_y_f * ratio, 0.0f), f.h);
        float w_f = min(max(pOut->objs[i].w_f * ratio, 0.0f), f.w);
        float h_f = min(max(pOut->objs[i].h_f * ratio, 0.0f), f.h);

        // 根据类别ID判断有效性
        switch (pOut->objs[i].class_id) {
            case 0:  // 边缘检测
                edge_count++;
                if (check_is_edge(pOut, i)) {
                    if (cv::pointPolygonTest(train_head_poly, cv::Point(r.x, r.y), false) > 0 &&
                        cv::pointPolygonTest(train_head_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0 &&
                        w_f > 150 && h_f > 200) {
                        real_res_edge.emplace_back(x_f, w_f);
                        isEffective = true;
                    }
                    if (cv::pointPolygonTest(detection_poly, cv::Point(r.x, r.y), false) > 0 &&
                        cv::pointPolygonTest(detection_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0 &&
                        w_f > 150 && h_f > 200) {
                        real_res_edge_b.emplace_back(x_f, w_f);
                        isEffective = true;
                    }
                }
                break;

            case 1:  // 边缘检测
                if (w_f > 10 && h_f > 600) {
                    real_res_margin.emplace_back(x_f, w_f);
                    isEffective = true;
                }
                break;

            case 2:  // 插销检测
                if (cv::pointPolygonTest(detection_poly, cv::Point(r.x, r.y), false) > 0 &&
                    cv::pointPolygonTest(detection_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0 &&
                    w_f > 300 && h_f > 80) {
                    real_res_latch.emplace_back(x_f, w_f);
                    isEffective = true;
                }
                break;

            case 3:  // 把手检测
                if (w_f > 200 && h_f > 20) {
                    real_res_handle.emplace_back(x_f, w_f);
                    isEffective = true;
                }
                break;

            case 4:  // 互锁检测
                if (w_f > 80 && h_f > 150) {
                    real_res_mutex.emplace_back(x_f, w_f);
                    isEffective = true;
                }
                break;

            case 5:  // 车头检测
                if (cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 &&
                    cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0 &&
                    w_f > 150 && h_f > 600) {
                    real_res_head.emplace_back(x_f, w_f);
                    isEffective = true;
                }
                break;

            case 6:  // 按钮检测
                if (cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 &&
                    cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0 &&
                    w_f > 100 && h_f > 100) {
                    real_res_button.emplace_back(x_f, w_f);
                    isEffective = true;
                }
            case 7: //lock检测
                if(cv::pointPolygonTest(train_edge_poly, cv::Point(r.x, r.y), false) > 0 &&
                    cv::pointPolygonTest(train_edge_poly, cv::Point(r.x + r.width, r.y + r.height), false) > 0 &&
                    w_f > 50 ) {
                    real_res_lock.emplace_back(x_f, w_f);
                    isEffective = true;
                }

                break;
        }

        // 记录当前元素是否有效
        effective_pOut[i] = isEffective ? 1 : 0;
    }

    // 第二步：使用双指针法将有效元素前移
    int writeIndex = 0;
    for (int readIndex = 0; readIndex < pOut->count; readIndex++) {
        if (effective_pOut[readIndex]) {
            if (readIndex != writeIndex) {
                pOut->objs[writeIndex] = pOut->objs[readIndex];  // 内存拷贝
            }
            writeIndex++;
        }
    }
    // 第三步：更新有效元素数量
    pOut->count = writeIndex;

    // 对所有检测结果按照第一个值center_x进行排序
    std::sort(real_res_margin.begin(), real_res_margin.end());
    std::sort(real_res_latch.begin(), real_res_latch.end());
    std::sort(real_res_edge.begin(), real_res_edge.end());
    std::sort(real_res_edge_b.begin(), real_res_edge_b.end());
    std::sort(real_res_handle.begin(), real_res_handle.end());
    std::sort(real_res_mutex.begin(), real_res_mutex.end());
    std::sort(real_res_head.begin(), real_res_head.end());
    std::sort(real_res_button.begin(), real_res_button.end());
    std::sort(real_res_lock.begin(), real_res_lock.end());

    cout << "button:" << real_res_button.size() << endl;
    cout << "button:" << res_button_match.size() << endl;
    cout << "latch:" << real_res_latch.size() << endl;
    cout << "latch:" << res_latch_match.size() << endl;
    cout << "mutex:" << real_res_mutex.size() << endl;
    cout << "mutex:" << res_mutex_match.size() << endl;
    cout << "edge:" << real_res_edge.size() << endl;
    cout << "edge:" << res_edge_match.size() << endl;
    cout << "handle:" << real_res_handle.size() << endl;
    cout << "handle:" << res_handle_match.size() << endl;
    cout << "margin:" << real_res_margin.size() << endl;
    cout << "margin:" << res_margin_match.size() << endl;    
    cout << "edge_b:" << real_res_edge_b.size() << endl;
    cout << "edge_b:" << res_edge_b_match.size() << endl;   
    cout << "head:" << real_res_head.size() << endl;
    cout << "head:" << res_head_match.size() << endl;
    cout << "lock:" << real_res_lock.size() << endl;

    /// 测速部分
    int target_used = -1;

    // 创建所有检测框合集
    current_boxes = {real_res_mutex, real_res_button, real_res_handle, real_res_edge_b, real_res_latch, real_res_margin, real_res_head, real_res_lock};
    prev_boxes =    {res_mutex_match,res_button_match,res_handle_match,res_edge_b_match,res_latch_match,res_margin_match,res_head_match,res_lock_match};

    // 匹配检测结果 forward_match:当前帧->上一帧, backward_match:上一帧->当前帧
    auto [forward_match, backward_match] = match_tracks(prev_boxes, current_boxes,150.0F);
    
    // 根据匹配结果计算每个检测框与上一帧中移动的距离
    string match_str[TARGET_NUMBER] = {"mutex ","button","handle","edge  ","latch ","margin","head  ","lock  "};
    cout << "forward_match dx" << endl;
    
    // 初始化 match_move_result 的大小
    match_move_result.resize(forward_match.size());

    // 输出匹配结果
    for (int i = 0; i < forward_match.size(); i++)
    {
        cout << match_str[i] << ": ";
        for (int j = 0; j < forward_match[i].size(); j++)
        {
            auto jt = forward_match[i][j];
            float dx = -1;
            if (jt != -1)
            {
                dx = abs(current_boxes[i][j].first - prev_boxes[i][jt].first);
                cout << dx << " ";
            }
            match_move_result[i].push_back(dx);
            all_match_move_result.push_back(dx);
        }
        cout << endl;
    }

    // save_move_distance("all_class_move_distance.csv",frame_info->video_frame.time_ref,carriage_number,match_move_result);


    // 计算画面中有两个相同标志物的时候，计算他们移动距离的差值；来判断透视矫正效果
    // FILE* fp1 = fopen("left&right.csv", "a");
    // // 写CSV头部（只在文件为空时写一次）
    // // if (ftell(fp1) == 0) {
    // //     fprintf(fp1, "class,left,right\n");
    // // }
    // for(int i = 0; i < match_move_result.size(); i++){
    //     if(match_move_result[i].size()>1&&match_move_result[i][0] != -1 && match_move_result[i][1] != -1){
    //         fprintf(fp1, "%d,%f,%f,%f,%f\n", i, current_boxes[i][0].first,current_boxes[i][1].first,match_move_result[i][0], match_move_result[i][1]);
    //         float dx = (match_move_result[i][0] - match_move_result[i][1]);
    //         left_minus_right[i].first += dx;
    //         left_minus_right[i].second++;
    //     }
    // }
    // for(int i = 0; i < match_move_result.size(); i++){
    //     cout << match_str[i] << "left-right: " << left_minus_right[i].first / left_minus_right[i].second << endl;
    // }
    // fclose(fp1);
    

    
    // // 计算每种检测框的移动距离和mutex移动距离的比值
    // for(int number = 1;number < TARGET_NUMBER;number++){
    //     if (match_move_result[number].size()>0 && match_move_result[0].size()>0)
    //     {   
    //         float avg_x = 0;
    //         int cont = 0;
    //         for(auto i=0;i < match_move_result[number].size();i++)
    //         {
    //             if(match_move_result[number][i] != -1){
    //                 avg_x +=  match_move_result[number][i];
    //                 cont++;
    //             }
                
    //         }
    //         if(cont>0){
    //             avg_x /= cont;
    //         }
            

    //         float avg_mutex_x = 0;
    //         cont = 0;
    //         for(auto i=0;i < match_move_result[0].size();i++)
    //         {
    //             if(match_move_result[0][i] != -1){
    //                 avg_mutex_x += match_move_result[0][i];
    //                 cont++;
    //             }
                
    //         }
    //         if(cont>0){
    //             avg_mutex_x /= cont;
    //         }
            
    //         if(avg_x > 0 && avg_mutex_x > 0){
    //             float rate = avg_mutex_x/avg_x;
    //             class_dx_ratio[number].push_back(rate);
    //         }
            
    //     }
    // }
    

    
    // 输出每个匹配上的框的x值
    // for (int i = 0; i < forward_match.size(); i++)
    // {
    //     auto it = forward_match[i];
    //     cout << match_str[i] << ": ";
    //     for (int j = 0; j < it.size(); j++)
    //     {
    //         auto jt = forward_match[i][j];
    //         float dx = -1;
    //         if (jt != -1)
    //         {
    //             cout << current_boxes[i][j].first << " ";
    //         }
    //     }
    //     cout << endl;
    // }

    
    // 测试定位，移动距离暂时使用第一个
    // for(auto it : match_move_result){
    //     if(!it.empty()){
    //         train_speed_frame = it[0]==-1?train_speed_frame:it[0];
    //         break;
    //     }
    // }

    // 比例尺
    
    // 移动距离使用优先级，有多个时使用离中线最近的那个
    vector<vector<pair<float, float>>> match_move_result_with_cutern_x(forward_match.size());        // 储存匹配后所有的移动结果以及对应框的x值
    for (int i = 0; i < forward_match.size(); i++)
    {
        auto it = forward_match[i];
        for (int j = 0; j < it.size(); j++)
        {
            auto jt = forward_match[i][j];
            float dx;
            if (jt != -1)
            {
                dx = current_boxes[i][j].first - prev_boxes[i][jt].first;
                float closest_dx = abs(current_boxes[i][j].first - f.w/2);
                match_move_result_with_cutern_x[i].emplace_back(closest_dx,dx);
            }
        }
    }
    for(int i = 0; i < match_move_result_with_cutern_x.size();i++){
        auto it = match_move_result_with_cutern_x[i];
        if(!it.empty()){
            std::sort(it.begin(), it.end());
            train_speed_frame = IS_TO_RIGHT * it[0].second * carriage_info2->scale_ratio[i];
            class_count[i]++;
            target_used = i;
            break;
        }
    }


    // 使用优先级
    // for(auto it : match_move_result){
    //     if(!it.empty()){
    //         train_speed_frame = 0;
    //         if(it.size()>2){
    //             for(int i=1;i<it.size()-1;i++){
    //                 train_speed_frame += it[i];
    //             }
    //             train_speed_frame = train_speed_frame / (float(it.size() - 2));
    //         }
    //         else if(it.size() > 0){
    //             for(auto &res : it){
    //                 train_speed_frame += res;
    //             }
    //             train_speed_frame = train_speed_frame / float(it.size());
    //         }
    //         break;
    //     }
    // }

    // 移动距离筛选：1-3个结果值选取中间值，超过三个去头去尾选取中间值
    // std::sort(all_match_move_result.begin(), all_match_move_result.end());
    // int count = all_match_move_result.size();
    // if (count == 0)
    // { // 没有检测结果保留上一帧的值
    //     train_speed_frame = train_speed_frame;
    // }
    // else if (count > 0 && count <= 3)
    // {
    //     train_speed_frame = count % 2 ? all_match_move_result[count / 2] : (all_match_move_result[count / 2 - 1] + all_match_move_result[count / 2]) / 2.0f;
    // }
    // else
    // {
    //     // 动态截断模式（保留中间50%）
    //     const size_t keep = count / 2;
    //     const size_t start = (count - keep) / 2;
    //     auto begin = all_match_move_result.begin() + start;
    //     auto end = begin + keep;

    //     // 第四步：计算截断区间的均值
    //     train_speed_frame = std::accumulate(begin, end, 0.0f) / keep;
    // }

    /*----优先级跟踪模式-------
        TODO：逻辑暂时有问题不能正常使用，
        并且当前场景下，目标最多同时出现两个，使用追踪最左边的和这个方法没有本质区别
        对一个标志物一直跟踪计算他的速度，当这个标志物消失之后
        寻找标志物进入画面一侧优先级最高的优先级标志物进行跟踪，
        以减少目标切换产生造成的误差
        如果切换的标志物不是最高优先级，则在下一帧继续寻找更高优先级的标志物进行跟踪
    */
    // cout << "1" <<endl;
    // if (last_box_id.first == -1)
    // { // 如果上一帧没有检测到任何标志物，则寻找当前帧中优先级最高且最靠近画面右侧的标志物进行跟踪
    //     cout << "2" <<endl;
    //     if (IS_TO_RIGHT)
    //     {
    //         bool found = false;
    //         for (int i = 0; i < current_boxes.size(); i++)
    //         {
    //             for (int j = 0; j < current_boxes[i].size(); j++)
    //             {
    //                 if (current_boxes[i][j].first != -1)
    //                 {
    //                     last_box_id = make_pair(i, j);
    //                     found = true;
    //                     break;
    //                 }
    //             }
    //             if(found) break;
    //         }
    //     }
    //     else
    //     {
            
    //         bool found = false;
    //         for (int i = 0; i < current_boxes.size(); i++)
    //         {
    //             for (int j = current_boxes[i].size() - 1; j >= 0; j--)
    //             {
    //                 if (current_boxes[i][j].first != -1)
    //                 {
    //                     last_box_id = make_pair(i, j);
    //                     found = true;
    //                     break;
    //                 }
    //             }
    //             if(found) break;
    //         }
    //     }
    // }
    // else
    // { 
    //     if(last_box_id.first != 0){ //如果上一个跟踪的检测框不是最高优先级 寻找最高优先级最靠边的计算
    //         cout << "3" <<endl;
    //         bool found = false;
    //         for(int i = 0; i < backward_match.size(); i++){
    //             if (IS_TO_RIGHT){
    //                 for(int j = 0; j < backward_match[i].size(); j++){
    //                     if(backward_match[i][j] != -1){
    //                         last_box_id = make_pair(i, j);
    //                         found = true;
    //                         break;
    //                     }
    //                 }
    //             }else{
    //                 for(int j = backward_match.size() - 1; j >= 0; j--){
    //                     if(backward_match[i][j] != -1){
    //                         last_box_id = make_pair(i, j);
    //                         found = true;
    //                         break;
    //                     }
    //                 }
    //             }
    //             if(found) break;
    //         }
    //         if(found){

    //             int curr_idx = backward_match[last_box_id.first][last_box_id.second];
    //             float current_x = current_boxes[last_box_id.first][curr_idx].first;
    //             float prev_x = prev_boxes[last_box_id.first][last_box_id.second].first;
    //             train_speed_frame = IS_TO_RIGHT * (current_x - prev_x);
    //             last_box_id = make_pair(last_box_id.first,curr_idx);
    //         }else{// 如果当前所有框都没有找到上一帧有对应的 维持当前移动距离 并设置最高优先级的最边一个为跟踪
    //             cout << "4" <<endl;
    //             train_speed_frame = train_speed_frame;
    //             for(int i = 0; i < current_boxes.size(); i++){
    //                 if(current_boxes[i].size()>0){
    //                     if(IS_TO_RIGHT){
    //                         last_box_id = make_pair(i,0);
    //                     }
    //                     else{
    //                         last_box_id = make_pair(i,current_boxes[i].size()-1);
    //                     }
                       
    //                 }
    //             }
    //         }
    //     }else{// 如果是最高优先级
    //         cout << "5" <<endl;
    //         for(int i = 0; i < backward_match.size(); i++){
    //             for(int j = 0; j < backward_match[i].size(); j++){
    //                 cout <<  backward_match[i][j] << " ";
    //             }
    //             cout << endl;
    //         }
    //         // 如果存在上一帧检测结果 通过匹配算法找到这个检测框在上一帧的位置
    //         if(backward_match[last_box_id.first].size()>0){
    //             cout << "6" <<endl;
    //             int curr_idx = backward_match[last_box_id.first][last_box_id.second];
    //             float current_x = current_boxes[last_box_id.first][curr_idx].first;
    //             float prev_x = prev_boxes[last_box_id.first][last_box_id.second].first;
    //             train_speed_frame = IS_TO_RIGHT * (current_x - prev_x);
    //             last_box_id = make_pair(last_box_id.first,curr_idx);
    //         }
    //         // 如果当前帧不存在上一帧跟踪的目标 则重新寻找目标跟踪
    //         else{
    //             cout << "7" <<endl;
    //             bool found = false;
    //             for(int i = 0; i < backward_match.size(); i++){
    //                 if (IS_TO_RIGHT){
    //                     for(int j = 0; j < backward_match[i].size(); j++){
    //                         if(backward_match[i][j] != -1){
    //                             last_box_id = make_pair(i, j);
    //                             found = true;
    //                             break;
    //                         }
    //                     }
    //                 }else{
    //                     for(int j = backward_match.size() - 1; j >= 0; j--){
    //                         if(backward_match[i][j] != -1){
    //                             last_box_id = make_pair(i, j);
    //                             found = true;
    //                             break;
    //                         }
    //                     }
    //                 }
    //                 if(found) break;
    //             }
    //             if(found){

    //                 int curr_idx = backward_match[last_box_id.first][last_box_id.second];
    //                 float current_x = current_boxes[last_box_id.first][curr_idx].first;
    //                 float prev_x = prev_boxes[last_box_id.first][last_box_id.second].first;
    //                 train_speed_frame = IS_TO_RIGHT * (current_x - prev_x);
    //                 last_box_id = make_pair(last_box_id.first,curr_idx);
    //             }else{// 如果当前所有框都没有找到上一帧有对应的 维持当前移动距离 并设置最高优先级的最边一个为跟踪
    //                 cout << "8" <<endl;
    //                 train_speed_frame = train_speed_frame;
    //                 for(int i = 0; i < current_boxes.size(); i++){
    //                     if(current_boxes[i].size()>0){
    //                         if(IS_TO_RIGHT){
    //                             last_box_id = make_pair(i,0);
    //                         }
    //                         else{
    //                             last_box_id = make_pair(i,current_boxes[i].size()-1);
    //                         }
    //                     }
    //                 }
    //             }
    //         }

    //     }  
    // }
    // cout << "Tracking: class=" << last_box_id.first 
    //  << ", index=" << last_box_id.second << endl;    

    // 设置权重
    float px_per_s_to_m_per_s = carriage_info2->px_to_meter_ratio;                  // px/s -> m/sr
    float m_per_s_to_km_per_h = px_per_s_to_m_per_s * 3.6;                          // m/s -> km/h
    // 设置速度计算累计帧数
    int n = 10;
    cout << "train_speed_frame: " << train_speed_frame << endl;
    frame_count++;                        // 帧计数器自增
        
    move_distance += train_speed_frame;
    train_speed_show_ = simple_moving_average(train_speed_frame);
    cout << "speed:" << train_speed_show_ * frame_rate << "pixel/s" << endl;
    cout << "speed:" << train_speed_show_ * frame_rate * px_per_s_to_m_per_s << "m/s" << endl;
    cout << "speed:" << train_speed_show_ * frame_rate * m_per_s_to_km_per_h << "km/h" << endl;
    cout << "time_ref:" << frame_info->video_frame.time_ref << endl;


    /// 定位部分
    // ===========================================================================
    // 边缘跟踪与车厢计数系统逻辑说明
    // 系统工作模式:
    //   1. 初始检测模式 - 连续5帧检测到边缘后进入跟踪模式
    //   2. 边缘跟踪模式 - 执行核心车厢计数和定位逻辑
    //
    // 核心状态变量:
    //   carriage_number   : 当前计数的车厢编号(从1开始)
    //   is_in_carriage    : 当前是否在车厢内部(true/false)
    //   is_edge_entered   : 边缘是否已进入检测区域
    //   move_state        : 边缘移动状态(1-4)
    //
    // 数据记录结构:
    //   train_distance_list : 记录每个车厢的完整长度
    //   train_frame_list    : 记录车厢起始/结束帧时间戳
    //   edge_head_tail      : 记录每个车厢头尾的绝对位置
    //
    // ===========================================================================
    // 边缘跟踪模式核心逻辑
    // ---------------------------------------------------------------------------
    // 步骤1：设置动态检测区域
    //   - 列车向右：检测区[中线左侧阈值, 中线]
    //   - 列车向左：检测区[中线, 中线右侧阈值]
    //
    // 步骤2：单边缘处理（检测区域内）
    //   A. 边缘进入区域：
    //        - 记录进入方向(is_left_entry)
    //        - 标记is_edge_entered=true
    //   
    //   B. 边缘移出区域：
    //        - 确定标志物类型（按钮>插销>车头）
    //        - 计算移动状态(move_state)：
    //           1: 正向通过  2: 反向通过  
    //           3: 正向进反向出  4: 反向进正向出
    //        - 判断车头/车尾(is_head)：
    //           * 有标志物：根据位置关系判断
    //           * 无标志物：根据移动状态和车厢内外状态判断
    //        - 状态处理：
    //          ┌──────────┬───────────────────────┬───────────────────────┐
    //          │ move_state │ 车头处理              │ 车尾处理              │
    //          ├──────────┼───────────────────────┼───────────────────────┤
    //          │    1      │ 车厢号++ 记录起始位置  │ 输出车厢长度          │
    //          │    2      │ 车厢号--              │ 撤销上次记录长度      │
    //          │    3      │ 标记离开车厢          │ 标记进入车厢          │
    //          │    4      │ 标记进入车厢          │ 标记离开车厢          │
    //          └──────────┴───────────────────────┴───────────────────────┘
    //
    // 步骤3：边缘位置追踪
    //   - 根据车厢位置（左/右区域）和运行方向：
    //       向右运行在车厢内：
    //         左区域→更新车尾位置 | 右区域→更新车头位置
    //       向左运行在车厢内：
    //         左区域→更新车头位置 | 右区域→更新车尾位置
    //   - 车厢外时更新相邻车厢位置
    //
    // 步骤4：边缘丢失处理
    //   - 连续5帧丢失边缘 → 退出跟踪模式
    //
    // ===========================================================================
    // 初始检测模式逻辑
    // ---------------------------------------------------------------------------
    //   - 持续累加运行距离(current_distance)
    //   - 边缘检测：
    //       连续5帧检测到边缘 → 进入跟踪模式
    //       连续5帧丢失边缘 → 重置检测状态
    //
    // ===========================================================================
    // 关键参数说明:
    //   IS_TO_RIGHT    : 列车运行方向(1:向右, -1:向左)
    //   ZONE_THRESHOLD : 检测区域宽度(像素)
    //   midPoint       : 画面中线坐标
    //   train_speed_frame : 每帧移动距离
    // ===========================================================================
    

    int start_case = -1;
    int end_case = -1;
    float start_offset = -1;
    float end_offset = -1;


    const float midPoint = f.w / 2;
    /// ================== 边缘跟踪模式 ==================
    if (is_find_edge)
    {

        // 车厢计数以及计算车长,当画面中只有一个edge,并且移动到中线时才计数
        current_distance += train_speed_frame;
        pair<float, float> detection_area;
        if(IS_TO_RIGHT == 1){
            // 车辆向右移动 判定区域为 中线靠左ZONE_THRESHOLD个像素到中线
            detection_area = {midPoint - ZONE_THRESHOLD, midPoint};
        }else{
            // 车辆向左移动 判定区域为 中线靠右ZONE_THRESHOLD个像素到中线
            detection_area = {midPoint, midPoint + ZONE_THRESHOLD};
        }
        
        
        if (real_res_edge_b.size() == 1)
        {
            float distance = midPoint - real_res_edge_b[0].first;      
            
            // 检测到 edge_b 进入判定区域
            if (real_res_edge_b[0].first>detection_area.first && real_res_edge_b[0].first<detection_area.second)
            {
                if (!is_edge_entered)
                {
                    // 记录进入状态
                    is_edge_entered = true;
                    
                    // 记录进入判定区域的 edge_b是从画面哪一边进入的判定区域 用来判断车厢是否倒退
                    int last_edge_b_idx = forward_match[3][0];
                    float last_edge_b_location = res_edge_b_match[last_edge_b_idx].first;// 当前帧edge_b对应上一帧的坐标
                    if(last_edge_b_location <= detection_area.first){
                        // 从左侧进入
                        is_left_entry = 1;
                    }else if(last_edge_b_location >= detection_area.second){
                        // 从右侧进入
                        is_left_entry = -1;
                    }
                }
            }
            else
            {
                // edge_b 移出判定区域
                if (is_edge_entered)
                {
                    // 此时才进行车头/车尾判断和处理
                    // 使用 enter_position 和当前 real_res_edge_b[0].first 进行逻辑判断

                    // 定义变量来存储检测到的标志物信息
                    int marker_type = 0;  // 0: 无, 1: 按钮, 2: 插销, 3: 头部
                    float marker_position = 0;
                    float marker_width = 0;
                    int case_type = 0;
                    float offset = 0;

                    // 检测不同类型的标志物
                    if (real_res_button.size() == 1)
                    {
                        marker_type = 1;
                        marker_position = real_res_button[0].first;
                        marker_width = real_res_button[0].second;
                        case_type = 1;
                    }
                    else if (real_res_latch.size() == 1)
                    {
                        marker_type = 2;
                        marker_position = real_res_latch[0].first;
                        marker_width = real_res_latch[0].second;
                        case_type = 2;
                    }
                    else if (!real_res_head.empty())
                    {
                        marker_type = 3;
                        marker_position = real_res_head[0].first;
                        marker_width = real_res_head[0].second;
                        case_type = 3;
                    }else{
                        // 未能找到其他标志物
                        marker_type = 0;
                        marker_position = real_res_edge_b[0].first;
                        marker_width = real_res_edge_b[0].second;
                        case_type = 0;
                    }

                    bool is_head = false;
                    int move_state = 0;
                    int is_right_out = 0;// edge_b是否从判定区域右侧移出 1:是 -1:否 0:未移出

                    if(real_res_edge_b[0].first <= detection_area.first){
                        // 从左侧移出
                        is_right_out = -1;
                    }else if(real_res_edge_b[0].first >= detection_area.second){
                        // 从右侧移出
                        is_right_out = 1;
                    }

                    /* 标志物edge_b通过判定区域的情况：
                        1、正向通过判定区域 从判定区域异侧进入和移出 移出方向和车厢运行方向相同：
                                                                        车头：正常计数 设置为在车厢内
                                                                        车尾：正常输出长度 设置为在车厢外
                        2、反向通过判定区域 从判定区域异侧进入和移出 移出方向和车厢运行方向不同：
                                                                        车头：车厢号减1 设置为在车厢外
                                                                        车尾：重置当前位置 删除上一次保存的车厢长度，设置为正在车厢内
                        3、正向进入 反向移出判定区域 从判定区域同侧进入和移出 进入方向和车厢运行方向相同 移出方向和车厢运行方向不同：
                                                                        车头：设置为在车厢外
                                                                        车尾：不输出长度 设置为在车厢内
                        4、反向进入 正向移出判定区域 从判定区域同侧进入和移出 进入方向和车厢运行方向不同 移出方向和车厢运行方向相同：    
                                                                        车头：设置为在车厢内
                                                                        车尾：不输出长度 设置为在车厢外
                        */
                    // 通过edge_b进入和移出判定区域的方向 来判断车厢是否产生倒退
                    
                    if(is_left_entry * is_right_out == 1){
                        // edge_b从判定区域异侧进入和移出
                        if(is_left_entry == IS_TO_RIGHT){
                            // edge_b 进入和移出方向与车厢运行方向相同
                            move_state = 1;
                        }else{
                            // edge_b 进入和移出方向与车厢运行方向不同
                            move_state = 2;
                        }
                    }else if(is_left_entry * is_right_out == -1){
                        // edge_b从判定区域异侧进入和移出
                        if(is_left_entry == IS_TO_RIGHT){
                            // edge_b 进入方向和车厢运行方向相同 移出方向和车厢运行方向不同
                            move_state = 3;
                        }else{
                            // edge_b 进入方向和车厢运行方向不同 移出方向和车厢运行方向相同
                            move_state = 4;
                        }
                    }

                    // 如果存在标志物，使用标志物判断车头或是车尾
                    if (marker_type != 0)
                    {
                        // 判断当前标志物所在的是车厢左侧边缘还是右侧边缘
                        bool is_carriage_left_side = (marker_type == 3) ? 
                            (marker_position < real_res_edge_b[0].first) : 
                            (marker_position > real_res_edge_b[0].first);

                        if(IS_TO_RIGHT == -1){
                            // 车厢向左移动，左侧车厢是车头
                            is_head = is_carriage_left_side;
                        }else{
                            // 车厢向右移动，右侧车厢是车头
                            is_head = !is_carriage_left_side;
                        }

                    }else{// 未能找到标志物来判断车头或是车尾
                        switch(move_state){
                            case 1:// 正向移动
                            case 3:// 正向进入，反向移出判定区域
                                if(is_in_carriage){
                                    // 正向移动，当前在车厢内，则为车尾
                                    is_head = false;
                                }else {
                                    // 正向移动，当前不在车厢内，则为车头
                                    is_head = true;
                                }
                                break;
                            case 2:// 反向移动
                            case 4:// 反向进入，正向移出判定区域
                                if(is_in_carriage){
                                    // 反向移动，当前在车厢内，则为车头
                                    is_head = true;
                                }else{
                                    // 反向移动，当前不在车厢内，则为车尾
                                    is_head = true;
                                }
                        }
                        
                    }

                    // 根据移动状态和车头车尾进行计数和输出
                    switch(move_state)
                    { 
                        case 1: // 正向通过判定区域 正常计数
                            if (!is_head)
                            {
                                // 车尾
                                if (carriage_number > 0)
                                {
                                    switch (marker_type)
                                    {
                                        case 0: // 未能找到标志物
                                            offset = -(midPoint - marker_position - marker_width / 2) * IS_TO_RIGHT + 450;
                                            break;
                                        case 1: // 按钮
                                            offset = -(marker_position - midPoint - marker_width / 2) * IS_TO_RIGHT + 1145;
                                            break;
                                        case 2: // 插销
                                            offset = -(marker_position - midPoint - marker_width / 2) * IS_TO_RIGHT + 1247;
                                            break;
                                        case 3: // 头部
                                            offset = (midPoint - marker_position) * IS_TO_RIGHT + marker_width / 2;
                                            break;
                                    }

                                    // // 测试1：使用头尾edge之间的距离
                                    // offset = real_res_edge_b[0].first - midPoint;

                                    train_distance_list.push_back(current_distance + offset);
                                    carriage_number;
                                    float temp = current_distance + offset;
                                    end_frame = frame_info->video_frame.time_ref;
                                    train_frame_list.emplace_back(start_frame, end_frame);
                                    end_case = case_type;
                                    end_offset = offset;
                                }
                                
                                is_in_carriage = TD_FALSE;
                            }
                            else
                            {   
                                // 车头
                                offset = -(real_res_edge_b[0].first - real_res_edge_b[0].second / 2) + 245;

                                // // 测试1：使用头尾edge之间的距离
                                // offset = - real_res_edge_b[0].first;

                                current_distance = midPoint + offset;


                                is_carriage_head = true;
                                carriage_number++;
                                start_frame = frame_info->video_frame.time_ref;
                                start_case = case_type;
                                start_offset = midPoint + offset;
                            
                                is_in_carriage = TD_TRUE;
                            }
                            break;
                        case 2:// 反向通过判定区域
                            if(carriage_number > 0){
                                if(!is_head){// 车尾
                                    switch (marker_type)
                                        {
                                            case 1: // 按钮
                                                offset = -(marker_position - midPoint - marker_width / 2) * IS_TO_RIGHT + 1145;
                                                break;
                                            case 2: // 插销
                                                offset = -(marker_position - midPoint - marker_width / 2) * IS_TO_RIGHT + 1247;
                                                break;
                                            case 3: // 头部
                                                offset = IS_TO_RIGHT * (midPoint - marker_position) + marker_width / 2;
                                                break;
                                        }

                                    // // 测试1：使用头尾edge之间的距离
                                    // offset = real_res_edge_b[0].first - midPoint;

                                    current_distance = train_distance_list.back() - offset;
                                    train_distance_list.pop_back();
                                    is_in_carriage = TD_TRUE;
                                }else{// 车头
                                    carriage_number--;
                                    is_in_carriage = TD_FALSE;
                                }
                            }else{
                                // LOG_ERROR("carriage_number error");
                            }
                            
                            break;
                        case 3:// 正向进入 反向移出判定区域
                            if(!is_head){
                                is_in_carriage = TD_TRUE;
                            }else{
                                is_in_carriage = TD_FALSE;
                            }
                            break;

                        case 4:// 反向进入 正向移出判定区域
                            if(!is_head){
                                is_in_carriage = TD_TRUE;
                            }else{
                                is_in_carriage = TD_FALSE;
                            }
                            break;

                        // default:
                            //状态异常
                            // LOG_ERROR("move_state error! is_left_entry:%d, is_right_out:%d, IS_TO_RIGHT:%d",
                            //                     is_left_entry,is_right_out,IS_TO_RIGHT);
                    } 
                    // 重置状态，为下一次检测做准备
                    is_edge_entered = false;
                    // 重置状态
                    is_left_entry = 0; 
                }
            }
        }
        // 边缘检测异常时重置状态
        else
        {
            is_carriage_head = false;
            is_carriage_tail = false;
        }

        /*
        画面中检测到edge 则给出车头或车位到中线的距离

                车辆移动方向(标向)          in_carriage           out_carriage
                                    图像左半边 | 图像右半边    图像左半边 | 图像右半边
                    向左                车头        车尾          车尾        车头
                    向右                车尾        车头          车头        车尾            
        */
        
        for (int i = 0;i < real_res_edge_b.size();i++){
            float edge_b_x = real_res_edge_b[i].first;// 当前edge的中心点坐标

            // carriage_number 计数从1开始,0表示没有
            if(edge_head_tail.size() <= carriage_number){
                edge_head_tail.emplace_back(NAN,NAN);
            }

            // 检查车头车尾列表中是否存在当前车厢的值,如果有就使用列表记录值,没有使用车型预设值(在火车产生倒退的时候使用)
            float carriage_length;
            if( edge_head_tail.size()> 0 && carriage_number > 0 && 
                !std::isnan(edge_head_tail[carriage_number-1].first) && 
                !std::isnan(edge_head_tail[carriage_number-1].second)){
                carriage_length = edge_head_tail[carriage_number-1].second - edge_head_tail[carriage_number-1].first;
            }else{
                carriage_length = carriage_info2->length_px;// 使用车型的预设值
            }

            if(IS_TO_RIGHT == 1){
                // 火车向右移动
                if(is_in_carriage){
                    // 在车厢内
                    if(edge_b_x < midPoint){
                        // edge 在画面左半边是车尾,重置车尾位置
                        to_current_tail_distance = midPoint - edge_b_x;
                        to_current_head_distance = to_current_tail_distance + carriage_length;
                        edge_head_tail[carriage_number-1].second = move_distance - to_current_tail_distance;// 记录并更新车尾在移动总长(move_distance)中的位置
                    }else{
                        // edge 在画面右半边是车头,重置车头位置
                        to_current_head_distance = midPoint - edge_b_x;
                        to_current_tail_distance = to_current_head_distance - carriage_length;
                        edge_head_tail[carriage_number-1].first = move_distance - to_current_head_distance;// 记录并更新车头在移动总长(move_distance)中的位置
                    }

                    // 将to_next_head_distance 置为无效值
                    to_next_head_distance = NAN;

                    // to_last_tail_distance 等于到当前车头的距离 + 当前车头和上一车尾的间距
                    if(carriage_number > 1){
                        to_last_tail_distance = to_current_head_distance + (edge_head_tail[carriage_number-1].first - edge_head_tail[carriage_number - 2].second);
                    }else{
                        to_last_tail_distance = NAN;// 如果没有上一车车尾 置为无效
                    }
                    
                }else{
                    // 不在车厢内
                    if(edge_b_x < midPoint){
                        // edge 在画面左半边是下一车车头,更新其位置
                        to_next_head_distance = midPoint - edge_b_x;
                        
                        // 更新下一车车头的位置
                        edge_head_tail[carriage_number].first = move_distance - to_next_head_distance;
                    }else{
                        // edge 在画面右半边是上一车车尾,更新其位置
                        to_last_tail_distance = midPoint - edge_b_x;
                        if(carriage_number > 0){
                            edge_head_tail[carriage_number - 1].second = move_distance - to_last_tail_distance;
                        }
                    }

                    // 将to_current_head_distance 和 to_current_tail_distance置为无效值
                    to_current_head_distance = NAN;
                    to_current_tail_distance = NAN;
                }

            
            }else if(IS_TO_RIGHT == -1){
                // 火车向左移动
                if(is_in_carriage){
                    // 在车厢内
                    if(edge_b_x < detection_area.first){
                        // edge 在画面左半边是车头,重置车头位置
                        to_current_head_distance = midPoint - edge_b_x;
                        to_current_tail_distance = to_current_head_distance - carriage_length;
                        edge_head_tail[carriage_number - 1].first = move_distance - to_current_head_distance;// 记录并更新车头在移动总长(move_distance)中的位置
                    }else if(edge_b_x > detection_area.second){
                        // edge 在画面右半边是车尾,重置车尾位置
                        to_current_tail_distance = midPoint - edge_b_x;
                        to_current_head_distance = to_current_tail_distance + carriage_length;
                        edge_head_tail[carriage_number - 1].second = move_distance - to_current_tail_distance;// 记录并更新车尾在移动总长(move_distance)中的位置
                    }
                    
                    // 将to_next_head_distance 置为无效值
                    to_next_head_distance = NAN;

                    // to_last_tail_distance 等于到当前车头的距离 + 当前车头和上一车尾的间距
                    if(carriage_number > 1){
                        to_last_tail_distance = to_current_head_distance + (edge_head_tail[carriage_number - 1].first - edge_head_tail[carriage_number - 2].second);
                    }else{
                        to_last_tail_distance = NAN;// 如果没有上一车车尾 置为无效
                    }
                }else{
                    // 不在车厢内
                    if(edge_b_x < detection_area.first){
                        // edge 在画面左半边是上一车车尾,更新其位置
                        to_last_tail_distance = midPoint - edge_b_x;
                        if(carriage_number > 0){
                            edge_head_tail[carriage_number - 1].second = move_distance - to_last_tail_distance;
                        }
                        
                    }else if(edge_b_x > detection_area.second){
                        // edge 在画面右半边是下一车车头,更新其位置
                        to_next_head_distance = midPoint - edge_b_x;

                        // 更新下一车车头的位置
                        edge_head_tail[carriage_number].first = move_distance - to_next_head_distance;
                    }

                    // 将to_current_head_distance 和 to_current_tail_distance置为无效值
                    to_current_head_distance = NAN;
                    to_current_tail_distance = NAN;
                }
            }
        }
        



        /// ----- 边缘丢失处理 -----
        if (real_res_edge_b.empty())
        {
            not_edge_count++; // 连续丢失计数器
            if (not_edge_count == 5)
            { // 连续5帧丢失则认为边缘消失
                find_edge_count = 0;
                is_find_edge = false; // 退出跟踪模式
            }
        }
    }

    /// ================== 初始检测模式 ==================
    else
    {
        // 重置车厢状态
        is_carriage_head = false;
        is_carriage_tail = false;

        // 累积计数
        if (is_in_carriage){
            current_distance += train_speed_frame;
        }

        to_current_head_distance += train_speed_frame;
        to_current_tail_distance += train_speed_frame;
        to_last_tail_distance    += train_speed_frame;
        to_next_head_distance    += train_speed_frame;

        // 无边缘检测时的处理
        if (real_res_edge_b.empty())
        {
            if (carriage_number == 0)
            {
                cout << "Not find train carriage" << endl;
            }

            // 已开始计数时，累加消失计数器
            if (start_count)
            {
                lose_count_after_start++;
            }

            // 连续消失5次后清除状态
            if (lose_count_after_start >= 5)
            {
                find_edge_count = 0;        // 重置边缘检测计数器
                lose_count_after_start = 0; // 重置消失计数器
                start_count = false;        // 停止计数（防止继续累加）
            }
        }
        // 检测到边缘时的处理
        else
        {
            find_edge_count++;
            start_count = true;         // 标记开始计数
            lose_count_after_start = 0; // 重置消失计数器（重要！）

            // 连续5次检测到边缘后确认有效
            if (find_edge_count == 5)
            {
                is_find_edge = true;
                not_edge_count = 0;
                if (carriage_number == 0)
                {
                    cout << "find first train carriage" << endl;
                }
            }

            // if (carriage_number > 0)
            // {
            //     current_distance += train_speed_frame;
            // }
        }
    }
    
    res_latch_match = real_res_latch;      // 更新门闩检测结果
    res_margin_match = real_res_margin;    // 更新边缘检测结果
    res_handle_match = real_res_handle;    // 更新把手检测结果
    res_mutex_match = real_res_mutex;      // 更新互斥检测结果
    res_button_match = real_res_button;    // 更新按钮检测结果
    res_edge_match = real_res_edge;     // 更新边缘检测结果
    res_edge_b_match = real_res_edge_b; // 更新边缘B检测结果
    res_head_match = real_res_head;     // 更新头部检测结果
    res_lock_match = real_res_lock;     // 更新锁检测结果


    // 向共享数据结构体写入数据
    speed_data->carriage_number = carriage_number;
    // speed_data->to_current_head_distance = to_current_head_distance;
    // speed_data->to_current_tail_distance = to_current_tail_distance;
    // speed_data->to_last_tail_distance = to_last_tail_distance;
    // speed_data->to_next_head_distance = to_next_head_distance;
    speed_data->is_in_carriage = is_in_carriage;
    speed_data->speed = train_speed_show_ * frame_rate * px_per_s_to_m_per_s; // m/s
    speed_data->time = frame_info->video_frame.pts;
    speed_data->time_ref = frame_info->video_frame.time_ref;
    if(edge_head_tail.size()<256){
        for(int i = 0; i < edge_head_tail.size(); i++){
            speed_data->carriage_distance_list[i][0] =  (move_distance - edge_head_tail[i].first ) * carriage_info2->px_to_meter_ratio;
            speed_data->carriage_distance_list[i][1] =  (move_distance - edge_head_tail[i].second) * carriage_info2->px_to_meter_ratio;
        }
    }else{
        
    }

    memset(speed_data->carriage_type, 0, sizeof(speed_data->carriage_type));
    strncpy(speed_data->carriage_type, carriage_info2->model_name.c_str(),carriage_info2->model_name.size());
    speed_data->carriage_type[sizeof(speed_data->carriage_type) - 1] = '\0';
    sem_post(&speed_data->sem);
    // printf("now time: %ld\n", get_time_ms());
    // printf("pts time: %ld\n", frame_info->video_frame.pts);
    // printf("get frame to speed_data cost time: %ld ms\n",get_time_ms() - frame_info->video_frame.pts);
    
    
    // 计算并显示当前距离
    cout << "current distance:" << current_distance << "pix" << endl;
    cout << "move distabce:" << move_distance << endl;
    // 显示当前车厢编号
    cout << "current carriage number:" << carriage_number << endl;
    // 显示输出数据
    // cout << "to_last_tail_distance:" << to_last_tail_distance << endl;
    // cout << "to_next_head_distance:" << to_next_head_distance << endl;
    // cout << "to_current_head_distance:" << to_current_head_distance << endl;
    // cout << "to_current_tail_distance:" << to_current_tail_distance << endl;
    for(int i = 0; i < edge_head_tail.size(); i++){
        cout << "head,tail:" << edge_head_tail[i].first << "," << edge_head_tail[i].second <<endl; 
    }

    // 显示已经过车厢长度
    for(int i = 0; i < train_distance_list.size(); i++){
        cout << "carriage distance["<< i+1 << "]:" << train_distance_list[i] << "  frame_number:"<< train_frame_list[i].second - train_frame_list[i].first << endl;
    }
    for(int i = 0; i < train_distance_list.size(); i++){
        cout << train_frame_list[i].first << "  " << train_frame_list[i].second << endl;
    }
    for(int i = 0; i < train_distance_list.size(); i++){
        cout << train_distance_list[i] << endl;
    }


    // 统计每个margin在火车的位置
    // if(real_res_margin.size()>0){ 
    //     if(margin_distance.size() < carriage_number){
    //         vector<pair<float, float>> tmp(6, make_pair(numeric_limits<float>::max(), 0.0f));
    //         margin_distance.push_back(tmp);
    //     }

    //     float distance = abs(1224 - real_res_margin[0].first);
    //     if (distance<80){
    //         int number = -1;
    //         if(current_distance>2200&&current_distance<3200){
    //             number = 0;
    //         }
    //         else if(current_distance>4500&&current_distance<6000){
    //             number = 1;
    //         }
    //         else if(current_distance>7000&&current_distance<8000){
    //             number = 2;
    //         }
    //         else if(current_distance>9000&&current_distance<11000){
    //             number = 3;
    //         }
    //         else if(current_distance>12000&&current_distance<13000){
    //             number = 4;
    //         }
    //         else if(current_distance>14600&&current_distance<15400){
    //             number = 5;
    //         }
    //         if(number>=0){
    //             if(distance < margin_distance[carriage_number-1][number].first){
    //                 margin_distance[carriage_number-1][number].first = distance;
    //                 margin_distance[carriage_number-1][number].second = current_distance + real_res_margin[0].first -1224;
    //             }
    //         }
    //     }
    // }
    
    // for(int i = 0; i < margin_distance.size(); i++){
    //     auto it = margin_distance[i];
    //     for(int j = 0; j < it.size(); j++){
    //         cout << it[j].second << ",";
    //     }
    //     cout << endl;
    // }
    

    // save_info("train_distance.csv",frame_info->video_frame.time_ref,carriage_number,train_speed_frame,current_distance,target_used,start_case,start_offset,end_case,end_offset);
    
    // 保存有效的检测框和对应上一帧的序号
    // save_effect_objs("effect_objs.csv",current_boxes,prev_boxes,frame_info->video_frame.time_ref,forward_match,carriage_number);

    // 如果没有检测到任何对象，计数器自增
    if (real_res_margin.empty() and real_res_edge_b.empty() 
        and real_res_latch.empty() and real_res_button.empty() 
        and real_res_handle.empty() and real_res_mutex.empty() 
        and real_res_head.empty() and real_res_lock.empty())
    {
        frame_count_end++;
        if (frame_count_end >= frame_rate * 600)
        {
            if (carriage_number > 0)
            {
                cout << "The train has left" << endl;
            }
            train_speed_reset_data();
            // frame_count = 0;
            // carriage_number = 0;
            // train_speed_frame = 0;
            // current_distance = 0;
            // train_speed_show_ = 0;
            // train_distance_list.clear();   // 清空累积数据
            // train_frame_list.clear();      // 清空累积数据
            // frame_count_end = 0;
        }
    }else{
        frame_count_end = 0;
    }

    cout << "\n\n\n"<< endl;
    return TD_SUCCESS;
}

