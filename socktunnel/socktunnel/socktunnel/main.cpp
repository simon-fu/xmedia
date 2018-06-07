//
//  main.cpp
//  socktunnel
//
//  Created by simon on 2018/3/10.
//  Copyright © 2018年 easemob. All rights reserved.
//

#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/http.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>

// mac
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "XSocket.h"
#include "XUDPPing.h"

#define DUMP_DATA 0
#define dbgd(...) // do{  printf("<tunnel>[D] " __VA_ARGS__); printf("\n"); }while(0)
#define dbgi(...) do{  printf("<tunnel>[I] " __VA_ARGS__); printf("\n"); }while(0)
#define dbge(...) do{  printf("<tunnel>[E] " __VA_ARGS__); printf("\n"); }while(0)

#define ZERO_ALLOC(o, type, sz) do{o=(type)malloc(sz); memset(o, 0, sz);}while(0)

#if DUMP_DATA
static char * bytesToAsciiString(const uint8_t * data, int dataLength, char * strBuf, int strBufSize){
    for(int i = 0; i < dataLength; i++){
        uint8_t b = data[i];
        if(b >= 32 && b <= 126){
            strBuf[i] = b;
        }else{
            strBuf[i] = '.';
        }
    }
    strBuf[dataLength] = '\0';
    return strBuf;
}

static void dispData(const std::string& name, const uint8_t * buf, int bytesRecv){
    dbgi("[%s] got %d bytes -----------------------", name.c_str(), bytesRecv);
    static char strBuf[4096+2];
    int dispMaxLen = 64;
    for(int offset = 0; offset < bytesRecv;){
        int remains = bytesRecv - offset;
        int dispLen  = remains < dispMaxLen ? remains : dispMaxLen;
        dbgi("[%s] %04X: |%s|", name.c_str(), offset, bytesToAsciiString(buf+offset, dispLen, strBuf, sizeof(strBuf)));
        offset += dispLen;
    }
}
#else
#define dispData(x, y, z)
#endif

static inline
int64_t getNextIdNum(){
    static int64_t num = 0;
    return ++num;
}

static
std::string makeNextId(const char * className){
    char buf[64];
    sprintf(buf, "%s-%lld", className, getNextIdNum());
    return buf;
}

class XIdObj{
protected:
    std::string id_;
public:
    XIdObj():id_(makeNextId("obj")){}
    XIdObj(const std::string& id):id_(id){}
    virtual ~XIdObj(){}
    virtual const std::string& id(){return id_;}
};

class BridgeManager;
class BridgeBase : public XIdObj {
protected:
    BridgeManager * mgr_ = NULL;
public:
    BridgeBase():XIdObj(makeNextId("bridge")){
    }
    BridgeBase(const std::string& id):XIdObj(id){
    }
    
    virtual ~BridgeBase(){}
    virtual void setManager(BridgeManager * mgr){
        mgr_ = mgr;
    }
    virtual bool timeToCheck(int64_t nowMS) = 0;
    virtual void close() =0 ;
};

class BridgeManager{
    XSocketFactory * factory_;
    std::map<std::string, BridgeBase *> bridges_;
    std::map<std::string, BridgeBase *> timeCheckBridges_;
public:
    BridgeManager(XSocketFactory * factory):factory_(factory){
        
    }
    
    virtual ~BridgeManager(){
        for(auto& o : bridges_){
            BridgeBase * bridge = o.second;
            delete bridge;
        }
    }
    
    XSocketFactory * getFactory(){
        return factory_;
    }
    
    void timeToCheck(){
        int64_t now = time(NULL)*1000;
        std::list<BridgeBase *> list4Remove;
        for(auto& o : timeCheckBridges_){
            BridgeBase * bridge = o.second;
            if(!bridge->timeToCheck(now)){
                list4Remove.push_back(bridge);
            }
        }
        for(auto& b : list4Remove){
            dbgd("remove from check [%s]", b->id().c_str());
            timeCheckBridges_.erase(b->id());
        }
    }
    
    void add(BridgeBase *bridge){
        auto it = bridges_.find(bridge->id());
        if(it == bridges_.end()){
            bridge->setManager(this);
            bridges_[bridge->id()] = bridge;
        }

    }
    
    void addToCheck(BridgeBase *bridge){
        auto it = timeCheckBridges_.find(bridge->id());
        if(it == timeCheckBridges_.end()){
            dbgd("add to check [%s]", bridge->id().c_str());
            timeCheckBridges_[bridge->id()] = bridge;
        }
    }
};



class ForwardListener{
public:
    virtual ~ForwardListener(){}
    virtual void onForwardClose(XIdObj * obj, const std::string& reason) = 0;
};

class TCPForwardPair : public XIdObj{
    class Leg : public XSocketCallback, public XIdObj{
        uint8_t buf_[2*1024];
        int bufSize_ = sizeof(this->buf_)-1;
        
    public:
        TCPForwardPair * owner_;
        XSocket *xsock_;
        XSocket *xother_;
        int64_t recvBytes_ = 0;
        
        Leg(TCPForwardPair * owner, XSocket *xsock, const std::string&  name, XSocket *other)
        : XIdObj(owner->id()+"-"+name), owner_(owner), xsock_(xsock), xother_(other){
            xsock_->registerCallback(this);
        }
        virtual ~Leg(){
            close();
        }
        virtual void onConnected(XSocket *xsock) override {
            dbgd("[%s] connected, local=[%s], remote=[%s]"
                 , id().c_str()
                 , xsock->getLocalAddress()->getString().c_str()
                 , xsock->getRemoteAddress()->getString().c_str());
        }
        
        virtual void onDisconnect(XSocket *xsock, bool isConnecting) override {
            if(isConnecting){
                dbge("[%s] fail to connect, local=[%s], remote=[%s], reason=[%s]"
                     , id().c_str()
                     , xsock->getLocalAddress()->getString().c_str()
                     , xsock->getRemoteAddress()->getString().c_str()
                     , XSocketFactory::getLastError(NULL).c_str());
                owner_->checkOnDisconn(id()+" conn");
            }else{
                dbgd("[%s] disconnected, reason=[%s]", id().c_str(), XSocketFactory::getLastError(NULL).c_str());
                owner_->checkOnDisconn(id()+" disconn");
            }
        }
        
        
        virtual void onSocketRecvDataReady(XSocket *xsock) override {
            int bytesRecv = xsock->recv(buf_, bufSize_);
            while( bytesRecv > 0){
                dispData(id(), buf_, bytesRecv);

                recvBytes_ += bytesRecv;
                XSocket * other = xother_;
                int bytesSent = 0;
                int ret = 0;
                while(bytesSent < bytesRecv){
                    ret = other->send(buf_+bytesSent, bytesRecv-bytesSent, NULL);
                    if( ret <= 0) break;
                    bytesSent += ret;
                }
                if(ret <= 0)break;
                bytesRecv = xsock->recv(buf_, bufSize_);
            }
        }
        
        int start(){
//            dbgi("[%s] start, local=[%s], remote=[%s]"
//                 , id().c_str()
//                 , xsock_->getLocalAddress()->getString().c_str()
//                 , xsock_->getRemoteAddress()->getString().c_str());
            int ret = xsock_->kickIO();
            if(ret){
                dbge("[%s] fail to start, reason=[%s]"
                     , id().c_str()
                     , XSocketFactory::getLastError(NULL).c_str());
            }
            return ret;
        }
        
        void close(){
            if(xsock_){
                delete xsock_;
                xsock_ = NULL;
                dbgd("[%s] closed", id().c_str());
            }
        }
    }; // class Leg
    
    Leg leg1_;
    Leg leg2_;
    ForwardListener * listener_;
    bool closed_ = false;
    
    void checkOnDisconn(const std::string& reason){
        if(listener_){
            listener_->onForwardClose(this, reason);
        }
    }
    
public:
    TCPForwardPair(XSocket *xsock1, XSocket *xsock2, ForwardListener * listener = NULL)
    : XIdObj(makeNextId("TCPFwd"))
    , leg1_(this, xsock1, "leg1", xsock2)
    , leg2_(this, xsock2, "leg2", xsock1)
    , listener_(listener){
    }
    
    virtual ~TCPForwardPair(){
        close("");
    }
    
    int start(){
        int ret = 0;
        do{
            ret = leg1_.start();
            if(ret) break;
            ret = leg2_.start();
            if(ret) break;
            ret = 0;
        }while (0);
        return ret;
    }
    
    void close(const std::string& reason){
        if(!closed_){
            leg1_.close();
            leg2_.close();
            closed_ = true;
            dbgi("[%s] closed, reason=[%s], fwd/back=%lld/%lld(bytes)", id().c_str(), reason.c_str(), leg1_.recvBytes_, leg2_.recvBytes_);
        }
    }
};


class TCPBridge : public ForwardListener, public XSocketCallback, public BridgeBase{
    XSocketFactory * factory_;
    std::string ip1_;
    int port1_ = -1;
    std::string ip2_;
    int port2_ = -1;
    XSocket * agentSock_ = NULL;
    std::map<std::string, TCPForwardPair * > forwards;
    bool closed_ = false;
    bool naglesEnable_ = false;
    virtual void onForwardClose(XIdObj * obj, const std::string& reason) override{
        TCPForwardPair * forward = (TCPForwardPair *) obj;
        forwards.erase(forward->id());
        forward->close(reason);
        delete forward;
    }

    
public:
    TCPBridge(XSocketFactory * factory, const std::string& ip1, int port1,  const std::string& ip2, int port2, bool naglesEnable)
    : BridgeBase(makeNextId("TCPBridge")), factory_(factory), ip1_(ip1), port1_(port1), ip2_(ip2), port2_(port2), naglesEnable_(naglesEnable){
    }
    virtual ~TCPBridge(){
        close();
    }
    int startServer2Client(){
        int ret = -1;
        do {
            agentSock_ = factory_->newListenTCPSocket(ip1_, port1_);
            agentSock_->enableNagles(naglesEnable_);
            agentSock_->registerCallback(this);
            ret = agentSock_->kickIO();
            if(ret == 0){
                //dbgi("bridge listen at [%s] -> [%s]", agentSock_->getLocalUrl().c_str(), XSocket::makeUrl(XSOCK_TCP, ip2_, port2_, false).c_str());
            }else{
                dbge("bridge faid to listen at [%s], reason=[%s]", agentSock_->getLocalUrl().c_str(), XSocketFactory::getLastError(NULL).c_str());
            }
        } while (0);
        return ret;
    }
    
    virtual void onAccept(XSocket *xsock, XSocket *newxsock) override {
        XSocket *xsock2 = factory_->newTCPSocket("", 0, ip2_, port2_);
        xsock2->enableNagles(naglesEnable_);
        TCPForwardPair * forward = new TCPForwardPair(newxsock, xsock2, this);
//        dbgi("new tcp forward [%s]", forward->id().c_str());

        
        int ret = forward->start();
        if(ret){
            delete forward;
        }else{
            dbgi("new tcp forward [%s]: [%s] <-> [%s]-[%s] <-> [%s]"
                 , forward->id().c_str()
                 , newxsock->getRemoteAddress()->getString().c_str()
                 , newxsock->getLocalAddress()->getString().c_str()
                 , xsock2->getLocalAddress()->getString().c_str()
                 , xsock2->getRemoteAddress()->getString().c_str());
            forwards[forward->id()] = forward;
        }
    }
    
    virtual bool timeToCheck(int64_t nowMS) override{
        return false;
    }
    
    virtual void close() override{
        while( !forwards.empty() ) {
            TCPForwardPair * forward = forwards.begin()->second;
            forwards.erase(forwards.begin());
            forward->close("shutdown");
            delete forward;
        }
        if(agentSock_){
            delete agentSock_;
            agentSock_ = NULL;
        }
        if(!closed_){
            dbgd("[%s] closed", id().c_str());
            closed_ = true;
        }
    }
};


#define FORWARD_MAX_SLOTS 2
class UDPBridge : public XSocketCallback, public BridgeBase{
    
    struct Forwarder : public XSocketCallback{
        std::string id_ = makeNextId("UDPFwd");
        UDPBridge * owner_ = NULL;
        uint8_t buf_[2*1024];
        int bufSize_ = sizeof(this->buf_);
        int64_t time_ = 0;
        int64_t forwardBytes_ = 0;
        int64_t forwardPackets_ = 0;
        int64_t backBytes_ = 0;
        int64_t backPackets_ = 0;
    public:
        XSockAddress * from_ = NULL;
        XSockAddress * to_ = NULL;
        XSocket * otherSock_ = NULL;
        int slot_ = -1;
        Forwarder(){}
        Forwarder(UDPBridge * owner, XSocket * other) : owner_(owner) , otherSock_(other){
        }
        virtual void onSocketRecvDataReady(XSocket *xsock) override {
            while(1) {
                int bytesRecv = xsock->recv(buf_, bufSize_);
                if(bytesRecv <= 0) break;
                if(xsock->getRemoteAddress()->equal(to_)){
                    // dbgi("udp forwarder got from [%s], %d byes", xsock->getRemoteAddress()->getString().c_str(), bytesRecv);
                    owner_->agentSock_->send(buf_, bytesRecv, from_);
                    time_ = 0;
                    backBytes_ += bytesRecv;
                    ++backPackets_;
                }
            }
        }
    };
    typedef std::map <struct sockaddr_in, Forwarder, addr_less_fun>  ForwarderMap;
    typedef std::map <struct sockaddr_in, Forwarder*, addr_less_fun> ForwarderPtrMap;
    XSocketFactory * factory_;
    std::string ip1_;
    int port1_ = -1;
    std::string ip2_;
    int port2_ = -1;
    XSocket * agentSock_ = NULL;
    uint8_t buf_[2*1024];
    int bufSize_ = sizeof(this->buf_);
    ForwarderPtrMap  forwardMap_ ;
    ForwarderPtrMap forwardSlots_[FORWARD_MAX_SLOTS];
    int slotCounter_ = 0;
    int checkCounter_ = 0;
    XSocket * preferSock_ = NULL;
    int preferSockRef_ = 0;
public:
    UDPBridge(XSocketFactory * factory, const std::string& ip1, int port1,  const std::string& ip2, int port2, XSocket * preferSock = NULL)
    : BridgeBase(makeNextId("UDPBridge")), factory_(factory), ip1_(ip1), port1_(port1), ip2_(ip2), port2_(port2), preferSock_(preferSock){
    }
    virtual ~UDPBridge(){
        close();
    }
    
    void freeFoward(Forwarder& o, const std::string& reason){
        if(o.otherSock_ && o.from_){
            dbgi("[%s] closed, reason=[%s], fwd/back=[%lld/%lld(bytes), %lld/%lld(packets)]", o.id_.c_str(), reason.c_str()
                 , o.forwardBytes_, o.backBytes_
                 , o.forwardPackets_, o.backPackets_);
        }
        
        if(o.otherSock_ && o.otherSock_ != agentSock_){
            if(o.otherSock_ != preferSock_){
                delete o.otherSock_;
            }else{
                dbgd("[%s] return prefer sock [%s]", o.id_.c_str(), preferSock_->getLocalUrl().c_str());
                --preferSockRef_;
            }
            o.otherSock_ = NULL;
        }
        if(o.from_){
            const sockaddr_in& key = *o.from_->getNativeAddrIn();
            if(o.slot_ >= 0){
                ForwarderPtrMap &m = forwardSlots_[o.slot_];
                m.erase(key);
            }
            forwardMap_.erase(key);
            delete o.from_;
            o.from_ = NULL;
        }
        if(o.to_){
            delete o.to_;
            o.to_ = NULL;
        }
    }
    
    virtual void close() override{
        while( !forwardMap_.empty() ) {
            Forwarder * forward = forwardMap_.begin()->second;
            this->freeFoward(*forward, "shutdown");
            delete forward;
        }
        
        if(agentSock_){
            delete agentSock_;
            agentSock_ = NULL;
        }
    }
    
    virtual bool timeToCheck(int64_t nowMS) override{
        const int64_t timeout = 90*1000;
        
        int slot = checkCounter_%FORWARD_MAX_SLOTS;
        ++checkCounter_;
        ForwarderPtrMap &m = forwardSlots_[slot];
        std::list<Forwarder *> timeoutList;
        for(auto& it : m){
            Forwarder * forward = it.second;
            if(forward->time_ > 0){
                if((nowMS - forward->time_) > timeout){
                    timeoutList.push_back(forward);
                }
            }else{
                forward->time_ = nowMS;
            }
        }
        for(auto& f : timeoutList){
            this->freeFoward(*f, "timeout");
            delete f;
        }
        return forwardMap_.size() > 0;
    }
    
    int start(){
        int ret = -1;
        do {
            agentSock_ = factory_->newUDPSocket(ip1_, port1_, "", 0);
            agentSock_->registerCallback(this);
            ret = agentSock_->kickIO();
            if(ret == 0){
//                dbgi("[%s] listen at [%s] -> [%s]", id_.c_str(), agentSock_->getLocalUrl().c_str(), XSocket::makeUrl(XSOCK_UDP, ip2_, port2_, false).c_str());
            }else{
                dbge("[%s] faid to listen at [%s], reason=[%s]", id_.c_str(), agentSock_->getLocalUrl().c_str(), XSocketFactory::getLastError(NULL).c_str());
            }
        } while (0);
        return ret;
    }
    
    virtual void onSocketRecvDataReady(XSocket *xsock) override {
        int ret = 0;
        while(1) {
            int bytesRecv = xsock->recv(buf_, bufSize_);
            if(bytesRecv <= 0) break;
            // dbgi("udp bridge got from [%s], %d byes", xsock->getRemoteAddress()->getString().c_str(), bytesRecv);
            Forwarder * forward = NULL;
            const sockaddr_in key = *xsock->getRemoteAddress()->getNativeAddrIn();
            auto it = forwardMap_.find(key);
            if(it == forwardMap_.end()) {
                XSocket * other = NULL;
                if(preferSock_ && preferSockRef_ == 0){
                    ++preferSockRef_;
                    other = preferSock_;
                }else{
                    other = factory_->newUDPSocket("", 0, ip2_, port2_);
                    ret = other->bind();
                    if(ret){
                        dbge("[%s] bind forward udp fail, reason=[%s]", id().c_str(), XSocketFactory::getLastError(NULL).c_str());
                        delete other;
                        break;
                    }
                }
                
                forwardMap_[key] = new Forwarder(this, other);
                forward = forwardMap_[key];
                forward->from_ = XSockAddress::newAddress(xsock->getRemoteAddress());
                forward->to_ = XSockAddress::newAddress(other->getRemoteAddress());
                forward->otherSock_->registerCallback(forward);
                forward->slot_ = slotCounter_%FORWARD_MAX_SLOTS;
                ForwarderPtrMap& m = forwardSlots_[forward->slot_];
                m[key] = forward;
                ++slotCounter_;
                ret = forward->otherSock_->kickIO();
                if(ret){
                    dbge("[%s] kick forward udp fail, reason=[%s]", id().c_str(), XSocketFactory::getLastError(NULL).c_str());
                    this->freeFoward(*forward,XSocketFactory::getLastError(NULL));
                    delete forward;
                    forward = NULL;
                    break;
                }
                if(forwardMap_.size() == 1){
                    mgr_->addToCheck(this);
                }
                dbgi("new udp forward [%s]: [%s] <-> [%s]-[%s] <-> [%s]"
                     , forward->id_.c_str()
                     , agentSock_->getRemoteAddress()->getString().c_str()
                     , agentSock_->getLocalAddress()->getString().c_str()
                     , other->getLocalAddress()->getString().c_str()
                     , other->getRemoteAddress()->getString().c_str());
                if(other == preferSock_){
                    dbgd("[%s] use prefer sock [%s]", forward->id_.c_str(), other->getLocalUrl().c_str());
                }
            }else{
                forward = it->second;
            }
            forward->otherSock_->send(buf_, bytesRecv, forward->otherSock_->getRemoteAddress());
            forward->time_ = 0;
            forward->forwardBytes_ += bytesRecv;
            ++forward->forwardPackets_;
        }
    }
};

static
void ev_signal_cb(evutil_socket_t sig, short events, void *user_data){
    struct event * ev = (struct event *) user_data;
    struct event_base * ev_base = event_get_base(ev);
    long seconds = 0;
    struct timeval delay = { seconds, 1 };
    dbgi("caught signal %d, gracefully exit...", event_get_signal(ev));
    event_base_loopexit(ev_base, &delay);
}

static void timer_event_handler(evutil_socket_t fd, short what, void* arg){
    BridgeManager * mgr = (BridgeManager *) arg;
    mgr->timeToCheck();
}

int addTCPServer2Client(BridgeManager * mgr
                        , const std::string& ip1, int beginPort1
                        , const std::string& ip2, int beginPort2
                        , int numPorts){
    int ret = 0;
    for(int i = 0; i < numPorts; i++){
        int port1 = beginPort1 + i;
        int port2 = beginPort2 + i;
        TCPBridge * bridge = new TCPBridge(mgr->getFactory(), ip1, port1, ip2, port2, false);
        ret = bridge->startServer2Client();
        if(ret) {
            delete bridge;
            break;
        }
        mgr->add(bridge);
    }
    if(ret == 0){
        if(numPorts > 1){
            dbgi("TCP bridge at [%s:%d~%d] -> [%s:%d~%d]", ip1.c_str(), beginPort1, beginPort1+numPorts-1
                 , ip2.c_str(), beginPort2, beginPort2+numPorts-1 );
        }else{
            dbgi("TCP bridge at [%s:%d] -> [%s:%d]", ip1.c_str(), beginPort1, ip2.c_str(), beginPort2 );
        }
        
    }
    return ret;
}

static int getNextPreferUdpPort(){
    static int port = 11000;
    return ++port;
}

int addUDPBridge(BridgeManager * mgr
                 , const std::string& ip1, int beginPort1
                 , const std::string& ip2, int beginPort2
                 , int numPorts){
    int ret = 0;
    for(int i = 0; i < numPorts; i++){
        int port1 = beginPort1 + i;
        int port2 = beginPort2 + i;
        int preferPort = 0;  // -1=disable , 0=enable;
        XSocket * preferSock = NULL;
        while(preferSock == NULL && preferPort >= 0 && preferPort < 65536){
            preferPort = getNextPreferUdpPort();
            preferSock = mgr->getFactory()->newUDPSocket("", preferPort, ip2, port2);
            ret = preferSock->bind();
            if(ret){
                delete preferSock;
                preferSock = NULL;
            }
        }
        UDPBridge * bridge = new UDPBridge(mgr->getFactory(), ip1, port1, ip2, port2, preferSock);
        ret = bridge->start();
        if(ret) {
            delete bridge;
            break;
        }
        mgr->add(bridge);
    }
    if(ret == 0){
        if(numPorts > 1){
            dbgi("UDP bridge at [%s:%d~%d] -> [%s:%d~%d]", ip1.c_str(), beginPort1, beginPort1+numPorts-1
                 , ip2.c_str(), beginPort2, beginPort2+numPorts-1 );
        }else{
            dbgi("UDP bridge at [%s:%d] -> [%s:%d]", ip1.c_str(), beginPort1, ip2.c_str(), beginPort2 );
        }

    }
    return ret;
}

static
int addWangza(BridgeManager * manager){
    int ret = 0;
    do{
//        ssh -L0.0.0.0:8443:101.37.228.109:443 -p3299 easemob@121.41.87.159
//        ssh -L0.0.0.0:8780:121.196.228.24:80 -p3299 easemob@121.41.87.159
//        ssh -L0.0.0.0:8980:101.37.182.240:80 -p3299 easemob@121.41.87.159
//        ssh -L0.0.0.0:9081:121.196.226.37:9081 -p3299 easemob@121.41.87.159
//        UDP:  30000 ~ 30099 -> 121.196.226.37:30000~30099
        
        ret = addTCPServer2Client(manager, "", 8443, "101.37.228.109", 443, 1);
        if(ret) break;
        ret = addTCPServer2Client(manager, "", 8780, "121.196.228.24", 80, 1);
        if(ret) break;
        ret = addTCPServer2Client(manager, "", 8980, "101.37.182.240", 80, 1);
        if(ret) break;
        ret = addTCPServer2Client(manager, "", 9081, "121.196.226.37", 9081, 1);
        if(ret) break;
        
        int udpBeginPort = 30000;
        int udpEndPort = 30099;
        ret = addUDPBridge(manager, "", udpBeginPort, "121.196.226.37", udpBeginPort, udpEndPort-udpBeginPort+1);
        if(ret) break;
        
    }while(0);
    return ret;
}

static
int addTurn2(BridgeManager * manager){
    int ret = 0;
    do{
        ret = addTCPServer2Client(manager, "", 9092, "121.41.87.159", 9092, 1);
        if(ret) break;
        
        ret = addUDPBridge(manager, "", 2000, "121.41.87.159", 2000, 1);
        if(ret) break;
        
        int udpBeginPort = 30100;
        int udpEndPort = 31999;
        ret = addUDPBridge(manager, "", udpBeginPort, "121.41.87.159", udpBeginPort, udpEndPort-udpBeginPort+1);
        if(ret) break;
    }while(0);
    return ret;
}

void run_tunnel_tcppair(){
    struct event_base * ev_base = event_base_new();
    struct event *            ev_timer = NULL;
    std::vector<struct event *> ev_signals;
    int signal_numbers[] = {SIGINT, SIGALRM};
    
    int ret = 0;
    XSocketFactory * factory = XSocketFactory::newFactory(ev_base);
    BridgeManager * manager = new BridgeManager(factory);
    do{
        ret = addTurn2(manager);
        if(ret) break;
        
//        ret = addWangza(manager);
//        if(ret) break;
        
        for(int i = 0; i < sizeof(signal_numbers)/sizeof(signal_numbers[0]); i++){
            struct event * ev_signal = evsignal_new(ev_base, signal_numbers[i], ev_signal_cb, (void *)ev_base);
            evsignal_assign(ev_signal, ev_base, signal_numbers[i], ev_signal_cb, ev_signal);
            event_add(ev_signal, NULL);
            ev_signals.push_back(ev_signal);
        }

        ev_timer = event_new(ev_base, -1, EV_PERSIST, timer_event_handler, manager);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        evtimer_add(ev_timer, &tv);
        
        event_base_dispatch(ev_base);
    }while(0);

    if(ev_timer){
        event_free(ev_timer);
        ev_timer = 0;
    }
    
    for(auto& o : ev_signals){
        event_free(o);
    }
    ev_signals.clear();

    if(manager){
        delete manager;
        manager = NULL;
    }
    
    if(factory){
        delete factory;
        factory  =NULL;
    }
    if(ev_base){
        event_base_free(ev_base);
        ev_base = NULL;
    }
}

//
// pgrep socktunnel | xargs kill -SIGALRM
//
int main(int argc, const char * argv[]) {
//    run_tunnel_server();
//    run_tunnel_tcppair();
    return xudp_ping_main(argc, argv);
    dbgi("bye");
    return 0;
}
