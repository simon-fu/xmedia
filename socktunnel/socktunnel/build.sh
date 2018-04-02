
mkdir -p bin/
g++ -Wno-c++11-extensions \
	-std=c++11 \
	-I/usr/local/Cellar/libevent/2.0.22/include \
	-L/usr/local/Cellar/libevent/2.0.22/lib \
	-levent \
	socktunnel/XSocket.cpp \
	socktunnel/main.cpp \
	-o bin/socktunnel

