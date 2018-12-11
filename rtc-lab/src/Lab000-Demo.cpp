

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

#include "Lab010-VP8.hpp"
#include "VP8Framework.hpp"
#include "SDLFramework.hpp"
#include "NRTPMap.hpp"
#include "NRTPReceiver.hpp"
#include "Lab000-Demo.hpp"
#include <iostream>

static NObjDumper maindumper(std::cout, "|   main|I| ");
#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
//#define DUMPER(x) NCoutDumper::get("|   main|I| " x)
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
    VP8Decoder * decoder_ = NULL;
    int layerId_ = 0;
    TLVDecodeUser(const std::string& name, int w, int h, const vpx_rational& timebase, int layerId = 0)
    : User(name, w, h, timebase)
    , decoder_(new VP8Decoder())
    , layerId_(layerId){
        
    }
    virtual ~TLVDecodeUser(){
        if(decoder_){
            delete decoder_;
            decoder_ = NULL;
        }
    }
    
    void decode(vpx_codec_pts_t pts, const uint8_t * frameData, size_t frameLength, int frameNo, int layerId, bool skip){
        
        if(layerId > layerId_){
            return ;
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
                odbgi("  decode=[%s]", txt);
            }
        }
    }
};


class XSWTLVPlayer{
    class RecordFlow{
    public:
        NRTPFlow rtpFlow;
        NRTPUlpfecWindow packetWin;
        NRTPDepackVP8 depacker;
    };
    
    NSDP remoteSDP_;
    NRTPReceiver receiver_;
    std::vector<RecordFlow> flows_;
    TLVDecodeUser decodeUser_;
    int64_t  firstTime_ = -1;
    
    const char * RAW_RTP_STR    = "  raw RTP   : ";
    const char * BIND_STR       = "  bind RTP  : ";
    const char * DEMUX_STR      = "  demux RTP : ";
    #define  RAW_RTCP_STR         "  raw RTCP  : "
    
public:
    XSWTLVPlayer():decodeUser_("decode1", 640, 480, {1, 90000}, 0){
        int ret = decodeUser_.window_->open();
        if(ret){
            odbge("error: open decode windows, ret=%d", ret);
        }
        ret = decodeUser_.decoder_->open();
        if(ret){
            odbge("error: open decoder , ret=%d", ret);
        }
    }
    
    void dump(const std::string& prefix
              , const NRTPData* rtpd
              , const NRTPMediaBrief* brief
              , const NRTPFlow * flow){
        odbgi("%s%s-%s-[%zu]"
              , prefix.c_str()
              , rtpd->header.Dump("").c_str()
              , brief->Dump("").c_str()
              , rtpd->payloadLen);
    }
    
    void dump(const std::string& prefix
              , const NRTPData* rtpd){
        odbgi("%s%s-[%zu]"
              , prefix.c_str()
              , rtpd->header.Dump("").c_str()
              , rtpd->payloadLen);
    }
    
    static void dump(const NSDP& sdp){
        std::list<std::string> lines;
        sdp.dump("", lines);
        for(auto line : lines){
            odbgi("%s", line.c_str());
        }
    }
    
    void decode(NMediaFrame * frame){
        int frameNo = 1;
        int layerId = 0;
        decodeUser_.decode(frame->getTime()
                           , frame->data()
                           , frame->size()
                           , frameNo, layerId, false);
        SDL::FlushEvent();
        usleep(33*1000);
    }
    
    int processRTP(int64_t&          recv_ts,
                   const uint8_t *   data_ptr,
                   size_t            data_len){
        
        //receiver.inputRTP(recv_ts, -1, data_ptr, data_len, dumpRTPFuncObj);
        
        NRTPData rtpd;
        
        //Parse RTP header and get payload
        auto pos = rtpd.parse(data_ptr, data_len);
        if (!pos){
            odbge("fail to parse rtp");
            return -1;
        }
        rtpd.timeMS = recv_ts;
        dump(RAW_RTP_STR, &rtpd);
        
        NRTPMediaDetail detail;
        NRTPMediaBrief brief;
        int ret = receiver_.bindMedia(-1, rtpd, detail, brief);
        if(ret < 0){
            //dump(RAW_RTP_STR, &rtpd);
            odbge("fail to bindMedia");
            return -1;
        }
        
        while(flows_.size() <= detail.source->flow->index){
            flows_.emplace_back();
        }
        
        RecordFlow& rec_flow = flows_[detail.source->flow->index];
        if(rec_flow.rtpFlow.index < 0){
            rec_flow.rtpFlow = *detail.source->flow;
            odbgi("add flow [%d]-[%s]", rec_flow.rtpFlow.index, NMedia::GetNameFor(rec_flow.rtpFlow.type));
        }
        
//        receiver_.demuxRTX(detail, brief, rtpd);
//        if(rec_flow.rtpFlow.type == NMedia::Video){
//            rec_flow.packetWin.insertPacket(rtpd, brief.codecType, detail.desc->codecs);
//        }
        
        
        dump(BIND_STR, &rtpd, &brief, detail.source->flow);
            
        ret = receiver_.demuxRTP(detail, rtpd, brief
                                 , [this, &rec_flow, &brief](const NRTPData* rtpd
                                          , const NRTPCodec* codec
                                          , const NRTPFlow * flow){
                                     brief.codecType = codec->type;
                                     dump(DEMUX_STR, rtpd, &brief, flow);
                                     if(rec_flow.rtpFlow.type == NMedia::Video){
                                         rec_flow.packetWin.insertPacket(*rtpd, *codec);
                                     }
                                     if(codec->type != NCodec::VP8){
                                         return;
                                     }
                                     
                                     
                                 });
        if(ret == 0 && rec_flow.rtpFlow.type == NMedia::Video){
            NRTPSeq from_seq = rec_flow.depacker.nextSeq();
            if(rec_flow.depacker.startPhase()){
                from_seq = rec_flow.packetWin.packetWin().startSeq();
            }
            rec_flow.packetWin.packetWin()
            .traverseUntilEmpty(from_seq
                                , [this, &rec_flow] (NCircularBufferSizeT n, NRTPPacketWindow::reference slot) -> int{
                                    NVP8PLDescriptor desc;
                                    auto ret = rec_flow.depacker.AddPacketVP8(slot.rtpd, desc);
                                    odbgi("%s", desc.Dump("  VP8-desc=").c_str());
                                    if(ret < 0){
                                        odbge("error VP8 packet with [%d]-[%s] =================", ret
                                              , NRTPDepackVP8::getNameForState(ret).c_str());
                                    }else{
                                        
                                        if(ret == NRTPDepackVP8::kComplete){
                                            auto frame = rec_flow.depacker.getFrame();
                                            odbgi("frame=[time=%lld, size=%zu]", frame->getTime(), frame->size());
                                            this->decode(frame);
                                        }
                                    }
                                    return 0;
            });
        }
        return ret;
    }// processRTP
    
    int processRTCP(int64_t&          recv_ts,
                   const uint8_t *   data_ptr,
                   size_t            data_len){
        auto state = NRtcp::Parser::DumpPackets(data_ptr, data_len, DUMPER());
        if(state != NRtcp::Parser::kNoError){
            odbge("error RTCP with [%d]-[%s]", state, NRtcp::Parser::getNameForState(state).c_str());
        }
        return receiver_.inputRTCP(recv_ts, -1, data_ptr, data_len);
    }
    
    int onData(int64_t          packet_num,
              int               dtype,
              const uint8_t *   data_ptr,
              size_t            data_len){
        int ret = 0;
        if(dtype == TLV_TYPE_CODEC) {
            odbgi("packet[%lld]: codec=[size=%zu]", packet_num, data_len);
            
        } else if(dtype == TLV_TYPE_SDP) {
            std::ostringstream oss;
            int ret = remoteSDP_.parse(data_ptr, data_len, oss);
            if(ret){
                odbgi("packet[%lld]: SDP=[%.*s]", packet_num, (int)data_len, data_ptr);
                odbge("fail to parse remote SDP, ret=[%d]-[%s]", ret, oss.str().c_str());
            }else{
                odbgi("parsed remote SDP OK");
                dump(remoteSDP_);
                receiver_.setRemoteSDP(remoteSDP_);
            }
            
        } else if(dtype == TLV_TYPE_RTP) {
            if(data_len < 8){
                odbge("too short of RTP, size=%zu", data_len);
            }else{
                int64_t recv_ts = NUtil::get8(data_ptr, 0);
                if(firstTime_ < 0){
                    firstTime_ = recv_ts;
                }
                odbgi("packet[%lld]: RTP=[time=%lld(%lld ms), size=%zu]", packet_num, recv_ts, recv_ts-firstTime_, data_len-8);
                processRTP(recv_ts, data_ptr+8, data_len-8);
            }
            
        } else if (dtype == TLV_TYPE_RTCP) {
            if(data_len < 8){
                odbge("too short of RTCP, size=%zu", data_len);
            }else{
                int64_t recv_ts = NUtil::get8(data_ptr, 0);
                if(firstTime_ < 0){
                    firstTime_ = recv_ts;
                }
                odbgi("packet[%lld]: RTCP=[time=%lld(%lld ms), size=%zu]", packet_num
                      , recv_ts
                      , recv_ts-firstTime_
                      , data_len-8);
                processRTCP(recv_ts, data_ptr+8, data_len-8);
            }
            
        } else if (dtype == TLV_TYPE_END) {
            odbgi("packet[%lld]: END", packet_num);
            return 0;
            
        } else {
            odbgi("packet[%lld]: unknown type=[%d]", packet_num, dtype);
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
            odbge("process tlv ret=[%d]-[%s]", ret, oss.str().c_str());
        }
        
        return ret;
        
    }
    
};

int lab_xswtlv_player_main(int argc, char* argv[]){
    int ret = SDL_Init(SDL_INIT_VIDEO) ;
    if(ret){
        odbge( "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    ret = TTF_Init();
    
    {
        XSWTLVPlayer player;
        SDL::FlushEvent();
        //usleep(10*1000*1000);
        //muxer.process("/Users/simon/Downloads/tmp/tmp/raw-tlv/rtx-fec-01.tlv");
        player.process("/Users/simon/Downloads/tmp/tmp/raw-tlv/rtx-fec-02-nodrop.tlv");
    }
    
    TTF_Quit();
    SDL_Quit();
    return ret;
}

int lab_demo_main(int argc, char* argv[]){

    //return lab_vp8_main(argc, argv);
    
    //return NObjDumperTester().test();
    
    return lab_xswtlv_player_main(argc, argv);

}

