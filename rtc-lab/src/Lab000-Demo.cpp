

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h> /* for IFF_LOOPBACK */
#include <vector>
#include <array>
#include <string>
#include <iostream>


#include "Lab010-VP8.hpp"
#include "VP8Framework.hpp"
#include "SDLFramework.hpp"
#include "NRTPMap.hpp"
#include "NRTPReceiver.hpp"
#include "Lab000-Demo.hpp"


//#include "spdlog/spdlog.h"
//#include "spdlog/sinks/stdout_color_sinks.h"
//#include "spdlog/fmt/bin_to_hex.h"


//static NObjDumperOStream maindumper(std::cout, "|   main|I| ");
//#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
//#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
//#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
//#define DUMPER() maindumper

static auto main_logger = NLogger::Get("main");
//static NObjDumperLog maindumper(*main_logger);
#define odbgd(FMT, ARGS...) main_logger->debug(FMT, ##ARGS)
#define odbgi(FMT, ARGS...) main_logger->info(FMT, ##ARGS)
#define odbge(FMT, ARGS...) main_logger->error(FMT, ##ARGS)
#define DUMPER() NObjDumperLog(*main_logger, NLogger::Level::debug)


class A{
public:
    A(){
        std::cout << "A<" << (void*)this << ">()" << std::endl;
    }
    virtual ~A(){
        std::cout << "~A<" << (void*)this << ">()" << std::endl;
    }
};

class AA{
public:
    AA(){
        std::cout << "AA<" << (void*)this << ">()" << std::endl;
    }
    virtual ~AA(){
        std::cout << "~AA<" << (void*)this << ">()" << std::endl;
    }
};

class B{
public:
    B(){
        std::cout << "B<" << (void*)this << ">()" << std::endl;
    }
    virtual ~B(){
        std::cout << "~B<" << (void*)this << ">()" << std::endl;
    }
};





static const int TLV_TYPE_CODEC = 2000;
static const int TLV_TYPE_SDP   = 2001;
static const int TLV_TYPE_RTP   = 2002;
static const int TLV_TYPE_RTCP  = 2003;
static const int TLV_TYPE_END   = 11;

#define TLV_ERR_READFAIL  -2
#define TLV_ERR_OPENFAIL  -1
#define TLV_ERR_REACHEND  1

class TLVFile{
    
public:
    
    typedef std::function<int(int64_t packet_num,
    int type,
    const uint8_t * data_ptr,
    size_t  data_len)> PacketFunc;
    
    static int process(const std::string& filename_
                       , std::ostringstream& oss
                       , PacketFunc func){

        FILE* filep = nullptr;
        int64_t packetNum = 0;
        
        uint8_t * buf_ptr = nullptr;
        size_t buf_size =  1*1024*1024;
        int ret = 0;
        do{
            filep = fopen(filename_.c_str(), "rb");
            if (filep == nullptr) {
                oss << "reader: can't open [" << filename_ << "]";
                ret = -1;
                break;
            }
            buf_ptr = new uint8_t[buf_size];
            
            while(true) {
                size_t length = 0;
                int dtype = 0;
                int ret = NUtil::ReadTLV(filep, buf_ptr, buf_size, &length, &dtype);
                if(ret < 0){
                    if(packetNum == 0){
                        oss << "reader: fail to read first packet";
                    }else{
                        // reach file end
                        ret = 0;
                    }
                    break;
                }
                ++packetNum;
                ret = func(packetNum, dtype, buf_ptr, length);
                if(ret != 0) break;
            }
            
        }while(0);
        
        if(filep){
            fclose(filep);
            filep = nullptr;
        }
        
        if(buf_ptr){
            delete[] buf_ptr;
            buf_ptr = nullptr;
        }
        
        return ret;
    }
    
public:
    DECLARE_CLASS_ENUM(ErrorCode,
                       kReadFail = TLV_ERR_READFAIL,
                       kOpenFail = TLV_ERR_OPENFAIL,
                       kNoError = 0,
                       kReachFileEnd = TLV_ERR_REACHEND
                       );
    struct PacketData{
        int64_t             number;
        int                 type;
        const uint8_t *     dataPtr;
        size_t              dataLen;
    };
    
    virtual ~TLVFile(){
        close();
    }
    
    ErrorCode open(const std::string& filename){
        close();
        filep_ = fopen(filename.c_str(), "rb");
        if (!filep_) {
            return kOpenFail;
        }
        buf_ptr_ = new uint8_t[buf_size_];
        return kNoError;
    }
    
    void close(){
        if(filep_){
            fclose(filep_);
            filep_ = nullptr;
        }
        
        if(buf_ptr_){
            delete[] buf_ptr_;
            buf_ptr_ = nullptr;
        }
    }

    ErrorCode next(PacketData& pkt){
        size_t length = 0;
        int dtype = 0;
        int ret = NUtil::ReadTLV(filep_, buf_ptr_, buf_size_, &length, &dtype);
        if(ret < 0){
            if(packetNum_ == 0){
                return kReadFail;
            }else{
                // reach file end
                ret = 0;
                return kReachFileEnd;
            }
        }
        ++packetNum_;
        pkt.number = packetNum_;
        pkt.type = dtype;
        pkt.dataPtr = buf_ptr_;
        pkt.dataLen = length;
        return kNoError;
    }
private:
    FILE* filep_ = nullptr;
    int64_t packetNum_ = 0;
    
    uint8_t * buf_ptr_ = nullptr;
    size_t buf_size_ =  1*1024*1024;
};


struct User{
    SDLWindowImpl * window_ = NULL;
    SDLVideoView * videoView_ = NULL;
    SDLTextView * frameTxtView_ = NULL;
    //VideoStati videoStati_;
    User(const std::string& name, int w, int h, const vpx_rational& timebase)
    : window_(new SDLWindowImpl(name, w, h))
    , videoView_(new SDLVideoView())
    , frameTxtView_(new SDLTextView())
    //, videoStati_(timebase)
    {
        window_->addView(videoView_);
        window_->addView(frameTxtView_);
    }
    
    User(User&& other)
    : window_(other.window_)
    , videoView_(other.videoView_)
    , frameTxtView_(other.frameTxtView_)
    {
        other.window_ = nullptr;
        other.videoView_ = nullptr;
        other.frameTxtView_ = nullptr;
    }
    
    User(User& other) = delete ;
    
    virtual ~User(){
        if(window_){
            delete window_;
            window_ = NULL;
        }
    }
    
    virtual void reset(){
        //videoStati_.reset(0);
    }
    
};

struct TLVDecodeUser : public User {
    VP8Decoder * decoder_ = nullptr;
    int layerId_ = 0;
    TLVDecodeUser(const std::string& name, int w, int h, const vpx_rational& timebase, int layerId = 0)
    : User(name, w, h, timebase)
    , decoder_(new VP8Decoder())
    , layerId_(layerId){
        
    }
    
    TLVDecodeUser()
    : User("", 640, 480, {1, 90000}){
        
    }
    
    TLVDecodeUser(TLVDecodeUser&& other):User((User&&)other), decoder_(other.decoder_), layerId_(other.layerId_){
        other.decoder_ = nullptr;
    }
    
    TLVDecodeUser(TLVDecodeUser& other) = delete;
    
    virtual ~TLVDecodeUser(){
        if(decoder_){
            delete decoder_;
            decoder_ = NULL;
        }
    }
    
    int decode(vpx_codec_pts_t pts, const uint8_t * frameData, size_t frameLength, int frameNo, int layerId, bool skip){
        
        if(layerId > layerId_){
            return 0;
        }
        
        int ret = 0;
        int refupdate = 0;
        int corrupted = 0;
        int refUsed = 0;
//        const uint8_t * frameData = NULL;
//        size_t frameLength = 0;
//        if(!skip){
//            frameData = encoder->getEncodedFrame();
//            frameLength = encoder->getEncodedLength();
//        }
        
        //videoStati_.processBytes(frameLength);
        ret = decoder_->decode(frameData, frameLength, &refupdate, &corrupted, &refUsed);
        if (ret == 0) {
            vpx_image_t * decimg = NULL;
            while ((decimg = decoder_->pullImage()) != NULL) {
                //videoStati_.check(pts, 1, 0);
                
                videoView_->drawYUV(decimg->d_w, decimg->d_h
                                    , decimg->planes[0], decimg->stride[0]
                                    , decimg->planes[1], decimg->stride[1]
                                    , decimg->planes[2], decimg->stride[2]);
                char txt[128];
                int txtlen = 0;
//                txtlen = makeTextCommon(encoder, frameNo, txt+0);
//                txtlen += sprintf(txt+txtlen, ", TL=%d", layerId );
//                txtlen += sprintf(txt+txtlen, ", fps=%d", videoStati_.getFPS() );
//                txtlen += sprintf(txt+txtlen, ", br=%lld", videoStati_.getBitrate()/1000 );
                if(corrupted){
                    txtlen += sprintf(txt+txtlen, ",corrupted" );
                }
                txtlen += sprintf(txt+txtlen, ", upd(%s %s %s)"
                                  ,(refupdate & VP8_LAST_FRAME)?"LF":""
                                  ,(refupdate & VP8_GOLD_FRAME)?"GF":""
                                  ,(refupdate & VP8_ALTR_FRAME)?"ALF":"");
                txtlen += sprintf(txt+txtlen, ", ref(%s %s %s)"
                                  ,(refUsed & VP8_LAST_FRAME)?"LF":""
                                  ,(refUsed & VP8_GOLD_FRAME)?"GF":""
                                  ,(refUsed & VP8_ALTR_FRAME)?"ALF":"");
                
                frameTxtView_->draw(txt);
                odbgi("  decode=[{}]", txt);
            }
        }
        return ret;
    }
};


//class XSWTLVPlayer1{
//    class RecordFlow{
//    public:
//        RecordFlow(){
//        }
//        
//        RecordFlow(RecordFlow &&other)
//        :rtpFlow(other.rtpFlow)
//        , decodeUser_(other.decodeUser_)
//        , depacker(other.depacker){
//            other.rtpFlow = nullptr;
//            other.decodeUser_ = nullptr;
//            other.depacker = nullptr;
//        }
//        
//        RecordFlow(RecordFlow &other) = delete;
//        
//        virtual ~RecordFlow(){
//            close();
//        }
//        
//        NRTPIncomingFlow * rtpFlow = nullptr;
//        NRTPTime rtptime;
//        TLVDecodeUser * decodeUser_ = nullptr;
//        int64_t lastPTS = -1;
//        int64_t lastRenderTime = -1;
//        NRTPDepacker * depacker = nullptr;
//        
//        int open(){
//            close();
//            decodeUser_ = new TLVDecodeUser("decode1", 640, 480, {1, 90000}, 0);
//            int ret = decodeUser_->window_->open();
//            if(ret){
//                odbge("error: open decode windows, ret=%d", ret);
//                return ret;
//            }
//            
//            ret = decodeUser_->decoder_->open();
//            if(ret){
//                odbge("error: open decoder , ret=%d", ret);
//                return ret;
//            }
//            
//            return ret;
//        }
//        void close(){
//            if(decodeUser_){
//                delete decodeUser_;
//                decodeUser_ = nullptr;
//            }
//            if(depacker){
//                delete depacker;
//                depacker = nullptr;
//            }
//        }
//    };
//    
//    NSDP remoteSDP_;
//    NRTPReceiver receiver_;
//    std::vector<RecordFlow> flows_;
//    int audioFlowIndex_ = -1;
//    int64_t  firstTime_ = -1;
//    
//    const char * RAW_RTP_STR    = "  raw RTP   : ";
//    const char * BIND_STR       = "  bind RTP  : ";
//    const char * DEMUX_STR      = "  demux RTP : ";
//    #define  RAW_RTCP_STR         "  raw RTCP  : "
//    
//public:
////    XSWTLVPlayer1(){
////        odbgi("flows_.size()={}", flows_.size());
////        odbgi("flows_.capacity()={}", flows_.capacity());
////    }
//    
//    
//    static void dump(const NSDP& sdp){
//        std::list<std::string> lines;
//        sdp.dump("", lines);
//        for(auto line : lines){
//            odbgi("{}", line.c_str());
//        }
//    }
//    
//    void decode(RecordFlow& rec_flow, NMediaFrame * frame, const NRTPTime & rtptime){
//        uint32_t rtpts = rtptime.rtpTimestamp();
//        int64_t pts = NNtp::NTP2Milliseconds(rtptime.localNTP());
//        if(rec_flow.lastRenderTime < 0){
//            rec_flow.lastPTS = pts;
//            rec_flow.lastRenderTime = NUtil::get_now_ms();
//        }else{
//            int64_t now_ms = NUtil::get_now_ms();
//            int64_t elapsed = now_ms - rec_flow.lastRenderTime;
//            int64_t duration = pts - rec_flow.lastPTS;
//            if(duration > elapsed){
//                NUtil::sleep_ms(duration-elapsed);
//            }
//            rec_flow.lastPTS = pts;
//            rec_flow.lastRenderTime = now_ms;
//        }
//        
//        int frameNo = 1;
//        int layerId = 0;
//        int ret = rec_flow.decodeUser_->decode(rtpts
//                           , frame->data()
//                           , frame->size()
//                           , frameNo, layerId, false);
//        SDL::FlushEvent();
//        if(ret == 0){
//            //NUtil::sleep_ms(33);
//        }else{
//            odbge("error: decode fail with {}", ret);
//        }
//    }
//    
//    void pullAndDecode(RecordFlow& rec_flow){
//        if(!rec_flow.depacker){
//            return;
//        }
//        
//        //const NRTPData * rtpd = rec_flow.rtpFlow->pullRTPInOrder();
//        NRTPPacketSlot * slot = nullptr;
//        auto pullstate = rec_flow.rtpFlow->packetWin.nextSlotInOrder(slot);
//        while(pullstate >= 0){
//            const NRTPData * rtpd = &slot->rtpd;
//            
//            auto ret = rec_flow.depacker->depack(*rtpd, true,
//                                                    [this, &rec_flow](NMediaFrame::Unique& frame){
//                                                        //NRTPDepackVP8::dump_vp8_frame(frame->data(), frame->size(), main_logger);
//                                                        auto& remoteNTP = rec_flow.rtpFlow->remoteNTP;
//                                                        uint32_t rtpts = (uint32_t)frame->getTime();
//                                                        uint64_t ntpts = remoteNTP.convert(rtpts, 90000);
//                                                        uint64_t local_ntpts = rec_flow.rtptime.input(rec_flow.depacker->firstTimeMS(),
//                                                                                                      rtpts,
//                                                                                                      90000,
//                                                                                                      ntpts);
//                                                        int64_t diff = 0;
//                                                        if(audioFlowIndex_ >= 0){
//                                                            auto& audioFlow = flows_[audioFlowIndex_];
//                                                            rec_flow.rtptime.estimate(audioFlow.rtptime, diff);
//                                                        }
//                                                        
//                                                        odbgi("video ntpts: remote={}, local={}, est={}, diff_audio={}ms",
//                                                              NNtp::toHuman2String(ntpts),
//                                                              NNtp::toHuman2String(local_ntpts),
//                                                              NNtp::toHuman2String(rec_flow.rtptime.localNTP()),
//                                                              NNtp::DeltaNTP2Milliseconds(diff) );
//                                                        this->decode(rec_flow, frame.get(), rec_flow.rtptime);
//
//                                                    });
//            DUMPER().dump("pktmedia", *rec_flow.depacker);
//            if(ret < 0){
//                odbge("error VP8 packet with [{}]-[{}] =================", ret
//                      , rec_flow.depacker->getErrorStr(ret));
//            }else{
//                if(ret == NRTPDepacker::kComplete){
//                    //auto frame = rec_flow.depacker->getFrame();
//
//                }
//            }
//            //rtpd = rec_flow.rtpFlow->pullRTPInOrder();
//            pullstate = rec_flow.rtpFlow->packetWin.nextSlotInOrder(slot);
//        }
//    }
//    
//    RecordFlow& checkFlow(NRTPIncomingFlow * rtp_flow, const NRTPReceiver::IncomingRTPInfo * info){
//        // check add record flow
//        while(flows_.size() <= rtp_flow->index){
//            flows_.emplace_back();
//        }
//        RecordFlow& rec_flow = flows_[rtp_flow->index];
//        if(!rec_flow.rtpFlow){
//            odbgi("add flow [{}]-[{}]", rtp_flow->index, NMedia::GetNameFor(rtp_flow->type));
//            rec_flow.rtpFlow = rtp_flow;
//            
//            if(rec_flow.rtpFlow->type == NMedia::Video){
//                rec_flow.open();
//            }else if(rec_flow.rtpFlow->type == NMedia::Audio){
//                audioFlowIndex_ = rtp_flow->index;
//            }
//        }
//        
//        if(!rec_flow.depacker && rtp_flow->type == NMedia::Video && info->codec->isNative()){
//            rec_flow.depacker = NRTPDepacker::CreateDepacker(info->codec);
//        }
//        
//        return rec_flow;
//    }
//    
//    int processRTP(int64_t&          recv_ts,
//                   const uint8_t *   data_ptr,
//                   size_t            data_len){
//        
//        RecordFlow * prec_flow = nullptr;
//        
//        int ret = receiver_.inputRTP(recv_ts, -1, data_ptr, data_len
//                                 , [this, &prec_flow](const NRTPReceiver::IncomingRTPInfo * info){
//                                     auto rtpd = info->rtpd;
//                                     RecordFlow& rec_flow = checkFlow(info->flow, info);
//                                     if(audioFlowIndex_ == rec_flow.rtpFlow->index){
//                                         uint64_t remote_ntp = rec_flow.rtpFlow->remoteNTP.convert(rtpd->header.timestamp, info->codec->clockrate);
//                                         rec_flow.rtptime.input(rtpd->timeMS,
//                                                                rtpd->header.timestamp,
//                                                                info->codec->clockrate,
//                                                                remote_ntp);
//                                     }
//                                     prec_flow = &rec_flow;
//                                 });
//        if(prec_flow){
//            if(prec_flow->decodeUser_){
//                pullAndDecode(*prec_flow);
//            }
//        }
//        return ret;
//    }// processRTP
//    
//    int processRTCP(int64_t&          recv_ts,
//                   const uint8_t *   data_ptr,
//                   size_t            data_len){
////        auto state = NRtcp::Parser::DumpPackets(data_ptr, data_len, DUMPER());
////        if(state != NRtcp::Parser::kNoError){
////            odbge("error RTCP with [{}]-[{}]", state, NRtcp::Parser::getNameForState(state).c_str());
////        }
//        return receiver_.inputRTCP(recv_ts, -1, data_ptr, data_len);
//    }
//    
//    int onData(int64_t          packet_num,
//              int               dtype,
//              const uint8_t *   data_ptr,
//              size_t            data_len){
//        int ret = 0;
//        if(dtype == TLV_TYPE_CODEC) {
//            odbgi("packet[{}]: codec=[size={}]", packet_num, data_len);
//            
//        } else if(dtype == TLV_TYPE_SDP) {
//            std::ostringstream oss;
//            int ret = remoteSDP_.parse(data_ptr, data_len, oss);
//            if(ret){
//                //odbgi("packet[%lld]: SDP=[%.*s]", packet_num, (int)data_len, data_ptr);
//                odbgi("packet[{}]: SDP=[{}]", packet_num, data_ptr);
//                odbge("fail to parse remote SDP, ret=[{}]-[{}]", ret, oss.str().c_str());
//            }else{
//                odbgi("parsed remote SDP OK");
//                dump(remoteSDP_);
//                receiver_.setRemoteSDP(remoteSDP_);
//            }
//            
//        } else if(dtype == TLV_TYPE_RTP) {
//            if(data_len < 8){
//                odbge("too short of RTP, size={}", data_len);
//            }else{
//                int64_t recv_ts = NUtil::get8(data_ptr, 0);
//                if(firstTime_ < 0){
//                    firstTime_ = recv_ts;
//                }
//                odbgi("packet[{}]: RTP=[time={}({} ms), size={}]", packet_num, recv_ts, recv_ts-firstTime_, data_len-8);
//                processRTP(recv_ts, data_ptr+8, data_len-8);
//            }
//            
//        } else if (dtype == TLV_TYPE_RTCP) {
//            if(data_len < 8){
//                odbge("too short of RTCP, size={}", data_len);
//            }else{
//                int64_t recv_ts = NUtil::get8(data_ptr, 0);
//                if(firstTime_ < 0){
//                    firstTime_ = recv_ts;
//                }
//                odbgi("packet[{}]: RTCP=[time={}({} ms), size={}]", packet_num
//                      , recv_ts
//                      , recv_ts-firstTime_
//                      , data_len-8);
//                processRTCP(recv_ts, data_ptr+8, data_len-8);
//            }
//            
//        } else if (dtype == TLV_TYPE_END) {
//            odbgi("packet[{}]: END", packet_num);
//            return 0;
//            
//        } else {
//            odbgi("packet[{}]: unknown type=[{}]", packet_num, dtype);
//        }
//        return ret;
//    }
//public:
//    
//    int play(const std::string& filename){
//        
//        std::ostringstream oss;
//        int ret = TLVFile::process(filename, oss, [this]  (int64_t packet_num,
//                                                       int dtype,
//                                                       const uint8_t * data_ptr,
//                                                       size_t  data_len
//                                                       )-> int{
//
//            return this->onData(packet_num, dtype, data_ptr, data_len);
//        });
//        if(ret < 0){
//            odbge("process tlv ret=[{}]-[{}]", ret, oss.str().c_str());
//        }
//        
//        return ret;
//    }
//    
//};


class XSWTLVDemuxer{

public:
    class MediaFlow{
    public:
        MediaFlow(){
        }
        
        MediaFlow(MediaFlow &&other)
        :rtpFlow(other.rtpFlow)
        , depacker(other.depacker){
            other.rtpFlow = nullptr;
            other.depacker = nullptr;
        }
        
        MediaFlow(MediaFlow &other) = delete;
        
        virtual ~MediaFlow(){
            if(depacker){
                delete depacker;
                depacker = nullptr;
            }
        }
        
        NRTPIncomingFlow * rtpFlow = nullptr;
        NRTPDepacker * depacker = nullptr;
        NRTPTime rtptime;
    };
    
    typedef std::function<void(MediaFlow& mflow, NMediaFrame::Unique& frame)> FrameFunc;
    
public:
    
    DECLARE_CLASS_ENUM(ErrorCode,
                       kSDPError = -100,
                       kRTPError,
                       kRTCPError,
                       kReadFail = TLV_ERR_READFAIL,
                       kOpenFail = TLV_ERR_OPENFAIL,
                       kNoError = 0,
                       kReachFileEnd = TLV_ERR_REACHEND,
                       kFrameComplete
                       );
    
    void checkAddMediaFlow(NRTPIncomingFlow * rtp_flow){
        // check add flow
        while(flows_.size() <= rtp_flow->index){
            flows_.emplace_back();
        }
        
        MediaFlow& mflow = flows_[rtp_flow->index];
        if(!mflow.rtpFlow){
            odbgi("add flow [{}]-[{}]", rtp_flow->index, NMedia::GetNameFor(rtp_flow->type));
            mflow.rtpFlow = rtp_flow;
        }
        
        if(!mflow.depacker && rtp_flow->mainCodec){
            mflow.depacker = NRTPDepacker::CreateDepacker(rtp_flow->mainCodec);
        }
    }
    
    void processFrame(MediaFlow& mflow, NMediaFrame::Unique& frame, const FrameFunc& func){
        if(audioFlowIndex_ < 0 && mflow.rtpFlow->type == NMedia::Audio){
            audioFlowIndex_ = mflow.rtpFlow->index;
        }
        
        uint32_t clockrate = mflow.rtpFlow->mainCodec->clockrate;
        auto& remoteNTP = mflow.rtpFlow->remoteNTP;
        uint32_t rtpts = (uint32_t)frame->getTime();
        uint64_t ntpts = remoteNTP.convert(rtpts, clockrate);
        uint64_t local_ntpts = mflow.rtptime.input(mflow.depacker->firstTimeMS(),
                                                      rtpts,
                                                      clockrate,
                                                      ntpts);
        int64_t diff = 0;
        if(audioFlowIndex_ >= 0 && mflow.rtpFlow->type == NMedia::Video){
            auto& audioFlow = flows_[audioFlowIndex_];
            mflow.rtptime.estimate(audioFlow.rtptime, diff);
            
            odbgi("video ntpts: remote={}, local={}, est={}, diff_audio={}ms",
                  NNtp::toHuman2String(ntpts),
                  NNtp::toHuman2String(local_ntpts),
                  NNtp::toHuman2String(mflow.rtptime.localNTP()),
                  NNtp::DeltaNTP2Milliseconds(diff) );
        }
        
        frame->setTime(NNtp::NTP2Milliseconds(mflow.rtptime.localNTP()));
        func(mflow, frame);
    }
    virtual void processFlow(MediaFlow& mflow, const FrameFunc& func){
        NRTPPacketSlot * slot = nullptr;
        auto pullstate = mflow.rtpFlow->packetWin.nextSlotInOrder(slot);
        while(pullstate >= 0){
            // NRTPPacketWindow::kNoError or kDiscontinuity
            if(mflow.depacker){
                int depack_ret =  mflow.depacker->depack(slot->rtpd, true,
                                                         [this, &mflow, &func](NMediaFrame::Unique& frame){
                                                             processFrame(mflow, frame, func);
                                                         });
                //DUMPER().dump("depack-state", *mflow.depacker);
                if(depack_ret < 0){
                    // TODO: stop process when error
                    odbge("fail to depack, error=[{}]-[{}]", depack_ret, mflow.depacker->getErrorStr(depack_ret));
                }
            }
            pullstate = mflow.rtpFlow->packetWin.nextSlotInOrder(slot);
        }
    }
    
    int processRTP(int64_t&             recv_ts,
                   const uint8_t *      data_ptr,
                   size_t               data_len
                   , const FrameFunc&   func){
        
        NRTPIncomingFlow * incoming_flow = nullptr;
        
        int ret = receiver_.inputRTP(recv_ts, -1, data_ptr, data_len,
                                     [this, &incoming_flow](const NRTPReceiver::IncomingRTPInfo * info){
                                         incoming_flow = info->flow;
                                     });
        if(ret < 0){
            // TODO: handle error
            return kRTPError;
        }
        
        if(incoming_flow){
            checkAddMediaFlow(incoming_flow);
            if(incoming_flow->packetWin.isFull()){
                processFlow(flows_[incoming_flow->index], func);
            }
            return kFrameComplete;
        }
        return kNoError;

    }// processRTP
    
    ErrorCode processUntilFrame(const FrameFunc& func){
        TLVFile::PacketData pkt;
        int errcode = tlv_.next(pkt);
        while(errcode == TLVFile::kNoError){
            if(pkt.type == TLV_TYPE_CODEC) {
                odbgi("packet[{}]: codec=[size={}]", pkt.number, pkt.dataLen);
                
            } else if(pkt.type == TLV_TYPE_SDP) {
                std::ostringstream oss;
                int ret = remoteSDP_.parse(pkt.dataPtr, pkt.dataLen, oss);
                if(ret){
                    //odbgi("packet[%lld]: SDP=[%.*s]", packet_num, (int)data_len, data_ptr);
                    //odbgi("packet[{}]: SDP=[{}]", pkt.number, data_ptr);
                    odbge("fail to parse remote SDP, ret=[{}]-[{}]", ret, oss.str().c_str());
                    errcode = kSDPError;
                    break;
                }else{
                    odbgi("parsed remote SDP OK");
                    std::list<std::string> lines;
                    remoteSDP_.dump("", lines);
                    for(auto line : lines){
                        odbgi("{}", line.c_str());
                    }
                    receiver_.setRemoteSDP(remoteSDP_);
                }
                
            } else if(pkt.type == TLV_TYPE_RTP) {
                if(pkt.dataLen < 8){
                    odbge("too short of RTP, size={}", pkt.dataLen);
                    errcode = kRTPError;
                    break;
                }else{
                    int64_t recv_ts = NUtil::get8(pkt.dataPtr, 0);
                    if(firstTime_ < 0){
                        firstTime_ = recv_ts;
                    }
                    odbgi("packet[{}]: RTP=[time={}({} ms), size={}]", pkt.number, recv_ts, recv_ts-firstTime_, pkt.dataLen-8);
                    errcode = processRTP(recv_ts, pkt.dataPtr+8, pkt.dataLen-8, func);
                    if(errcode == kFrameComplete){
                        break;
                    }
                }
                
            } else if (pkt.type == TLV_TYPE_RTCP) {
                if(pkt.dataLen < 8){
                    odbge("too short of RTCP, size={}", pkt.dataLen);
                    errcode = kRTCPError;
                    break;
                }else{
                    int64_t recv_ts = NUtil::get8(pkt.dataPtr, 0);
                    if(firstTime_ < 0){
                        firstTime_ = recv_ts;
                    }
                    odbgi("packet[{}]: RTCP=[time={}({} ms), size={}]", pkt.number
                          , recv_ts
                          , recv_ts-firstTime_
                          , pkt.dataLen-8);
                    receiver_.inputRTCP(recv_ts, -1, pkt.dataPtr+8, pkt.dataLen-8);
                }
                
            } else if (pkt.type == TLV_TYPE_END) {
                odbgi("packet[{}]: END", pkt.number);
                errcode = kReachFileEnd;
                break;
                
            } else {
                odbgi("packet[{}]: unknown type=[{}]", pkt.number, pkt.type);
            }
            
            errcode = tlv_.next(pkt);
        }
        return (ErrorCode)errcode;
    }
    
    ErrorCode process(const std::string& filename, const FrameFunc& func){
        int errcode = tlv_.open(filename);
        if(errcode < 0){
            return (ErrorCode)errcode;
        }
        
        while (errcode >= 0 && errcode != kReachFileEnd ) {
            errcode = processUntilFrame(func);
        }

        return (ErrorCode)errcode;
    }
    
private:
    TLVFile tlv_;
    NSDP remoteSDP_;
    NRTPReceiver receiver_;
    std::vector<MediaFlow> flows_;
    int64_t  firstTime_ = -1;
    int audioFlowIndex_ = -1;
};


class XSWTLVProber {
public:
    struct ProbeFlow{
        ProbeFlow():mainCodec(NCodec::UNKNOWN){}
        int index = -1;
        NRTPCodec mainCodec;
        int64_t numDecodableFrames = 0;
        std::vector<NVideoSize> historyVideoSizes;
        NVideoSize maxVideoWin;
    };
public:

    NError probe(const std::string& filename){
        std::vector<ProbeFlow> flows;
        return probe(filename, flows);
    }
    NError probe(const std::string& filename, std::vector<ProbeFlow>& flows){
        XSWTLVDemuxer demuxer;
        int errcode = demuxer.process(filename,
                                  [this, &flows](XSWTLVDemuxer::MediaFlow& mflow, NMediaFrame::Unique& frame){
                                      processFrame(checkAddFlow(mflow, flows), frame);
                                  });
        odbgi("probe result: [{}]-[{}]", errcode, XSWTLVDemuxer::getNameForErrorCode(errcode));
        if(errcode >= 0){
            for(auto& flow : flows){
                std::string str = fmt::format("  flow[{}]: codec={}, decodable-frames={}",
                                              flow.index,
                                              flow.mainCodec.name,
                                              flow.numDecodableFrames);
                
                if(NCodec::GetMediaForCodec(flow.mainCodec.type) == NMedia::Video ){
                    str += fmt::format(", max-video-win={}x{}",
                                       flow.maxVideoWin.width, flow.maxVideoWin.height);
                }
                odbgi("{}", str);
                
                if(flow.historyVideoSizes.size() > 0){
                    for(size_t n = 0; n < flow.historyVideoSizes.size(); ++n){
                        auto& o = flow.historyVideoSizes[n];
                        odbgi("    video-size[{}]={}x{}", n, o.width, o.height);
                    }
                }
            }
        }
        return NError(errcode, XSWTLVDemuxer::getNameForErrorCode(errcode));
    }
    
private:
    ProbeFlow& checkAddFlow(XSWTLVDemuxer::MediaFlow& mflow, std::vector<ProbeFlow>& flows){
        // check add flow
        while(flows.size() <= mflow.rtpFlow->index){
            flows.emplace_back();
            if((flows.size()-1) == mflow.rtpFlow->index){
                flows.back().index = mflow.rtpFlow->index;
                flows.back().mainCodec = *mflow.rtpFlow->mainCodec;
            }
        }
        return flows[mflow.rtpFlow->index];
    }

    void processFrame(ProbeFlow& flow, NMediaFrame::Unique& frame){
        ++flow.numDecodableFrames;
        if(frame->getMediaType() == NMedia::Video){
            NVideoFrame * video_frame = (NVideoFrame *)frame.get();
            processVideoFrame(flow, video_frame);
        }
    }

    void processVideoFrame(ProbeFlow& flow, NVideoFrame * video_frame){
        if(video_frame->isKeyframe()){
            const NVideoSize& vsize = video_frame->videoSize();
            if(flow.historyVideoSizes.empty()
               || vsize.width != flow.historyVideoSizes.back().width
               || vsize.height != flow.historyVideoSizes.back().height){
                // TODO: test with mobile
                odbgi("change video size, flow=[{}-{}], vsize=[{}]",
                      flow.index, flow.mainCodec.name, vsize);
                flow.historyVideoSizes.emplace_back(vsize);
            }

            if(vsize.width > flow.maxVideoWin.width){
                flow.maxVideoWin.width = vsize.width;
            }

            if(vsize.height > flow.maxVideoWin.height){
                flow.maxVideoWin.height = vsize.height;
            }
        }
    }

};

class XSWTLVPlayer {
public:
    struct Flow{
        Flow():mainCodec(NCodec::UNKNOWN){}
        
        Flow(Flow &&other)
        : mainCodec(NCodec::UNKNOWN)
        , decodeUser_(other.decodeUser_){
            other.decodeUser_ = nullptr;
        }
        
        Flow(Flow &other) = delete;
        
        virtual ~Flow(){
            close();
        }
        
        int open(){
            close();
            decodeUser_ = new TLVDecodeUser("decode1", 640, 480, {1, 1000}, 0);
            int ret = decodeUser_->window_->open();
            if(ret){
                odbge("error: open decode windows, ret=%d", ret);
                return ret;
            }
            
            ret = decodeUser_->decoder_->open();
            if(ret){
                odbge("error: open decoder , ret=%d", ret);
                return ret;
            }
            
            return ret;
        }
        void close(){
            if(decodeUser_){
                delete decodeUser_;
                decodeUser_ = nullptr;
            }
        }
        
        NRTPCodec mainCodec;
        int index = -1;
        //NRTPTime rtptime;
        TLVDecodeUser * decodeUser_ = nullptr;
        int64_t lastPTS = -1;
        int64_t lastRenderTime = -1;
    };
public:
    
    NError play(const std::string& filename){
        XSWTLVDemuxer demuxer;
        std::vector<Flow> flows;
        int errcode = demuxer.process(filename,
                                      [this, &flows](XSWTLVDemuxer::MediaFlow& mflow, NMediaFrame::Unique& frame){
                                          processFrame(checkAddFlow(mflow, flows), mflow, frame);
                                      });
        return NError(errcode, XSWTLVDemuxer::getNameForErrorCode(errcode));
    }
    
private:
    Flow& checkAddFlow(XSWTLVDemuxer::MediaFlow& mflow, std::vector<Flow>& flows){
        // check add flow
        while(flows.size() <= mflow.rtpFlow->index){
            flows.emplace_back();
        }
        Flow& flow = flows[mflow.rtpFlow->index];
        if(flow.index < 0){
            flow.index = mflow.rtpFlow->index;
            flow.mainCodec = *mflow.rtpFlow->mainCodec;
            if(mflow.rtpFlow->type == NMedia::Video){
                flow.open();
            }
        }
        return flow;
    }
    
    void processFrame(Flow& flow, XSWTLVDemuxer::MediaFlow& mflow, NMediaFrame::Unique& frame){
        if(frame->getMediaType() == NMedia::Video){
            NVideoFrame * video_frame = (NVideoFrame *)frame.get();
            processVideoFrame(flow, video_frame);
        }
    }
    
    void processVideoFrame(Flow& flow, NVideoFrame * video_frame){
        if(video_frame->isKeyframe()){
            const NVideoSize& vsize = video_frame->videoSize();
            
        }
        this->decode(flow, video_frame);
    }
    
    void decode(Flow& rec_flow, NMediaFrame * frame){
        int64_t pts = frame->getTime();
        if(rec_flow.lastRenderTime < 0){
            rec_flow.lastPTS = pts;
            rec_flow.lastRenderTime = NUtil::get_now_ms();
        }else{
            int64_t now_ms = NUtil::get_now_ms();
            int64_t elapsed = now_ms - rec_flow.lastRenderTime;
            int64_t duration = pts - rec_flow.lastPTS;
            if(duration > elapsed){
                NUtil::sleep_ms(duration-elapsed);
            }
            rec_flow.lastPTS = pts;
            rec_flow.lastRenderTime = now_ms;
        }
        
        int frameNo = 1;
        int layerId = 0;
        int ret = rec_flow.decodeUser_->decode(pts
                                               , frame->data()
                                               , frame->size()
                                               , frameNo, layerId, false);
        SDL::FlushEvent();
        if(ret == 0){
            //NUtil::sleep_ms(33);
        }else{
            odbge("error: decode fail with {}", ret);
        }
    }
};


int lab_xswtlv_player_main(int argc, char* argv[]){
    spdlog::set_pattern("|%H:%M:%S.%e|%n|%L| %v");
    spdlog::set_level(spdlog::level::debug);
    
    int ret = SDL_Init(SDL_INIT_VIDEO) ;
    if(ret){
        odbge( "Could not initialize SDL - {}\n", SDL_GetError());
        return -1;
    }
    ret = TTF_Init();
    
    {
        //const char * filename = "/Users/simon/Downloads/tmp/tmp/raw-tlv/rtx-fec-01.tlv";
        const char * filename = "/Users/simon/Downloads/tmp/tmp/raw-tlv/rtx-fec-02-nodrop.tlv";
        XSWTLVPlayer().play(filename);
        //XSWTLVProber().probe(filename);
    }
    
    TTF_Quit();
    SDL_Quit();
    return ret;
}





int lab_demo_main(int argc, char* argv[]){

    //return lab_vp8_main(argc, argv);
    
    //return NObjDumperTester().test();

    //return  test_logger();
    
    //return test_time();
    
    return lab_xswtlv_player_main(argc, argv);

}

