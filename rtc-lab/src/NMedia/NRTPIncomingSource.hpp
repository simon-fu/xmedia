

#ifndef NRTPIncomingSource_hpp
#define NRTPIncomingSource_hpp

#include <stdio.h>
#include "NRTPMap.hpp"
#include "NRTPPacketWindow.hpp"

class NRTPFlow{
public:
    virtual ~NRTPFlow(){}
    int index = -1;
    NMedia::Type type = NMedia::Unknown;
    std::set<uint32_t> ssrcs;
};

class NRTPSource{
public:
    NRTPFlow * flow;
    int         mlineIndex;
    uint32_t    ssrc;
    uint32_t    extSeq;
    uint32_t    cycles;
    uint64_t    numPackets;
    uint64_t    numRTCPPackets;
    uint64_t    totalBytes;
    uint64_t    totalRTCPBytes;
    uint32_t    jitter;
    uint32_t    bitrate;
    
    NRTPSource(){
        flow            = nullptr;
        mlineIndex      = -1;
        ssrc            = 0;
        extSeq          = 0;
        cycles          = 0;
        numPackets      = 0;
        numRTCPPackets  = 0;
        totalBytes      = 0;
        totalRTCPBytes  = 0;
        jitter          = 0;
        bitrate         = 0;
    }
    
    virtual ~NRTPSource(){
        
    }
};

class NRTPIncomingFlow : public NRTPFlow{
public:
    NRTPIncomingFlow(bool win)
    : packetWin(win?128:0){
    }
    
    virtual ~NRTPIncomingFlow(){
        
    }
    
    const NRTPData * pullRTPInOrder(){
        NRTPPacketSlot * slot = packetWin.nextSlotInOrder();
        if(!slot) return nullptr;
        return &slot->rtpd;
    }
    
    NRTPUlpfecWindow packetWin;
    NRTPTime::RemoteNTP remoteNTP;
    
    // TODO: NAck History
};

class NRTPIncomingSource : public NRTPSource{
public:
    
    uint32_t    lostPackets;
    uint32_t    dropPackets;
    uint32_t    totalPacketsSinceLastSR;
    uint32_t    totalBytesSinceLastSR;
    uint32_t    minExtSeqNumSinceLastSR ;
    uint32_t    lostPacketsSinceLastSR;
    uint64_t    lastReceivedSenderNTPTimestamp;
    uint64_t    lastReceivedSenderReport;
    uint64_t    lastReport;
    uint64_t    lastPLI;
    uint32_t    totalPLIs;
    uint32_t    totalNACKs;
    uint64_t    lastNACKed;
    
    NRTPIncomingSource() : NRTPSource(){
        lostPackets         = 0;
        dropPackets         = 0;
        totalPacketsSinceLastSR     = 0;
        totalBytesSinceLastSR     = 0;
        lostPacketsSinceLastSR   = 0;
        lastReceivedSenderNTPTimestamp = 0;
        lastReceivedSenderReport = 0;
        lastReport         = 0;
        lastPLI             = 0;
        totalPLIs         = 0;
        totalNACKs         = 0;
        lastNACKed         = 0;
        minExtSeqNumSinceLastSR  = NRTP::MaxExtSeqNum;
    }
    
    virtual ~NRTPIncomingSource() = default;
    
public:
    using shared = std::shared_ptr<NRTPIncomingSource>;
    typedef std::map<uint32_t, NRTPIncomingSource *> Map;
};

#endif /* NRTPIncomingSource_hpp */
