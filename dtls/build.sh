

gcc -g -o dtls_srtp dtls_srtp.cpp -lstdc++ `pkg-config --cflags --libs glib-2.0 gio-2.0 nice libsrtp2 libssl log4cplus` -I/usr/local/opt/openssl/include  -L/usr/local/opt/openssl/lib

