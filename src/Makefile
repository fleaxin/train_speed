export TOP_DIR=$(CURDIR)/../
export SCRIPTS_DIR=$(TOP_DIR)/scripts
export OUTPUT_DIR=$(TOP_DIR)/output
export CROSS_COMPILE ?= aarch64-mix210-linux-
export CC=$(CROSS_COMPILE)gcc
include $(SCRIPTS_DIR)/mpp/sample/Makefile.param

C_SRCS := $(wildcard *.c)
CPP_SRCS += $(wildcard *.cpp)

TARGET := camera_test

TARGET_PATH := $(OUTPUT_DIR)/

CFLAGS += -I../include/ -L$(REL_LIB) -lsecurec -I $(REL_INC)/svp_npu -I $(REL_INC)/npu
CXXFLAGS += $(CFLAGS)

# 添加svp相关链接
MPI_LIBS += $(REL_LIB)/libss_ive.a
MPI_LIBS += $(REL_LIB)/libss_md.a
MPI_LIBS += $(REL_LIB)/libss_mau.a
MPI_LIBS += $(REL_LIB)/libss_dpu_rect.a
MPI_LIBS += $(REL_LIB)/libss_dpu_match.a
MPI_LIBS += $(REL_LIB)/libss_dsp.a
MPI_LIBS += $(REL_LIB)/svp_npu/libsvp_acl.a
MPI_LIBS += -L$(REL_LIB)/svp_npu -lprotobuf-c
CFLAGS += -L$(REL_LIB)/npu/stub -lascendcl

SRCFILE_DIR=$(PWD)/source_file

# compile linux or LITEOS
include $(SCRIPTS_DIR)/mpp/sample/$(ARM_ARCH)_$(OSTYPE).mak
