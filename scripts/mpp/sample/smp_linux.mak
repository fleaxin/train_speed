
# target source
OBJS  := $(C_SRCS:%.c=%.o)
OBJS  += $(CPP_SRCS:%.cpp=%.o)
CFLAGS += $(COMM_INC)

MPI_LIBS += $(REL_LIB)/libss_hdmi.a
MPI_LIBS += $(LIBS_LD_CFLAGS)

.PHONY: all clean copy_other

all:$(TARGET) copy_other

$(TARGET): $(COMM_OBJ) $(OBJS)
	mkdir -p $(TARGET_PATH)
	$(CC) $(CFLAGS) -lpthread -lm -o $(TARGET_PATH)/$@ $^ -Wl,--start-group $(MPI_LIBS) $(SENSOR_LIBS) $(AUDIO_LIBA)  $(INIPARSER_LIB) $(THIRD_LIBS)  $(REL_LIB)/libsecurec.a -Wl,--end-group

# 编译 .c 文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -g

# 编译 .cpp 文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ -g

clean:
	rm -rf $(TARGET_PATH)
	rm -f $(OBJS)
	rm -f $(COMM_OBJ)

cleanstream:
	@rm -f *.h264
	@rm -f *.h265
	@rm -f *.jpg
	@rm -f *.mjp
	@rm -f *.mp4

copy_other:
	@if [ -d "$(SRCFILE_DIR)" ]; then \
		mkdir -p $(TARGET_PATH);\
		cp -r $(SRCFILE_DIR) $(TARGET_PATH)/; \
	fi