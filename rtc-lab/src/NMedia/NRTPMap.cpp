


#include "NRTPMap.hpp"

enum NRTPError{
    NRTP_ERROR_BASE = -1000,
    NRTP_ERROR_DATA_TOO_SHORT =     NRTP_ERROR_BASE - 1,
    NRTP_ERROR_FORMAT =             NRTP_ERROR_BASE - 2,
    NRTP_ERROR_UNKNOWN_MEDIA =      NRTP_ERROR_BASE - 3,
};


static
int parse_mline(NLineParser& line
                , NSDP::MediaDesc& media
                , std::ostringstream& oss){
    
    // m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13

    //auto& codecs = media.codecs;
    
    int ret = 0;
    do{

        std::string media_name;
        std::string proto;
        if(!line.nextWord(media_name)
           || !line.nextU32(media.port)
           || !line.nextWord(proto)){
            ret = NRTP_ERROR_FORMAT;
            oss << "SDP: wrong format of mline";
            break;
        }
        
        media.type = NMedia::GetMediaFor(media_name.c_str());
        
        NLineParser protoline(proto);
        std::string str;
        bool rtp = false;
        bool udp = false;
        bool tls = false;
        while(protoline.nextWord(str, '/')){
            if(str == "RTP"){
                rtp = true;
            }else if(str == "AVP"){
                media.protoProfile = NSDP::ProtoProfile::AVP;
            }else if(str == "SAVP"){
                media.protoProfile = NSDP::ProtoProfile::SAVP;
            }else if(str == "AVPF"){
                media.protoProfile = NSDP::ProtoProfile::AVPF;
            }else if(str == "SAVPF"){
                media.protoProfile = NSDP::ProtoProfile::SAVPF;
            }else if(str == "UDP"){
                udp = true;
            }else if(str == "TLS"){
                tls = true;
            }else {
                ret = NRTP_ERROR_FORMAT;
                oss << "SDP: unknown proto profile in mline " << media.mlineIndex;
                break;
            }
        }
        if(ret < 0) break;
        if(!rtp){
            ret = NRTP_ERROR_FORMAT;
            oss << "SDP: unsupport non-rtp profile in mline " << media.mlineIndex;
            break;
        }
        if(media.protoProfile == NSDP::ProtoProfile::Unknown){
            ret = NRTP_ERROR_FORMAT;
            oss << "SDP: no profile in mline " << media.mlineIndex;
            break;
        }
        
        if(udp && tls){
            media.dtls = true;
        }
        
        uint32_t codec_pt = 0;
        while(line.nextU32(codec_pt)){
            media.payloadTypes.emplace_back(codec_pt);
            ret = 0;
        }
        // TODO: aaa
    }while(0);
    return ret;
}

static
bool check_read(NLineParser& line
               , const std::string& prefix
               , std::string& str
               , int& ret
               , std::ostringstream& oss){
    ret = 0;
    if(line.nextWith(prefix)){
        if(!line.nextWord(str)){
            oss << "SDP: wrong format of " << prefix;
            ret = NRTP_ERROR_FORMAT;
        }
        return true;
    }
    return false;
}

static
bool check_read(NLineParser& line
               , const std::string& prefix
               , std::string& str1
               , std::string& str2
               , int& ret
               , std::ostringstream& oss){
    if(!check_read(line, prefix, str1, ret, oss)){
        return false;
    }
    if(ret == 0){
        if(!line.nextWord(str2)){
            oss << "SDP: wrong format of " << prefix;
            ret =  NRTP_ERROR_FORMAT;
        }
    }
    return true;
}


// const static std::string STR_AUDIO = "audio";
// const static std::string STR_VIDEO = "video";
// const static std::string STR_APPLICATION = "application";
//const static std::string STR_BUNDLE_PRE = "group:BUNDLE";
const static std::string STR_GROUP_PRE = "group:";
const static std::string STR_CANDIDATE_PRE = "candidate:";
const static std::string STR_MID_PRE = "mid:";
const static std::string STR_ICEUFRAG_PRE = "ice-ufrag:";
const static std::string STR_ICEPWD_PRE = "ice-pwd:";
const static std::string STR_FINGERPRINT_PRE = "fingerprint:";
const static std::string STR_RTCPMUX_PRE = "rtcp-mux";
const static std::string STR_SSRCGROUP_PRE = "ssrc-group:";
const static std::string STR_SSRC_PRE = "ssrc:";
const static std::string STR_RTPMAP_PRE = "rtpmap:";
const static std::string STR_FMTP_PRE = "fmtp:";
const static std::string STR_RTCPFB_PRE = "rtcp-fb:";
const static std::string STR_EXTMAP_PRE = "extmap:";


int NSDP::parse(const uint8_t* data,const size_t size, std::ostringstream& oss){
    return parse((const char*)data, size, oss);
}

// see https://tools.ietf.org/id/draft-ietf-rtcweb-sdp-08.html
// see https://webrtchacks.com/sdp-anatomy/
int NSDP::parse(const char* data,const size_t size, std::ostringstream& oss){
    int ret = -1;

    do{
        if(size <= 3){
            ret = NRTP_ERROR_DATA_TOO_SHORT;
            oss << "SDP: data length is too short";
            break;
        }
        
        if(data[0] != 'v' || data[1] != '=' || data[2] != '0'){
            ret = NRTP_ERROR_FORMAT;
            oss << "SDP: error magic words";
            break;
        }
        
        this->reset();
        MediaDesc default_media;
        MediaDesc * current_media = &default_media;

        int mlineindex = 0;
        LinesBuffer lines(data, data+size);
        ret = 0;
        
        while(lines.next()){
            NLineParser& line = lines.line();
            
            oss.str("line ");
            oss << lines.lineNum() << ":";
            
            char field = 0;
            char equ_char = 0;
            if(!line.nextChar(field)){
                // empty line ?
                continue;
            }
            
            if(!line.nextChar(equ_char)){
                ret = NRTP_ERROR_FORMAT;
                oss << "SDP: line length is too short";
                break;
            }
            
            if(equ_char != '='){
                ret = NRTP_ERROR_FORMAT;
                oss << "SDP: second char is NOT =";
                break;
            }
            
            // printf("line %02zu: %.*s\n", lines.lineNum(), (int)line.length(), line.begin());
            
            if(field == 'm'){
                // m=audio 9 UDP/TLS/RTP/SAVPF 111 103
                // m=application 0 DTLS/SCTP 0
                
                // TODO: add application support
                
                this->medias.emplace_back(MediaDesc());
                current_media = &medias.back();
                current_media->mlineIndex = mlineindex;
                ret = parse_mline(line , *current_media, oss);
                if(ret){
                    break;
                }
                mlineindex++;
                
            }else if(field == 'a'){
                
                if(line.nextWith(STR_GROUP_PRE)){
                    // a=group:BUNDLE audio video
                    if(mlineindex > 0){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: NOT in session section of " << STR_GROUP_PRE;
                        break;
                    }
                    
                    this->groups.emplace_back(MediaGroup());
                    MediaGroup &group = this->groups.back();
                    if(!line.nextWord(group.semantics)){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: wrong format of " << STR_GROUP_PRE;
                        break;
                    }
                    
                    std::string str;
                    while(line.nextWord(str)){
                        group.mids.emplace_back(str);
                    }
                    if(group.mids.size() == 0){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: wrong format of " << STR_GROUP_PRE;
                        break;
                    }
                    
                }else if(line.nextWith(STR_CANDIDATE_PRE)){
                    // a=candidate:556447776 1 udp 2122260223 172.17.2.253 53023 typ host generation 0
                    current_media->candidates.emplace_back(line.begin());
                    
                }else if(check_read(line, STR_MID_PRE, current_media->mid, ret, oss)){
                    if(ret < 0) break;
                    
                }else if(check_read(line, STR_ICEUFRAG_PRE, current_media->iceUFrag, ret, oss)){
                    if(ret < 0) break;
                    
                }else if(check_read(line, STR_ICEPWD_PRE, current_media->icePwd, ret, oss)){
                    if(ret < 0) break;
                    
                }else if(check_read(line
                                    , STR_FINGERPRINT_PRE
                                    , current_media->fingerprintHashName
                                    , current_media->fingerprintValue
                                    , ret
                                    , oss)){
                    // a=fingerprint:sha-256 76:40:CD:3B:48:A0:E1:36:20:D2:7C:98:17:0B:26:C6:3D:10:6B:C1:99:59:7F:BD:BF:84:70:9B:41:55:D4:96
                    if(ret < 0) break;
                    
                }else if(line.nextWith(STR_RTCPMUX_PRE)){
                    // a=rtcp-mux
                    current_media->rtcpmux = true;
                    
                }else if(line.nextWith(STR_SSRCGROUP_PRE)){
                    /*
                     *  a=ssrc-group:FID 586466331 2053167359 (media rtx)
                     *  see https://tools.ietf.org/html/rfc3388#section-7
                     */
                    
                    current_media->ssrcGroups.emplace_back(SSrcGroup());
                    SSrcGroup &group = current_media->ssrcGroups.back();
                    if(!line.nextWord(group.semantics)){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: wrong format of " << STR_SSRCGROUP_PRE;
                        break;
                    }
                    uint32_t ssrc = 0;
                    while(line.nextU32(ssrc)){
                        group.ssrcs.emplace_back(ssrc);
                    }
                    if(group.ssrcs.size() == 0){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: wrong format of " << STR_SSRCGROUP_PRE;
                        break;
                    }
                    
                }else if(line.nextWith(STR_SSRC_PRE)){
                    // a=ssrc:1126623721 cname:6R/KsKYHkXfK70rL
                    // a=ssrc:1126623721 msid:m111 L222
                    // a=ssrc:1126623721 mslabel:m111
                    // a=ssrc:1126623721 label:L222
                    uint32_t ssrc = 0;
                    if(!line.nextU32(ssrc)){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: wrong format of " << STR_SSRC_PRE;
                        break;
                    }
                    
                    auto it = current_media->ssrcs.find(ssrc);
                    if(it == current_media->ssrcs.end()){
                        current_media->ssrcs[ssrc] = NSSrc(ssrc);
                        it = current_media->ssrcs.find(ssrc);
                    }
                    
                    bool ok = true;
                    if(line.nextWordWith("cname:")){
                        ok = line.nextWord(it->second.cname);
                        
                    }else if(line.nextWordWith("msid:")){
                        ok = (line.nextWord(it->second.mslabel) && line.nextWord(it->second.label));
                        
                    }else if(line.nextWordWith("mslabel:")){
                        ok = line.nextWord(it->second.mslabel);
                    }else if(line.nextWordWith("label:")){
                        ok = line.nextWord(it->second.label);
                    }
                    if(!ok){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: wrong format of " << STR_SSRC_PRE;
                        break;
                    }
                    
                }else if(line.nextWith(STR_RTPMAP_PRE)){
                    // a=rtpmap:111 opus/48000/2
                    // a=rtpmap:100 VP8/90000
                    static const char delimiter = '/';
                    
                    uint32_t codec_pt = 0;
                    std::string codec_name;
                    if(!line.nextU32(codec_pt) || !line.nextWord(codec_name, delimiter)){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: wrong format of " << STR_RTPMAP_PRE;
                        break;
                    }
                    
                    //NCodec:: Type codec_type = NCodec::GetCodecForName(codec_name.c_str());
                    auto ret_pr = current_media->codecs.insert(std::make_pair(codec_pt, NRTPCodec::makeWith(codec_name)));
                    if(!ret_pr.second){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: duplicate pt in mline " << current_media->mlineIndex;
                        break;
                    }
                    NRTPCodec& codec = *ret_pr.first->second;
                    codec.pt = codec_pt;
                    codec.name = codec_name;
                    if(line.nextU32(codec.clockrate, delimiter)
                       && line.nextU32(codec.channels, delimiter)){
                    }
                    
                    // TODO: set default codec parameters (name, samplerate, channels) for the pt
                    
                    
//                    auto it = current_media->codecs.find(codec_pt);
//                    if(it != current_media->codecs.end()){
//                        it->second.name = codec_name;
//                        it->second.type =  NCodec::GetCodecForName(codec_name.c_str());
//                        if(line.nextU32(it->second.clockrate, delimiter)
//                           && line.nextU32(it->second.channels, delimiter)){
//                        }
//                    }
                    
                } else if (line.nextWith(STR_FMTP_PRE)) {
                    // a=fmtp:97 apt=96
                    // a=fmtp:101 0-15
                    // a=fmtp:100 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
                    
                    uint32_t pt = 0;
                    if(!line.nextU32(pt)){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: wrong format of " << STR_FMTP_PRE;
                        break;
                    }
                    
                    auto it = current_media->codecs.find(pt);
                    if(it == current_media->codecs.end()){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: unknown pt " << pt << " of " << STR_FMTP_PRE;
                        break;
                    }
                    
                    std::string fmtp;
                    if(!line.nextWord2End(fmtp)){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: empty " << STR_FMTP_PRE << " of pt " << pt;
                        break;
                    }
                    
                    auto& codec = *it->second;
                    if(!codec.parseFMTP(fmtp)){
                        codec.fmtps.emplace_back(fmtp);
                    }
                    
                } else if (line.nextWith(STR_RTCPFB_PRE)) {
                    // a=rtcp-fb:96 nack
                    // a=rtcp-fb:96 nack pli
                    
                    uint32_t pt = 0;
                    if(!line.nextU32(pt)){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: wrong format of " << STR_RTCPFB_PRE;
                        break;
                    }
                    
                    auto it = current_media->codecs.find(pt);
                    if(it == current_media->codecs.end()){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: unknown pt " << pt << " of " << STR_RTCPFB_PRE;
                        break;
                    }
                    auto& codec = *it->second;
                    
                    std::string param;
                    if(!line.nextWord2End(param)){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: empty " << STR_RTCPFB_PRE << " of pt " << pt;
                        break;
                    }
                    
                    if(!codec.rtcpfb.setStr(param)){
                        it->second->rtcpfbs.insert(param);
                    }
                    
                } else if (line.nextWith("sendrecv")) {
                    // a=sendrecv
                    current_media->direction = Direction::sendrecv;
                    
                } else if (line.nextWith("sendonly")) {
                    current_media->direction = Direction::sendonly;
                    
                } else if (line.nextWith("recvonly")) {
                    current_media->direction = Direction::recvonly;
                    
                } else if (line.nextWith("inactive")) {
                    current_media->direction = Direction::inactive;
                    
                } else if (line.nextWith(STR_EXTMAP_PRE)) {
                    
                    // a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
                    // a=extmap:2 urn:ietf:params:rtp-hdrext:toffset
                    
                    uint32_t extid = 0;
                    std::string uri;
                    if(!line.nextU32(extid) || !line.nextWord(uri)){
                        ret = NRTP_ERROR_FORMAT;
                        oss << "SDP: wrong format of " << STR_EXTMAP_PRE;
                    }
                    
                    auto ext_type = NRTPHeaderExtension::GetExtensionForName(uri.c_str());
                    if(ext_type != NRTPHeaderExtension::UNKNOWN){
                        current_media->extensions[extid] = ext_type;
                    }
                }
                if(ret < 0) break;

            } // end of a
        } // end of for
        if(ret < 0) break;
        
        for(auto& media : this->medias){
            media.complete(default_media);
        }
        
        ret = 0;
    }while(0);

    return ret;
}

//NRTPCodec::shared NRTPCodec::makeWith(NCodec::Type type){
//    if(type == NCodec::RTX){
//        return std::make_shared<NRTPCodecRTX>();
//    }else{
//        return std::make_shared<NRTPCodec>(type);
//    }
//}

NRTPCodec::shared NRTPCodec::makeWith(const std::string& codec_name){
    NCodec:: Type codec_type = NCodec::GetCodecForName(codec_name.c_str());
    if(codec_type == NCodec::RTX){
        return std::make_shared<NRTPCodecRTX>();
    }else{
        return std::make_shared<NRTPCodec>(codec_type);
    }
}








class BitReader{
public:
    BitReader(const uint8_t *data,const uint32_t size)
    {
        //Store
        buffer = data;
        bufferLen = size;
        //nothing in the cache
        cached = 0;
        cache = 0;
        bufferPos = 0;
        //No error
        error = false;
    }
    inline uint32_t Get(uint32_t n)
    {
        uint32_t ret = 0;
        if (n>cached)
        {
            //What we have to read next
            uint8_t a = n-cached;
            //Get remaining in the cache
            ret = cache >> (32-n);
            //Cache next
            Cache();
            //Get the remaining
            ret =  ret | GetCached(a);
        } else
            ret = GetCached(n);
        //Debug("Readed %d:\n",n);
        //BitDump(ret,n);
        return ret;
    }
    
    inline bool Check(int n,uint32_t val)
    {
        return Get(n)==val;
    }
    
    inline void Skip(uint32_t n)
    {
        if (n>cached)
        {
            //Get what is left to skip
            uint8_t a = n-cached;
            //Cache next
            Cache();
            //Skip cache
            SkipCached(a);
        } else
            //Skip cache
            SkipCached(n);
    }
    
    inline uint64_t Left()
    {
        return cached+bufferLen*8;
    }
    
    inline uint32_t Peek(uint32_t n)
    {
        uint32_t ret = 0;
        if (n>cached)
        {
            //What we have to read next
            uint8_t a = n-cached;
            //Get remaining in the cache
            ret = cache >> (32-n);
            //Get the remaining
            ret =  ret | (PeekNextCached() >> (32-a));
        } else
            ret = cache >> (32-n);
        //Debug("Peeked %d:\n",n);
        //BitDump(ret,n);
        return ret;
    }
    
    inline uint32_t GetPos()
    {
        return bufferPos*8-cached;
    }
public:
    inline uint32_t Cache()
    {
        //Check left buffer
        if (bufferLen>3)
        {
            //Update cache
            cache = NUtil::get4(buffer,0);
            //Update bit count
            cached = 32;
            //Increase pointer
            buffer += 4;
            bufferPos += 4;
            //Decrease length
            bufferLen -= 4;
        } else if(bufferLen==3) {
            //Update cache
            cache = NUtil::get3(buffer,0)<<8;
            //Update bit count
            cached = 24;
            //Increase pointer
            buffer += 3;
            bufferPos += 3;
            //Decrease length
            bufferLen -= 3;
        } else if (bufferLen==2) {
            //Update cache
            cache = NUtil::get2(buffer,0)<<16;
            //Update bit count
            cached = 16;
            //Increase pointer
            buffer += 2;
            bufferPos += 2;
            //Decrease length
            bufferLen -= 2;
        } else if (bufferLen==1) {
            //Update cache
            cache  = NUtil::get1(buffer,0)<<24;
            //Update bit count
            cached = 8;
            //Increase pointer
            buffer++;
            bufferPos++;
            //Decrease length
            bufferLen--;
        } else {
            //We can't use exceptions so set error flag
            error = true;
            //Exit
            return 0;
        }
        
        
        //Debug("Reading int cache");
        //BitDump(cache,cached);
        
        //return number of bits
        return cached;
    }
    
    inline uint32_t PeekNextCached()
    {
        //Check left buffer
        if (bufferLen>3)
        {
            //return  cached
            return NUtil::get4(buffer,0);
        } else if(bufferLen==3) {
            //return  cached
            return NUtil::get3(buffer,0)<<8;
        } else if (bufferLen==2) {
            //return  cached
            return NUtil::get2(buffer,0)<<16;
        } else if (bufferLen==1) {
            //return  cached
            return NUtil::get1(buffer,0)<<24;
        } else {
            //We can't use exceptions so set error flag
            error = true;
            //Exit
            return 0;
        }
    }
    
    
    inline void SkipCached(uint32_t n)
    {
        //Move
        cache = cache << n;
        //Update cached bytes
        cached -= n;
        
    }
    
    inline uint32_t GetCached(uint32_t n)
    {
        //Get bits
        uint32_t ret = cache >> (32-n);
        //Skip thos bits
        SkipCached(n);
        //Return bits
        return ret;
    }
    
    inline bool Error()
    {
        //We won't use exceptions, so we need to signal errors somehow
        return error;
    }
    
private:
    const uint8_t* buffer;
    uint32_t bufferLen;
    uint32_t bufferPos;
    uint32_t cache;
    uint8_t  cached;
    bool  error;
};

class BitWritter{
public:
    BitWritter(uint8_t *data,uint32_t size)
    {
        //Store pointers
        this->data = data;
        this->size = size;
        //And reset to init values
        Reset();
    }
    
    inline void Reset()
    {
        //Init
        buffer = data;
        bufferLen = 0;
        bufferSize = size;
        //nothing in the cache
        cached = 0;
        cache = 0;
        //No error
        error = false;
    }
    
    inline uint32_t Flush()
    {
        Align();
        FlushCache();
        return bufferLen;
    }
    
    inline void FlushCache()
    {
        //Check if we have already finished
        if (!cached)
            //exit
            return;
        //Check size
        if (cached>bufferSize*8)
        {
            //We can't use exceptions so set error flag
            error = true;
            //Exit
            return;
        }
        //Debug("Flushing  cache");
        //BitDump(cache,cached);
        if (cached==32)
        {
            //Set data
            NUtil::set4(buffer,0,cache);
            //Increase pointers
            bufferSize-=4;
            buffer+=4;
            bufferLen+=4;
        } else if (cached==24) {
            //Set data
            NUtil::set3(buffer,0,cache);
            //Increase pointers
            bufferSize-=3;
            buffer+=3;
            bufferLen+=3;
        }else if (cached==16) {
            NUtil::set2(buffer,0,cache);
            //Increase pointers
            bufferSize-=2;
            buffer+=2;
            bufferLen+=2;
        }else if (cached==8) {
            NUtil::set1(buffer,0,cache);
            //Increase pointers
            --bufferSize;
            ++buffer;
            ++bufferLen;
        }
        //Nothing cached
        cached = 0;
    }
    
    inline void Align()
    {
        if (cached%8==0)
            return;
        
        if (cached>24)
            Put(32-cached,0);
        else if (cached>16)
            Put(24-cached,0);
        else if (cached>8)
            Put(16-cached,0);
        else
            Put(8-cached,0);
    }
    
    inline uint32_t Put(uint8_t n,uint32_t v)
    {
        if (n+cached>32)
        {
            uint8_t a = 32-cached;
            uint8_t b =  n-a;
            //Add to cache
            cache = (cache << a) | ((v>>b) & (0xFFFFFFFF>>cached));
            //Set cached bytes
            cached = 32;
            //Flush into memory
            FlushCache();
            //Set new cache
            cache = v & (0xFFFFFFFF>>(32-b));
            //Increase cached
            cached = b;
        } else {
            //Add to cache
            cache = (cache << n) | (v & (0xFFFFFFFF>>(32-n)));
            //Increase cached
            cached += n;
        }
        //If it is last
        if (cached==bufferSize*8)
            //Flush it
            FlushCache();
        
        return v;
    }
    
    inline uint32_t Put(uint8_t n,BitReader &reader)
    {
        return Put(n,reader.Get(n));
    }
    
    inline bool Error()
    {
        //We won't use exceptions, so we need to signal errors somehow
        return error;
    }
private:
    uint8_t* data;
    uint32_t size;
    uint8_t* buffer;
    uint32_t bufferLen;
    uint32_t bufferSize;
    uint32_t cache;
    uint8_t  cached;
    bool  error;
};


/*
 
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           timestamp                           |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |           synchronization source (SSRC) identifier            |
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 |            contributing source (CSRC) identifiers             |
 |                             ....                              |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 
 
 
 */
size_t NRTPHeader::Parse(const uint8_t* data,const size_t size){
    //Ensure minumim size
    if (size<12)
        return 0;
    
    //Bite reader
    BitReader r(data,2);
    
    //If not an rtp packet
    if (r.Peek(2)!=2)
        return 0;
    
    //Get data
    version        = r.Get(2);
    padding        = r.Get(1);
    extension    = r.Get(1);
    cc        = r.Get(4);
    mark        = r.Get(1);
    payloadType    = r.Get(7);
    
    //Get sequence number
    sequenceNumber = NUtil::get2(data,2);
    
    //Get timestamp
    timestamp = NUtil::get4(data,4);
    
    //Get ssrc
    ssrc = NUtil::get4(data,8);
    
    return 12;
    
//    // Ensure size
//    if (size < (12+cc*4)){
//        //Error
//        return 0;
//    }
//
//    //Get all csrcs
//    for (int i=0;i<cc;++i)
//        //Get them
//        csrcs.emplace_back(NUtil::get4(data,12+i*4));
//
//    //Return size
//    return 12+csrcs.size()*4;
}

size_t NRTPHeader::Serialize(uint8_t* data, const size_t size) const
{
    //Check size
    if(size < 12){
        return 0;
    }
//    if (size < GetSize())
//        //Error
//        return 0;
    
    //Bite writter
    BitWritter w(data,2);
    
    //Set data
    w.Put(2,version);
    w.Put(1,padding);
    w.Put(1,extension);
    w.Put(4,(uint32_t)cc);
    w.Put(1,mark);
    w.Put(7,payloadType);
    
    //set sequence number
    NUtil::set2(data,2,sequenceNumber);
    
    //Get sequence number
    NUtil::set4(data,4,timestamp);
    
    //Get ssrc
    NUtil::set4(data,8,ssrc);
    
    return 12;
    
//    int len=12;
//    //Get all csrcs
//    for (auto it=csrcs.begin();it!=csrcs.end();++it)
//    {
//        //Get them
//        NUtil::set4(data,len,*it);
//        //Increas len
//        len += 4;
//    }
//
//    //Return size
//    return len;
    
}

//size_t NRTPHeader::GetSize() const{
//    return 12+cc*4;
//}

void NRTPHeader::Dump(const std::string& prefix, std::list<std::string>& lines) const{
    lines.emplace_back(Dump(prefix));
}

std::string NRTPHeader::Dump(const std::string& prefix) const{
    std::ostringstream oss;
    oss << prefix << "[v=" << (unsigned)version
    << " p=" << padding
    << " x=" << extension
    << " cc=" << (unsigned)cc
    << " m=" << mark
    << " pt=" << std::setw(3) << payloadType
    << " seq=" << sequenceNumber
    << " ts=" << timestamp
    << " ssrc=" << ssrc
    << "]";
    return oss.str();
}



size_t WriteHeaderIdAndLength(uint8_t* data, uint32_t pos, uint8_t id, size_t length)
{
    //Check id is valid
//    if (id==RTPMap::NotFound)
//        return 0;
    //Check size
    if (!length || (length-1)>0x0f)
        return 0;
    
    //Set id && length
    data[pos] = id << 4 | (length-1);
    
    //OK
    return 1;
}

/*
 https://tools.ietf.org/html/rfc5285
 
 0             1             2             3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |     0xBE    |    0xDE     |         length=3        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  ID   | L=0   |     data    |  ID   |  L=1  |   data...
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 ...data   |    0 (pad)    |    0 (pad)    |  ID   | L=3   |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                  data                       |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */
uint32_t NRTPHeaderExtension::Parse(const Map &extMap,const uint8_t* data,const size_t size){
    
    //If not enought size for header
    if (size<4)
        //ERROR
        return 0;
    
    //Get magic header
    uint16_t magic = NUtil::get2(data,0);
    
    //Ensure it is magical
    if (magic!=0xBEDE){ // 48862
        //ERROR
        //return Error("Magic cookie not found");
        return 0;
    }
    
    
    //Get length
    uint16_t length = NUtil::get2(data,2)*4;
    
    //Ensure we have enought
    if (size<length+4){
        //ERROR
        //return Error("Not enought data");
        return 0;
    }

    
    //Loop
    uint16_t i = 0;
    const uint8_t* ext = data+4;
    
    //::Dump(ext,length);
    
    //Read all
    while (i<length)
    {
        //Get header
        const uint8_t header = ext[i++];
        //If it is padding
        if (!header)
            //skip
            continue;
        //Get extension element id
        uint8_t id = header >> 4;
        //Get extenion element length
        uint8_t len = (header & 0x0F) + 1;
        //Get mapped extension
        uint8_t t = extMap.getFor(id);
        //Debug("-RTPExtension [type:%d,codec:%d,len:%d]\n",id,t,len);
        this->exist = true;
        //Check type
        switch (t)
        {
            case SSRCAudioLevel:
                // The payload of the audio level header extension element can be
                // encoded using either the one-byte or two-byte
                // 0             1
                //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |  ID   | len=0 |V| level     |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // 0             1             2             3
                //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |  ID   | len=1 |V|   level     |    0x00     |    0x00     |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-s+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //
                // Set extennsion
                hasAudioLevel    = true;
                vad        = (ext[i] & 0x80) >> 7;
                level        = (ext[i] & 0x7f);
                break;
            case TimeOffset:
                //  0             1             2             3
                //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |  ID   | len=2 |          transmission offset          |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //
                // Set extension
                hasTimeOffset = true;
                timeOffset = NUtil::get3(ext,i);
                //Check if it is negative
                if (timeOffset & 0x800000)
                    // Negative offset
                    timeOffset = -(timeOffset & 0x7FFFFF);
                break;
            case AbsoluteSendTime:
                //  0             1             2             3
                //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |  ID   | len=2 |          absolute send time           |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
                // Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
                // Set extension
                hasAbsSentTime = true;
                absSentTime = ((uint64_t)NUtil::get3(ext,i))*1000 >> 18;
                break;
            case CoordinationOfVideoOrientation:
                // Bit#        7   6   5   4   3   2   1  0(LSB)
                // Definition    0   0   0   0   C   F   R1 R0
                // With the following definitions:
                // C = Camera: indicates the direction of the camera used for this video stream. It can be used by the MTSI client in receiver to e.g. display the received video differently depending on the source camera.
                //     0: Front-facing camera, facing the user. If camera direction is unknown by the sending MTSI client in the terminal then this is the default value used.
                // 1: Back-facing camera, facing away from the user.
                // F = Flip: indicates a horizontal (left-right flip) mirror operation on the video as sent on the link.
                //     0: No flip operation. If the sending MTSI client in terminal does not know if a horizontal mirror operation is necessary, then this is the default value used.
                //     1: Horizontal flip operation
                // R1, R0 = Rotation: indicates the rotation of the video as transmitted on the link. The receiver should rotate the video to compensate that rotation. E.g. a 90° Counter Clockwise rotation should be compensated by the receiver with a 90° Clockwise rotation prior to displaying.
                // Set extension
                hasVideoOrientation = true;
                //Get all cvo data
                cvo.facing    = ext[i] >> 3;
                cvo.flip    = ext[i] >> 2 & 0x01;
                cvo.rotation    = ext[i] & 0x03;
                break;
            case TransportWideCC:
                //  0                   1                   2                   3
                //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |  ID   | L=1   |transport-wide sequence number | zero padding  |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                hasTransportWideCC = true;
                transportSeqNum =  NUtil::get2(ext,i);
                break;
                
            case FrameMarking:
                // For Frame Marking RTP Header Extension:
                //
                // https://tools.ietf.org/html/draft-ietf-avtext-framemarking-04#page-4
                // This extensions provides meta-information about the RTP streams outside the
                // encrypted media payload, an RTP switch can do codec-agnostic
                // selective forwarding without decrypting the payload
                //
                // for Non-Scalable Streams
                //
                //     0                   1
                //     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
                //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //    |  ID=? |  L=0  |S|E|I|D|0 0 0 0|
                //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //
                // for Scalable Streams
                //
                //     0                   1                   2                   3
                //     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //    |  ID=? |  L=2  |S|E|I|D|B| TID |   LID         |    TL0PICIDX  |
                //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                //
                //Set header
                hasFrameMarking = true;
                // Set frame marking ext
                frameMarks.startOfFrame    = ext[i] & 0x80;
                frameMarks.endOfFrame    = ext[i] & 0x40;
                frameMarks.independent    = ext[i] & 0x20;
                frameMarks.discardable    = ext[i] & 0x10;
                
                // Check variable length
                if (len==1) {
                    // We are non-scalable
                    frameMarks.baseLayerSync    = 0;
                    frameMarks.temporalLayerId    = 0;
                    frameMarks.layerId        = 0;
                    frameMarks.tl0PicIdx        = 0;
                } else if (len==3) {
                    // Set scalable parts
                    frameMarks.baseLayerSync    = ext[i] & 0x08;
                    frameMarks.temporalLayerId    = ext[i] & 0x07;
                    frameMarks.layerId        = ext[i+1];
                    frameMarks.tl0PicIdx        = ext[i+2];
                } else {
                    // Incorrect length
                    hasFrameMarking = false;
                }
                break;
                // SDES string items
            case RTPStreamId:
                hasRTPStreamId = true;
                rid.assign((const char*)ext+i,len);
                break;
            case RepairedRTPStreamId:
                hasRepairedRTPStreamId = true;
                repairedId.assign((const char*)ext+i,len);
                break;
            case MediaStreamId:
                hasMediaStreamId = true;
                mid.assign((const char*)ext+i,len);
                break;
            default:
                //UltraDebug("-Unknown or unmapped extension [%d]\n",id);
                break;
        }
        //Skip length
        i += len;
    }
    
    return 4+length;
}


uint32_t NRTPHeaderExtension::Serialize(const Map &extMap,uint8_t* data,const size_t size) const
{
    size_t n;
    
    //If not enought size for header
    if (size<4)
        //ERROR
        return 0;
    
    //Set magic header
    NUtil::set2(data,0,0xBEDE);
    
    //Parse and set length
    uint32_t len = 4;
    
    //For each extension
    if (hasAudioLevel)
    {
        //Get id for extension
        auto it = extMap.search(SSRCAudioLevel);
        //uint8_t id = extMap.GetTypeForCodec(SSRCAudioLevel);
        
        // The payload of the audio level header extension element can be
        // encoded using either the one-byte or two-byte
        //  0             1
        //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // |  ID   | len=0 |V| level       |
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
        //Write header
        if ( it != extMap.end() && (n = WriteHeaderIdAndLength(data,len,it->first,1)))
        {
            //Inc header len
            len += n;
            //Set vad
            data[len++] = (vad ? 0x80 : 0x00) | (level & 0x07);
        }
    }
    
    if (hasTimeOffset)
    {
        //Get id for extension
        auto it = extMap.search(TimeOffset);
        //uint8_t id = extMap.GetTypeForCodec(TimeOffset);
        //  0                   1                   2                   3
        //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // |  ID   | len=2 |          transmission offset           |
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //
        //Write header
        if (it != extMap.end() && (n = WriteHeaderIdAndLength(data,len,it->first,3)))
        {
            //Inc header len
            len += n;
            //if it is negative
            if (timeOffset<0)
            {
                //Set value
                NUtil::set3(data,len,-timeOffset);
                //Set sign
                data[0] = data[0] | 0x80;
            } else {
                //Set value
                NUtil::set3(data,len,timeOffset & 0x7FFFFF);
            }
            //Increase length
            len += 3;
        }
    }
    
    if (hasAbsSentTime)
    {
        //Get id for extension
        auto it = extMap.search(AbsoluteSendTime);
        //uint8_t id = extMap.GetTypeForCodec(AbsoluteSendTime);
        
        //  0                   1                   2                   3
        //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // |  ID   | len=2 |          absolute send time           |
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
        // Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
        //If found
        if (it != extMap.end() && (n = WriteHeaderIdAndLength(data,len,it->first,3)))
        {
            //Inc header len
            len += n;
            //Calculate absolute send time field (convert ms to 24-bit unsigned with 18 bit fractional part.
            // Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).
            //Set it
            NUtil::set3(data,len,((absSentTime << 18) / 1000));
            //Inc length
            len += 3;
        }
    }
    
    if (hasVideoOrientation)
    {
        //Get id for extension
        auto it = extMap.search(CoordinationOfVideoOrientation);
        //uint8_t id = extMap.GetTypeForCodec(CoordinationOfVideoOrientation);
        
        // Bit#        7   6   5   4   3   2   1  0(LSB)
        // Definition    0   0   0   0   C   F   R1 R0
        // With the following definitions:
        // C = Camera: indicates the direction of the camera used for this video stream. It can be used by the MTSI client in receiver to e.g. display the received video differently depending on the source camera.
        //     0: Front-facing camera, facing the user. If camera direction is unknown by the sending MTSI client in the terminal then this is the default value used.
        // 1: Back-facing camera, facing away from the user.
        // F = Flip: indicates a horizontal (left-right flip) mirror operation on the video as sent on the link.
        //     0: No flip operation. If the sending MTSI client in terminal does not know if a horizontal mirror operation is necessary, then this is the default value used.
        //     1: Horizontal flip operation
        // R1, R0 = Rotation: indicates the rotation of the video as transmitted on the link. The receiver should rotate the video to compensate that rotation. E.g. a 90° Counter Clockwise rotation should be compensated by the receiver with a 90° Clockwise rotation prior to displaying.
        
        //Write header
        if (it != extMap.end() && (n = WriteHeaderIdAndLength(data,len,it->first,1)))
        {
            //Inc header len
            len += n;
            //Get all cvo data
            data[len++] = (cvo.facing ? 0x08 : 0x00) | (cvo.flip ? 0x04 : 0x00) | (cvo.rotation & 0x03);
        }
    }
    
    if (hasTransportWideCC)
    {
        //Get id for extension
        auto it = extMap.search(TransportWideCC);
        //uint8_t id = extMap.GetTypeForCodec(TransportWideCC);
        
        //  0                   1                   2
        //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // |  ID   | L=1   |transport-wide sequence number |
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
        //Write header
        if (it != extMap.end() && (n = WriteHeaderIdAndLength(data,len,it->first,2)))
        {
            //Inc header len
            len += n;
            //Set them
            NUtil::set2(data,len,transportSeqNum);
            //Inc length
            len += 2;
        }
    }
    
    if (hasFrameMarking)
    {
        //Get id for extension
        auto it = extMap.search(FrameMarking);
        //uint8_t id = extMap.GetTypeForCodec(FrameMarking);
        
        
        // For Frame Marking RTP Header Extension:
        //
        // https://tools.ietf.org/html/draft-ietf-avtext-framemarking-04#page-4
        // This extensions provides meta-information about the RTP streams outside the
        // encrypted media payload, an RTP switch can do codec-agnostic
        // selective forwarding without decrypting the payload
        //
        // for Non-Scalable Streams
        //
        //     0             1
        //     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
        //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //    |  ID=? |  L=0  |S|E|I|D|0 0 0 0|
        //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //
        // for Scalable Streams
        //
        //     0                   1                   2                   3
        //     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //    |  ID=? |  L=2  |S|E|I|D|B| TID |   LID       |    TL0PICIDX  |
        //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //
        
        bool scalable = false;
        // Check if it is scalable
        if (frameMarks.baseLayerSync || frameMarks.temporalLayerId || frameMarks.layerId || frameMarks.tl0PicIdx)
            //It is scalable
            scalable = true;
        
        //Write header
        if (it != extMap.end() && (n = WriteHeaderIdAndLength(data,len,it->first,(scalable ? 0x03 : 0x01))))
        {
            //Inc header len
            len += n;
            //Set common part
            data[len] = frameMarks.startOfFrame ? 0x80 : 0x00;
            data[len] |= frameMarks.endOfFrame ? 0x40 : 0x00;
            data[len] |= frameMarks.independent ? 0x20 : 0x00;
            data[len] |= frameMarks.discardable ? 0x10 : 0x00;
            
            //If it is scalable
            if (scalable)
            {
                //Set scalable data
                data[len] |= frameMarks.baseLayerSync ? 0x08 : 0x00;
                data[len] |= (frameMarks.temporalLayerId & 0x07);
                //Inc len
                len++;
                //Set other bytes
                data[len++] = frameMarks.layerId;
                data[len++] = frameMarks.tl0PicIdx;
            } else {
                //Inc len
                len++;
            }
        }
    }
    
    if (hasRTPStreamId)
    {
        //Get id for extension
        auto it = extMap.search(RTPStreamId);
        //uint8_t id = extMap.GetTypeForCodec(RTPStreamId);
        
        //Write header
        if (it != extMap.end() && (n = WriteHeaderIdAndLength(data,len,it->first,rid.length())))
        {
            //Inc header len
            len += n;
            //Copy str contents
            memcpy(data+len,rid.c_str(),rid.length());
            //Append length
            len+=rid.length();
        }
    }
    
    if (hasRepairedRTPStreamId)
    {
        //Get id for extension
        auto it = extMap.search(RepairedRTPStreamId);
        //uint8_t id = extMap.GetTypeForCodec(RepairedRTPStreamId);
        
        //Write header
        if (it != extMap.end() && (n = WriteHeaderIdAndLength(data,len,it->first,repairedId.length())))
        {
            //Inc header len
            len += n;
            //Copy str contents
            memcpy(data+len,repairedId.c_str(),repairedId.length());
            //Append length
            len+=repairedId.length();
        }
    }
    
    if (hasMediaStreamId)
    {
        //Get id for extension
        auto it = extMap.search(MediaStreamId);
        //uint8_t id = extMap.GetTypeForCodec(MediaStreamId);
        
        //Write header
        if (it != extMap.end() && (n = WriteHeaderIdAndLength(data,len,it->first,mid.length())))
        {
            //Inc header len
            len += n;
            //Copy str contents
            memcpy(data+len,mid.c_str(),mid.length());
            //Append length
            len+=mid.length();
        }
    }
    
    //Pad to 32 bit words
    while(len%4)
        data[len++] = 0;
    
    //Set length
    NUtil::set2(data,2,(len/4)-1);
    
    //Return
    return len;
}

void NRTPHeaderExtension::Dump(const std::string& prefix, std::list<std::string>& lines) const {
    std::ostringstream oss;
//    oss << prefix << "[RTPHeaderExt]";
//    lines.emplace_back(oss.str());oss.str("");
    
    if (hasAudioLevel){
        oss << prefix << "AudioLevel=[vad=" << vad << " level=" << level << "]";
        lines.emplace_back(oss.str());oss.str("");
    }
    
    if (hasTimeOffset){
        oss << prefix << "TimeOffset=[" << timeOffset << "]";
        lines.emplace_back(oss.str());oss.str("");
    }
    
    if (hasAbsSentTime){
        oss << prefix << "AbsSentTime=[" << absSentTime << "]";
        lines.emplace_back(oss.str());oss.str("");
    }
    
    if (hasVideoOrientation){
        oss << prefix << "VideoOrientation=[facing=" << cvo.facing
        << " flip=" << cvo.flip
        << " rotation=" << cvo.rotation
        << "]";
        lines.emplace_back(oss.str());oss.str("");
    }
    
    if (hasTransportWideCC){
        oss << prefix << "TransportWideCC=[" << transportSeqNum << "]";
        lines.emplace_back(oss.str());oss.str("");
    }
    
    if (hasFrameMarking){
        oss << prefix << "FrameMarking=[start=" << frameMarks.startOfFrame
        << " end=" << frameMarks.endOfFrame
        << " indp=" << frameMarks.independent
        << " discard=" << frameMarks.discardable
        << " sync=" << frameMarks.baseLayerSync
        << " tid=" << frameMarks.temporalLayerId
        << " layer=" << frameMarks.layerId
        << " t0pic=" << frameMarks.tl0PicIdx
        << "]";
        lines.emplace_back(oss.str());oss.str("");
    }
    
    if (hasRTPStreamId){
        oss << prefix << "RTPStreamId=[" << rid << "]";
        lines.emplace_back(oss.str());oss.str("");
    }
    
    if (hasRepairedRTPStreamId){
        oss << prefix << "RepairedRTPStreamId=[" << repairedId << "]";
        lines.emplace_back(oss.str());oss.str("");
    }
    
    if (hasMediaStreamId){
        oss << prefix << "MediaStreamId=[" << mid << "]";
        lines.emplace_back(oss.str());oss.str("");
    }
}




size_t NRTPData::serialize(uint8_t* datap, const size_t size, NRTPData& other) const{
    size_t sz = serialize(datap, size);
    if(!sz){
        return 0;
    }
    
    other = * this;
    
    other.data = datap;
    other.size = sz;
    
    if(other.extLength > 0){
        other.extPtr = datap + 12 + (csrcs.size() * 4);
    }else{
        other.extPtr = nullptr;
    }
    
    other.payloadPtr = datap + getHeadSize();
    
    return sz;
}

size_t NRTPData::serialize(uint8_t* data,const size_t size) const{
    if(size < getHeadSize()){
        return 0;
    }
    
    size_t pos = 0;
    auto sz = header.Serialize(data+pos, size);
    pos += sz;
    
    for (auto it=csrcs.begin(); it!=csrcs.end(); ++it){
        NUtil::set4(data, pos, *it);
        pos += 4;
    }
    
    if(extLength > 0){
        memcpy(data+pos, extPtr, extLength);
        pos += extLength;
    }
    
    if(payloadLen > 0){
        if(size < (pos + payloadLen)){
            return 0;
        }
        memcpy(data+pos, payloadPtr, payloadLen);
        pos += payloadLen;
    }
    
    return pos;
}

size_t NRTPData::parse(const uint8_t* data, size_t size, bool remove_padding){
    
    // fixed header
    auto pos = header.Parse(data, size);
    if (!pos){
        return 0;
    }
    
    // csrcs
    if (size < (pos+header.cc*4)){
        return 0;
    }
    for (uint8_t i=0; i < header.cc; ++i){
        csrcs.emplace_back(NUtil::get4(data, pos+i*4));
    }
    pos += header.cc*4;
    
    
    if(header.extension){
        
        if ((size - pos) < 4){
            return 0;
        }
        
        uint16_t length = NUtil::get2(data, pos+2)*4;
        
        if ((size - pos) < (length+4)){
            return 0;
        }
        
        this->extPtr = data + pos;
        this->extLength =  4+length;
        pos += this->extLength;
    }else{
        this->extPtr = nullptr;
        this->extLength = 0;
    }
    
    size_t payload_len = size-pos;
    setPayload(data + pos,  payload_len);
    
    if(remove_padding){
        if(removePadding() < 0){
            return 0;
        }
    }
    
    this->data = data;
    this->size = size;
    
    return pos;
}

int NRTPData::removePadding(){
    if (header.padding){
        uint16_t padding = NUtil::get1(data,size-1);
        if (this->payloadLen < padding){
            return -1;
        }
        this->payloadLen -= padding;
        header.padding = false;
        return padding;
    }
    return 0;
}

size_t NRTPData::RecoverOSN(NRTPPayloadType pltype){
    /*
     The format of a retransmission packet is shown below:
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                         RTP Header                            |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |            OSN                |                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
     |                  Original RTP Packet Payload                  |
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */
    // Ensure we have enought data
    if (payloadLen < 2){
        return 0;
    }
    
    // Get original sequence number
    uint16_t osn = NUtil::get2(payloadPtr,0);
    payloadPtr += 2;
    payloadLen -= 2;
    
    header.sequenceNumber = osn;
    header.payloadType = pltype;
    
    return 2;
}

int NRTPData::demuxRED(std::vector<REDBlock>& blocks) const{
    return demuxRED([&blocks](REDBlock& block){
        blocks.emplace_back(block);
    });
}

int NRTPData::demuxRED(const std::function<void(NRTPData& rtpd)> &func) const{
    NRTPData newrtpd = *this;
    return demuxRED([this, &newrtpd, &func](REDBlock& block){
        newrtpd.header.payloadType = block.pt;
        newrtpd.header.timestamp = this->header.timestamp + block.tsOffset;
        newrtpd.setPayload(block.data, block.length);
        func(newrtpd);
    });
}

int NRTPData::demuxRED(const std::function<void(REDBlock& block)> &func) const{
    
    if(payloadLen < 1){
        return -1;
    }
    
    REDBlock block;
    
    // Index of payloadPtr
    uint16_t i = 0;
    
    // Check if it is the last
    bool last = !(payloadPtr[i]>>7);
    
    // Read redundant headers
    while(!last){
        
        // TODO: webrtc never reach here
        
        if((payloadLen - i) < 4){
            return -2;
        }
        
        /*
         0                   1                    2                   3
         0 1 2 3 4 5 6 7 8 9 0 1 2 3  4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |1|   block PT  |  timestamp offset         |   block length    |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         F: 1 bit First bit in header indicates whether another header block
         follows.  If 1 further header blocks follow, if 0 this is the
         last header block.
         
         block PT: 7 bits RTP payload type for this block.
         
         timestamp offset:  14 bits Unsigned offset of timestamp of this block
         relative to timestamp given in RTP header.  The use of an unsigned
         offset implies that redundant payload must be sent after the primary
         payload, and is hence a time to be subtracted from the current
         timestamp to determine the timestamp of the payload for which this
         block is the redundancy.
         
         block length:  10 bits Length in bytes of the corresponding payload
         block excluding header.
         
         */
        
        // Get block PT
        block.pt = payloadPtr[i] & 0x7F;
        i++;
        
        // Get timestamp offset
        block.tsOffset = payloadPtr[i++];
        block.tsOffset = block.tsOffset <<6 | (payloadPtr[i] >> 2);
        
        // Get block length
        block.length = payloadPtr[i++] & 0x03;
        block.length = block.length << 6 | payloadPtr[i++];
        
        block.data = payloadPtr+i;
        
        //blocks.emplace_back(block);
        func(block);
        
        // Skip the block data
        i += block.length;
        
        if((payloadLen - i) < 1){
            return -3;
        }
        
        //Check if it is the last
        last = !(payloadPtr[i]>>7);
    }
    
    /*
     *  0 1 2 3 4 5 6 7
     +-+-+-+-+-+-+-+-+
     |0|   Block PT  |
     +-+-+-+-+-+-+-+-+
     */
    
    block.pt = payloadPtr[i++] & 0x7F;
    block.tsOffset = 0;
    block.data = payloadPtr+i;
    block.length = payloadLen - i;
    //blocks.emplace_back(block);
    func(block);
    
    return 0;
}





