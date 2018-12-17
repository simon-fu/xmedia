

#include "NRTPReceiver.hpp"




//class NRTPTime0{
//    struct Pair{
//        uint64_t local;
//        uint64_t remote;
//        Pair(uint64_t l, uint64_t r):local(l), remote(r){}
//        Pair():Pair(0, 0){}
//    };
//public:
//    NRTPLocalNTP local_;
//    uint64_t remoteNTP_ = 0;
//    void input(int64_t now_ms, uint32_t timestamp, uint64_t remote_ntp){
//        local_.input(now_ms, timestamp);
//        remoteNTP_ = remote_ntp;
//    }
//
//    uint64_t remote() const{
//        return remoteNTP_;
//    }
//
//    uint64_t local() const{
//        return local_.current();
//    }
//};
