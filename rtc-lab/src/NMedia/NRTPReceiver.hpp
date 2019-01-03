

#ifndef NRTPReceiver_hpp
#define NRTPReceiver_hpp

#include <stdio.h>
#include <deque>
#include "NRTPMap.hpp"
#include "NRTCPPacket.hpp"
#include "NRTPPacketWindow.hpp"
#include "NRTPIncomingSource.hpp"

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
    using Unique = NObjectPool<NMediaFrame>::Unique;
    
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
    
    void setCodecType(NCodec::Type codec_type, NMedia::Type media_type){
        codecType_ = codec_type;
        mediaType_ = media_type;
    }
    
    NCodec::Type getCodecType() const{
        return codecType_;
    }
    
    NMedia::Type getMediaType() const{
        return mediaType_;
    }
    
protected:
    NMedia::Type    mediaType_;
    NCodec::Type    codecType_;
    int64_t         time_ = -1;

};

//class NMediaFramePool {
//public:
//    NMediaFramePool(NCodec::Type codec_type, size_t start_capacity=1)
//    : codecType_(codec_type), mediaType_(NCodec::GetMediaForCodec(codec_type)){
//        if(mediaType_ == NMedia::Video){
//            frameStartSize_ = 30*1024;
//            frameStepSize_ = 30*1024;
//        }else{
//            frameStartSize_ = 1700;
//            frameStepSize_ = 1024;
//        }
//        for(size_t n = 0; n < start_capacity; ++n){
//            freeQ_.emplace_back(new NMediaFrame(frameStartSize_, frameStepSize_, mediaType_, codecType_));
//        }
//
//    }
//
//    virtual ~NMediaFramePool(){
//        for(auto& frame : freeQ_){
//            delete frame;
//        }
//    }
//
//    virtual NMediaFrame * alloc() {
//        if(freeQ_.empty()){
//            return new NMediaFrame(frameStartSize_, frameStepSize_, mediaType_, codecType_);
//        }else{
//            NMediaFrame * frame = freeQ_.back();
//            freeQ_.pop_back();
//            return frame;
//        }
//    }
//
//    virtual void free(NMediaFrame *frame){
//        freeQ_.emplace_back(frame);
//    }
//
//protected:
//    NCodec::Type codecType_;
//    NMedia::Type mediaType_ ;
//    std::deque<NMediaFrame *> freeQ_;
//    size_t frameStartSize_;
//    size_t frameStepSize_;
//};


class NAudioFrame : public NMediaFrame{
public:
    static const size_t kStartCapacity = 1700;
    static const size_t kStepCapacity = 1*1024;
    class Pool : public NPool<NAudioFrame, NMediaFrame>{
        
    };
public:
    NAudioFrame():NAudioFrame(NCodec::UNKNOWN){}
    NAudioFrame(NCodec::Type ctype) : NMediaFrame(kStartCapacity, kStepCapacity, NMedia::Audio){
        
    }
};

class NVideoFrame : public NMediaFrame{
public:
    static const size_t kStartCapacity = 30*1024;
    static const size_t kStepCapacity = 30*1024;
    class Pool : public NPool<NVideoFrame, NMediaFrame>{
        
    };
public:
    NVideoFrame():NVideoFrame(NCodec::UNKNOWN){}
    NVideoFrame(NCodec::Type ctype) : NMediaFrame(kStartCapacity, kStepCapacity, NMedia::Video){
        
    }
    
    virtual bool isKeyframe() const = 0;
    
    virtual const NVideoSize& videoSize() const  = 0;
};


struct NVP8PLDescriptor : public NObjDumper::Dumpable
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
        extendedControlBitsPresent  = data[0] >> 7;
        nonReferencePicture         = data[0] >> 5 & 0x01;
        startOfPartition            = data[0] >> 4 & 0x01;
        partitionIndex              = data[0] & 0x0F; // TODO: why partitionIndex is always 0
        
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
    
    virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
        dumper.objB();
        dumper.kv("N", (int)nonReferencePicture);
        dumper.kv("S", (int)startOfPartition);
        dumper.kv("PartIdx", (unsigned)partitionIndex);
        
        if(extendedControlBitsPresent){
            if(pictureIdPresent){
                dumper.kv(fmt::format("PicID({})", (unsigned)pictureIdLength), pictureId);
            }
            
            if(temporalLevelZeroIndexPresent){
                dumper.kv("TL0PICIDX", temporalLevelZeroIndex);
            }
            
            if (temporalLayerIndexPresent || keyIndexPresent){
                dumper.kv("TID", temporalLayerIndex);
                dumper.kv("Y", layerSync);
                dumper.kv("KEYIDX", keyIndex);
            }
            
        }
        dumper.objE();
        return dumper;
    }
};

struct VP8PayloadHeader : public NObjDumper::Dumpable
{
    bool showFrame = 0;
    bool isKeyFrame = 0;
    uint8_t version = 0;
    uint32_t firstPartitionSize = 0;
//    uint16_t width = 0;
//    uint16_t height = 0;
    NVideoSize videoSize;
    uint8_t horizontalScale = 0;
    uint8_t verticalScale = 0;
    
    uint32_t Parse(const uint8_t* data, uint32_t size)
    {
        //Check size
        if (size<3)
            //Invalid
            return 0;
        
        //   0 1 2 3 4 5 6 7
        //  +-+-+-+-+-+-+-+-+
        //  |Size0|H| VER |P|
        //  +-+-+-+-+-+-+-+-+
        //  |     Size1     |
        //  +-+-+-+-+-+-+-+-+
        //  |     Size2     |
        //  +-+-+-+-+-+-+-+-+
        
        uint8_t firstByte = data[0];
        isKeyFrame = (firstByte >> 0 & 0x1) == 0;
        version = firstByte >> 1 & 0x7;
        showFrame = (firstByte >> 4 & 0x1) ;
        firstPartitionSize = (((firstByte >> 5) & 0x7) << 0) | (data[1] << 3) | (data[2] << 11);
        
        if (!isKeyFrame){
            //No key frame
            return 3;
        }
        
        //Check size
        if (size<10){
            // Invalid
            return 0;
        }
        
        // Check Start code
        if (data[3]!=0x9d || data[4] != 0x01 || data[5] != 0x2a){
            // Invalid
            return 0;
        }
        
        uint16_t wordH = NUtil::le_get2(data, 6);
        uint16_t wordV = NUtil::le_get2(data, 8);
        horizontalScale = (wordH >> 14) & 0x3;
        verticalScale = (wordV >> 14) & 0x3;
        videoSize.width = wordH & 0x3FFF;
        videoSize.height = wordV & 0x3FFF;
//        width = wordH & 0x3FFF;
//        height = wordV & 0x3FFF;
        
        return 10;
    }
    
    uint32_t GetSize()
    {
        return isKeyFrame ? 10 : 3;
    }
    
    virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
        dumper.objB();
        dumper.kv("kfrm", (unsigned)isKeyFrame);
        dumper.kv("ver", (unsigned)version);
        dumper.kv("show", (unsigned)showFrame);
        dumper.kv("part0SZ", (unsigned)firstPartitionSize);
        if(isKeyFrame){
            dumper.kv("w", (unsigned)videoSize.width);
            if(horizontalScale != 0){
                dumper.kv("scaleH", (unsigned)horizontalScale);
            }
            dumper.kv("h", (unsigned)videoSize.height);
            if(verticalScale != 0){
                dumper.kv("scaleV", (unsigned)verticalScale);
            }
        }
        dumper.objE();
        return dumper;
    }
};

class NVP8Frame : public NVideoFrame{
public:
    class Pool : public NPool<NVP8Frame, NMediaFrame>{
        
    };
public:
    NVP8Frame() : NVideoFrame(NCodec::VP8){
        
    }
    
    virtual bool isKeyframe() const override{
        return vp8Header_.isKeyFrame;
    }
    
    virtual const NVideoSize& videoSize() const override{
        return vp8Header_.videoSize;
    }
//    virtual int width() const override{
//        return vp8Header_.width;
//    }
//
//    virtual int height() const override{
//        return vp8Header_.height;
//    }
    
    const VP8PayloadHeader& getHeader() const{
        return vp8Header_;
    }
    
    void setHeader(const VP8PayloadHeader &header){
        vp8Header_ = header;
    }
    
private:
    VP8PayloadHeader vp8Header_;
};


#define NRTPDEPACK_NO_ERROR 0
#define NRTPDEPACK_COMPLETE 1
class NRTPDepacker : public NObjDumper::Dumpable{
public:
    DECLARE_CLASS_ENUM(ErrorCode,
                       kNoError = NRTPDEPACK_NO_ERROR,
                       kComplete = NRTPDEPACK_COMPLETE,
                       );
    typedef std::function<void(NMediaFrame::Unique& frame)> FrameFunc;
    
    NRTPDepacker() {
    }
    
    virtual ~NRTPDepacker(){
    }
    
    virtual const char * getErrorStr(int errorCode) const {
        return getNameForErrorCode((ErrorCode)errorCode).c_str();
    }
    
    virtual const NRTPSeq& nextSeq() const{
        return nextSeq_;
    }
    
    virtual int64_t firstTimeMS() const{
        return firtTimeMS_;
    }
    
    virtual int depack(const NRTPData& rtpd, bool add_payload, const FrameFunc& func) = 0;
    
    virtual int nextDecodable(const NRTPData& rtpd) = 0;
    
public:
    static NRTPDepacker* CreateDepacker(const NRTPCodec * codec);
    
protected:
    bool                startPhase_     = true;
    int64_t             firtTimeMS_     = 0;
    NRTPSeq             nextSeq_        = 0u;

};

class NRTPDepackVP8 : public NRTPDepacker{
public:
    DECLARE_CLASS_ENUM(ErrorCode,
                       kErrorTimestamp = -7,
                       kNoPictureID = -6,
                       kNotDecodable = -5,
                       kNotFirst = -4,
                       kPictureDisorder = -3,
                       kSeqDisorder = -3,
                       kErrorHeaderSize = -2,
                       kErrorDescriptorSize = -2,
                       kZeroLength = -1,
                       kNoError = NRTPDEPACK_NO_ERROR,
                       kComplete = NRTPDEPACK_COMPLETE,
                       );
    const int16_t kNoPictureId = -1;
public:
    NRTPDepackVP8(std::shared_ptr<NVP8Frame::Pool>& pool)
    :NRTPDepacker(),
    pool_(pool),
    frame_(pool_->get())
    {
        frame_->Clear();
    }
    
    virtual ~NRTPDepackVP8(){
    }
    
    virtual const char * getErrorStr(int errorCode) const override{
        return getNameForErrorCode((ErrorCode)errorCode).c_str();
    }
    
    virtual int depack(const NRTPData& rtpd, bool add_payload, const FrameFunc& func) override{
        ErrorCode state = (ErrorCode) nextDecodable(rtpd);
        if(state < 0){
            expectKeyFrame_ = true;
            return state;
        }
        
        expectKeyFrame_ = false;
        
        if(descLen_ > 0){
            if(add_payload){
                frame_->AppendData(rtpd.payloadPtr+descLen_, rtpd.payloadLen-descLen_);
            }
            if(firstPacket_){
                firtTimeMS_ = rtpd.timeMS;
                frame_->setTime(rtpd.header.timestamp);
                firstPacket_ = false;
            }
            startPhase_ = false;
        }
        
        nextSeq_ = NRTPSeq(rtpd.header.sequenceNumber) + 1;
        
        if(rtpd.header.mark){
            firstPacket_ = true;
            frame_->setCodecType(NCodec::VP8, NMedia::Video);
            NVP8Frame * vp8_frame = (NVP8Frame *) frame_.get();
            vp8_frame->setHeader(this->vp8header_);
            
            func(frame_);
            
            if(!frame_){
                frame_ = pool_->get();
            }else{
                frame_->Clear();
            }
            return kComplete;
        }else{
            return kNoError;
        }
    }
    
    
    // TODO: aaa add TLayer support
    virtual int nextDecodable(const NRTPData& rtpd) override{
        ErrorCode state = (ErrorCode) parsePacket(rtpd);
        if(state < 0) return state;
        
        if(!expectKeyFrame_){
            state = (ErrorCode)continuousPacket(rtpd);
            if(state < 0){
                return state;
            }
        }else{
            if(descLen_ == 0 || !vp8desc_.startOfPartition || vp8desc_.partitionIndex != 0){
                return kNotFirst;
            }
            
            if(!vp8header_.isKeyFrame){
                return kNotDecodable;
            }
        }
        return kNoError;
    }
    
    int addContinuousPacket(const NRTPData& rtpd, bool add_payload = true){
        ErrorCode state = (ErrorCode) parsePacket(rtpd);
        if(state < 0) return state;
        
        state = (ErrorCode)continuousPacket(rtpd);
        if(state < 0){
            return state;
        }
        
        if(descLen_ > 0 && add_payload){
            frame_->AppendData(rtpd.payloadPtr+descLen_, rtpd.payloadLen-descLen_);
        }
        
        startPhase_ = false;
        nextSeq_ = NRTPSeq(rtpd.header.sequenceNumber) + 1;
        frame_->setTime(rtpd.header.timestamp);
        
        if(rtpd.header.mark){
            firstPacket_ = true;
            return kComplete;
        }else{
            firstPacket_ = false;
            return kNoError;
        }
    }
    
    int parsePacket(const NRTPData& rtpd){
        descLen_ = 0;
        headerLen_ = 0;
        
        // check lenght
        if (!rtpd.payloadLen){
            return kZeroLength;
        }
        
        if(rtpd.codecType == NCodec::VP8){
            // decode payload descriptr
            descLen_ = vp8desc_.Parse(rtpd.payloadPtr, (uint32_t)rtpd.payloadLen);
            // check size
            if (!descLen_ || rtpd.payloadLen < descLen_){
                descLen_ = 0;
                return kErrorDescriptorSize;
            }
            
            if(vp8desc_.startOfPartition && vp8desc_.partitionIndex == 0){
                headerLen_ = (size_t)vp8header_.Parse(rtpd.payloadPtr+descLen_,
                                                      (uint32_t) (rtpd.payloadLen-descLen_));
                if(!headerLen_){
                    return kErrorHeaderSize;
                }
            }
        }
        return kNoError;
    }
    
    int continuousPacket(const NRTPData& rtpd){
        if(rtpd.codecType != NCodec::VP8){
            if(startPhase_){
                return kNotFirst;
            }
            return continuousSeq(rtpd);
        }
        
        if(frame_->getTime() != rtpd.header.timestamp){
            if(!firstPacket_){
                return kErrorTimestamp;
            }
            frame_->Clear();
        }
        
        if(firstPacket_ && (!vp8desc_.startOfPartition || vp8desc_.partitionIndex != 0)){
            return kNotFirst;
        }
        
        if(lastPicId_ == kNoPictureId){
            return continuousSeq(rtpd);
        }
        
        // here lastPicId_ exist
        
        if(!vp8desc_.pictureIdPresent){
            return kNoPictureID;
        }
        
        return continuousPictureId(vp8desc_.pictureId);
    }
    
    int continuousPictureId(int picture_id) const {
        int next_picture_id = lastPicId_ + 1;
        if (picture_id < lastPicId_) {
            // Wrap
            if (lastPicId_ >= 0x80) {
                // 15 bits used for picture id
                if((next_picture_id & 0x7FFF) == picture_id){
                    return kNoError;
                }
            } else {
                // 7 bits used for picture id
                if((next_picture_id & 0x7F) == picture_id){
                    return kNoError;
                }
            }
        }else {
            // No wrap
            if(next_picture_id == picture_id){
                return kNoError;
            }
        }
        
        return kPictureDisorder;
    }
    
    int continuousSeq(const NRTPData& rtpd){
        return (startPhase_ || nextSeq_ == rtpd.header.sequenceNumber) ?
        kNoError : kSeqDisorder;
    }
    
    virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
        dumper.objB();
        dumper.kv("needkey", (unsigned)expectKeyFrame_);
        if(descLen_ > 0){
            dumper.dump("desc", vp8desc_);
        }
        
        if(headerLen_ > 0){
            dumper.dump("hdr", vp8header_);
        }
        
        dumper.objE();
        return dumper;
    }
    
    static
    void dump_vp8_frame(uint8_t * frameData, size_t frameSize, NLogger::shared& logger){
        uint8_t firstByte = frameData[0];
        int frmtype = (firstByte >> 0 & 0x1);
        int ver = firstByte >> 1 & 0x7;
        int disp = (firstByte >> 4 & 0x1) ;
        int part1Len = (((firstByte >> 5) & 0x7) << 0) | (frameData[1] << 3) | (frameData[2] << 11);
        logger->info("  frame: type={}, ver={}, disp={}, part1={}", frmtype, ver, disp, part1Len);
        if(frmtype == 0){
            // startcode=[9d 01 2a]
            // Horizontal 16 bits      :     (2 bits Horizontal Scale << 14) | Width (14 bits)
            // Vertical   16 bits      :     (2 bits Vertical Scale << 14) | Height (14 bits)
            uint8_t * ptr = frameData + 3;
            uint16_t wordH = ptr[4] << 8 | ptr[3];
            uint16_t wordV = ptr[6] << 8 | ptr[5];
            int scaleH = (wordH >> 14) & 0x3;
            int width = wordH & 0x3FFF;
            int scaleV = (wordV >> 14) & 0x3;
            int height = wordV & 0x3FFF;
            
            uint8_t * part1Data = (frameData+10);
            int color = part1Data[0] & 0x1;
            int pixel = (part1Data[0]>>1) & 0x1;
            
            logger->info(
                         //"         startcode=[%02x %02x %02x], horiz=[%d, %d], vert=[%d, %d], color=%d, pixel=%d"
                         "         startcode=[{:02x} {:02x} {:02x}], horiz=[{}, {}], vert=[{}, {}], color={}, pixel={}"
                         , ptr[0], ptr[1], ptr[2]
                         , scaleH, width, scaleV, height
                         , color, pixel);
        }
        
    }
    
private:
    std::shared_ptr<NVP8Frame::Pool> pool_;
    NMediaFrame::Unique frame_;
    bool                expectKeyFrame_ = true;
    bool                firstPacket_    = true;
    NVP8PLDescriptor    vp8desc_;
    VP8PayloadHeader    vp8header_;
    size_t              descLen_ = 0;
    size_t              headerLen_ = 0;
    int                 lastPicId_ = kNoPictureId;
};


class NRTPReceiver {
public:
    class IncomingRTPInfo : public NObjDumper::Dumpable{
    public:
        // bind info
        const NRTPData  *       rtpd = nullptr;
        NSDP::MediaDesc *       desc = nullptr;
        NRTPCodec *             codec = nullptr;
        NRTPIncomingSource *    source = nullptr;
        NRTPIncomingFlow *      flow = nullptr;
        
        // demux info
        NRTPHeaderExtension     extension;
        bool                    isRTX = false;  // recover from RTX packet
        bool                    isRED = false;  // demux from RED packet
        
        virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
            dumper.objB();
            rtpd->dumpFields(dumper);
            if(isRTX){
                dumper.kv("rtx", 1);
            }
            if(isRED){
                dumper.kv("red", 1);
            }
            // TODO: dump extension
            dumper.objE();
            return dumper;
        }
    };
    
private:
    NLogger::shared logger_;
    NSDP::shared remoteSDP_;
    int flowIndex_ = 0;
    std::vector<NRTPIncomingFlow *> sourceGroupFlows;
    std::vector<NRTPIncomingFlow *> mediaFlows;
    NRTPIncomingSource::Map sourceMap; // ssrc -> source
    NRtcp::Parser rtcpParser;
    
    NRTPIncomingSource * addSource(const uint32_t ssrc,
                                   const NSDP::MediaDesc& media,
                                   NRTPFlow* flow){
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
                
                auto flow = new NRTPIncomingFlow(true);
                flow->index = this->flowIndex_++;
                flow->type = media.type;
                sourceGroupFlows.emplace_back(flow);
                
                for(auto& ssrc : group.ssrcs){
                    addSource(ssrc, media, flow);
                }
                
            } // for media.ssrcGroups
            
            // media flow
            auto mflow = new NRTPIncomingFlow(true);
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
        logger_ = NLogger::Get("recv");
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
                       , IncomingRTPInfo& detail){
        do{
            auto src_it = sourceMap.find(rtphdr.ssrc);
            if(src_it != sourceMap.end()){
                detail.source = src_it->second;
                detail.flow = static_cast<NRTPIncomingFlow*>(src_it->second->flow);
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
    
    typedef std::function<void(const IncomingRTPInfo *)> RTPDataFunc;
    
    int demuxRTP(IncomingRTPInfo& detail,
                 NRTPData& rtpd,
                 const RTPDataFunc& func){
        int ret = 0;
        
        auto& codecs = detail.desc->codecs;
        NRTPCodec * &mcodec = detail.codec;
        
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
            rtpd.codecType = mcodec->type;
            detail.isRTX = true;
            
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
                    mcodec = acodec_it->second.get();
                    newrtpd.header.payloadType = block.pt;
                    newrtpd.header.timestamp = rtpd.header.timestamp + block.tsOffset;
                    newrtpd.setPayload(block.data, block.length);
                    newrtpd.codecType = acodec_it->second->type;
                    detail.isRED = true;
                    detail.rtpd = &newrtpd;
                    func(&detail);
                    
                }else{
                    printf("  block=[pt=%u, tsoffset=%u, len=%u, name=unknown]\n"
                           , block.pt, block.tsOffset, block.length);
                }
            }
            
        }else {
            func(&detail);
        }
        
        return 0;
    }
    
    int bindMedia(int mline_index
                  , NRTPData& rtpData
                  , IncomingRTPInfo& detail){
        int ret = 0;
        ret = checkFindMedia(mline_index, rtpData.header, detail);
        if(ret < 0){
            return ret;
        }
        
        detail.rtpd = &rtpData;
        rtpData.codecType = detail.codec->type;
        
        return 0;
    }
    
    int inputRTP(NRTPData& rtpData,
                 IncomingRTPInfo& detail,
                 const RTPDataFunc& func){
        auto& header = rtpData.header;
        if (header.extension){
            auto extlen = detail.extension.Parse(detail.desc->extensions
                                                , rtpData.getExtData()
                                                , rtpData.extLength);
            if (!extlen){
                return -20;
            }
        }
        
        int ret = demuxRTP(detail, rtpData,
                           [this, &detail, &func](const IncomingRTPInfo * info){
                               NRTPIncomingFlow * inflow = detail.flow ;
                               NObjDumperLog(*logger_, NLogger::Level::debug).dump("demux RTP", *info);
                               if(inflow->packetWin.capacity() > 0){
                                   inflow->packetWin.insertPacket(*info->rtpd, *detail.codec);
                               }
                               if(!inflow->mainCodec && info->codec->isNative()){
                                   // TODO: remove cast
                                   inflow->mainCodec = info->codec;
                               }
                               func(info);
                           });
        
        return ret;
    }
    
    int inputRTP(int mline_index, NRTPData& rtpd, const RTPDataFunc& func){
        IncomingRTPInfo detail;
        int ret = bindMedia(mline_index, rtpd, detail);
        if(ret < 0){
            return ret;
        }
        
        NObjDumperLog(*logger_, NLogger::Level::debug).dump("bind RTP", detail);
        
        ret = inputRTP(rtpd, detail, func);
        return ret;
    }
    
    // mlineIndex - mline index of remote SDP; unknown if < 0;
    int inputRTP(int64_t time_ms, int mline_index, const uint8_t * data, size_t size, const RTPDataFunc& func){
        
        // Parse RTP header and get payload
        NRTPData rtpData;
        auto pos = rtpData.parse(data, size);
        if (!pos){
            logger_->warn("fail to parse RTP, size={}", size);
            // TODO: dump data
            return -1;
        }
        rtpData.timeMS = time_ms;
        
        return inputRTP(mline_index, rtpData, func);
    }
    
    int inputRTCP(int64_t time_ms, int mline_index, const uint8_t * data, size_t size){
        rtcpParser.Parse(data, size, [this](NRtcp::Packet &pkt ){
            NObjDumperLog(*logger_, NLogger::Level::debug).dump("RTCP", pkt);
            if(pkt.type == NRtcp::kSenderReport){
                NRtcp::SenderReport& sr = (NRtcp::SenderReport&) pkt;
                
                auto src_it = sourceMap.find(sr.ssrc);
                if(src_it != sourceMap.end()){
                    NRTPIncomingFlow * flow = static_cast<NRTPIncomingFlow*>(src_it->second->flow);
                    flow->remoteNTP.updateNTP(sr.ntpSec, sr.ntpFrac, sr.rtpTimestamp);
                }
            }
            
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
