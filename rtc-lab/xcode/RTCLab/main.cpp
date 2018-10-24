

#include <iostream>
#include "Lab000-Demo.hpp"
#include "Lab010-VP8.hpp"
#include "Lab020-Beauty.hpp"
#include "Lab030-RTMPPush.hpp"
#include "Lab034-EVRTMPPush.hpp"
#include "Lab040-BStreamDump.hpp"
#include "Lab041-BStreamTLVPlayer.hpp"

int main(int argc, char * argv[]) {
    int ret = 0;
    //ret = lab_vp8_main(argc, argv);
    //ret = lab020_beauty_main(argc, argv);
    //ret = lab030_rtmp_push_main(argc, argv);
    ret = lab034_evrtmp_push_main(argc, argv);
//    ret = lab_bstream_dump_main(argc, argv);
//    ret = lab_bstream_tlv_player_main(argc, argv);
    //ret = lab_demo_main(argc, argv);
    return 0;
}
