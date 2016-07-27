g++ `pkg-config libevent --cflags` main_udp_dump.cpp util.cpp -o udp_dump `pkg-config libevent --libs`

g++ main_udp_replay.cpp util.cpp -o udp_replay 

g++ main_convert_dump_to_h264.cpp util.cpp xrtp_h264.cpp xutil.cpp -o convert_dump_to_h264

g++ `pkg-config libevent --cflags` `pkg-config libevent --libs` main_rtp_relay.cpp util.cpp xrtp_h264.cpp xutil.cpp -o rtp_relay  




