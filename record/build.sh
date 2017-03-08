g++ `pkg-config libavcodec libavformat libavutil --cflags` main.cc rav_record.cc -o demo1 `pkg-config libavcodec libavformat libavutil --libs`

gcc `pkg-config libavcodec libavformat libavutil libswscale libswresample --cflags` muxingav.c -o muxingav `pkg-config libavcodec libavformat libavutil libswscale libswresample --libs`



