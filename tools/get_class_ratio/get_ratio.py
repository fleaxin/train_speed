import pandas as pd
import numpy as np
import os
from collections import defaultdict

# 配置参数
INPUT_CSV = "all_class_move_distance.csv"  # 替换为实际路径
OUTPUT_DIR = "class_ratio_analysis"
CLASS_NAMES = ["mutex", "button", "handle", "edge", "latch", "margin", "head", "lock"]
NUM_CLASSES = len(CLASS_NAMES)

def load_data():
    """加载并验证数据"""
    df = pd.read_csv(INPUT_CSV)
    
    # 验证必要字段
    required_columns = ["time_ref", "carriage_number", "class_id", "class_name", "move_distance"]
    if not all(col in df.columns for col in required_columns):
        raise ValueError("CSV文件格式不符合预期，缺少必要字段")
        
    # 转换数据类型
    df["class_id"] = df["class_id"].astype(int)
    df["move_distance"] = df["move_distance"].astype(float)
    
    return df

def analyze_class_ratios(df):
    """分析不同标志物移动比率"""
    # 创建输出目录
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    # 新增：初始化汇总表格
    all_carriage_ratios = {}  # 格式: {carriage: {class_name: ratio}}
    
    # 按车厢分组
    carriages = df["carriage_number"].unique()
    
    for carriage in carriages:
        carriage_df = df[df["carriage_number"] == carriage]
        print(f"\n分析车厢 {carriage} 的标志物比率...")
        
        # 初始化8x8的累加表格
        ratio_table = np.zeros((NUM_CLASSES, NUM_CLASSES, 2))  # 2: first, second
        
        # 按时间帧分组
        time_groups = carriage_df.groupby("time_ref")
        
        for time_ref, frame in time_groups:
            # 存储当前帧每个类的平均移动距离
            class_averages = {}
            
            # 按类计算平均值
            for class_id in range(NUM_CLASSES):
                class_data = frame[frame["class_id"] == class_id]["move_distance"]
                if not class_data.empty:
                    class_averages[class_id] = class_data.mean()
            
            # 更新比率表格
            for i in range(NUM_CLASSES):
                if i not in class_averages:
                    continue
                    
                for j in range(NUM_CLASSES):
                    if j not in class_averages:
                        continue
                        
                    # 累加到对应位置
                    if class_averages[i] > 8: 
                        ratio_table[i][j][0] += class_averages[i]
                    if class_averages[j] > 8: 
                        ratio_table[i][j][1] += class_averages[j]
        
        # 新增：保存当前车厢的比率数据到汇总表
        carriage_ratios = {}
        for j in range(NUM_CLASSES):
            first, second = ratio_table[0][j]
            if second != 0:
                carriage_ratios[CLASS_NAMES[j]] = first / second
            else:
                # 尝试通过中间节点计算
                for k in range(1, NUM_CLASSES):
                    first_k, second_k = ratio_table[0][k]
                    first_j, second_j = ratio_table[k][j]
                    if second_k != 0 and second_j != 0:
                        carriage_ratios[CLASS_NAMES[j]] = (first_k/second_k) * (first_j/second_j)
                        break
        
        all_carriage_ratios[f"carriage_{carriage}"] = carriage_ratios
        
        # 保存结果（保持原有代码不变）
        save_results(carriage, ratio_table)
    
    # 新增：保存汇总表格
    save_all_carriages_ratios(all_carriage_ratios)

def save_all_carriages_ratios(all_carriage_ratios):
    """保存所有车厢的mutex基准比率到一个文件中"""
    # 创建汇总数据
    summary_data = {cls: {} for cls in CLASS_NAMES}
    
    for carriage, ratios in all_carriage_ratios.items():
        for cls, ratio in ratios.items():
            summary_data[cls][carriage] = ratio
    
    # 转换为DataFrame
    summary_df = pd.DataFrame.from_dict(summary_data, orient='index')
    
    # 重排序（可选）
    carriage_cols = sorted(summary_df.columns, key=lambda x: int(x.split('_')[1]))
    summary_df = summary_df[carriage_cols]
    
    # 保存CSV
    summary_df.to_csv(os.path.join(OUTPUT_DIR, "all_carriages_mutex_ratios.csv"))
    
    print(f"所有车厢的mutex基准比率已汇总保存到 {os.path.join(OUTPUT_DIR, 'all_carriages_mutex_ratios.csv')}")

def save_results(carriage, ratio_table):
    """保存分析结果"""
    # 创建输出目录
    carriage_dir = os.path.join(OUTPUT_DIR, f"carriage_{carriage}")
    os.makedirs(carriage_dir, exist_ok=True)
    
    # 保存原始数据表格
    raw_data = []
    for i in range(NUM_CLASSES):
        row = []
        for j in range(NUM_CLASSES):
            first, second = ratio_table[i][j]
            # print(f"{CLASS_NAMES[i]} -> {CLASS_NAMES[j]}: {first:.4f}/{second:.4f}")
            row.append(f"{first:.4f}/{second:.4f}" if second != 0 else "N/A")
        raw_data.append(row)
    
    raw_df = pd.DataFrame(raw_data, index=CLASS_NAMES, columns=CLASS_NAMES)
    raw_df.to_csv(os.path.join(carriage_dir, "raw_ratio_table.csv"))
    
    # 生成基于mutex的比率表格
    mutex_ratios = np.zeros(NUM_CLASSES)
    path_info = [""] * NUM_CLASSES
    
    for j in range(NUM_CLASSES):
        # 1. 检查直接比率
        first, second = ratio_table[0][j]
        if second != 0:
            mutex_ratios[j] = first / second
            path_info[j] = f"{CLASS_NAMES[0]} -> {CLASS_NAMES[j]}"
            continue
            
        # 2. 寻找中间节点（按CLASS_NAMES顺序）
        found = False
        for k in range(1, NUM_CLASSES):  # 从第二个标志物开始
            # 检查是否存在路径 mutex -> k -> j
            first_k, second_k = ratio_table[0][k]
            first_j, second_j = ratio_table[k][j]
            
            if second_k != 0 and second_j != 0:
                ratio_k = first_k / second_k
                ratio_j = first_j / second_j
                mutex_ratios[j] = ratio_k * ratio_j
                path_info[j] = f"{CLASS_NAMES[0]} -> {CLASS_NAMES[k]} -> {CLASS_NAMES[j]}"
                found = True
                break
                
        if not found:
            mutex_ratios[j] = np.nan
            path_info[j] = "No valid path found"

    # 3. 保存基于mutex的比率
    mutex_df = pd.DataFrame({
        "Target_Class": CLASS_NAMES,
        "Mutex_Ratio": mutex_ratios,
        "Path_Info": path_info
    })
    
    mutex_df.to_csv(os.path.join(carriage_dir, "mutex_based_ratios.csv"), index=False)
    
    print(f"车厢 {carriage} 的分析结果已保存到 {carriage_dir}")

def main():
    # 加载数据
    df = load_data()
    
    # 分析数据
    analyze_class_ratios(df)

if __name__ == "__main__":
    main()