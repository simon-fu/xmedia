
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


COMMON_CPP_SRC += src/xwavfile.cpp 
COMMON_H += src/xwavfile.h 
COMMON_OBJ += $(foreach src, $(COMMON_CPP_SRC), $(addsuffix .o, $(basename $(src))))
ALL_CPP_SRC += $(COMMON_CPP_SRC)


HEAD_DIRS += ./src/
HEAD_DIRS += ../xctools/src/

# !!! there is google/protobuf/stubs/common.h -> /usr/local/include/
# !!! confict with chromium/src/third_party/protobuf/src/
# !!! comment out this include path
# HEAD_DIRS += /usr/local/include/

# webrtc env
# HEAD_DIRS += /Users/simon/Desktop/simon/projects/easemob/src/rtc/rtccore/mediacore/

HEAD_DIRS += /Users/simon/Desktop/simon/projects/easemob/src/webrtc/webrtc_ios/webrtc2016/src/out_mac/Release/gen/protoc_out/
HEAD_DIRS += /Users/simon/Desktop/simon/projects/easemob/src/webrtc/webrtc_ios/webrtc2016/src/chromium/src/third_party/protobuf/src/
HEAD_DIRS += /Users/simon/Desktop/simon/projects/easemob/src/webrtc/webrtc_ios/webrtc2016/src/chromium/src/testing/gtest/include/
HEAD_DIRS += /Users/simon/Desktop/simon/projects/easemob/src/webrtc/webrtc_ios/webrtc2016/src
LIB_PATHS += -L/Users/simon/Desktop/simon/projects/easemob/src/webrtc/webrtc_ios/webrtc2016/src/out_mac/Release
# MY_LD_FLAGS += -laudio_processing -lcommon_audio 
MY_LD_FLAGS +=  \
    -laudio_processing -laudioproc_debug_proto \
    -laudioproc_test_utils -laudioproc_protobuf_utils -lgtest \
    -lsystem_wrappers -ltest_support -lrtc_base_approved \
    -lwebrtc_common -lcommon_audio -lcommon_audio_sse2 -lisac \
    -laudio_decoder_interface -laudio_encoder_interface \
    -lisac_common -lprotobuf_lite -laudio_processing_sse2 \
    -lfield_trial_default -lmetrics_default -lgmock

	# -lisac_fix \
	# -lwebrtc_test_common \

MY_LD_FLAGS += -lm -framework AudioToolbox -framework CoreAudio -framework Foundation -framework AVFoundation \
	-framework Cocoa -framework IOKit
MY_FLAGS += -DEXPAT_RELATIVE_PATH -DWEBRTC_POSIX -DWEBRTC_MAC


TARGET_AEC_REPLAY=$(OUTPUT_DIR)/aec-replay
TARGETS += $(TARGET_AEC_REPLAY)
AEC_REPLAY_OBJ = src/main_aec.o 
ALL_CPP_OBJ += $(AEC_REPLAY_OBJ)

TARGET_MIXWAVE=$(OUTPUT_DIR)/mix-wav-file
TARGETS += $(TARGET_MIXWAVE)
MIXWAVE_OBJ = src/main_mix.o 
ALL_CPP_OBJ += $(MIXWAVE_OBJ)

AUDIO_PROC=audioproc
TARGET_AUDIOPROC=$(OUTPUT_DIR)/$(AUDIO_PROC)
TARGETS += $(TARGET_AUDIOPROC)
AUDIOPROC_OBJ = src/main_audio_proc.o
ALL_CPP_OBJ += $(AUDIOPROC_OBJ)




MY_FLAGS += -g
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


INCLUDE_DIRS += $(addprefix -I,$(HEAD_DIRS))

#CFLAGS = -fPIC -fno-strict-aliasing  -Wall -O2 $(INCLUDE_DIRS) 
CFLAGS= $(INCLUDE_DIRS) $(MACROS)  $(MY_FLAGS) 
#CFLAGS += -fstack-protector 
#CFLAGS += -std=c++11
#CFLAGS += -std=c++0x


CPPFLAGS=$(INCLUDE_DIRS) $(MACROS)  $(MY_FLAGS) -std=c++11



LIB_PATHS+= -L/usr/local/lib 
MY_LD_FLAGS += $(LIB_PATHS)
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


$(TARGET_AEC_REPLAY): $(AEC_REPLAY_OBJ) $(COMMON_OBJ) $(COMMON_H) $(OUTPUT_DIR) 
	$(CC)  $(AEC_REPLAY_OBJ) $(COMMON_OBJ) $(MY_LD_FLAGS) -o $@
	
$(TARGET_MIXWAVE): $(MIXWAVE_OBJ) $(COMMON_OBJ) $(COMMON_H) $(OUTPUT_DIR) 
	$(CC)  $(MIXWAVE_OBJ) $(COMMON_OBJ) $(MY_LD_FLAGS) -o $@

$(AUDIO_PROC): $(TARGET_AUDIOPROC)
	echo build $@ done

$(TARGET_AUDIOPROC): $(AUDIOPROC_OBJ) $(COMMON_OBJ) $(COMMON_H) $(OUTPUT_DIR) 
	$(CC)  $(AUDIOPROC_OBJ) $(COMMON_OBJ) $(MY_LD_FLAGS) -o $@


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
	@echo TARGETS=$(TARGETS)
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




