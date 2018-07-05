#! /bin/sh

OUT_BIN="./a.out"
g++ -std=c++11  yuvplayer.cpp -o $OUT_BIN -I /usr/local/include -L /usr/local/lib \
-lSDL2main -lSDL2 \
&& $OUT_BIN

