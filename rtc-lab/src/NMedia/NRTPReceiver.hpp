

#ifndef NRTPReceiver_hpp
#define NRTPReceiver_hpp

#include <stdio.h>
#include "NRTPMap.hpp"
#include "NRTCPPacket.hpp"

struct NData{
    const uint8_t *     data;
    uint32_t            length;
};

template <typename T>
class NBuffer {
public:
    
    // An empty buffer
    NBuffer() : size_(0), capacity_(0), data_(nullptr) {
        
    }
    
    // Copy size and contents of an existing buffer
    NBuffer(const NBuffer& buf) : NBuffer(buf.data(), buf.size(), buf.capacity()) {
    }
    
    // Move contents from an existing buffer
    NBuffer(NBuffer&& buf)
    : size_(buf.size()),
    capacity_(buf.capacity()),
    data_(std::move(buf.data_)) {
        
    }
    
    NBuffer(size_t size) : NBuffer(size, 1) {
    }
    
    NBuffer(size_t capacity, size_t step_capacity)
    : size_(0),
    capacity_(capacity),
    capacity_step_(step_capacity),
    data_(new T[capacity_]) {

    }
    
    
    NBuffer(const T* data, size_t size, size_t capacity)
    : NBuffer(size, capacity) {
        std::memcpy(data_.get(), data, size);
    }
    
    // Note: The destructor works even if the buffer has been moved from.
    virtual ~NBuffer(){
        
    }

    
    const T* data() const {
        return (data_.get());
    }
    
    
    T* data() {
        return (data_.get());
    }
    
    size_t size() const {
        return size_;
    }
    
    size_t capacity() const {
        return capacity_;
    }
    
    NBuffer& operator=(const NBuffer& buf) {
        if (&buf != this){
            SetData(buf.data(), buf.size());
        }
        return *this;
    }
    
    NBuffer& operator=(NBuffer&& buf) {
        size_ = buf.size_;
        capacity_ = buf.capacity_;
        data_ = std::move(buf.data_);
        return *this;
    }
    
    bool operator==(const NBuffer& buf) const {
        return size_ == buf.size() && memcmp(data_.get(), buf.data(), size_) == 0;
    }
    
    bool operator!=(const NBuffer& buf) const { return !(*this == buf); }
    
    T& operator[](size_t index) {
        return data()[index];
    }
    
    T operator[](size_t index) const {
        return data()[index];
    }
    
    void SetData(const T* data, size_t size) {
        size_ = 0;
        AppendData(data, size);
    }
    
    void AppendData(const T* data, size_t size) {
        const size_t new_size = size_ + size;
        EnsureCapacity(new_size);
        std::memcpy(data_.get() + size_, data, size);
        size_ = new_size;
    }

    
    void SetData(const NBuffer& buf) { SetData(buf.data(), buf.size()); }
    
    void SetSize(size_t size) {
        EnsureCapacity(size);
        size_ = size;
    }

    void EnsureCapacity(size_t capacity) {
        if (capacity <= capacity_)
            return;

        const size_t multi = (capacity + capacity_step_-1) / capacity_step_ ;
        capacity = multi * capacity_step_;
        
        std::unique_ptr<T[]> new_data(new T[capacity]);
        std::memcpy(new_data.get(), data_.get(), size_);
        data_ = std::move(new_data);
        capacity_ = capacity;
    }
    
    void Clear() {
        size_ = 0;
    }
    
    friend void swap(NBuffer& a, NBuffer& b) {
        using std::swap;
        swap(a.size_, b.size_);
        swap(a.capacity_, b.capacity_);
        swap(a.data_, b.data_);
    }
    
protected:
    size_t size_;
    size_t capacity_;
    size_t capacity_step_ = 1;
    std::unique_ptr<T[]> data_;
};


class NMediaFrame : public NBuffer<uint8_t>{
public:

    NMediaFrame(size_t capacity, size_t step_capacity=1024
                , NMedia::Type mtype = NMedia::Unknown, NCodec::Type ctype = NCodec::UNKNOWN)
    :NBuffer(capacity),  mediaType_(mtype), codecType_(ctype) {
    }
    
    virtual ~NMediaFrame(){
    }
    
    void setTime(int64_t t){
        time_ = t;
    }
    
    int64_t getTime(){
        return time_;
    }
    
    
protected:
    NMedia::Type    mediaType_;
    NCodec::Type    codecType_;
    int64_t         time_ = -1;

};

class NVideoFrame : public NMediaFrame{
public:
    static const size_t kStartCapacity = 30*1024;
    static const size_t kStepCapacity = 30*1024;
public:
    NVideoFrame(NCodec::Type ctype) : NMediaFrame(kStartCapacity, kStepCapacity, NMedia::Video){
        
    }

};



struct NVP8PLDescriptor
{
    bool extendedControlBitsPresent;
    bool nonReferencePicture;
    bool startOfPartition;
    uint8_t partitionIndex;
    bool pictureIdPresent;
    uint8_t pictureIdLength;
    bool temporalLevelZeroIndexPresent;
    bool temporalLayerIndexPresent;
    bool keyIndexPresent;
    bool layerSync;
    uint16_t pictureId;
    uint8_t temporalLevelZeroIndex;
    uint8_t temporalLayerIndex;
    uint8_t keyIndex;
    
    NVP8PLDescriptor()
    {
        //Empty
        extendedControlBitsPresent = 0;
        nonReferencePicture = 0;
        startOfPartition = 0;
        partitionIndex = 0;
        pictureIdPresent = 0;
        pictureIdLength = 1;
        temporalLevelZeroIndexPresent = 0;
        temporalLayerIndexPresent = 0;
        keyIndexPresent = 0;
        layerSync = 0;
        pictureId = 0;
        temporalLevelZeroIndex = 0;
        temporalLayerIndex = 0;
        keyIndex = 0;
    }
    
    NVP8PLDescriptor(bool startOfPartition,uint8_t partitionIndex)
    {
        //Empty
        extendedControlBitsPresent = 0;
        nonReferencePicture = 0;
        startOfPartition = 0;
        partitionIndex = 0;
        pictureIdPresent = 0;
        pictureIdLength = 1;
        temporalLevelZeroIndexPresent = 0;
        temporalLayerIndexPresent = 0;
        keyIndexPresent = 0;
        layerSync = 0;
        pictureId = 0;
        temporalLevelZeroIndex = 0;
        temporalLayerIndex = 0;
        keyIndex = 0;
        //Set values
        this->startOfPartition = startOfPartition;
        this->partitionIndex = partitionIndex;
    }
    
    uint32_t GetSize()
    {
        //Base size
        uint32_t len = 1;
        if (extendedControlBitsPresent)
        {
            len++;
            if (pictureIdPresent)
            {
                if (pictureId>7)
                    len+=2;
                else
                    len+=1;
            }
            if (temporalLevelZeroIndexPresent)
                len++;
            if (temporalLayerIndexPresent || keyIndexPresent)
                len++;
        }
        return len;
    }
    
    uint32_t Parse(const uint8_t* data, uint32_t size)
    {
        //Check size
        if (size<1)
            //Invalid
            return 0;
        //Start parsing
        uint32_t len = 1;
        
        //Read first
        extendedControlBitsPresent    = data[0] >> 7;
        nonReferencePicture        = data[0] >> 5 & 0x01;
        startOfPartition        = data[0] >> 4 & 0x01;
        partitionIndex            = data[0] & 0x0F;
        
        //check if more
        if (extendedControlBitsPresent)
        {
            //Check size
            if (size<len+1)
                //Invalid
                return 0;
            
            //Read second
            pictureIdPresent        = data[1] >> 7;
            temporalLevelZeroIndexPresent    = data[1] >> 6 & 0x01;
            temporalLayerIndexPresent     = data[1] >> 5 & 0x01;
            keyIndexPresent            = data[1] >> 4 & 0x01;
            //Increase len
            len++;
            
            //Check if we need to read picture id
            if (pictureIdPresent)
            {
                //Check size
                if (size<len+1)
                    //Invalid
                    return 0;
                //Check mark
                if (data[len] & 0x80)
                {
                    //Check size again
                    if (size<len+2)
                        //Invalid
                        return 0;
                    //Two bytes
                    pictureIdLength = 2;
                    pictureId = (data[len] & 0x7F) << 8 | data[len+1];
                    //Inc len
                    len+=2;
                } else {
                    //One byte
                    pictureIdLength = 1;
                    pictureId = data[len];
                    //Inc len
                    len++;
                }
            }
            //check if present
            if (temporalLevelZeroIndexPresent)
            {
                //Check size
                if (size<len+1)
                    //Invalid
                    return 0;
                //read it
                temporalLevelZeroIndex = data[len];
                //Inc len
                len++;
            }
            //Check present
            if (temporalLayerIndexPresent || keyIndexPresent)
            {
                //Check size
                if (size<len+1)
                    //Invalid
                    return 0;
                //read it
                temporalLayerIndex    = data[len] >> 6;
                layerSync        = data[len] >> 5 & 0x01;
                keyIndex        = data[len] & 0x1F;
                //Inc len
                len++;
            }
        }
        
        return len;
    }
    
    uint32_t Serialize(uint8_t *data,uint32_t size)
    {
        //Check size
        if (size<GetSize())
            //Error
            return 0;
        
        /*
         The first octets after the RTP header are the VP8 payload descriptor,
         with the following structure.
         
         0 1 2 3 4 5 6 7
         +-+-+-+-+-+-+-+-+
         |X|R|N|S|PartID | (REQUIRED)
         +-+-+-+-+-+-+-+-+
         X: |I|L|T|K|RSV-A  | (OPTIONAL)
         +-+-+-+-+-+-+-+-+
         I: |   PictureID   | (OPTIONAL 8b/16b)
         +-+-+-+-+-+-+-+-+
         L: |   TL0PICIDX   | (OPTIONAL)
         +-+-+-+-+-+-+-+-+
         T: |TID|Y|  RSV-B  | (OPTIONAL)
         +-+-+-+-+-+-+-+-+
         */
        
        data[0] = extendedControlBitsPresent;
        data[0] = data[0]<<2 | nonReferencePicture;
        data[0] = data[0]<<1 | startOfPartition;
        data[0] = data[0]<<4 | (partitionIndex & 0x0F);
        
        //If nothing more present
        if (!extendedControlBitsPresent)
            //Exit now
            return 1;
        data[1] = pictureIdPresent;
        data[1] = data[1] << 1 | temporalLevelZeroIndexPresent;
        data[1] = data[1] << 1 | temporalLayerIndexPresent;
        data[1] = data[1] << 1 | keyIndexPresent;
        data[1] = data[1] << 4;
        
        uint32_t len = 2;
        
        //Check if picture id present
        if (pictureIdPresent)
        {
            //Check long is the picture id
            if (pictureIdLength == 2)
            {
                //Set picture id
                data[len]  = pictureId>>8 | 0x80;
                data[len+1] = pictureId & 0xFF;
                //Inc lenght
                len+=2;
            } else {
                //Set picture id
                data[len] = pictureId & 0x7F;
                //Inc lenght
                len++;
            }
        }
        
        if (temporalLevelZeroIndexPresent)
        {
            //Set picture id
            data[len] = temporalLevelZeroIndex;
            //Inc lenght
            len++;
        }
        
        if (temporalLayerIndexPresent || keyIndexPresent)
        {
            //Set picture id
            data[len] = temporalLayerIndex;
            data[len] = data[len]<<1 | layerSync;
            data[len] = data[len]<<1 | (keyIndex & 0x1F);
            data[len] = data[len]<<4;
            //Inc lenght
            len++;
        }
        
        return len;
    }
    
    std::string Dump(const std::string& prefix){
        std::ostringstream oss;
        oss << prefix << "[N=" << nonReferencePicture
        << ", S=" << startOfPartition
        << ", PartID=" << (unsigned)partitionIndex;
        
        if(extendedControlBitsPresent){
            if(pictureIdPresent){
                oss << ", PicID(" << (unsigned)pictureIdLength << ")=" << pictureId;
            }
            
            if(temporalLevelZeroIndexPresent){
                oss << ", TL0PICIDX=" << temporalLevelZeroIndex;
            }
            
            if (temporalLayerIndexPresent || keyIndexPresent){
                oss << ", TID=" << temporalLayerIndex;
                oss << ", Y=" << layerSync;
                oss << ", KEYIDX=" << keyIndex;
            }
            
        }
        oss << "]";
        
        return oss.str();
    }
};

struct VP8PayloadHeader
{
    bool showFrame = 0;
    bool isKeyFrame = 0;
    uint8_t version = 0;
    uint32_t firstPartitionSize = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t horizontalScale = 0;
    uint8_t verticalScale = 0;
    
    uint32_t Parse(const uint8_t* data, uint32_t size)
    {
        //Check size
        if (size<3)
            //Invalid
            return 0;
        
        //Read comon 3 bytes
        //   0 1 2 3 4 5 6 7
        //  +-+-+-+-+-+-+-+-+
        //  |Size0|H| VER |P|
        //  +-+-+-+-+-+-+-+-+
        //  |     Size1     |
        //  +-+-+-+-+-+-+-+-+
        //  |     Size2     |
        //  +-+-+-+-+-+-+-+-+
        firstPartitionSize    = data[0] >> 5;
        showFrame        = data[0] >> 4 & 0x01;
        version            = data[0] >> 1 & 0x07;
        isKeyFrame        = (data[0] & 0x0F) == 0;
        
        //check if more
        if (isKeyFrame)
        {
            //Check size
            if (size<10){
                // Invalid
                return 0;
            }
            
            //Check Start code
            if (NUtil::get3(data,3)!=0x9d012a){
                // Invalid
                return 0;
            }
            //Get size
            uint16_t hor = NUtil::get2(data,6);
            uint16_t ver = NUtil::get2(data,8);
            //Get dimensions and scale
            width        = hor & 0xC0;
            horizontalScale = hor >> 14;
            height        = ver & 0xC0;
            verticalScale    = ver >> 14;
            //Key frame
            return 10;
        }
        //No key frame
        return 3;
    }
    
    uint32_t GetSize()
    {
        return isKeyFrame ? 10 : 3;
    }
    
//    void Dump()
//    {
//        Debug("[VP8PayloadHeader \n");
//        Debug("\t isKeyFrame=%d\n"        , isKeyFrame);
//        Debug("\t version=%d\n"            , version);
//        Debug("\t showFrame=%d\n"        , showFrame);
//        Debug("\t firtPartitionSize=%d\n"    , firstPartitionSize);
//        if (isKeyFrame)
//        {
//            Debug("\t width=%d\n"        , width);
//            Debug("\t horizontalScale=%d\n"    , horizontalScale);
//            Debug("\t height=%d\n"        , height);
//            Debug("\t verticalScale=%d\n"    , verticalScale);
//        }
//        Debug("/]\n");
//    }
};

class NRTPDepackVP8{
public:
    DECLARE_CLASS_ENUM(State,
                       kNotFirst = -4,
                       kDisorder = -3,
                       kDescriptorSize = -2,
                       kZeroLength = -1,
                       kNoError = 0,
                       kComplete,
                       );
    
public:
    NRTPDepackVP8():frame_(NCodec::VP8){}
    virtual ~NRTPDepackVP8(){}

    virtual int AddPacket(const NRTPData& rtpd) {
        NVP8PLDescriptor desc;
        return AddPacketVP8(rtpd, desc);
    }
    
    virtual int AddPacketVP8(const NRTPData& rtpd, NVP8PLDescriptor &desc) {
        //Check lenght
        if (!rtpd.payloadLen){
            return kZeroLength;
        }
        
        // Check new frame
        if(frame_.getTime() != rtpd.header.timestamp){
            first_ = true;
            frame_.SetSize(0);
        }
        
        if(first_){
            frame_.setTime(rtpd.header.timestamp);
            nextSeq_ = rtpd.header.sequenceNumber;
        }
        
        if(nextSeq_ != rtpd.header.sequenceNumber){
            return kDisorder;
        }

        // Decode payload descriptr
        auto descLen = desc.Parse(rtpd.payloadPtr, (uint32_t)rtpd.payloadLen);
        
        // Check size
        if (!descLen || rtpd.payloadLen < descLen){
            return kDescriptorSize;
        }
        
        if(first_ && (!desc.startOfPartition || desc.partitionIndex != 0)){
            return kNotFirst;
        }
        
        frame_.AppendData(rtpd.payloadPtr+descLen, rtpd.payloadLen-descLen);
        
        nextSeq_ = nextSeq_+1;
        first_ = false;
        startPhase_ = false;
        
        return rtpd.header.mark ? kComplete : kNoError;
    }
    
    virtual NMediaFrame* getFrame() {
        return &frame_;
    }
    
    virtual bool startPhase() const {
        return startPhase_;
    }
    
    virtual const NRTPSeq& nextSeq() const {
        return nextSeq_;
    }
    
private:
    bool            startPhase_ = true;
    bool            first_      = true;
    NRTPSeq         nextSeq_    = 0u;
    NVideoFrame     frame_;
};


class NRTPReceiver{
    
private:
    NSDP::shared remoteSDP_;
    int flowIndex_ = 0;
    std::vector<NRTPIncomingFlow *> sourceGroupFlows;
    std::vector<NRTPIncomingFlow *> mediaFlows;
    NRTPIncomingSource::Map sourceMap; // ssrc -> source
    NRtcp::Parser rtcpParser;
    
    NRTPIncomingSource * addSource(const uint32_t ssrc
                                   , const NSDP::MediaDesc& media
                                   , NRTPFlow* flow){
        if(!flow){
            flow = mediaFlows[media.mlineIndex];
        }
        flow->ssrcs.insert(ssrc);
        auto source = new NRTPIncomingSource();
        source->mlineIndex = media.mlineIndex;
        source->ssrc = ssrc;
        source->flow = flow;
        sourceMap.insert(std::make_pair(ssrc, source));
        return source;
    }
    
    void addSourcesAndFlows(){
        for(auto& media : remoteSDP_->medias){
            
            // add ssrc-group flow
            for(auto& group : media.ssrcGroups){
                
                auto group_type = NSDP::GetGroupTypeFor(group.semantics.c_str());
                if(group_type != NSDP::FID){
                    // TODO: support NSDP::FEC
                    continue;
                }
                
                auto flow = new NRTPIncomingFlow();
                flow->index = this->flowIndex_++;
                flow->type = media.type;
                sourceGroupFlows.emplace_back(flow);
                
                for(auto& ssrc : group.ssrcs){
                    addSource(ssrc, media, flow);
                }
                
            } // for media.ssrcGroups
            
            // media flow
            auto mflow = new NRTPIncomingFlow();
            mflow->index = this->flowIndex_++;
            mflow->type = media.type;
            mediaFlows.emplace_back(mflow);
            
            // add media flow
            for(auto it : media.ssrcs){
                auto& ssrc = it.second.ssrc;
                if(sourceMap.find(ssrc) == sourceMap.end()){
                    addSource(ssrc, media, mflow);
                }
            } // media.ssrcs
            
        }// for sdp->medias
    }
    
    
public:
    
    NRTPReceiver(){
        
    }
    
    virtual ~NRTPReceiver(){
        
        for(auto it : sourceMap){
            auto& o = it.second;
            if(o){
                delete o;
            }
        }
        
        for(auto& o : sourceGroupFlows){
            if(o){
                delete o;
            }
        }
        
        for(auto& o : mediaFlows){
            if(o){
                delete o;
            }
        }
    }
    
    void setRemoteSDP(const NSDP& sdp){
        remoteSDP_ = std::make_shared<NSDP>(sdp);
        this->addSourcesAndFlows();
    }
    
    int checkFindMedia(int mlineIndex
                       , const NRTPHeader& rtphdr
                       , NRTPMediaDetail& detail){
        do{
            auto src_it = sourceMap.find(rtphdr.ssrc);
            if(src_it != sourceMap.end()){
                detail.source = src_it->second;
                mlineIndex = detail.source->mlineIndex;
            }
            
            if(mlineIndex >= 0 && mlineIndex < remoteSDP_->medias.size()){
                detail.desc = &remoteSDP_->medias[mlineIndex];
                break;
            }
            
            for(auto& m : this->remoteSDP_->medias){
                auto it = m.codecs.find(rtphdr.payloadType);
                if(it != m.codecs.end()){
                    detail.desc = &m;
                    break;
                }
            }
        }while(0);
        
        if(detail.desc){
            // check codec
            auto codec_it = detail.desc->codecs.find(rtphdr.payloadType);
            if(codec_it != detail.desc->codecs.end()){
                detail.codec = codec_it->second.get();
                
                if(!detail.source){
                    detail.source = addSource(rtphdr.ssrc, *detail.desc, nullptr);
                }
                
                return 0;
            }
        }
        return -1;
    }
    
    typedef std::function<void(const NRTPData *, const NRTPCodec*, const NRTPFlow * flow )> RTPDataFunc;
    
    int demuxRTP(const NRTPMediaDetail& detail
                 , NRTPData& rtpd
                 , NRTPMediaBrief& brief
                 , const RTPDataFunc& func){
        int ret = 0;
        
        auto& codecs = detail.desc->codecs;
        const NRTPCodec * mcodec = detail.codec;
        
        if(mcodec->type == NCodec::RTX){
            // recover payload type
            auto rtxcodec = static_cast<const NRTPCodecRTX *>(mcodec);
            //rtpd.header.payloadType = rtxcodec->apt;
            
            // recover seq
            if(rtpd.RecoverOSN(rtxcodec->apt) == 0){
                return -1;
            }
            
            // recover codec type
            auto acodec_it = codecs.find(rtxcodec->apt);
            if(acodec_it == codecs.end()){
                printf("=========================================== error:  name=unknown\n");
                return -1;
            }
            mcodec = acodec_it->second.get();
            brief.codecType = mcodec->type;
            brief.isRTX = true;
            
            // TODO: recover ssrc
            
        }
        
        if(mcodec->type == NCodec::RED){
            std::vector<NRTPData::REDBlock> blocks;
            ret = rtpd.demuxRED(blocks);
            if(ret < 0){
                return -1;
            }
            
            NRTPData newrtpd = rtpd;
            
            for(auto& block : blocks){
                auto acodec_it = codecs.find(block.pt);
                if(acodec_it != codecs.end()){
                    
                    // TODO: recover ssrc of media ?
                    newrtpd.header.payloadType = block.pt;
                    newrtpd.header.timestamp = rtpd.header.timestamp + block.tsOffset;
                    newrtpd.setPayload(block.data, block.length);
                    //brief.codecType = acodec_it->second->type;
                    func(&newrtpd, acodec_it->second.get(), detail.source->flow);
                    
                    //                    if(acodec_it->second->isNative()){
                    //
                    //                        func(&newrtpd, &brief, detail.source->flow);
                    //                    }else{
                    //                        // TODO: recover packets
                    //                        printf("  block=[pt=%u, tsoffset=%u, len=%u, name=%s]\n"
                    //                               , block.pt, block.tsOffset, block.length, acodec_it->second->name.c_str());
                    //                        brief.isFEC = true;
                    //                    }
                }else{
                    printf("  block=[pt=%u, tsoffset=%u, len=%u, name=unknown]\n"
                           , block.pt, block.tsOffset, block.length);
                }
            }
            
        }else {
            if(mcodec->isNative()){
                //func(&rtpd, &brief, detail.source->flow);
            }
            func(&rtpd, detail.codec, detail.source->flow);
        }
        
        return 0;
    }
    
    int bindMedia(int mline_index
                  , const NRTPData& rtpData
                  , NRTPMediaDetail& detail
                  , NRTPMediaBrief& brief){
        int ret = 0;
        ret = checkFindMedia(mline_index, rtpData.header, detail);
        if(ret < 0){
            return ret;
        }
        
        brief.codecType = detail.codec->type;
        brief.mlineIndex = detail.desc->mlineIndex;
        
        auto& header = rtpData.header;
        
        // parse RTP header extensions
        if (header.extension){
            auto extlen = brief.extension.Parse(detail.desc->extensions
                                                , rtpData.getExtData()
                                                , rtpData.extLength);
            if (!extlen){
                return -20;
            }
        }
        
        return 0;
    }
    
    int inputRTP(int mline_index, NRTPData& rtpd, const RTPDataFunc& func){
        NRTPMediaDetail detail;
        NRTPMediaBrief  brief;
        int ret = bindMedia(mline_index, rtpd, detail, brief);
        if(ret < 0){
            return ret;
        }
        
        ret = demuxRTP(detail, rtpd, brief, func);
        return ret;
    }
    
    // mlineIndex - mline index of remote SDP; unknown if < 0;
    int inputRTP(int64_t time_ms, int mline_index, const uint8_t * data, size_t size, const RTPDataFunc& func){
        
        // Parse RTP header and get payload
        NRTPData rtpData;
        auto pos = rtpData.parse(data, size);
        if (!pos){
            return -1;
        }
        rtpData.timeMS = time_ms;
        
        return inputRTP(mline_index, rtpData, func);
    }
    
    int inputRTCP(int64_t time_ms, int mline_index, const uint8_t * data, size_t size){
        rtcpParser.Parse(data, size, [](NRtcp::Packet &pkt ){
            //pkt.Dump(NCoutDumper::get("RTCP: "));
            
        });
        return 0;
    }
};

class NRTPPeer{
    const NSDP::shared sdp_;
public:
    using shared = std::shared_ptr<NRTPPeer>;
public:
    NRTPPeer(const NSDP::shared sdp):sdp_(sdp){
    }
    
    static NRTPPeer::shared makeWith(const NSDP::shared sdp){
        return NULL;
    }
};

#endif /* NRTPReceiver_hpp */
