#!/bin/bash

#输入图像中校正后各顶点在原图中的坐标
#  左上 右上 左下 右下
src_x1=206
src_y1=165
src_x2=2389
src_y2=118
src_x3=283
src_y3=1144
src_x4=2389
src_y4=1118

#输出图像坐标
dst_x1=206
dst_y1=118
dst_x2=2389
dst_y2=118
dst_x3=206
dst_y3=1118
dst_x4=2389
dst_y4=1118

# 运行程序并传递参数
./get_pmf_param $src_x1 $src_y1 $src_x2 $src_y2 $src_x3 $src_y3 $src_x4 $src_y4 $dst_x1 $dst_y1 $dst_x2 $dst_y2 $dst_x3 $dst_y3 $dst_x4 $dst_y4
