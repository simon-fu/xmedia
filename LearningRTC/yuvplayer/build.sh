#! /bin/sh

g++ -std=c++11  yuvplayer.cpp -g -o yuvplayer -I /usr/local/include -L /usr/local/lib \
-lSDL2main -lSDL2 \


# -lavformat -lavcodec -lavutil -lswscale
