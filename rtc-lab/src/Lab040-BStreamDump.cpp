
#include <string>
#include <map>
#include <vector>
#include <list>
#include <sstream>
#include <random>
#include <stdio.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/http.h>
#include "event2/keyvalq_struct.h"
//#include <opus/opus.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

//extern "C"{
//    #include "vpx/vpx_encoder.h"
//    #include "vpx/vp8cx.h"
//    #include "vpx/vpx_decoder.h"
//    #include "vpx/vp8dx.h"
//}

//#include "SDLFramework.hpp"
#include "Lab040-BStreamDump.hpp"
#include "XSocket.h"
#include "xtlv_file.h"
#include "BStreamProto.h"


#define odbgd(FMT, ARGS...) do{  printf("|%7s|D| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbgi(FMT, ARGS...) do{  printf("|%7s|I| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)
#define odbge(FMT, ARGS...) do{  printf("|%7s|E| " FMT, "main", ##ARGS); printf("\n"); fflush(stdout); }while(0)

using namespace rapidjson;

static inline
char generate_random_char(){
    static std::random_device rd;
    return 'a' + rd()%26;
}

static inline
std::string generate_sub_id(){
    static uint64_t next_id = 0;
    static char prefix[] = {'s', 'u', 'b'};
    static int prefix_length = sizeof(prefix)/sizeof(prefix[0]);
    
    char buf[64];
    size_t bufsz = sizeof(buf)/sizeof(buf[0]);
    ++next_id;
    snprintf(buf, bufsz, "%.*s-%lld-%c%c%c"
                    , prefix_length, prefix
                    , next_id
                    , generate_random_char()
                    , generate_random_char()
                    , generate_random_char()
                    );
    return buf;
}

static inline
uint32_t generate_ssrc(){
    static std::random_device rd;
    return rd();
}

static
int64_t get_now_ms(){
    unsigned long milliseconds_since_epoch =
    std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();
    return milliseconds_since_epoch;
};


struct AppConfig{
    std::string httpLocalIp; // = "172.17.1.216";
    int httpLocalPort = 8800;
    std::string httServiceUrl; // = "http://172.17.1.216:8800/";
    std::string mediaLocalIp ; // = "172.17.1.216";
    std::string workPath = "/tmp";
    void initWithIp(const std::string& ip){
        httpLocalIp = ip;
        char buf[512];
        sprintf(buf, "http://%s:%d/", ip.c_str(), httpLocalPort);
        httServiceUrl = buf;
        mediaLocalIp = ip;
        
    }
};

#define MAX_SSRCS 6
class SsrcGroup{
    uint32_t ssrcs_[MAX_SSRCS];
    int num_ = 0;
public:
    SsrcGroup(){
        for(int i = 0; i < MAX_SSRCS; ++i){
            ssrcs_[i] = 0;
        }
    }
    int add(uint32_t ssrc){
        if(num_ == MAX_SSRCS) return -1;
        ssrcs_[num_] = ssrc;
        ++num_;
        return 0;
    }
    int groupNum(){
        return (num_>>1);
    }
    
    uint32_t mediaSSRC(int group){
        if(group >= (num_>>1)){
            return 0;
        }
        return ssrcs_[group<<1];
    }
    
    bool equals(const SsrcGroup& other) const{
        if(other.num_ != this->num_) return false;
        for(int i = 0; i < num_; ++i){
            if(other.ssrcs_[i] != this->ssrcs_[i]) return false;
        }
        return true;
    }
};

struct Session{
    std::string sessionId_;
    std::string confrId_;
    std::string streamId_;
    std::string memName_;
    std::string subRtcId_;
    std::string pubRtcId_;
    SsrcGroup peerAudioSSRCs_;
    SsrcGroup peerVideoSSRCs_;
    
    uint32_t localAudioSSRC = 0;
    uint32_t localVideoSSRC = 0;
    XSocket * udpsock_ = NULL;
    int localPort = 0;
    int64_t recvBytes_ = 0;
    int64_t lastActiveTime_ = 0;
    XSockAddress * remoteAddr_ = NULL;
    
    Session(){
        this->extendLife();
    }
    
    virtual ~Session(){
        if(udpsock_){
            delete udpsock_;
            udpsock_ = NULL;
        }
        if(remoteAddr_){
            delete remoteAddr_;
            remoteAddr_ = NULL;
        }
    }
    
    void extendLife(){
        lastActiveTime_ = get_now_ms();
    }
    
    int64_t timeSinceActive(int64_t now_ms){
        return now_ms - lastActiveTime_;
    }
    
    std::string toString(){
        std::string str;
        str.append("[")
        .append("id=").append(sessionId_)
        .append(", stream=").append(streamId_)
        .append(", subRtc=").append(subRtcId_)
        .append(", pubRtc=").append(pubRtcId_)
        .append(", link=").append(udpsock_->getLocalAddress()->getString())
        .append("]");
        return str;
    }
};
typedef std::map<std::string, Session *> SessionMap;



class SessionManager : public XSocketCallback{
    XSocketFactory * factory_ = NULL;
    AppConfig appCfg_;
    SessionMap sessions_;
    tlv_file_t dump_tlv_ = NULL;
protected:

public:
    SessionManager(XSocketFactory * factory, AppConfig& cfg)
    : factory_(factory)
    , appCfg_(cfg){
        
    }
    virtual ~SessionManager(){
        for(auto o : sessions_){
            delete o.second;
        }
        sessions_.clear();
        if(dump_tlv_){
            tlv_file_close(dump_tlv_);
            dump_tlv_ = NULL;
        }
    }
    
    void demuxRTP(Session * session, uint8_t *data, int size){
        
        // see RFC3550 for rtp
        bool ext_exist = (data[0]>>4&1) == 1;
        int num_csrcs = data[0]&0xf;
        int header_size = 12 + num_csrcs*4;
        uint8_t rtp_pt = data[1]&0x7f;
        uint16_t rtp_seq = data[2]<<8 | data[3];
        
        // see RFC5285 for rtp header extensions
        int ext_size = 0;
        if (ext_exist) {
            ext_size = (data[header_size+2] << 8) | (data[header_size+3]);
            ext_size = ext_size*4 + 4;
        }
        
        uint8_t *rtp_payload = data+header_size+ext_size;
        size_t rtp_payload_len = size-header_size-ext_size;
        odbgi("  rtp: pt=%d, seq=%d, payload_len=%ld", rtp_pt, rtp_seq, rtp_payload_len);
    }
    
    void onSocketRecvDataReady(XSocket *xsock) {
        Session * session = static_cast<Session*> (xsock->getUserData());
        uint8_t buf[1800];
        uint8_t * dataPtr = buf + 4;
        int dataSize = sizeof(buf) - 4;
        bool gotUDP = false;
        while(1){
            int bytesRecv = session->udpsock_->recv(dataPtr, dataSize);
            if(bytesRecv <= 0) break ;
            session->recvBytes_ += bytesRecv;
            if(!session->remoteAddr_){
                session->remoteAddr_ = XSockAddress::newAddress(session->udpsock_->getRemoteAddress());
                odbgi("session[%s] got remote address [%s], bytes %d", session->sessionId_.c_str()
                      , session->remoteAddr_->getString().c_str()
                      , bytesRecv);
            }
            if(dump_tlv_){
                int64_t now_ms = get_now_ms();
                uint8_t tmpbuf[16];
                be_set_i64(now_ms, tmpbuf);
                be_set_u32(session->localPort, tmpbuf+8);
                tlv_file_write2(dump_tlv_, TLV_TYPE_TIME_PORT_RECVUDP, 12, tmpbuf, bytesRecv, dataPtr);
                //odbgi("dump: type=%d, ts=%lld, bytes=%d", TLV_TYPE_TIME_PORT_RECVUDP, now_ms, bytesRecv);
            }
            //demuxRTP(session, buf, bytesRecv);
            gotUDP = true;
        }
        if(gotUDP){
            session->extendLife();
        }
    }
    
    void timeCheck(){
        int64_t now_ms = get_now_ms();
        std::list<Session *> timeoutList;
        for(auto& o : sessions_){
            Session * session = o.second;
            if(session->timeSinceActive(now_ms) >= 60*1000){
                timeoutList.push_back(session);
            }else{
                

                int audioRRs = 0;
                int videoRRs = 0;
                if(session->remoteAddr_){
                    /*
                            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                     header |V=2|P|    RC   |   PT=RR=201   |             length            |
                            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                            |                     SSRC of packet sender                     |
                            +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
                    */
                    uint8_t buf[128];
                    buf[0] = 0x80; // V=2, P=0, RC=0
                    buf[1] = 201; // PT=RR=201
                    buf[2] = 0;
                    buf[3] = 1; // length=1
                    
                    if(session->peerAudioSSRCs_.mediaSSRC(0) != 0){
                        be_set_u32(session->localAudioSSRC, &buf[4]);
                        session->udpsock_->send(buf, 8, session->remoteAddr_);
                        ++audioRRs;
                    }
                    
                    if(session->peerVideoSSRCs_.mediaSSRC(0) != 0){
                        be_set_u32(session->localVideoSSRC, &buf[4]);
                        session->udpsock_->send(buf, 8, session->remoteAddr_);
                        ++videoRRs;
                    }
                }
                odbgi("session[%s] alive, recvBytes=%lld, audioRRs=%d, videoRRs=%d"
                      , session->sessionId_.c_str(), session->recvBytes_
                      , audioRRs, videoRRs);
            }
        }
        for(auto& o : timeoutList){
            Session * session = o;
            map_get_p_by_str<Session>(&sessions_, session->sessionId_, true);
            odbgi("timeout session[%s]", session->sessionId_.c_str());
            delete session;
            if(dump_tlv_){
                std::ostringstream oss;
                oss << "\"op\":\"timeout-subRtc\""
                    << ",\"subRtcId\":\"" << session->subRtcId_ << "\""
                    << ",\"pubRtcId\":\"" << session->pubRtcId_ << "\"";
                std::string msg = oss.str();
                odbgi("dump msg=[%s]", msg.c_str());
                
                int64_t now_ms = get_now_ms();
                uint8_t tmpbuf[16];
                be_set_i64(now_ms, tmpbuf);
                be_set_u32(session->localPort, tmpbuf+8);
                int lengths[] = {(int)msg.length()+1};
                void * values[] = {(void*)msg.c_str()};
                int vlen=0;
                tlv_file_writen(dump_tlv_, TLV_TYPE_TIME_PORT_REMOVE, 12, tmpbuf
                                , lengths, values, sizeof(lengths)/sizeof(lengths[0])
                                , &vlen);
                //odbgi("dump: type=%d, ts=%lld, vlen=%d", TLV_TYPE_TIME_PORT_REMOVE, now_ms, vlen);
            }
        }
    }
    
    int handleJsonReq(const char * json, size_t json_length, std::ostringstream& oss_err){
        int ret = 0;
        Document req_doc;
        do{
            req_doc.Parse((const char *)json);
            if(!req_doc.IsObject()){
                odbgi("[%s]", json);
                oss_err << "msg is NOT Object";
                ret = -1;
                break;
            }
            
            Value::ConstMemberIterator it_op = req_doc.FindMember("op");
            if(it_op == req_doc.MemberEnd() || !it_op->value.IsString()){
                oss_err << "get field op error";
                ret = -1;
                break;
            }
            std::string op (it_op->value.GetString(), it_op->value.GetStringLength());
            if(op == "subR"){
                ret = handleSubR(json, json_length, req_doc, oss_err);
            }else if(op == "usubR"){
                ret = handleUsubR(json, json_length, req_doc, oss_err);
            }
            else{
                oss_err << "unknown op [" << op << "]";
                ret = -1;
            }
            
        }while(0);
        return ret;
    }
    
    int parseSSRCs(XProtoField& field, SsrcGroup& ssrcs, std::ostringstream& oss_err){
        
        if(field.iter_->value.IsNull()){
            return 0;
        }
        
        if(!field.iter_->value.IsArray()){
            oss_err << "NOT array field of " << field.name_;
            return -1;
        }
        Value::ConstArray arr = field.iter_->value.GetArray();
        if(arr.Size() > MAX_SSRCS){
            oss_err << "too many ssrcs of " << field.name_;
            return -1;
        }
        for(int num = 0; num < arr.Size(); ++num){
            auto& val = arr[num];
            if(!val.IsInt64()){
                oss_err << "ssrc NOT valid";
                return -1;
            }
            ssrcs.add((uint32_t)val.GetInt64());
        }
        if((arr.Size() % 2) != 0){
            ssrcs.add(0);
        }
        return 0;
    }
    
    void checkSessionStringField(Session * session, std::string&val, XProtoField& field){
        if(val != field.strvalue_){
            odbgi("update session [%s]: %s [%s]->[%s]"
                  , session->sessionId_.c_str()
                  , field.name_.c_str()
                  , val.c_str()
                  , field.strvalue_.c_str());
        }
    }
    void checkSessionSSRC(Session * session, const std::string& name, const SsrcGroup& ssrcs, const SsrcGroup& new_ssrcs){
        if(!ssrcs.equals(new_ssrcs)){
            odbgi("update session [%s]: %s", session->sessionId_.c_str(), name.c_str());
        }
    }
    
    int handleSubR(const char * json, size_t json_length, Document& req_doc, std::ostringstream& oss_err){
        int ret = 0;
        Session * newSession = NULL;
        do{
            XProtoSubR proto;
            ret = proto.parse(req_doc, oss_err);
            if(ret){
                break;
            }
            
            if(proto.subRtcId_.strvalue_.empty()){
                proto.subRtcId_.strvalue_ = generate_sub_id();
            }
            
            SsrcGroup audioSSRCs;
            SsrcGroup videoSSRCs;
            ret = parseSSRCs(proto.peerAudioSSRCs_, audioSSRCs, oss_err);
            if(ret) break;
            ret = parseSSRCs(proto.peerVideoSSRCs_, videoSSRCs, oss_err);
            if(ret) break;
            
            std::string& sessionId = proto.subRtcId_.strvalue_;
            Session * existSession = map_get_p_by_str<Session>(&sessions_, sessionId, false);
            Session * session = NULL;
            if(existSession){
                session = existSession;
                checkSessionStringField(session, session->confrId_, proto.confrId_);
                checkSessionStringField(session, session->memName_, proto.memName_);
                checkSessionStringField(session, session->streamId_, proto.streamId_);
                checkSessionStringField(session, session->subRtcId_, proto.subRtcId_);
                checkSessionStringField(session, session->pubRtcId_, proto.pubRtcId_);
                checkSessionSSRC(session, "peerAudioSSRCs", session->peerAudioSSRCs_, audioSSRCs);
                checkSessionSSRC(session, "peerVideoSSRCs", session->peerVideoSSRCs_, videoSSRCs);
            }else{
                newSession = new Session;
                session = newSession;
                session->localAudioSSRC = 1111; // generate_ssrc(); // TODO: use dyn ssrc
                session->localVideoSSRC = 2222; // generate_ssrc();
                session->udpsock_ = factory_->newUDPSocket(appCfg_.mediaLocalIp, 0, "", 0);
                session->udpsock_->setUserData(session);
                session->udpsock_->registerCallback(this);
                session->udpsock_->kickIO();
                session->localPort = session->udpsock_->getLocalAddress()->getLocalPort();
            }
            
            session->sessionId_ = sessionId;
            session->confrId_ = proto.memName_.strvalue_;
            session->memName_ = proto.memName_.strvalue_;
            session->streamId_ = proto.streamId_.strvalue_;
            session->subRtcId_ = proto.subRtcId_.strvalue_;
            session->pubRtcId_ = proto.pubRtcId_.strvalue_;
            session->peerAudioSSRCs_ = audioSSRCs;
            session->peerVideoSSRCs_ = videoSSRCs;
            
            if(newSession){
                sessions_[session->sessionId_] = session;
                odbgi("add session [%s], num=%ld", session->sessionId_.c_str(), sessions_.size());
                newSession = NULL;
            }

            
            if(!dump_tlv_){
                time_t          sec;
                struct tm *     s = 0;
                time(&sec);
                s = localtime(&sec);
                
                char tlv_filename[1024];
                sprintf(tlv_filename, "%s/branch_%04d%02d%02d_%02d%02d%02d.tlv", appCfg_.workPath.c_str()
                        , 1900+s->tm_year, s->tm_mon+1, s->tm_mday, s->tm_hour, s->tm_min, s->tm_sec);
                dump_tlv_ = tlv_file_open(tlv_filename);
                if(dump_tlv_){
                    odbgi("opened output tlv [%s]", tlv_filename);
                }else{
                    odbge("fail to open output tlv [%s]", tlv_filename);
                }
            }
            
            StringBuffer sb;
            Writer<StringBuffer> writer(sb);
            writer.StartObject();
            
            writer.Key("op");
            writer.String("rsp");
            
            writer.Key("status");
            writer.Int(0);
            
            writer.Key("link-addr");
            writer.String(session->udpsock_->getLocalAddress()->getString().c_str());
            
            writer.Key("link-update");
            writer.Bool(true);
            
            writer.Key("subRtcId");
            writer.String(session->subRtcId_.c_str());
            
            writer.Key("audioSSRCs");
            writer.StartArray();
            writer.Int64(session->localAudioSSRC);
            writer.Int64(0);
            writer.EndArray();
            
            writer.Key("videoSSRCs");
            writer.StartArray();
            writer.Int64(session->localVideoSSRC);
            writer.Int64(0);
            writer.EndArray();
            
            if(proto.select_.intvalue_){
                writer.Key("url");
                writer.String(appCfg_.httServiceUrl.c_str());
            }

            writer.EndObject();
            const char * rsp_msg = sb.GetString();
            int rsp_len = (int)sb.GetSize();
            oss_err << rsp_msg;
            
            if(dump_tlv_){
                int64_t now_ms = get_now_ms();
                uint8_t tmpbuf[16];
                be_set_i64(now_ms, tmpbuf);
                be_set_u32(session->localPort, tmpbuf+8);
                int lengths[] = {(int)json_length+1, rsp_len+1};
                void * values[] = {(void*)json, (void*)rsp_msg};
                int vlen = 0;
                tlv_file_writen(dump_tlv_, TLV_TYPE_TIME_PORT_ADD, 12, tmpbuf
                                , lengths, values, sizeof(lengths)/sizeof(lengths[0])
                                , &vlen);
                //odbgi("dump: type=%d, ts=%lld, vlen=%d", TLV_TYPE_TIME_PORT_ADD, now_ms, vlen);
            }
        }while(0);
        if(newSession){
            delete newSession;
            newSession = NULL;
        }
        return ret;
    }
    
    int handleUsubR(const char * json, size_t json_length, Document& req_doc, std::ostringstream& oss_err){
        int ret = 0;
        do{
            XProtoUsubR proto;
            ret = proto.parse(req_doc, oss_err);
            if(ret){
                break;
            }
            
            std::string& sessionId = proto.subRtcId_.strvalue_;
            Session * session = map_get_p_by_str<Session>(&sessions_, sessionId, true);
            if(!session){
                oss_err << "no session [" << sessionId << "]";
                ret = -1;
                break;
            }
            
            StringBuffer sb;
            Writer<StringBuffer> writer(sb);
            writer.StartObject();
            
            writer.Key("op");
            writer.String("rsp");
            
            writer.Key("status");
            writer.Int(0);
            
            writer.EndObject();
            const char * rsp_msg = sb.GetString();
            int rsp_len = (int)sb.GetSize();
            oss_err << rsp_msg;
            
            if(dump_tlv_){
                int64_t now_ms = get_now_ms();
                uint8_t tmpbuf[16];
                be_set_i64(now_ms, tmpbuf);
                be_set_u32(session->localPort, tmpbuf+8);
                int lengths[] = {(int)json_length+1, rsp_len+1};
                void * values[] = {(void*)json, (void*)rsp_msg};
                int vlen = 0;
                tlv_file_writen(dump_tlv_, TLV_TYPE_TIME_PORT_REMOVE, 12, tmpbuf
                                , lengths, values, sizeof(lengths)/sizeof(lengths[0])
                                , &vlen);
                //odbgi("dump: type=%d, ts=%lld, vlen=%d", TLV_TYPE_TIME_PORT_REMOVE, now_ms, vlen);
            }
            
            odbgi("remove session [%s]", session->sessionId_.c_str());
            delete session;
        }while(0);
        return ret;
    }
}; // end of SessionManager


class EventApp{
protected:
    AppConfig appCfg_;
    struct event_base * ev_base_ = NULL;
    struct event * ev_timer_ = NULL;
    std::vector<struct event *> ev_signals_;
    struct evhttp * ev_http_ = NULL;
    struct evbuffer * ev_buf_ = NULL;
    const size_t http_buf_size_ = 8*1024;
    uint8_t * http_buf_ = NULL;
    XSocketFactory * factory_ = NULL;
    SessionManager * mgr_ = NULL;
protected:
    void kickTimer(){
        if(ev_base_ && !ev_timer_){
            ev_timer_ = event_new(ev_base_, -1, EV_PERSIST, timer_event_handler, this);
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            evtimer_add(ev_timer_, &tv);
        }
    }
    virtual int open(){
        this->close();
        int signal_numbers[] = {SIGINT, SIGALRM};
        ev_base_ = event_base_new();
        factory_ = XSocketFactory::newFactory(ev_base_);
        for(int i = 0; i < sizeof(signal_numbers)/sizeof(signal_numbers[0]); i++){
            struct event * ev_signal = evsignal_new(ev_base_, signal_numbers[i], ev_signal_cb, (void *)ev_base_);
            evsignal_assign(ev_signal, ev_base_, signal_numbers[i], ev_signal_cb, ev_signal);
            event_add(ev_signal, NULL);
            ev_signals_.push_back(ev_signal);
        }
        
        this->kickTimer();
        
        int ret = 0;
        do {
            http_buf_ = new uint8_t[http_buf_size_];
            
            ev_http_ = evhttp_new(ev_base_);
            if (!ev_http_) {
                odbge("can't create ev_http");
                ret = -1;
                break;
            }
            ev_buf_ = evbuffer_new();
            
            evhttp_set_gencb(ev_http_, on_http_req, this);
            
            int http_port = appCfg_.httpLocalPort;
            int ret = evhttp_bind_socket(ev_http_, "0.0.0.0", http_port);
            if (ret != 0) {
                odbge("can't bind http at port %d !!!\n", http_port);
                break;
            }
            odbgi("http service at %d\n", http_port);
            mgr_ = new SessionManager(factory_, appCfg_);
            ret = 0;
        } while (0);
        if(ret){
            this->close();
        }
        return ret;
    }
    virtual void close(){
        
        if(ev_http_){
            evhttp_free(ev_http_);
            ev_http_ = NULL;
        }
        
        if(ev_buf_){
            evbuffer_free(ev_buf_);
            ev_buf_ = NULL;
        }
        
        if(http_buf_){
            delete http_buf_;
            http_buf_ = NULL;
        }
        
        if(ev_timer_){
            event_free(ev_timer_);
            ev_timer_ = 0;
        }
        
        for(auto& o : ev_signals_){
            event_free(o);
        }
        ev_signals_.clear();
        
        if(factory_){
            delete factory_;
            factory_  =NULL;
        }
        
        if(ev_base_){
            event_base_free(ev_base_);
            ev_base_ = NULL;
        }
        
        if(mgr_){
            delete mgr_;
            mgr_ = NULL;
        }
    }
    
public:
    EventApp(AppConfig& config) : appCfg_(config){
        
    }
    virtual ~EventApp(){
        this->close();
    }
    virtual int run(){
        int ret = 0;
        do{
            ret = this->open();
            if(ret) break;
            event_base_dispatch(ev_base_);
        }while(0);
        this->close();
        return ret;
    }
    
    void handleHttp(struct evhttp_request *req){
        int ret = 0;
        std::ostringstream oss_err;
        size_t reqlen = 0;
        http_buf_[0] = '\0';
        do {
            //this->dumpHttpReq(req);
            //const char *uri = evhttp_request_get_uri(req);
            evhttp_cmd_type req_cmd = evhttp_request_get_command(req);
            if(req_cmd != EVHTTP_REQ_POST){
                ret = -1;
                oss_err << "NOT POST req";
                odbge("handleREST: expect POST but %d", req_cmd);
                break;
            }
            
            size_t recv_buf_size = http_buf_size_;
            struct evbuffer *buf = evhttp_request_get_input_buffer(req);
            if(evbuffer_get_length(buf) > (recv_buf_size-1)){
                ret = -1;
                oss_err << "too big body";
                odbge("handleREST: too big body, len=%d", (int)evbuffer_get_length(buf));
                break;
            }
            
            ret = evbuffer_remove(buf, http_buf_, recv_buf_size);
            if( ret < 0){
                oss_err << "sth wrong with buf";
                odbge("handleREST: sth wrong with buf, error=%d", ret);
                break;
            }
            reqlen = ret;
            http_buf_[reqlen] = '\0';
            odbgi("http req: [%s]", http_buf_);
            ret = mgr_->handleJsonReq((char *)http_buf_, reqlen, oss_err);
        } while (0);
        

        std::string body = oss_err.str();
        odbgi("http rsp: ret=[%d], body=[%s]", ret, body.c_str());
        
        evbuffer_add_printf(ev_buf_, "%s", body.c_str());
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "application/json;charset=UTF-8");
        if(ret){
            evhttp_send_reply(req, 400, "Bad Request", ev_buf_);
        }else{
            evhttp_send_reply(req, 200, "OK", ev_buf_);
        }
    }
    
    void dumpHttpReq(struct evhttp_request *req)
    {
        const char *cmdtype;
        struct evkeyvalq *headers;
        struct evkeyval *header;
        struct evbuffer *buf;
        
        switch (evhttp_request_get_command(req)) {
            case EVHTTP_REQ_GET: cmdtype = "GET"; break;
            case EVHTTP_REQ_POST: cmdtype = "POST"; break;
            case EVHTTP_REQ_HEAD: cmdtype = "HEAD"; break;
            case EVHTTP_REQ_PUT: cmdtype = "PUT"; break;
            case EVHTTP_REQ_DELETE: cmdtype = "DELETE"; break;
            case EVHTTP_REQ_OPTIONS: cmdtype = "OPTIONS"; break;
            case EVHTTP_REQ_TRACE: cmdtype = "TRACE"; break;
            case EVHTTP_REQ_CONNECT: cmdtype = "CONNECT"; break;
            case EVHTTP_REQ_PATCH: cmdtype = "PATCH"; break;
            default: cmdtype = "unknown"; break;
        }
        
        odbgi("Http Request: method=[%s], uri=[%s]", cmdtype, evhttp_request_get_uri(req));
        odbgi("Headers:");
        headers = evhttp_request_get_input_headers(req);
        for (header = headers->tqh_first; header;
             header = header->next.tqe_next) {
            odbgi("  [%s]=[%s]", header->key, header->value);
        }
        
        buf = evhttp_request_get_input_buffer(req);
        size_t contentSize = evbuffer_get_length(buf);
        char * contentData = (char *)malloc(contentSize+1);
        if(contentSize > 0){
            evbuffer_copyout(buf, contentData, contentSize);
        }
        contentData[contentSize] = '\0';
        odbgi("Content(%ld bytes)=[%s]", contentSize, contentData);
        free(contentData);
        
        //    evhttp_send_reply(req, 200, "OK", NULL);
    }
    
    static void on_http_req(struct evhttp_request *req, void *arg);
    static void ev_signal_cb(evutil_socket_t sig, short events, void *user_data);
    static void timer_event_handler(evutil_socket_t fd, short what, void* arg);
};


void EventApp::ev_signal_cb(evutil_socket_t sig, short events, void *user_data){
    struct event * ev = (struct event *) user_data;
    struct event_base * ev_base = event_get_base(ev);
    long seconds = 0;
    struct timeval delay = { seconds, 1 };
    odbgi("caught signal %d, gracefully exit...", event_get_signal(ev));
    event_base_loopexit(ev_base, &delay);
}

void EventApp::timer_event_handler(evutil_socket_t fd, short what, void* arg){
    //odbgi("timer: what=%d", what);
    EventApp * thiz = (EventApp *) arg;
    if(thiz->mgr_){
        thiz->mgr_->timeCheck();
    }
}

void EventApp::on_http_req(struct evhttp_request *req, void *arg){
    EventApp * thiz = (EventApp *) arg;
    thiz->handleHttp(req);
}



int lab_bstream_dump_main(int argc, char* argv[]){
    int ret = 0;
//    ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) ;
//    if(ret){
//        odbge( "fail too init SDL, err=[%s]", SDL_GetError());
//        return -1;
//    }
//    ret = TTF_Init();
    
    
    AppConfig cfg;
    cfg.initWithIp("172.17.1.216");
    EventApp app(cfg);
    app.run();
    
//    TTF_Quit();
//    SDL_Quit();
    return ret;
}

/*
curl 'http://127.0.0.1:8800/rest' -i -X POST -H 'Content-Type: application/json' -d '{
"op" : "subR",
"confrId" : "confr001",
"memName" : "John",
"streamId" : "stream1",
"pubRtcId" : "pub1",
"subRtcId" : "sub1",
"peerAudioSSRCs" : [5555, 5556],
"peerVideoSSRCs" : [7777, 7778]
}'

pkill -14 RTCLab
 
*/

