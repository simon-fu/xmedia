
SRC_ROOT=$(shell pwd)
TARGET_PLATFORM=$(shell uname)

OUTPUT_DIR=$(SRC_ROOT)/bin






#functions
get_files =$(foreach dir,$(subst $(DIR_DIVIDER),$(MK_DIR_DIVIDER),$(1)),$(wildcard $(dir)$(MK_DIR_DIVIDER)$(2)))
#SRC_DIRS += ./src/
#C_SRC += $(call get_files,$(SRC_DIRS),*.c)
#CPP_SRC += $(call get_files,$(SRC_DIRS),*.cpp)
#ALL_C_SRC += $(C_SRC)
#ALL_CPP_SRC += $(CPP_SRC)


COMMON_CPP_SRC += src/xcutil.cpp src/xrtp_h264.cpp  src/xtrans_codec.cpp
COMMON_H += src/xcutil.h src/xrtp_defs.h src/xrtp_h264.h src/xtrans_codec.h
COMMON_OBJ += $(foreach src, $(COMMON_CPP_SRC), $(addsuffix .o, $(basename $(src))))
ALL_CPP_SRC += $(COMMON_CPP_SRC)


HEAD_DIRS += ./src/
HEAD_DIRS += /usr/local/include/
INCLUDE_DIRS += $(addprefix -I,$(HEAD_DIRS))



TARGET_UDP_DUMP=$(OUTPUT_DIR)/udp_dump
TARGETS += $(TARGET_UDP_DUMP)
UDP_DUMP_OBJ = src/main_udp_dump.o 
ALL_CPP_OBJ += $(UDP_DUMP_OBJ)

TARGET_UDP_REPLAY=$(OUTPUT_DIR)/udp_replay
TARGETS += $(TARGET_UDP_REPLAY)
UDP_REPLAY_OBJ = src/main_udp_replay.o 
ALL_CPP_OBJ += $(UDP_REPLAY_OBJ)


TARGET_CONVERT_DUMP_H264=$(OUTPUT_DIR)/convert_dump_to_h264
TARGETS += $(TARGET_CONVERT_DUMP_H264)
CONVERT_DUMP_H264_OBJ = src/main_convert_dump_to_h264.o 
ALL_CPP_OBJ += $(CONVERT_DUMP_H264_OBJ)

TARGET_RTP_RELAY=$(OUTPUT_DIR)/rtp_relay
TARGETS += $(TARGET_RTP_RELAY)
RTP_RELAY_OBJ = src/main_rtp_relay.o 
ALL_CPP_OBJ += $(RTP_RELAY_OBJ)

TARGET_RTP_REPLAY=$(OUTPUT_DIR)/rtp_replay
TARGETS += $(TARGET_RTP_REPLAY)
RTP_REPLAY_OBJ = src/main_rtp_replay.o 
ALL_CPP_OBJ += $(RTP_REPLAY_OBJ)




MY_FLAGS= -g
# for MacOS
MY_FLAGS += -fno-pie 


#CC=gcc
CC=g++

MV = mv
RM = rm -fr
STRIP = strip


ALL_C_OBJ += $(foreach src, $(ALL_C_SRC), $(addsuffix .o, $(basename $(src))))
ALL_CPP_OBJ += $(foreach cpp, $(ALL_CPP_SRC), $(addsuffix .o, $(basename $(cpp))))


# -fno-strict-aliasing remove the warning "dereferencing type-punned pointer will break strict-aliasing rules "
# 

ifeq '$(STRESS_TEST)' ''
STRESS_TEST=0
endif



ifneq '$(TARGET_PLATFORM)' 'Darwin'
#MY_FLAGS+= -std=c++11
LIB_PATHS += -L/usr/lib64 
else
#MY_FLAGS+= -std=c++0x
#MY_FLAGS+= -Wc++11-extensions
endif

#CFLAGS = -fPIC -fno-strict-aliasing  -Wall -O2 $(INCLUDE_DIRS) 
CFLAGS= $(INCLUDE_DIRS) $(MACROS)  $(MY_FLAGS) 
#CFLAGS += -fstack-protector 
#CFLAGS += -std=c++11
#CFLAGS += -std=c++0x


CPPFLAGS=$(INCLUDE_DIRS) $(MACROS)  $(MY_FLAGS) -std=c++11



LIB_PATHS+= -L/usr/local/lib 
MY_LD_FLAGS = $(LIB_PATHS)
# MY_LD_FLAGS += -levent
MY_LD_FLAGS += -lstdc++ 
MY_LD_FLAGS += -lm
# MY_LD_FLAGS += -lboost_system
# MY_LD_FLAGS += -lhiredis
MY_LD_FLAGS += -lpthread
MY_LD_FLAGS += -lcrypto
# MY_LD_FLAGS += -lopus
# MY_LD_FLAGS += -lspeex
MY_LD_FLAGS += -g -fno-pie


CFLAGS += $(shell "pkg-config libevent --cflags")
CPPFLAGS += $(shell pkg-config libevent --cflags)
MY_LD_FLAGS += $(shell pkg-config libevent --libs)

CFLAGS += $(shell "pkg-config speex --cflags")
CPPFLAGS += $(shell pkg-config speex --cflags)
MY_LD_FLAGS += $(shell pkg-config speex --libs)

CFLAGS += $(shell "pkg-config opus --cflags")
CPPFLAGS += $(shell pkg-config opus --cflags)
MY_LD_FLAGS += $(shell pkg-config opus --libs)



all: $(TARGETS)
	@echo build success


$(TARGET_UDP_DUMP): $(UDP_DUMP_OBJ) $(COMMON_OBJ) $(COMMON_H) $(OUTPUT_DIR) 
	$(CC)  $(UDP_DUMP_OBJ) $(COMMON_OBJ) $(MY_LD_FLAGS) -o $@
	

$(TARGET_UDP_REPLAY): $(UDP_REPLAY_OBJ) $(COMMON_OBJ) $(COMMON_H) $(OUTPUT_DIR) 
	$(CC)  $(UDP_REPLAY_OBJ) $(COMMON_OBJ) $(MY_LD_FLAGS) -o $@

$(TARGET_CONVERT_DUMP_H264): $(CONVERT_DUMP_H264_OBJ) $(COMMON_OBJ) $(COMMON_H) $(OUTPUT_DIR) 
	$(CC)  $(CONVERT_DUMP_H264_OBJ) $(COMMON_OBJ) $(MY_LD_FLAGS) -o $@

$(TARGET_RTP_RELAY): $(RTP_RELAY_OBJ) $(COMMON_OBJ) $(COMMON_H) $(OUTPUT_DIR) 
	$(CC)  $(RTP_RELAY_OBJ) $(COMMON_OBJ) $(MY_LD_FLAGS) -o $@

$(TARGET_RTP_REPLAY): $(RTP_REPLAY_OBJ) $(COMMON_OBJ) $(COMMON_H) $(OUTPUT_DIR) 
	$(CC)  $(RTP_REPLAY_OBJ) $(COMMON_OBJ) $(MY_LD_FLAGS) -o $@

$(OUTPUT_DIR):
	mkdir -p $@



bd:$(C_OBJ)
	echo $<

$(ALL_C_OBJ):%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(ALL_CPP_OBJ):%.o:%.cpp
	$(CC) -c $(CPPFLAGS) $< -o $@

	
.PHONY: clean print_info
clean:
	$(RM) $(TARGETS)
	$(RM) $(ALL_C_OBJ)
	$(RM) $(ALL_CPP_OBJ)
#	$(RM) ./bin/mserver.dSYM

print:
	@echo TARGET_PLATFORM=$(TARGET_PLATFORM)
	@echo ----------------------
	@echo CC=$(CC)
	@echo ----------------------
	@echo CFLAGS=$(CFLAGS)
	@echo ----------------------
	@echo CPPFLAGS=$(CPPFLAGS)
	@echo ----------------------
	@echo C_SRC=$(C_SRC)
	@echo ----------------------
	@echo CPP_SRC=$(CPP_SRC)
	@echo ----------------------
	@echo C_OBJ=$(C_OBJ)
	@echo ----------------------
	@echo CPP_OBJ=$(CPP_OBJ)
	@echo ----------------------
	@echo ALL_C_SRC=[$(ALL_C_SRC)]
	@echo ----------------------
	@echo ALL_CPP_SRC=[$(ALL_CPP_SRC)]




