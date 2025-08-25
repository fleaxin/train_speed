# 指定交叉编译工具链前缀
CROSS_COMPILE = aarch64-mix210-linux-

# 指定编译器
CC = $(CROSS_COMPILE)g++

# 指定编译选项
CFLAGS = -Wall -O2 -std=c++17 -I./../../mpp/out/third/opencv/include

# 指定链接选项
LDFLAGS = -L./../../mpp/out/third/opencv/lib -lopencv_world

# 指定源文件
SRCS = get_pmf_param.cpp

# 指定目标文件
OBJS = $(SRCS:.cpp=.o)

# 指定可执行文件名
TARGET = get_pmf_param

# 默认目标
all: $(TARGET)

# 链接规则
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ 

# 编译规则
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# 清理规则
clean:
	rm -f $(OBJS) $(TARGET)