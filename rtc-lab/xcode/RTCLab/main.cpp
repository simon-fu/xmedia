

#include <iostream>
#include "Lab010-VP8.hpp"
#include "Lab020-Beauty.hpp"
#include "Lab030-RTMPPush.hpp"

int main(int argc, char * argv[]) {
    int ret = 0;
    //ret = lab_vp8_main(argc, argv);
    //ret = lab020_beauty_main(argc, argv);
    ret = lab030_rtmp_push_main(argc, argv);
    return 0;
}
