g++ `pkg-config libavcodec libavformat libavutil --cflags` main.cc rav_record.cc -o demo1 `pkg-config libavcodec libavformat libavutil --libs`

gcc `pkg-config libavcodec libavformat libavutil libswscale libswresample --cflags` muxingav.c -o muxingav `pkg-config libavcodec libavformat libavutil libswscale libswresample --libs`

# g++ `pkg-config libavcodec libavformat libavutil --cflags` \
# 	main.cc \
# 	rav_record.cc \
# 	medooze-media-server/mod/RTPPacket.cpp \
# 	medooze-media-server/mod/RTPHeader.cpp \
# 	medooze-media-server/mod/RTPHeaderExtension.cpp \
# 	medooze-media-server/mod/vp8depacketizer.cpp \
# 	-I./medooze-media-server/mod/ \
# 	-o demo1 \
# 	`pkg-config libavcodec libavformat libavutil --libs`

