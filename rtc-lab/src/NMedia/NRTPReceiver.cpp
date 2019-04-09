

#include "NRTPReceiver.hpp"

template <class PoolType>
class NRTPDepackGeneric : public NRTPDepacker{
public:
    DECLARE_CLASS_ENUM(ErrorCode,
                       kNotDecodable = -5,
                       kNoError = NRTPDEPACK_NO_ERROR,
                       kComplete = NRTPDEPACK_COMPLETE,
                       );
//    const int16_t kNoPictureId = -1;
public:
    NRTPDepackGeneric(NCodec::Type codec_type, std::shared_ptr<PoolType>& pool)
    :NRTPDepacker(),
    codecType_(codec_type),
    pool_(pool),
    frame_(pool_->get()){
        mediaType_ = NCodec::GetMediaForCodec(codecType_);
        frame_->Clear();
        frame_->setCodecType(codecType_, mediaType_);
    }

    virtual ~NRTPDepackGeneric(){
    }

    virtual const char * getErrorStr(int errorCode) const override{
        return getNameForErrorCode((ErrorCode)errorCode).c_str();
    }


    virtual int depack(const NRTPData& rtpd, bool add_payload, const FrameFunc& func) override{
        if(rtpd.codecType != frame_->getCodecType()){
            nextSeq_ = NRTPSeq(rtpd.header.sequenceNumber) + 1;
            return kNoError;
        }else{
            firtTimeMS_ = rtpd.timeMS;
            frame_->setTime(rtpd.header.timestamp);
            if(add_payload){
                frame_->SetData(rtpd.payloadPtr, rtpd.payloadLen);
            }
            frame_->setCodecType(codecType_, mediaType_);
            nextSeq_ = NRTPSeq(rtpd.header.sequenceNumber) + 1;
            func(frame_);
            
            if(!frame_){
                frame_ = pool_->get();
                frame_->setCodecType(codecType_, mediaType_);
            }else{
                frame_->Clear();
            }
            
            return kComplete;
        }
    }

    virtual int nextDecodable(const NRTPData& rtpd) override{
        if(rtpd.codecType != frame_->getCodecType()){
            return kNotDecodable;
        }
        return kNoError;
    }

    virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
        dumper.objB();
        dumper.objE();
        return dumper;
    }

private:
    NCodec::Type                codecType_;
    NMedia::Type                mediaType_;
    std::shared_ptr<PoolType>   pool_;
    NMediaFrame::Unique         frame_;
};

NRTPDepacker* NRTPDepacker::CreateDepacker(const NRTPCodec * codec){
    return CreateDepacker(codec->type);
}

NRTPDepacker* NRTPDepacker::CreateDepacker(const NCodec::Type typ){
    if(typ == NCodec::VP8){
        std::shared_ptr<NVP8Frame::Pool> pool(new NVP8Frame::Pool());
        return new NRTPDepackVP8(pool);
    }else if(NCodec::GetMediaForCodec(typ) == NMedia::Audio){
        // TODO: add media type to NRTPCodec for performance
        std::shared_ptr<NAudioFrame::Pool> pool(new NAudioFrame::Pool());
        return new NRTPDepackGeneric<NAudioFrame::Pool>(typ, pool);
    }else{
        //return new NRTPDepackGeneric(codec_type, pool);
        return nullptr;
    }
}

