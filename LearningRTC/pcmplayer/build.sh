#! /bin/sh

g++ -std=c++11  pcmplayer.cpp -o pcmplayer -I /usr/local/include -L /usr/local/lib \
-lSDL2main -lSDL2 \


