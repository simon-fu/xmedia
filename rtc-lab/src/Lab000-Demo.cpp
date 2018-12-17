

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

static auto main_logger = spdlog::stdout_logger_mt("main");
static NObjDumperLog maindumper(*main_logger);
#define odbgd(FMT, ARGS...) main_logger->debug(FMT, ##ARGS)
#define odbgi(FMT, ARGS...) main_logger->info(FMT, ##ARGS)
#define odbge(FMT, ARGS...) main_logger->error(FMT, ##ARGS)
#define DUMPER() maindumper


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


class XSWTLVPlayer{
    class RecordFlow{
    public:
        RecordFlow(){
        }
        
        RecordFlow(RecordFlow &&other)
        :rtpFlow(other.rtpFlow)
        , decodeUser_(other.decodeUser_)
        , depacker(other.depacker){
            other.rtpFlow = nullptr;
            other.decodeUser_ = nullptr;
        }
        
        RecordFlow(RecordFlow &other) = delete;
        
        virtual ~RecordFlow(){
            close();
        }
        
        NRTPIncomingFlow * rtpFlow = nullptr;
        NRTPTime rtptime;
        TLVDecodeUser * decodeUser_ = nullptr;
        int64_t lastPTS = -1;
        int64_t lastRenderTime = -1;
        NRTPDepackVP8 depacker;
        
        int open(){
            close();
            decodeUser_ = new TLVDecodeUser("decode1", 640, 480, {1, 90000}, 0);
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
    };
    
    NSDP remoteSDP_;
    NRTPReceiver receiver_;
    std::vector<RecordFlow> flows_;
    int audioFlowIndex_ = -1;
    int64_t  firstTime_ = -1;
    
    const char * RAW_RTP_STR    = "  raw RTP   : ";
    const char * BIND_STR       = "  bind RTP  : ";
    const char * DEMUX_STR      = "  demux RTP : ";
    #define  RAW_RTCP_STR         "  raw RTCP  : "
    
public:
    XSWTLVPlayer(){
        odbgi("flows_.size()={}", flows_.size());
        odbgi("flows_.capacity()={}", flows_.capacity());
    }
    
    
    static void dump(const NSDP& sdp){
        std::list<std::string> lines;
        sdp.dump("", lines);
        for(auto line : lines){
            odbgi("{}", line.c_str());
        }
    }
    
    void decode(RecordFlow& rec_flow, NMediaFrame * frame, const NRTPTime & rtptime){
        uint32_t rtpts = rtptime.rtpTimestamp();
        int64_t pts = NNtp::NTP2Milliseconds(rtptime.localNTP());
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
        int ret = rec_flow.decodeUser_->decode(rtpts
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
    
    void pullAndDecode(RecordFlow& rec_flow){
        NVP8PLDescriptor desc;
        const NRTPData * rtpd = rec_flow.rtpFlow->pullRTPInOrder();
        while(rtpd){
            auto ret = rec_flow.depacker.AddPacketVP8(*rtpd, desc);
            odbgi("{}", desc.Dump("  VP8-desc=").c_str());
            if(ret < 0){
                odbge("error VP8 packet with [{}]-[{}] =================", ret
                      , NRTPDepackVP8::getNameForState(ret).c_str());
            }else{
                if(ret == NRTPDepackVP8::kComplete){
                    auto frame = rec_flow.depacker.getFrame();
                    
                    auto& remoteNTP = rec_flow.rtpFlow->remoteNTP;
                    uint32_t rtpts = (uint32_t)frame->getTime();
                    uint64_t ntpts = remoteNTP.convert(rtpts, 90000);
                    uint64_t local_ntpts = rec_flow.rtptime.input(rec_flow.depacker.firtTimeMS(),
                                                                  rtpts,
                                                                  90000,
                                                                  ntpts);
                    int64_t diff = 0;
                    if(audioFlowIndex_ >= 0){
                        auto& audioFlow = flows_[audioFlowIndex_];
                        rec_flow.rtptime.estimate(audioFlow.rtptime, diff);
                    }
                    
                    odbgi("video ntpts: remote={}, local={}, est={}, diff_audio={}ms",
                          NNtp::toHuman2String(ntpts),
                          NNtp::toHuman2String(local_ntpts),
                          NNtp::toHuman2String(rec_flow.rtptime.localNTP()),
                          NNtp::DeltaNTP2Milliseconds(diff) );
                    this->decode(rec_flow, frame, rec_flow.rtptime);
                }
            }
            rtpd = rec_flow.rtpFlow->pullRTPInOrder();
        }
    }
    
    RecordFlow& checkFlow(NRTPIncomingFlow * rtp_flow){
        // check add record flow
        while(flows_.size() <= rtp_flow->index){
            flows_.emplace_back();
        }
        RecordFlow& rec_flow = flows_[rtp_flow->index];
        if(!rec_flow.rtpFlow){
            odbgi("add flow [{}]-[{}]", rtp_flow->index, NMedia::GetNameFor(rtp_flow->type));
            rec_flow.rtpFlow = rtp_flow;
            
            if(rec_flow.rtpFlow->type == NMedia::Video){
                rec_flow.open();
            }else if(rec_flow.rtpFlow->type == NMedia::Audio){
                audioFlowIndex_ = rtp_flow->index;
            }
        }
        return rec_flow;
    }
    
    int processRTP(int64_t&          recv_ts,
                   const uint8_t *   data_ptr,
                   size_t            data_len){
        
        RecordFlow * prec_flow = nullptr;
        
        int ret = receiver_.inputRTP(recv_ts, -1, data_ptr, data_len
                                 , [this, &prec_flow](const NRTPReceiver::IncomingRTPInfo * info){
                                     auto rtpd = info->rtpd;
                                     RecordFlow& rec_flow = checkFlow(info->flow);
                                     if(audioFlowIndex_ == rec_flow.rtpFlow->index){
                                         uint64_t remote_ntp = rec_flow.rtpFlow->remoteNTP.convert(rtpd->header.timestamp, info->codec->clockrate);
                                         rec_flow.rtptime.input(rtpd->timeMS,
                                                                rtpd->header.timestamp,
                                                                info->codec->clockrate,
                                                                remote_ntp);
                                     }
                                     prec_flow = &rec_flow;
                                 });
        if(prec_flow){
            if(prec_flow->decodeUser_){
                pullAndDecode(*prec_flow);
            }
        }
        return ret;
    }// processRTP
    
    int processRTCP(int64_t&          recv_ts,
                   const uint8_t *   data_ptr,
                   size_t            data_len){
//        auto state = NRtcp::Parser::DumpPackets(data_ptr, data_len, DUMPER());
//        if(state != NRtcp::Parser::kNoError){
//            odbge("error RTCP with [{}]-[{}]", state, NRtcp::Parser::getNameForState(state).c_str());
//        }
        return receiver_.inputRTCP(recv_ts, -1, data_ptr, data_len);
    }
    
    int onData(int64_t          packet_num,
              int               dtype,
              const uint8_t *   data_ptr,
              size_t            data_len){
        int ret = 0;
        if(dtype == TLV_TYPE_CODEC) {
            odbgi("packet[{}]: codec=[size={}]", packet_num, data_len);
            
        } else if(dtype == TLV_TYPE_SDP) {
            std::ostringstream oss;
            int ret = remoteSDP_.parse(data_ptr, data_len, oss);
            if(ret){
                //odbgi("packet[%lld]: SDP=[%.*s]", packet_num, (int)data_len, data_ptr);
                odbgi("packet[{}]: SDP=[{}]", packet_num, data_ptr);
                odbge("fail to parse remote SDP, ret=[{}]-[{}]", ret, oss.str().c_str());
            }else{
                odbgi("parsed remote SDP OK");
                dump(remoteSDP_);
                receiver_.setRemoteSDP(remoteSDP_);
            }
            
        } else if(dtype == TLV_TYPE_RTP) {
            if(data_len < 8){
                odbge("too short of RTP, size={}", data_len);
            }else{
                int64_t recv_ts = NUtil::get8(data_ptr, 0);
                if(firstTime_ < 0){
                    firstTime_ = recv_ts;
                }
                odbgi("packet[{}]: RTP=[time={}({} ms), size={}]", packet_num, recv_ts, recv_ts-firstTime_, data_len-8);
                processRTP(recv_ts, data_ptr+8, data_len-8);
            }
            
        } else if (dtype == TLV_TYPE_RTCP) {
            if(data_len < 8){
                odbge("too short of RTCP, size={}", data_len);
            }else{
                int64_t recv_ts = NUtil::get8(data_ptr, 0);
                if(firstTime_ < 0){
                    firstTime_ = recv_ts;
                }
                odbgi("packet[{}]: RTCP=[time={}({} ms), size={}]", packet_num
                      , recv_ts
                      , recv_ts-firstTime_
                      , data_len-8);
                processRTCP(recv_ts, data_ptr+8, data_len-8);
            }
            
        } else if (dtype == TLV_TYPE_END) {
            odbgi("packet[{}]: END", packet_num);
            return 0;
            
        } else {
            odbgi("packet[{}]: unknown type=[{}]", packet_num, dtype);
        }
        return ret;
    }
public:
    
    int process(const std::string& filename){
        
        std::ostringstream oss;
        int ret = TLVFile::process(filename, oss, [this]  (int64_t packet_num,
                                                       int dtype,
                                                       const uint8_t * data_ptr,
                                                       size_t  data_len
                                                       )-> int{

            return this->onData(packet_num, dtype, data_ptr, data_len);
        });
        if(ret < 0){
            odbge("process tlv ret=[{}]-[{}]", ret, oss.str().c_str());
        }
        
        return ret;
        
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
        XSWTLVPlayer player;
        //muxer.process("/Users/simon/Downloads/tmp/tmp/raw-tlv/rtx-fec-01.tlv");
        player.process("/Users/simon/Downloads/tmp/tmp/raw-tlv/rtx-fec-02-nodrop.tlv");
    }
    
    TTF_Quit();
    SDL_Quit();
    return ret;
}

int test_logger(){
    auto console = spdlog::stdout_logger_mt("console");
    console->info("Welcome to spdlog!");
    console->info("logger: spdlog-{}.{}.{}", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
    
    std::array<char, 40> buf;
    for(size_t i = 0; i < buf.size(); ++i){
        buf[i] = i;
    }
    //console->info("Binary example: {}", spdlog::to_hex(buf));
    for(size_t i = 0; i < buf.size(); ){
        auto len = std::min((size_t)16, buf.size()-i);
        //console->info("Another binary example:{:n}", spdlog::to_hex(std::begin(buf)+i, std::begin(buf) + i+len));
        console->info("Another binary example:{}", spdlog::to_hex(std::begin(buf)+i, std::begin(buf) + i+len));
        i+=len;
    }
    
    int i = 0;
    fmt::memory_buffer raw;
    fmt::format_to(raw, "i1={}", i);
    fmt::format_to(raw, ",i2={}{{", i);
    char c = 'c';
    raw.append(&c, (&c)+1);

    //std::string s1 = format(fmt("{}"), 42);  // good
    //std::string s2 = format(fmt("{2}"), 42); // error
    //std::string s3 = format(fmt("{"), 42); // error
    
    console->log(spdlog::level::info, raw.data());
    raw.clear();
    fmt::format_to(raw, "n={}", 10);
    c = '\0';
    raw.append(&c, (&c)+1);
    console->log(spdlog::level::info, raw.data());
    return 0;
}

inline void print_time(int64_t time_ms){
    odbgi("time_ms={}, seconds={}, days={}, months={}, years={}"
          , time_ms
          , time_ms/1000
          , time_ms/1000/60/60/24
          , time_ms/1000/60/60/24/30
          , time_ms/1000/60/60/24/365);
}

int test_time(){
    print_time(NUtil::get_ms_monotonic());
    int64_t start = NUtil::get_now_ms();
    print_time(start);
    NUtil::sleep_ms(1200);
    int64_t elapsed = NUtil::get_now_ms() - start;
    print_time(elapsed);
    return 0;
}

int lab_demo_main(int argc, char* argv[]){

    //return lab_vp8_main(argc, argv);
    
    //return NObjDumperTester().test();

    //return  test_logger();
    
    //return test_time();
    
    return lab_xswtlv_player_main(argc, argv);

}

