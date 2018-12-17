

#ifndef NRTPMAP_H
#define NRTPMAP_H

#include "NRTPTime.hpp"
#include "NLogger.hpp"
#include "NUtil.hpp"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <list>
#include <sstream>
#include <stack>
#include <iomanip> 




// see https://www.iana.org/assignments/media-types/media-types.xhtml
class NMedia{
public:
    enum Type {Audio=0,Video=1,Text=2,Application=3,Unknown=-1};
    static const char* GetNameFor(Type typ){
        switch (typ){
            case Audio:         return "audio";
            case Video:         return "video";
            case Text:          return "text";
            case Application:   return "application";
            default:            return "unknown";
        }
    }
    
    static Type GetMediaFor(const char* media){
        if    (strcasecmp(media,"audio")==0) return Audio;
        else if (strcasecmp(media,"video")==0) return Video;
        else if (strcasecmp(media,"text")==0) return Text;
        else if (strcasecmp(media,"application")==0) return Application;
        return Unknown;
    }
    
};

//typedef uint8_t NCodecNumType;
typedef int NCodecNumType;

typedef int NRTPPayloadType;
#define UNKNOWN_PAYLOAD_TYPE -1

class NCodec{
public:
    enum Type{
        UNKNOWN=-1,
        
        // audio codec
        PCMA=8,
        PCMU=0,
        GSM=3,
        G722=9,
        SPEEX16=117,
        AMR=118,
        TELEPHONE_EVENT=100,
        NELLY8=130,
        NELLY11=131,
        OPUS=98,
        AAC=97,
        
        // video codec
        H263_1996=34,
        H263_1998=103,
        MPEG4=104,
        H264=99,
        SORENSON=100,
        VP6=106,
        VP8=107,
        VP9=112,
        
        ULPFEC=108,
        FLEXFEC=113,
        RED=109,
        RTX=110,
        
        // text codec
        T140=102,
        T140RED=105
    };
    
public:
    static Type GetCodecForName(const char* codec){
        if    (strcasecmp(codec,"PCMA")==0) return PCMA;
        else if (strcasecmp(codec,"PCMU")==0) return PCMU;
        else if (strcasecmp(codec,"GSM")==0) return GSM;
        else if (strcasecmp(codec,"SPEEX16")==0) return SPEEX16;
        else if (strcasecmp(codec,"NELLY8")==0) return NELLY8;
        else if (strcasecmp(codec,"NELLY11")==0) return NELLY11;
        else if (strcasecmp(codec,"OPUS")==0) return OPUS;
        else if (strcasecmp(codec,"G722")==0) return G722;
        else if (strcasecmp(codec,"AAC")==0) return AAC;
        
        else if (strcasecmp(codec,"H263_1996")==0) return H263_1996;
        else if (strcasecmp(codec,"H263-1996")==0) return H263_1996;
        else if (strcasecmp(codec,"H263")==0) return H263_1996;
        else if (strcasecmp(codec,"H263P")==0) return H263_1998;
        else if (strcasecmp(codec,"H263_1998")==0) return H263_1998;
        else if (strcasecmp(codec,"H263-1998")==0) return H263_1998;
        else if (strcasecmp(codec,"MPEG4")==0) return MPEG4;
        else if (strcasecmp(codec,"H264")==0) return H264;
        else if (strcasecmp(codec,"SORENSON")==0) return SORENSON;
        else if (strcasecmp(codec,"VP6")==0) return VP6;
        else if (strcasecmp(codec,"VP8")==0) return VP8;
        else if (strcasecmp(codec,"VP9")==0) return VP9;
        else if (strcasecmp(codec,"ULPFEC")==0) return ULPFEC;
        else if (strcasecmp(codec,"FLEXFEC")==0) return FLEXFEC;
        else if (strcasecmp(codec,"RED")==0) return RED;
        else if (strcasecmp(codec,"RTX")==0) return RTX;
        return UNKNOWN;
    }
    
    static const char* GetNameFor(Type codec){
        switch (codec){
            case PCMA:    return "PCMA";
            case PCMU:    return "PCMU";
            case GSM:    return "GSM";
            case SPEEX16:    return "SPEEX16";
            case NELLY8:    return "NELLY8Khz";
            case NELLY11:    return "NELLY11Khz";
            case OPUS:    return "OPUS";
            case G722:    return "G722";
            case AAC:    return "AAC";
                
            case H263_1996:    return "H263_1996";
            case H263_1998:    return "H263_1998";
            case MPEG4:    return "MPEG4";
            case H264:    return "H264";
            case SORENSON:  return "SORENSON";
            case VP6:    return "VP6";
            case VP8:    return "VP8";
            case VP9:    return "VP9";
            case RED:    return "RED";
            case RTX:    return "RTX";
            case ULPFEC:    return "ULPFEC";
            case FLEXFEC:    return "flexfec-03";
                
            case T140:    return "T140";
            case T140RED:    return "T140RED";

            default:    return "unknown";
        }
    }
    
    static bool IsNative(Type codec){
        switch (codec){
            case PCMA:
            case PCMU:
            case GSM:
            case SPEEX16:
            case NELLY8:
            case NELLY11:
            case OPUS:
            case G722:
            case AAC:
                
            case H263_1996:
            case H263_1998:
            case MPEG4:
            case H264:
            case SORENSON:
            case VP6:
            case VP8:
            case VP9: return true;
            
            default:    return false;
        }
    }
    
    static uint32_t GetAudioClockRate(Type codec)
    {
        switch (codec)
        {
            case PCMA:    return 8000;
            case PCMU:    return 8000;
            case GSM:    return 8000;
            case SPEEX16:    return 16000;
            case NELLY8:    return 8000;
            case NELLY11:    return 11000;
            case OPUS:    return 48000;
            case G722:    return 16000;
            case AAC:    return 90000;
            default:    return 8000;
        }
    }
    
    static uint32_t GetClockRate(Type codec){
        if(GetMediaForCodec(codec) == NMedia::Audio){
            return GetAudioClockRate(codec);
        }else{
            return 90000;
        }
    }
    
    static NMedia::Type GetMediaForCodec(Type codec)
    {
        switch (codec)
        {
            case PCMA:
            case PCMU:
            case GSM:
            case SPEEX16:
            case NELLY8:
            case NELLY11:
            case OPUS:
            case G722:
            case AAC:
                return NMedia::Audio;
            case H263_1996:
            case H263_1998:
            case MPEG4:
            case H264:
            case SORENSON:
            case VP6:
            case VP8:
            case VP9:
            case RED:
            case RTX:
            case ULPFEC:
            case FLEXFEC:
                return NMedia::Video;
            case T140:
            case T140RED:
                return NMedia::Text;
            default:
                return NMedia::Unknown;
        }
    }
    
};// class NCodec

class NRTPHeaderExtension{
public:
    enum Type {
        UNKNOWN                         = 0,
        SSRCAudioLevel                  = 1,
        TimeOffset                      = 2,
        AbsoluteSendTime                = 3,
        CoordinationOfVideoOrientation  = 4,
        TransportWideCC                 = 5,
        FrameMarking                    = 6,
        RTPStreamId                     = 7,
        RepairedRTPStreamId             = 8,
        MediaStreamId                   = 9,
    };
    
    static Type GetExtensionForName(const char* ext){
        if    (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:toffset")==0)                        return TimeOffset;
        else if (strcasecmp(ext,"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time")==0)            return AbsoluteSendTime;
        else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:ssrc-audio-level")==0)                    return SSRCAudioLevel;
        else if (strcasecmp(ext,"urn:3gpp:video-orientation")==0)                            return CoordinationOfVideoOrientation;
        else if (strcasecmp(ext,"http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01")==0)    return TransportWideCC;
        else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:framemarking")==0)                        return FrameMarking;
        else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id")==0)                    return RTPStreamId;
        else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id")==0)                return RepairedRTPStreamId;
        else if (strcasecmp(ext,"urn:ietf:params:rtp-hdrext:sdes:mid")==0)                        return MediaStreamId;
        return UNKNOWN;
    }
    
    static const char* GetNameFor(Type ext){
        switch (ext){
            case TimeOffset:            return "TimeOffset";
            case AbsoluteSendTime:            return "AbsoluteSendTime";
            case SSRCAudioLevel:            return "SSRCAudioLevel";
            case CoordinationOfVideoOrientation:    return "CoordinationOfVideoOrientation";
            case TransportWideCC:            return "TransportWideCC";
            case FrameMarking:            return "FrameMarking";
            case RTPStreamId:            return "RTPStreamId";
            default:                return "unknown";
        }
    }
    
    struct VideoOrientation
    {
        bool facing;
        bool flip;
        uint8_t rotation;
        
        VideoOrientation()
        {
            facing        = 0;
            flip        = 0;
            rotation    = 0;
        }
    };
    
    // For Frame Marking RTP Header Extension:
    // https://tools.ietf.org/html/draft-ietf-avtext-framemarking-04#page-4
    // This extensions provides meta-information about the RTP streams outside the
    // encrypted media payload, an RTP switch can do codec-agnostic
    // selective forwarding without decrypting the payload
    //   o  S: Start of Frame (1 bit) - MUST be 1 in the first packet in a
    //      frame; otherwise MUST be 0.
    //   o  E: End of Frame (1 bit) - MUST be 1 in the last packet in a frame;
    //      otherwise MUST be 0.
    //   o  I: Independent Frame (1 bit) - MUST be 1 for frames that can be
    //      decoded independent of prior frames, e.g. intra-frame, VPX
    //      keyframe, H.264 IDR [RFC6184], H.265 IDR/CRA/BLA/RAP [RFC7798];
    //      otherwise MUST be 0.
    //   o  D: Discardable Frame (1 bit) - MUST be 1 for frames that can be
    //      discarded, and still provide a decodable media stream; otherwise
    //      MUST be 0.
    //   o  B: Base Layer Sync (1 bit) - MUST be 1 if this frame only depends
    //      on the base layer; otherwise MUST be 0.  If no scalability is
    //      used, this MUST be 0.
    //   o  TID: Temporal ID (3 bits) - The base temporal layer starts with 0,
    //      and increases with 1 for each higher temporal layer/sub-layer.  If
    //      no scalability is used, this MUST be 0.
    //   o  LID: Layer ID (8 bits) - Identifies the spatial and quality layer
    //      encoded.  If no scalability is used, this MUST be 0 or omitted.
    //      When omitted, TL0PICIDX MUST also be omitted.
    //   o  TL0PICIDX: Temporal Layer 0 Picture Index (8 bits) - Running index
    //      of base temporal layer 0 frames when TID is 0.  When TID is not 0,
    //      this indicates a dependency on the given index.  If no scalability
    //      is used, this MUST be 0 or omitted.  When omitted, LID MUST also
    //      be omitted.
    struct FrameMarks {
        bool startOfFrame;
        bool endOfFrame;
        bool independent;
        bool discardable;
        bool baseLayerSync;
        uint8_t temporalLayerId;
        uint8_t layerId;
        uint8_t tl0PicIdx;
        
        FrameMarks()
        {
            startOfFrame = false;
            endOfFrame = false;
            independent = false;
            discardable = false;
            baseLayerSync = false;
            temporalLayerId = 0;
            layerId = 0;
            tl0PicIdx = 0;
        }
    };
    
    class Map : public std::map<uint32_t, uint32_t> {
    public:
        uint32_t getFor(uint32_t id) const{
            auto it = find(id);
            if(it == end()){
                return UNKNOWN;
            }
            return it->second;
        }
        
        const_iterator search(uint32_t typ) const{
//            return std::find_if(begin(), end(),
//                                [&typ](uint32_t const& value){
//                                    return value == typ;
//                                });
            
            auto it = begin();
            for (; it!=end(); ++it){
                if (it->second==typ){
                    return it;
                }
            }
            return it;
        }
    };
    
public:
    NRTPHeaderExtension()
    {
        absSentTime = 0;
        timeOffset = 0;
        vad = 0;
        level = 0;
        transportSeqNum = 0;
        hasAbsSentTime = 0;
        hasTimeOffset =  0;
        hasAudioLevel = 0;
        hasVideoOrientation = 0;
        hasTransportWideCC = 0;
        hasFrameMarking = 0;
        hasRTPStreamId = 0;
        hasRepairedRTPStreamId = 0;
        hasMediaStreamId = 0;
        exist = false;
    }
    
    uint32_t Parse(const Map &extMap,const uint8_t* data, const size_t size);
    uint32_t Serialize(const Map &extMap, uint8_t* data, const size_t size) const;
    void  Dump(const std::string& prefix, std::list<std::string>& lines) const;
public:
    uint64_t    absSentTime;
    int    timeOffset;
    bool    vad;
    uint8_t    level;
    uint16_t    transportSeqNum;
    VideoOrientation cvo;
    FrameMarks frameMarks;
    std::string rid;
    std::string repairedId;
    std::string mid;
    bool    hasAbsSentTime;
    bool    hasTimeOffset;
    bool    hasAudioLevel;
    bool    hasVideoOrientation;
    bool    hasTransportWideCC;
    bool    hasFrameMarking;
    bool    hasRTPStreamId;
    bool    hasRepairedRTPStreamId;
    bool    hasMediaStreamId;
    bool    exist;
};


#define STR_GOOG_REMB   "goog-remb"
#define STR_TRANS_CC    "transport-cc"
#define STR_CCM_FIR     "ccm fir"
#define STR_NACK        "nack"
#define STR_NACK_PLI    "nack pli"



class NRTPCodec{
public:
    class RtcpFBMask{
        uint32_t rtcpfb_mask_ = 0;
    public:
        enum RTCPFB{
            GOOG_REMB =    1 << 0,
            TRANS_CC =     1 << 1,
            CCM_FIR =      1 << 2,
            NACK =         1 << 3,
            NACK_PLI =     1 << 4,
        };
        
        bool hasRtcpFB(RTCPFB fb) const{
            return (rtcpfb_mask_ & fb);
        }
        
        void setRtcpFB(RTCPFB fb){
            rtcpfb_mask_ |= fb;
        }
        
        bool setStr(const std::string& o){
            if(o == STR_GOOG_REMB){
                this->setRtcpFB(GOOG_REMB);
            }else if(o == STR_TRANS_CC){
                this->setRtcpFB(TRANS_CC);
            }else if(o == STR_CCM_FIR){
                this->setRtcpFB(CCM_FIR);
            }else if(o == STR_NACK){
                this->setRtcpFB(NACK);
            }else if(o == STR_NACK_PLI){
                this->setRtcpFB(NACK_PLI);
            }else {
                return false;
            }
            return true;
        }
        
        void dump(const std::string& prefix, std::list<std::string>& lines) const{
            std::ostringstream oss;
            
            if(hasRtcpFB(GOOG_REMB)){
                oss << prefix << "rtcp-fb=[" STR_GOOG_REMB "]";
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
            
            if(hasRtcpFB(TRANS_CC)){
                oss << prefix << "rtcp-fb=[" STR_TRANS_CC "]";
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
            
            if(hasRtcpFB(CCM_FIR)){
                oss << prefix << "rtcp-fb=[" STR_CCM_FIR "]";
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
            
            if(hasRtcpFB(NACK)){
                oss << prefix << "rtcp-fb=[" STR_NACK "]";
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
            
            if(hasRtcpFB(NACK_PLI)){
                oss << prefix << "rtcp-fb=[" STR_NACK_PLI "]";
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
        }
    };
    
public:
    NRTPPayloadType pt = UNKNOWN_PAYLOAD_TYPE;
    std::string name;
    NCodec::Type type;
    uint32_t clockrate;
    uint32_t channels = 1;
    bool native = false;
    RtcpFBMask rtcpfb;
    std::list<std::string> fmtps;  // unknown fmtp
    std::set<std::string> rtcpfbs; // unknown rtcp-fb
    
public:
//    NRTPCodec() : NRTPCodec(NCodec::UNKNOWN){
//    }
    
    NRTPCodec(NCodec::Type typ) : type(typ), clockrate(NCodec::GetClockRate(typ)){
        this->native = NCodec::IsNative(this->type);
    }
    
    virtual ~NRTPCodec(){
    }
    
    virtual bool isNative() const{
        return this->native;
    }
    
    virtual bool parseFMTP(const std::string& fmtp) {
        return false;
    }
    
    virtual void dump(const std::string& prefix, std::list<std::string>& lines) const{
        std::ostringstream oss;
        oss << prefix << "codec=["
        << pt
        << ", " << name // NCodec::GetNameFor(type)
        << "/" << clockrate;
        if(channels != 1){
            oss << "/" << channels;
        }
        oss << "]";
        
        lines.emplace_back(oss.str()); oss.str("");oss.clear();
        
        for(auto& o : rtcpfbs){
            oss << prefix << "  rtcp-fb=[" << o << "]";
            lines.emplace_back(oss.str());oss.str("");oss.clear();
        }
        
        rtcpfb.dump(prefix+"  ", lines);
        
        for(auto& o : fmtps){
            oss << prefix << "  fmtp=[" << o << "]";
            lines.emplace_back(oss.str());oss.str("");oss.clear();
        }
    }
    
public:
    using shared = std::shared_ptr<NRTPCodec>;
    
    typedef std::map<NRTPPayloadType, shared> Map;

    static void parseFMTPPair(const std::string& fmtp
                              , std::function<void(const std::string& key,
                                                   const std::string& value)> func){
        NLineParser line(fmtp.c_str(), fmtp.length());
        std::string str;
        while(line.nextWord(str, ';')){
            NLineParser eval(str.c_str(), str.length());
            std::string key;
            std::string value;
            if(eval.nextWord(key, '=')){
                eval.nextWord(value, '=');
                func(key, value);
            }
        }// while
    }
    
    //static shared makeWith(NCodec::Type type);
    static shared makeWith(const std::string& codec_name);
};

class NRTPCodecRTX : public NRTPCodec{
public:

    NRTPPayloadType apt = UNKNOWN_PAYLOAD_TYPE;
    
    NRTPCodecRTX():NRTPCodec(NCodec::RTX){
        
    }
    
    virtual bool parseFMTP(const std::string& fmtp) override{
        NRTPCodec::parseFMTPPair(fmtp, [&](const std::string & key, const std::string & value){
            if(key == "apt"){
                this->apt = NLineParser::StrToU32(value);
            }
        });
        return true;
    }
    
    virtual void dump(const std::string& prefix, std::list<std::string>& lines) const override{
        NRTPCodec::dump(prefix, lines);
        
        std::ostringstream oss;
        if(this->apt != UNKNOWN_PAYLOAD_TYPE){
            oss << prefix << "  apt=[" << this->apt << "]" ;
            lines.emplace_back(oss.str());oss.str("");oss.clear();
        }
    }
};

struct NSSrc{
    uint32_t ssrc;
    std::string cname;
    std::string mslabel;
    std::string label;
    NSSrc(uint32_t s) : ssrc(s){}
    NSSrc() : ssrc(0){}
};



class NSDP{
public:
    using shared = std::shared_ptr<NSDP>;
    
    enum Direction{
        inactive,
        sendrecv,
        sendonly,
        recvonly,
        unknown = -1,
    };
    
    static const char * GetDirectionNameFor(Direction dir){
        switch (dir){
            case Direction::inactive:          return "inactive";
            case Direction::sendrecv:          return "sendrecv";
            case Direction::sendonly:          return "sendonly";
            case Direction::recvonly:          return "recvonly";
            default:                return "unknown";
        }
    }
    
    enum GroupType{
        LS = 0,
        FID,
        FEC,
        BUNDLE ,
        SIM, // simulcast
        UNKNOWN = -1,
    };
    
    static const char * GetGroupTypeNameFor(GroupType semantics){
        switch (semantics){
            case GroupType::LS:         return "LS";
            case GroupType::FID:        return "FID";
            case GroupType::FEC:        return "FEC";
            case GroupType::BUNDLE:     return "BUNDLE";
            case GroupType::SIM:        return "SIM";
            default:                return "unknown";
        }
    }
    
    static GroupType GetGroupTypeFor(const char* name){
        if    (strcasecmp(name,"LS")==0) return GroupType::LS;
        else if (strcasecmp(name,"FID")==0) return GroupType::FID;
        else if (strcasecmp(name,"FEC")==0) return GroupType::FEC;
        else if (strcasecmp(name,"BUNDLE")==0) return GroupType::BUNDLE;
        else if (strcasecmp(name,"SIM")==0) return GroupType::SIM;
        return GroupType::UNKNOWN;
    }
    
    
    struct MediaGroup{
        std::string semantics;
        std::list<std::string> mids;
    };
    
    struct SSrcGroup{
        std::string semantics;
        std::list<uint32_t> ssrcs;
    };
    
    enum ProtoProfile {
        Unknown = -1,
        AVP =  0,
        SAVP,
        AVPF,
        SAVPF
    };
    
    static const char * GetProfileNameFor(ProtoProfile profile){
        switch (profile){
            case ProtoProfile::AVP:         return "AVP";
            case ProtoProfile::SAVP:        return "SAVP";
            case ProtoProfile::AVPF:        return "AVPF";
            case ProtoProfile::SAVPF:       return "SAVPF";
            default:                return "unknown";
        }
    }
    
    struct MediaDesc{
        //
        // REQUIRED fields
        //
        NMedia::Type type = NMedia::Unknown;
        int mlineIndex = -1;
        uint32_t port = 0;
        ProtoProfile protoProfile = ProtoProfile::Unknown;
        bool dtls = false;
        
        // common to session-level and media-level
        Direction direction = Direction::unknown;
        std::string iceUFrag;
        std::string icePwd;
        std::string fingerprintHashName;
        std::string fingerprintValue;
        NRTPHeaderExtension::Map extensions;
        
        // media-level only
        NRTPCodec::Map codecs;
        std::list<NRTPPayloadType> payloadTypes;
        std::string mid;
        bool rtcpmux = false;
        std::map<uint32_t, NSSrc> ssrcs;
        std::vector<SSrcGroup> ssrcGroups;
        std::list<std::string> candidates; // candidate start with "a="
        
        
        bool valid(){
            return this->type != NMedia::Unknown;
        }
        
        bool complete(const MediaDesc& default_media){
            if(!valid()){
                return false;
            }
            
            if(this->direction == Direction::unknown){
                if(default_media.direction != Direction::unknown){
                    this->direction = default_media.direction;
                }else{
                    // see https://tools.ietf.org/html/rfc3264
                    // "since sendrecv is the default"
                    this->direction = Direction::sendrecv;
                }
            }
            
            checkAssignString(this->iceUFrag, default_media.iceUFrag);
            checkAssignString(this->icePwd, default_media.icePwd);
            checkAssignString(this->fingerprintHashName, default_media.fingerprintHashName);
            checkAssignString(this->fingerprintValue, default_media.fingerprintValue);
            
            for(auto& o : default_media.extensions){
                auto it = this->extensions.find(o.first);
                if(it == this->extensions.end()){
                    extensions[o.first] = o.second;
                }
            }
            
            return true;
        }
        
        void dump(const std::string& prefix, std::list<std::string>& lines) const{
            std::ostringstream oss;
            
            oss << prefix << "mline[" << this->mlineIndex << "]=["
            << NMedia::GetNameFor(this->type)
            << "," << this->port
            << "," << GetProfileNameFor(protoProfile)
            << (this->dtls ? "/DTLS" : "")
            << "]" ;
            lines.emplace_back(oss.str());oss.str("");oss.clear();
            
            oss << prefix << "  direction=[" << GetDirectionNameFor(this->direction)  << "]" ;
            lines.emplace_back(oss.str());oss.str("");oss.clear();
            
            oss << prefix << "  rtcp-mux=[" << this->rtcpmux  << "]" ;
            lines.emplace_back(oss.str());oss.str("");oss.clear();
            
            if(!this->iceUFrag.empty()){
                oss << prefix << "  ice-ufrag=[" << this->iceUFrag  << "]" ;
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }

            if(!this->icePwd.empty()){
                oss << prefix << "  ice-pwd=[" << this->icePwd  << "]" ;
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
            
            if(!this->fingerprintHashName.empty()){
                oss << prefix << "  fingerprint=[" << this->fingerprintHashName  << "]-[" << this->fingerprintValue << "]";
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
            
            if(!this->mid.empty()){
                oss << prefix << "  mid=[" << this->mid  << "]" ;
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
            
            for(auto& o : extensions){
                NRTPHeaderExtension::Type typ = (NRTPHeaderExtension::Type) o.second;
                oss << prefix << "  ext[" << o.first  << "]=[" << NRTPHeaderExtension::GetNameFor(typ)<< "]";
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
            
            for(auto& pt : payloadTypes){
                auto it = codecs.find(pt);
                if(it != codecs.end()){
                    auto& codec = *it->second;
                    codec.dump(prefix+"  ", lines);
                }
            }
            
            for(auto& o : ssrcGroups){
                oss << prefix << "  ssrc-group=[" << o.semantics ;
                for(auto& ssrc : o.ssrcs){
                    oss << "," << ssrc ;
                }
                oss << "]" ;
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
            
            for(auto& o : ssrcs){
                oss << prefix << "  ssrc=[" << o.second.ssrc
                << "; " << o.second.cname
                << "; " << o.second.label
                << "; " << o.second.mslabel
                << "]" ;
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
            
            for(auto& o : candidates){
                oss << prefix << "  cand=[" << o << "]" ;
                lines.emplace_back(oss.str());oss.str("");oss.clear();
            }
        }
        
    private:
        static bool checkAssignString(std::string& str, const std::string& def){
            if(str.empty() && !def.empty()){
                str = def;
                return true;
            }else{
                return false;
            }
        }
    };
    
public:
    std::vector<MediaDesc> medias;
    std::vector<MediaGroup> groups;
    
public:
    void reset(){
        this->medias.clear();
        this->groups.clear();
    }
    
    int parse(const uint8_t* data,const size_t size, std::ostringstream& oss);
    
    MediaDesc * findMedia(const std::string& mid){
        for(auto& media : medias){
            if(media.mid == mid){
                return &media;
            }
        }
        return nullptr;
    }
    
    void dump(const std::string& prefix, std::list<std::string>& lines) const{
        
        lines.emplace_back(prefix + "SDP=");
        
        for(auto& o : groups){
            std::ostringstream oss;
            oss << prefix << "  group=[" << o.semantics ;
            for(auto& mid : o.mids){
                oss << "," << mid ;
            }
            oss << "]" ;
            lines.emplace_back(oss.str());oss.str("");oss.clear();
        }
        
        for(auto& o : medias){
            o.dump(prefix+"  ", lines);
        }
    }
}; // NSDP

class NRTP{
public:
    static const uint32_t MaxExtSeqNum = 0xFFFFFFFF;
    static const std::set<NCodec::Type>& supportedCodecs(){
        static std::set<NCodec::Type> codecs;
        if(codecs.size() == 0){
            codecs.emplace(NCodec::OPUS);
            codecs.emplace(NCodec::PCMU);
            codecs.emplace(NCodec::VP8);
            codecs.emplace(NCodec::RTX);
            codecs.emplace(NCodec::RED);
            codecs.emplace(NCodec::ULPFEC);
        }
        return codecs;
    }
    static bool support(NCodec::Type type){
        const std::set<NCodec::Type>& codecs = supportedCodecs();
        return codecs.find(type) != codecs.end();
    }
};

template <
class VType
, class DType
, class EType>
class RingNumber{
private:
    VType value_;
    
public:
    RingNumber() = delete;
    RingNumber(int) = delete;
    
    RingNumber(VType v) : value_(v){}
    RingNumber(unsigned v) : value_((VType)v){}
    
    VType value() const { return value_;}
    
    //    RingNumber& operator=(RingNumber other) default;
    RingNumber operator+(EType dist) const {
        VType v = value_ + (DType)dist;
        return RingNumber(v);
    }
    
    bool operator==(const RingNumber& other) const {
        return value_ == other.value_;
    }
    
    bool operator!=(const RingNumber& other) const {
        return value_ != other.value_;
    }
    
    bool operator <(const RingNumber& other) const {
        return distance(other) > 0;
    }
    
    bool operator >(const RingNumber& other) const {
        return distance(other) < 0;
    }
    
    RingNumber operator-(EType dist) const {
        VType v = value_ - (DType)dist;
        return RingNumber(v);
    }
    
    EType distance(const RingNumber& other) const {
        return (DType)(other.value_ - value_);
    }
    
    // self=100, dist=10, [100, 110] -> true
    // self=100, dist=-10, [90, 100] -> true
    bool withinDistance(const RingNumber& other, EType dist) const{
        DType dshort = (DType) dist;
        EType realdist = other.distance(*this);
        if(dshort >= 0){
            return realdist >= 0 && realdist <= dshort;
        }else{
            return realdist >= dshort && realdist <= 0;
        }
    }
    
    bool withinRange(const RingNumber& low, const RingNumber& high) const {
        DType dist = (DType) low.distance(high);
        return dist >= 0 && withinDistance(low, dist);
    }
    
    bool isReset(const RingNumber& next) const {
        DType dist = (DType) distance(next);
        return outOfOrder(dist);
    }
    
};


typedef uint16_t RTPSeqType;
typedef int16_t RTPSeqDistance;
static const RTPSeqDistance RTPSEQ_DISTANCE_MAX = INT16_MAX;
static const RTPSeqDistance RTPSEQ_DISTANCE_MIN = INT16_MIN;
static const RTPSeqDistance RTPSEQ_REST_DISTANCE_HIGH = 10000;
static const RTPSeqDistance RTPSEQ_REST_DISTANCE_LOW = -10000;

typedef RingNumber < RTPSeqType
, RTPSeqDistance
, RTPSeqDistance> NRTPSeq;

//class NRTPSeq : public RingNumber < RTPSeqType
//, RTPSeqDistance
//, RTPSeqDistance> {
//public:
//
//    NRTPSeq(unsigned v) : NRTPSeq((RTPSeqType)v){}
//
//    NRTPSeq(RTPSeqType v):RingNumber< RTPSeqType
//    , RTPSeqDistance
//    , RTPSeqDistance>(v){
//
//    }
//    bool operator <(const RingNumber& other) const {
//        return distance(other) > 0;
//    }
//};

struct NRTPSeqComparator{
    bool operator()(const NRTPSeq& lhs, const NRTPSeq& rhs) const{
        return lhs.distance(rhs) > 0;
    }
};




typedef std::map<NRTPPayloadType, NRTPCodec::shared> NRTPMapType;
typedef NRTPMapType NRTPMap;


class NRTPMedia{
public:
    using shared = std::shared_ptr<NRTPMedia>;
    
public:
    NMedia::Type type = NMedia::Unknown;
    int mlineIndex = -1;
    NSDP::ProtoProfile protoProfile = NSDP::ProtoProfile::Unknown;
    NSDP::Direction direction = NSDP::Direction::unknown;
    NRTPHeaderExtension::Map extensions;
    NRTPMapType rtpmaps;
    
    static shared makeWith(const NSDP::MediaDesc& desc){
        shared mediap = std::make_shared<NRTPMedia>();
        NRTPMedia media = *mediap;
        media.type = desc.type;
        media.mlineIndex = desc.mlineIndex;
        media.protoProfile = desc.protoProfile;
        media.direction = desc.direction;
        media.extensions = desc.extensions;
        
//        for(auto& o : desc.codecs){
//            auto& src = o.second;
//
//            // only process known codec
//            if(src.type == NCodec::UNKNOWN || !NRTP::support(src.type)){
//                continue;
//            }
//
//            NRTPCodec::shared codec = NRTPCodec::makeWith(src);
//            auto ret_pr = media.rtpmaps.insert(std::make_pair(codec->pt(), codec));
//            if(!ret_pr.second){
//                continue; // ignore dup pt
//            }
//        }
        
        return mediap;
    }
};




class NRTPHeader : public NObjDumper::Dumpable{
public:
    NRTPHeader() = default;
    
    size_t Parse(const uint8_t* data,const size_t size);
    size_t Serialize(uint8_t* data,const size_t size) const;
    //size_t GetSize() const;
    void Dump(const std::string& prefix, std::list<std::string>& lines) const;
    std::string Dump(const std::string& prefix) const;
    virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
        dumper.objB();
        dumpFields(dumper);
        dumper.objE();
        return dumper;
    }
    
    inline NObjDumper& dumpFields(NObjDumper& dumper) const{
        return dumper
        .kv("v", version)
        .kv("p", (int)padding)
        .kv("x", (int)extension)
        .kv("cc", (unsigned)cc)
        .kv("m", (int)mark)
        .kv("pt", payloadType)
        .kv("seq", sequenceNumber)
        .kv("ts", timestamp)
        .kv("ssrc", ssrc);
    }
    
public:
    uint8_t     version         = 2;
    uint8_t     cc              = 0;
    bool        padding         = false;
    bool        extension       = false;
    bool        mark            = false;
    uint16_t    payloadType     = 0;
    uint16_t    sequenceNumber  = 0;
    uint32_t    timestamp       = 0;
    uint32_t    ssrc            = 0;
    
    //
};

class NRTPData : public NObjDumper::Dumpable{
public:
    
    struct REDBlock{
        uint8_t pt;
        uint16_t tsOffset;
        const uint8_t * data;
        uint16_t length;
    };
    
public:
    NCodec::Type            codecType = NCodec::UNKNOWN;
    int64_t                 timeMS = 0;
    NRTPHeader              header;
    std::vector<uint32_t>   csrcs;
    const uint8_t *         data = NULL;
    size_t                  size = 0;
    const uint8_t *         extPtr = NULL;
    uint16_t                extLength = 0;
    const uint8_t *         payloadPtr = nullptr;
    size_t                  payloadLen = 0;
    
    std::string Dump(const std::string& prefix) const{
        std::ostringstream oss;
        oss << prefix << header.Dump("")
        << "-" << "[pylen=" << payloadLen
        << ",extlen=" << extLength << "]";
        return oss.str();
    }
    
    virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
        dumper.objB();
        dumpFields(dumper);
        dumper.objE();
        return dumper;
    }
    
    inline NObjDumper& dumpFields(NObjDumper& dumper) const{
        header.dumpFields(dumper);
        dumper.kv("elen", extLength);
        dumper.kv("plen", payloadLen);
        dumper.kv("codec", NCodec::GetNameFor(codecType));
        return dumper;
    }
    
    const uint8_t * getExtData() const{
        return extPtr;
    }
    
    size_t getHeadSize() const{
        return  12 + (csrcs.size() * 4) + extLength;
    }
    
    size_t serialize(uint8_t* datap, const size_t size, NRTPData& other) const;
    
    size_t serialize(uint8_t* data,const size_t size) const ;
    
    size_t parse(const uint8_t* data, size_t size, bool remove_padding = true);
    
    void setPayload(const uint8_t * ptr, size_t length){
        payloadPtr = ptr;
        payloadLen = length;
    }
    
    int removePadding();
    
    size_t RecoverOSN(NRTPPayloadType pltype);
    
    int demuxRED(std::vector<REDBlock>& blocks) const;
    
    int demuxRED(const std::function<void(NRTPData& rtpd)> &func) const;
    
    int demuxRED(const std::function<void(REDBlock& block)> &func) const;
};


#endif /* NRTPMAP_H */


