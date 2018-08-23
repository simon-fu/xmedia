#!/usr/bin/env bash
g++ --std=c++11  main.cpp `pkg-config --libs --cflags opus`  -o rtp_receiver
./rtp_receiver &

PID=$! 
echo PID=$PID
tcpreplay -i lo0 -L 3000 ./asr.json.pcapng 
kill -INT $PID
