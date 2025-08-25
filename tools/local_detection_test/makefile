# 指定交叉编译工具链前缀
CROSS_COMPILE = aarch64-mix210-linux-

# 指定编译器
CC = $(CROSS_COMPILE)g++

# 获取主目录的绝对地址
ABS_DIR = $(CURDIR)/../..

# 指定编译选项
CFLAGS = -Wall -O0  -I$(ABS_DIR)/mpp/out/third/opencv/include
CFLAGS += -I$(ABS_DIR)/include -I$(ABS_DIR)/mpp/out/include -I$(ABS_DIR)/mpp/out/include/svp_npu
CFLAGS += -I./

# 指定链接选项
LDFLAGS = -L$(ABS_DIR)/mpp/out/third/opencv/lib -lopencv_world -lpthread

# 指定源文件
SRCS = train_speed_from_local_date.cpp 
SRCS += $(ABS_DIR)/src/train_speed.cpp 
# SRCS += train_speed_origin.cpp

# 指定目标文件
OBJS = $(SRCS:.cpp=.o) 

# 指定可执行文件名
TARGET = train_speed_from_local_date

# 默认目标
all: $(TARGET)

# 链接规则
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ 

# 编译规则
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -g -o $@

# 清理规则
clean:
	rm -f $(OBJS) $(TARGET)