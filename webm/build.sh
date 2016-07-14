gcc `pkg-config libavcodec libavformat libavutil --cflags` main.c rav_record.c -o demo1 `pkg-config libavcodec libavformat libavutil --libs`

gcc `pkg-config libavcodec libavformat libavutil libswscale libswresample --cflags` muxingav.c -o muxingav `pkg-config libavcodec libavformat libavutil libswscale libswresample --libs`



