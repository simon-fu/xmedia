
#ifndef NRTPPacketWindow_hpp
#define NRTPPacketWindow_hpp

#include <stdio.h>
//#include <list> // std::list
//#include <sstream> // std::ostringstream
#include "NLogger.hpp"
#include "NCircularBuffer.hpp"
#include "NRTPMap.hpp"


class NRTPPacketSlot{
public:
    static const size_t SIZE = 1700;
    static const size_t PREFIX = 200;
    uint8_t     buffer[SIZE+PREFIX];
    NRTPData    rtpd;
    //NRTPMediaBrief brief;
public:
    
    size_t reset(const NRTPData& d){
        size_t sz = d.serialize(buffer+PREFIX, SIZE, this->rtpd);
        if(!sz){
            return 0;
        }
        return sz;
    }
    
    bool valid(uint16_t seq){
        return (seq == rtpd.header.sequenceNumber && rtpd.payloadPtr);
    }
    NCircularBufferSizeT index;
};


class NRTPPacketWindow : public NCircularVector<NRTPPacketSlot>{
public:
    typedef NCircularVector<NRTPPacketSlot> Parent;
    enum State {
        kReachEmptySlot = -5,
        kOutOfRange = -4,
        kUnknownData = -3,
        kDuplicatePacket = -2,
        kOutOfDatePacket = -1,
        kNoError = 0,
        kFirstPacket ,
        kPrepend,
        kAppend,
        kOutOfOrderReset,
        kDisorderPacket,
    };
    
public:
    NRTPPacketWindow(NCircularBufferSizeT cap = 128)
    : NCircularVector<NRTPPacketSlot>(cap) //packets_(cap)
    , startSeq_(0u)
    , nextSeq_(startSeq_)
    , lastSlot_(&back()){
        auto& packets_ = *this;
        for(NCircularBufferSizeT i = 0; i < packets_.capacity(); ++i){
            packets_[i].index = i;
        }
    }
    
    NRTPPacketSlot * getSlotInWindow(const NRTPSeq& seq) {
        auto& packets_ = *this;
        auto dist = startSeq_.distance(seq);
        if(dist >= 0 && dist < packets_.size()){
            return &packets_[dist];
        }else{
            return nullptr;
        }
    }
    
    State checkPlaceSlot(const NRTPData& rtpd, NRTPPacketSlot& slot, State state_normal){
        if(rtpd.header.sequenceNumber == slot.rtpd.header.sequenceNumber
           && slot.rtpd.payloadPtr){
            return kDuplicatePacket;
        }
        emplaceSlot(rtpd, slot);
        return state_normal;
    }
    
    void emplaceSlot(const NRTPData& rtpd, NRTPPacketSlot& slot){
        slot.reset(rtpd);
        lastSlot_ = &slot;
    }
    
    void reset(const NRTPData& rtpd){
        auto& packets_ = *this;
        packets_.clear();
        packets_.shiftBack();
        emplaceSlot(rtpd, packets_.back());
        startSeq_ = rtpd.header.sequenceNumber;
        nextSeq_ = startSeq_;
    }
    
    NRTPPacketSlot& lastSlot() const{
        return *lastSlot_;
    }
    
    const NRTPSeq& startSeq() const{
        return startSeq_;
    }
    
    bool distance(const NRTPSeq& seq, RTPSeqDistance& dist) const{
        auto& packets_ = *this;
        dist = startSeq_.distance(seq);
        return(dist >= 0 && dist < packets_.size());
    }
    
    RTPSeqDistance distance(const NRTPSeq& seq) const{
        return startSeq_.distance(seq);
    }
    
    State insertPacket(const NRTPData& rtpd);
    
    int traverseUntilEmpty (const NRTPSeq& from_seq
                            , const TraverseFuncT& func)  ;
    
    NRTPPacketSlot* nextSlotInOrder();
    
private:
    NRTPSeq startSeq_;
    NRTPSeq nextSeq_;  // next in order
    NRTPPacketSlot* lastSlot_;
};


class NULPFecData{
private:
    struct FECHeader{
        uint8_t reoveryH8[8];
        uint16_t recoveryLength = 0;
    };
    
    struct Level{
        const uint8_t* data;
        uint16_t length;
        uint64_t maskCover = 0;
        uint64_t maskRecv = 0;
        
        void clear(){
            data = nullptr;
            length = 0;
            maskCover = 0;
            maskRecv = 0;
        }
        
        void setMask(const uint8_t * m, uint16_t len){
            maskCover = 0;
            for(uint16_t i = 0; i < len; ++i){
                uint16_t shift = (7-i) << 3;
                uint64_t b = m[i];
                maskCover |= b << shift;
            }
            maskRecv = maskCover;
        }
        
        bool checkSet(uint16_t offset){
            uint64_t m = (uint64_t)1 << (63-offset);
            if((maskCover&m) && (maskRecv&m)){
                maskRecv &= ~m;
                return true;
            }
            return false;
        }
        
        bool checkClear(uint16_t offset){
            uint64_t m = (uint64_t)1 << (63-offset);
            if((maskCover&m) && !(maskRecv&m)){
                maskRecv |= m;
                return true;
            }
            return false;
        }
        
        bool isCover(uint16_t offset)const{
            uint64_t m = (uint64_t)1 << (63-offset);
            return (maskCover&m) != 0;
        }
        
        bool isAllRecv()const{
            return maskRecv == 0;
        }
        
        bool isMissOne()const{
            return (maskRecv & (maskRecv-1)) == 0;
        }
        
        uint16_t firstMissOffset() const{
            return NUtil::count_leading_0bits(maskRecv);
        }
        
        uint16_t firstCoverOffset() const{
            return NUtil::count_leading_0bits(maskCover);
        }
    };
    
public:
    
    uint16_t maxProtectLength = 0;
    uint16_t maskLength = 0;
    NRTPSeq snBase = 0u;
    FECHeader header;
    //std::vector<Level> levels;
    Level level0; // only support level 0 for now
    
    bool valid(){
        return maskLength > 0;
    }
    
    void clear(){
        maxProtectLength = 0;
        maskLength = 0;
        level0.clear();
    }
    
    bool isAllReceived() const{
        return level0.isAllRecv();
    }
    
    NRTPSeq missSeq(const Level& level){
        RTPSeqDistance dist = level.firstMissOffset();
        return snBase + dist;
    }
    
    size_t parse(const uint8_t* data, size_t size) ;
    
    bool checkSet(uint16_t seq);
    
    void recoveryBegin(const Level& level, NRTPPacketSlot& outpkt);
    
    bool recoveryAdd(const Level& level, const NRTPPacketSlot& inpkt, NRTPPacketSlot& outpkt);
    
    void recoveryEnd(const Level& level, uint16_t seq, NRTPPacketSlot& outpkt);
};

class NRTPUlpfecWindow : public NRTPPacketWindow{
public:
    NRTPUlpfecWindow(NCircularBufferSizeT cap = 128)
    : NRTPPacketWindow(cap), feclist_((cap+7)/8), packetWin_(*this){
        
    }
    
    void reset(){
        feclist_.clear();
    }
    
    NRTPPacketWindow::State insertPacket(const NRTPData& rtpd_in, const NRTPCodec& codec, const NRTPCodec::Map& codecs);
    
    NRTPPacketWindow::State insertPacket(const NRTPData& rtpd_in, const NRTPCodec& codec);
    
    bool recoverULPFEC(NULPFecData& fecd);
    
    bool recoverULPFEC(NULPFecData& fecd
                       , RTPSeqDistance offset
                       , const NRTPSeq& miss_seq
                       , NRTPPacketSlot& miss_slot) ;
    
    bool verifyFEC(NULPFecData& fecd_in) ;
    
    bool addFEC(const NRTPData& rtpd);
    
    void processFEC();
    
private:
    NCircularVector<NULPFecData> feclist_;
    NRTPPacketWindow& packetWin_;
};


#endif /* NRTPPacketWindow_hpp */
