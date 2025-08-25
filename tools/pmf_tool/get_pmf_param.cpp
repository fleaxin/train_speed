#include <opencv2/opencv.hpp>
#include <cmath>
#include <algorithm>
using namespace std;
using namespace cv;

struct HiPMFParams {
    int32_t pmf_coef[9]; // 使用int32_t存储定点数参数
};

// 带范围限制的定点数转换函数
int32_t scaleParameter(double val, int32_t min, int32_t max) {
    const int32_t FIXED_SCALE = 524288; // 对应pmf_coef[8]的固定值
    int32_t scaled = static_cast<int32_t>(std::round(val * FIXED_SCALE));
    return std::clamp(scaled, min, max);
}

HiPMFParams calculateHiPMF(const std::vector<cv::Point2f>& src,
                          const std::vector<cv::Point2f>& dst) {
    // 参数校验
    if (src.size() != 4 || dst.size() != 4) {
        throw std::runtime_error("需要4对匹配点");
    }

    // 计算正变换矩阵
    cv::Mat M = cv::getPerspectiveTransform(src, dst);
    
    // 计算逆矩阵并进行归一化
    cv::Mat M_inv = M.inv(cv::DECOMP_LU);
    double m22 = M_inv.at<double>(2,2);
    M_inv /= m22; // 归一化使右下角元素为1.0

    // 参数转换
    HiPMFParams params;
    params.pmf_coef[0] = scaleParameter(M_inv.at<double>(0,0), 157286, 891289);
    params.pmf_coef[1] = scaleParameter(M_inv.at<double>(0,1), -367001, 367001);
    params.pmf_coef[2] = scaleParameter(M_inv.at<double>(0,2), INT32_MIN, INT32_MAX);
    params.pmf_coef[3] = scaleParameter(M_inv.at<double>(1,0), -367001, 367001);
    params.pmf_coef[4] = scaleParameter(M_inv.at<double>(1,1), 157286, 891289);
    params.pmf_coef[5] = scaleParameter(M_inv.at<double>(1,2), INT32_MIN, INT32_MAX);
    params.pmf_coef[6] = scaleParameter(M_inv.at<double>(2,0), -26, 104);
    params.pmf_coef[7] = scaleParameter(M_inv.at<double>(2,1), -26, 104);
    params.pmf_coef[8] = 524288; // 固定参数

    return params;
}

int main(int argc, char* argv[]) {
    if (argc != 17) {
        cerr << "Usage: " << argv[0] << " src_x1 src_y1 src_x2 src_y2 src_x3 src_y3 src_x4 src_y4 dst_x1 dst_y1 dst_x2 dst_y2 dst_x3 dst_y3 dst_x4 dst_y4" << endl;
        return 1;
    }

    vector<Point2f> srcPoints;
    vector<Point2f> dstPoints;

    try {
        for (int i = 0; i < 4; ++i) {
            srcPoints.emplace_back(stof(argv[1 + 2 * i]), stof(argv[2 + 2 * i]));
            dstPoints.emplace_back(stof(argv[9 + 2 * i]), stof(argv[10 + 2 * i]));
        }
    } catch (const std::invalid_argument& e) {
        cerr << "Invalid argument: " << e.what() << endl;
        return 1;
    } catch (const std::out_of_range& e) {
        cerr << "Out of range: " << e.what() << endl;
        return 1;
    }

    HiPMFParams pmf_params;

    try {
        pmf_params = calculateHiPMF(srcPoints, dstPoints);
    } catch (const std::runtime_error& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    for(int i = 0; i < 9; i++){
        cout << pmf_params.pmf_coef[i] << ", ";
    }
    cout<<endl;
    
    return 0;
}