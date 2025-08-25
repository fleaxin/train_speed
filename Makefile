
JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
MAKE_CMD = $(MAKE) -j$(JOBS)

# 获取项目根目录绝对路径
PROJECT_ROOT := $(shell pwd)

# 第三方库安装路径
OPENSSL_PREFIX := $(PROJECT_ROOT)/build/openssl
OPENCV_PREFIX := $(PROJECT_ROOT)/build/opencv
ZLMEDIAKIT_PREFIX := $(PROJECT_ROOT)/build/zlmediakit
ZLOG_PREFIX := $(PROJECT_ROOT)/build/zlog

# 编译参数
CFLAGS := -I$(OPENSSL_PREFIX)/include -I$(OPENCV_PREFIX)/include
LDFLAGS := -L$(OPENSSL_PREFIX)/lib -L$(OPENCV_PREFIX)/lib

all: build_project


# 编译第三方库
build_third:build_openssl build_zlmediakit build_opencv build_zlog
	mkdir -p $(PROJECT_ROOT)/mpp/out/third/opencv/include
	mkdir -p $(PROJECT_ROOT)/mpp/out/third/opencv/lib
	mkdir -p $(PROJECT_ROOT)/mpp/out/third/ZLMediaKit/include
	mkdir -p $(PROJECT_ROOT)/mpp/out/third/ZLMediaKit/lib
	mkdir -p $(PROJECT_ROOT)/mpp/out/third/zlog/include
	mkdir -p $(PROJECT_ROOT)/mpp/out/third/zlog/lib
	cp -r $(OPENCV_PREFIX)/include $(PROJECT_ROOT)/mpp/out/third/opencv/
	cp -r $(OPENCV_PREFIX)/lib $(PROJECT_ROOT)/mpp/out/third/opencv/
	cp -r $(ZLMEDIAKIT_PREFIX)/include $(PROJECT_ROOT)/mpp/out/third/ZLMediaKit/
	cp -r $(ZLMEDIAKIT_PREFIX)/lib $(PROJECT_ROOT)/mpp/out/third/ZLMediaKit/
	cp -r $(ZLOG_PREFIX)/include $(PROJECT_ROOT)/mpp/out/third/zlog/
	cp -r $(ZLOG_PREFIX)/lib $(PROJECT_ROOT)/mpp/out/third/zlog/
	
# 编译 OpenSSL
# 定义 OpenSSL 安装完成的标记文件
OPENSSL_INSTALLED = $(OPENSSL_PREFIX)/lib/libssl.so

build_openssl: $(OPENSSL_INSTALLED)

$(OPENSSL_INSTALLED):
	@echo "Building OpenSSL..."
	cd third_party/openssl-1.1.1w && \
	./Configure linux-aarch64 \
		no-asm \
		shared \
		no-async \
		--prefix=$(OPENSSL_PREFIX) \
		--cross-compile-prefix=aarch64-mix210-linux- \
		-fPIC
	$(MAKE) -C third_party/openssl-1.1.1w -j$(JOBS)
	$(MAKE) -C third_party/openssl-1.1.1w install

# 构建 ZLMediaKit
# 定义 ZLMediaKit 安装完成的标记文件
ZLMEDIAKIT_INSTALLED = $(ZLMEDIAKIT_PREFIX)/lib/libmk_api.so

build_zlmediakit: $(ZLMEDIAKIT_INSTALLED)

$(ZLMEDIAKIT_INSTALLED):build_openssl
	@echo "Building ZLMediaKit..."
	mkdir -p third_party/ZLMediaKit/build && \
	cd third_party/ZLMediaKit/build && \
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(PROJECT_ROOT)/cmake/aarch64-toolchain.cmake \
		-DCMAKE_INSTALL_PREFIX=$(ZLMEDIAKIT_PREFIX) \
		-DENABLE_WEBRTC=on \
		-DENABLE_OPENSSL=ON \
		-DENABLE_TESTS=OFF \
		-DOPENSSL_ROOT_DIR=$(OPENSSL_PREFIX) \
		..
	$(MAKE) -C third_party/ZLMediaKit/build -j$(JOBS)
	$(MAKE) -C third_party/ZLMediaKit/build install
	
# 编译 OpenCV
# 定义 ZLMediaKit 安装完成的标记文件
OPENCV_INSTALLED = $(OPENCV_PREFIX)/lib/libopencv_world.so

build_opencv: $(OPENCV_INSTALLED)

$(OPENCV_INSTALLED):
	@echo "Building OpenCV..."
	mkdir -p third_party/opencv-3.4.3/build && \
	cd third_party/opencv-3.4.3/build && \
	cmake \
		-G "Unix Makefiles" \
		-C $(PROJECT_ROOT)/cmake/opencv-cross.cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(PROJECT_ROOT)/cmake/aarch64-toolchain.cmake \
		-DBULID_ZLIB=ON \
		-DBUILD_opencv_world=ON \
		../ \
		..
	$(MAKE) -C third_party/opencv-3.4.3/build -j$(JOBS)
	$(MAKE) -C third_party/opencv-3.4.3/build install

# 编译 zlog
# 定义 zlog 安装完成的标记文件
ZLOG_INSTALLED = $(ZLOG_PREFIX)/lib/libzlog.so

build_zlog: $(ZLOG_INSTALLED)

$(ZLOG_INSTALLED):
	@echo "Building zlog..."
	$(MAKE) \
		-C third_party/zlog/src \
		CC=aarch64-mix210-linux-gcc \
		-j$(JOBS)
	$(MAKE) \
	-C third_party/zlog/src \
	PREFIX=$(ZLOG_PREFIX) \
	install

# 编译主项目
build_project:
	@echo "Building main project..."
	$(MAKE) -C src 

clean:
	$(MAKE) -C src clean
	
# 清理第三方库的编译文件和安装目录
clean_third:clean_openssl clean_zlmediakit clean_opencv clean_zlog
	@echo "Cleaning thirdparty libraries..."

clean_openssl:
	# Clean OpenSSL
	-$(MAKE) -C third_party/openssl-1.1.1w clean
	rm -rf $(OPENSSL_PREFIX)
	
clean_zlmediakit:
	# Clean ZLMediaKit
	-$(MAKE) -C third_party/ZLMediaKit/build clean
	rm -rf third_party/ZLMediaKit/build
	rm -rf third_party/ZLMediaKit/release
	rm -rf $(ZLMEDIAKIT_PREFIX)

clean_opencv:
	# Clean OpenCV
	rm -rf third_party/opencv-3.4.3/build
	rm -rf $(OPENCV_PREFIX)

clean_zlog:
	# Clean zlog
	-$(MAKE) -C third_party/zlog/src clean
	rm -rf $(ZLOG_PREFIX)
	
.PHONY: all clean build_openssl build_opencv build_zlmediakit build_project
