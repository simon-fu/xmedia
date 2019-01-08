

#include "NRTPPacketWindow.hpp"

NRTPPacketWindow::State NRTPPacketWindow::insertPacket(const NRTPData& rtpd){
    
    auto& packets_ = *this;
    auto& circular = packets_;
    
    if(packets_.isEmpty()){
        reset(rtpd);
        return kFirstPacket;
    }
    
    
    auto dist = startSeq_.distance(rtpd.header.sequenceNumber);
    RTPSeqDistance high = circular.capacity() *2;
    RTPSeqDistance low = -high;
    if(dist < low || dist > high){
        reset(rtpd);
        return kOutOfOrderReset;
    }
    
    if(dist < 0){
        auto abs_dist = 0-dist;
        
        if(circular.isFull()){
            // running phase
            return kOutOfDatePacket;
        }
        
        // start phase: out of date packet
        if((abs_dist + circular.size()) < circular.capacity()){
            return kOutOfDatePacket;
        }
        
        // start phase: prepend for older seq;
        circular.unshiftFront(abs_dist);
        startSeq_ = rtpd.header.sequenceNumber;
        return checkPlaceSlot(rtpd, circular.front(), kPrepend);
    }
    
    auto abs_dist = dist;
    if(abs_dist < circular.size()){
        // in window
        return checkPlaceSlot(rtpd, circular[abs_dist], kDisorderPacket);
        
    }else if(abs_dist == circular.size()){
        // next expect seq
        auto over_num = circular.shiftBack();
        startSeq_ = startSeq_ + over_num;
        return checkPlaceSlot(rtpd, circular.back(), kNoError);
        
    }else if(abs_dist < 2*circular.size()){
        // newer packet
        auto num = circular.size() - abs_dist + 1;
        auto over_num = circular.shiftBack(num);
        startSeq_ = startSeq_ + over_num;
        return checkPlaceSlot(rtpd, circular.back(), kDiscontinuity);
        
    }else {
        // too newer packet, never reach here
        reset(rtpd);
        return kOutOfOrderReset;
    }
}

int NRTPPacketWindow::traverseUntilEmpty (const NRTPSeq& from_seq
                        , const TraverseFuncT& func) {
    RTPSeqDistance dist = startSeq_.distance(from_seq);
    if(dist < 0){
        return kOutOfRange;
    }
    
    return Parent::traverse((NCircularBufferSizeT)dist,
                            [&from_seq, &func](NCircularBufferSizeT n , reference slot) -> int{
                                NRTPSeq expect_seq = from_seq + n;
                                if(!slot.valid(expect_seq.value())){
                                    return kReachEmptySlot;
                                }
                                func(n, slot);
                                return kNoError;
                            });
}

NRTPPacketWindow::State NRTPPacketWindow::nextSlotInOrder(NRTPPacketSlot* &slot, bool skip_empty){
    State state = kNoError;
    while(true){
        auto dist = startSeq_.distance(nextSeq_);
        if(dist >= this->size()){
            // no expect packet
            return kOutOfRange;
        }
        
        if(dist < 0){
            // out of date, reset to start seq
            nextSeq_ = startSeq_;
            dist = 0;
            state = kDiscontinuity;
        }
        
        slot = &(*this)[dist];
        if(!slot->valid(nextSeq_.value())){
            if(!skip_empty){
                return kReachEmptySlot;
            }
            state = kDiscontinuity;
            nextSeq_ = nextSeq_ + 1;
        }else{
            nextSeq_ = nextSeq_ + 1;
            return state;
        }
        state = kReachEmptySlot;
    }
    return state;
}


size_t NULPFecData::parse(const uint8_t* data, size_t size) {
    clear();
    if(size < 10){
        return 0;
    }
    memcpy(header.reoveryH8, data, 8);
    snBase = NUtil::get2(data, 2);
    header.recoveryLength = NUtil::get2(data, 8);
    maskLength = (data[0] & 0x40) == 0 ? 2 : 6;
    
    size_t pos = 10;
    while(pos < size){
        auto remains = size - pos;
        if (remains < (maskLength+2)) {
            return 0;
        }
        
        Level& level = level0;
        
        level.length = NUtil::get2(data, pos);
        pos += 2;
        
        level.setMask(data + pos, maskLength);
        pos += maskLength;
        
        level.data = data + pos;
        pos += level.length;
        
        if(pos > size){
            return 0;
        }
        
        if(level.length > maxProtectLength){
            maxProtectLength = level.length;
        }
        break;
    }
    
    return pos;
}

bool NULPFecData::checkSet(uint16_t seq){
    auto dist = snBase.distance(seq);
    if(dist < 0 || dist >= (maskLength<<3)){
        return false;
    }
    
    if(level0.checkSet(dist)){
        return true;
    }
    return false;
}

void NULPFecData::recoveryBegin(const Level& level, NRTPPacketSlot& outpkt){
    uint8_t * data = outpkt.buffer + NRTPPacketSlot::PREFIX;
    memset(data, 0, maxProtectLength+12);
    outpkt.rtpd.size = 0;
}

bool NULPFecData::recoveryAdd(const Level& level, const NRTPPacketSlot& inpkt, NRTPPacketSlot& outpkt){
    auto dist = snBase.distance(inpkt.rtpd.header.sequenceNumber);
    if(dist < 0 || dist >= (maskLength<<3)){
        return false;
    }
    
    if(!level.isCover(dist)){
        return false;
    }
    uint8_t * data = outpkt.buffer + NRTPPacketSlot::PREFIX;
    for(size_t i = 0; i < (level.length+12); ++i){
        data[i] ^= inpkt.rtpd.data[i];
    }
    outpkt.rtpd.size ^= (inpkt.rtpd.size-12);
    return true;
}

void NULPFecData::recoveryEnd(const Level& level, uint16_t seq, NRTPPacketSlot& outpkt){
    uint8_t * data = outpkt.buffer + NRTPPacketSlot::PREFIX;
    
    for(size_t i = 0; i < level.length; ++i){
        data[12+i] ^= level.data[i];
    }
    
    // recover length
    outpkt.rtpd.size ^= header.recoveryLength;
    outpkt.rtpd.size &= 0x0FFFF;
    outpkt.rtpd.size += 12;
    
    // recover header
    for(size_t i = 0; i < 8; ++i){
        data[i] ^= header.reoveryH8[i];
    }
    data[0] = (data[0] & 0x3F) | 0x80;
    NUtil::set2(data, 2, seq);
    // TODO: recover ssrc
    
    outpkt.rtpd.parse(data, outpkt.rtpd.size);
}



NRTPPacketWindow::State NRTPUlpfecWindow::insertPacket(const NRTPData& rtpd_in, const NRTPCodec& codec, const NRTPCodec::Map& codecs){
    if(codec.type == NCodec::RED){
        NRTPPacketWindow::State ret = NRTPPacketWindow::kNoError;
        rtpd_in.demuxRED([this, &ret, &codecs](NRTPData& newrtpd){
            auto it = codecs.find(newrtpd.header.payloadType);
            if(it != codecs.end() && it->second->type == NCodec::ULPFEC){
                addFEC(newrtpd);
                NRTPPacketWindow::State state = insertPacket(newrtpd, *it->second);
                if(state < 0){
                    ret = state;
                }
            }
        });
        return ret;
    }else{
        return insertPacket(rtpd_in, codec);
    }
}

NRTPPacketWindow::State NRTPUlpfecWindow::insertPacket(const NRTPData& rtpd_in, const NRTPCodec& codec){
    if(codec.type != NCodec::ULPFEC&& !codec.isNative()){
        return NRTPPacketWindow::kUnknownData;
    }
    
    NRTPPacketWindow::State state = packetWin_.insertPacket(rtpd_in);
    //printf("insertPacket: state=%d, start=%u, pkt=%s\n", state, packetWin_.startSeq().value(), rtpd_in.Dump("").c_str());
    if(state < 0){
        return state;
    }
    
    if(state == NRTPPacketWindow::kOutOfOrderReset){
        reset();
    }
    
    NRTPData& rtpd = packetWin_.lastSlot().rtpd;
    
    if(codec.type == NCodec::ULPFEC){
        addFEC(rtpd);
        
    }else if(codec.isNative()){
        for(NCircularBufferSizeT i = 0; i < feclist_.size(); ++i){
            auto fecd = feclist_[i];
            fecd.checkSet(rtpd.header.sequenceNumber);
        }
    }
    
    this->processFEC();
    
    return state;
}

bool NRTPUlpfecWindow::recoverULPFEC(NULPFecData& fecd){
    RTPSeqDistance offset = fecd.level0.firstMissOffset();
    NRTPSeq miss_seq = fecd.snBase + offset;
    auto miss_slot = packetWin_.getSlotInWindow(miss_seq);
    if(!miss_slot || miss_seq == miss_slot->rtpd.header.sequenceNumber){
        return false;
    }
    return recoverULPFEC(fecd, offset, miss_seq, *miss_slot);
}

bool NRTPUlpfecWindow::recoverULPFEC(NULPFecData& fecd
                   , RTPSeqDistance offset
                   , const NRTPSeq& miss_seq
                   , NRTPPacketSlot& miss_slot) {
    
    RTPSeqDistance dist;
    if(!packetWin_.distance(fecd.snBase, dist)){
        return false;
    }
    
    NCircularBufferSizeT num = fecd.maskLength << 8;
    fecd.recoveryBegin(fecd.level0, miss_slot);
    packetWin_.traverse(dist, num, [&fecd, &offset, &miss_slot](auto n, NRTPPacketSlot& slot)-> int{
        if(n != offset){
            fecd.recoveryAdd(fecd.level0, slot, miss_slot);
        }
        return 0;
    });
    fecd.recoveryEnd(fecd.level0, miss_seq.value(), miss_slot);
    fecd.level0.checkSet(offset);
    return true;
}

bool NRTPUlpfecWindow::verifyFEC(NULPFecData& fecd_in) {
    NULPFecData fecd = fecd_in;
    NCircularBufferSizeT num = fecd.maskLength << 8;
    for(RTPSeqDistance offset = 0; offset < num; ++offset){
        if(fecd.level0.checkClear(offset)){
            NRTPSeq miss_seq = fecd.snBase + offset;
            auto exist_slot = packetWin_.getSlotInWindow(miss_seq);
            if(!exist_slot) continue;
            
            NRTPPacketSlot * miss_slot = new NRTPPacketSlot();
            recoverULPFEC(fecd, offset, miss_seq, *miss_slot);
            
            if(miss_slot->rtpd.header.extension != exist_slot->rtpd.header.extension
               || miss_slot->rtpd.header.mark != exist_slot->rtpd.header.mark
               || miss_slot->rtpd.header.cc != exist_slot->rtpd.header.cc
               || miss_slot->rtpd.header.payloadType != exist_slot->rtpd.header.payloadType
               || miss_slot->rtpd.header.sequenceNumber != exist_slot->rtpd.header.sequenceNumber
               || miss_slot->rtpd.header.timestamp != exist_slot->rtpd.header.timestamp
               ){
                return false;
            }
            
            if(miss_slot->rtpd.payloadLen != exist_slot->rtpd.payloadLen){
                return false;
            }
            size_t len = miss_slot->rtpd.payloadLen;
            
            auto equ = memcmp(miss_slot->rtpd.payloadPtr, exist_slot->rtpd.payloadPtr, len);
            if(equ != 0){
                return false;
            }
            
            delete miss_slot;
            return true;
        }
    }
    return false;
}

bool NRTPUlpfecWindow::addFEC(const NRTPData& rtpd){
    feclist_.shiftBack();
    auto& fecd = feclist_.back();
    auto last_slot = packetWin_.lastSlot();
    auto sz = fecd.parse(rtpd.payloadPtr, rtpd.payloadLen);
    if(sz == 0){
        fecd.clear();
        feclist_.unshiftBack();
        return false;
    }
    
    RTPSeqDistance dist;
    if(!packetWin_.distance(fecd.snBase, dist)){
        fecd.clear();
        feclist_.unshiftBack();
        return false;
    }
    
    NCircularBufferSizeT num = fecd.maskLength << 3;
    packetWin_.traverse(dist, num, [&fecd] (auto n, NRTPPacketSlot& slot) -> int{
        fecd.checkSet(slot.rtpd.header.sequenceNumber);
        return 0;
    });
    
    return true;
}

void NRTPUlpfecWindow::processFEC(){
    while(!feclist_.isEmpty()){
        NULPFecData& fecd = feclist_.front();
        if(!fecd.level0.isAllRecv() && fecd.level0.isMissOne()){
            this->recoverULPFEC(fecd);
        }
        
        if(fecd.isAllReceived() || packetWin_.distance(fecd.snBase) < 0){
            //bool ok = verifyFEC(fecd);
            fecd.clear();
            feclist_.popFront();
        }else {
            break;
        }
    }
}

