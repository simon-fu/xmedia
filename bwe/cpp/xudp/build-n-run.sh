g++ -Wno-c++11-extensions main.cpp xcmdline.cpp  \
plw_sock_only/plw_drv_linux.c \
plw_sock_only/plw_sock.c \
plw_sock_only/plw_sock_linux.c \
plw_sock_only/plw_drv_sock_ansi.c \
-I./plw_sock_only/ && ./a.out $@
