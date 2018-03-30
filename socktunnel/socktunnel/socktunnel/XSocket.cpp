
#include "XSocket.h"
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/http.h>
// mac
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>     // for TCP_NODELAY
#include <arpa/inet.h>       // for IPPROTO_TCP

#define dbgv(...) do{  printf("<xsock>[D] " __VA_ARGS__); printf("\n"); }while(0)
#define dbgi(...) do{  printf("<xsock>[I] " __VA_ARGS__); printf("\n"); }while(0)
#define dbge(...) do{  printf("<xsock>[E] " __VA_ARGS__); printf("\n"); }while(0)

static int lastErrorCode = 0;
static std::string lastErrorString;
static void set_last_error(int error, const char * str){
    lastErrorCode = error;
    lastErrorString = str;
}
const std::string& XSocketFactory::getLastError(int * error){
    if(error){
        *error = lastErrorCode;
    }
    return lastErrorString;
}

static inline
void socket_addrin_init(const char * ip, int port, struct sockaddr_in * addr){
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if(ip && ip[0] != '\0'){
        addr->sin_addr.s_addr = inet_addr(ip);
    }else{
        addr->sin_addr.s_addr = htonl(INADDR_ANY);
    }
}

static inline
int sock_addr_is_valid(struct sockaddr_in * src){
    return (src->sin_family > 0
            && src->sin_addr.s_addr > 0
            && src->sin_port > 0);
}

#define ADDR_IS_EQU(x1, x2) ((x1).sin_port == (x2).sin_port &&  (x1).sin_addr.s_addr == (x2).sin_addr.s_addr)
//struct addr_hash_func{
//    size_t operator()(const sockaddr_in &addr) const
//    {
//        return addr.sin_port*9999 + addr.sin_addr.s_addr;
//    }
//};
//
//struct addr_cmp_fun{
//    bool operator()(const sockaddr_in &addr1, const sockaddr_in &addr2) const
//    {
//        //return memcmp(&addr1, &addr2, sizeof(sockaddr_in)) == 0 ? true:false;
//        //return addr1.sin_port == addr2.sin_port &&  addr1.sin_addr.s_addr == addr2.sin_addr.s_addr;
//        return ADDR_IS_EQU(addr1, addr2);
//    }
//};
//
//struct addr_less_fun{
//    bool operator()(const sockaddr_in &addr1, const sockaddr_in &addr2) const
//    {
//        if(addr1.sin_addr.s_addr > addr2.sin_addr.s_addr) return false;
//        if(addr1.sin_addr.s_addr < addr2.sin_addr.s_addr) return true;
//        if(addr1.sin_port < addr2.sin_port) return true;
//        return false;
//    }
//};

bool addr_less_fun::operator()(const sockaddr_in &addr1, const sockaddr_in &addr2) const
{
    if(addr1.sin_addr.s_addr > addr2.sin_addr.s_addr) return false;
    if(addr1.sin_addr.s_addr < addr2.sin_addr.s_addr) return true;
    if(addr1.sin_port < addr2.sin_port) return true;
    return false;
}



class XSockAddressImpl : public XSockAddress{
public:
    union {
        struct sockaddr_storage storage;
        struct sockaddr addr;
        struct sockaddr_in addr_in;
    } nativeAddr;
    std::string string_;
//    XSockProto proto;

    virtual const sockaddr_in* getNativeAddrIn() const override {
        return &this->nativeAddr.addr_in;
    }
    
    virtual const std::string getString() override{
        if(string_.empty()){
            struct sockaddr_in * addr = &this->nativeAddr.addr_in;
            char buf[64];
            sprintf(buf, "%s:%d", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
            string_ = buf;
        }
        return string_;
    }
    
    virtual bool equal(XSockAddress *other) override{
        return ADDR_IS_EQU(this->nativeAddr.addr_in, *other->getNativeAddrIn());
    }
    
    void setAddress(const std::string& ip, int port){
        socket_addrin_init(ip.c_str(), port, &this->nativeAddr.addr_in);
        string_.clear();
    }
    
    void setAddress(const sockaddr_in* addrIn){
        this->nativeAddr.addr_in = *addrIn;
        string_.clear();
    }
    
    bool isValid(){
        return sock_addr_is_valid(&this->nativeAddr.addr_in);
    }
};

XSockAddress * XSockAddress::newAddress(const std::string& ip, int port){
    XSockAddressImpl * impl = new XSockAddressImpl();
    impl->setAddress(ip, port);
    return impl;
}

XSockAddress * XSockAddress::newAddress(const XSockAddress * other){
//    XSockAddressImpl * impl = new XSockAddressImpl(*(XSockAddressImpl *)other);
    XSockAddressImpl * impl = new XSockAddressImpl();
    impl->setAddress(other->getNativeAddrIn());
    return impl;
}

static inline
const char * xsockproto_string(XSockProto proto){
    const char * str = NULL;
    switch(proto){
        case XSOCK_NONE: str = "none"; break;
        case XSOCK_TCP: str = "tcp"; break;
        case XSOCK_UDP: str = "udp"; break;
        default: str = "unknown";
    }
    return str;
}

static inline
std::string XSockMakeUrl(const char * ip, int port, XSockProto proto, bool isServer){
    char buf[64];
    const char * direction = "";
//    if(proto == XSOCK_TCP){
//        if(isServer){
//            direction = "serv";
//        }else{
//            direction = "clt";
//        }
//    }
    sprintf(buf, "%s%s://%s:%d",xsockproto_string(proto), direction, ip, port);
    return std::string(buf);
}

static inline
const std::string XSockMakeUrl (const struct sockaddr_in * addr, XSockProto proto, bool isServer){
    return XSockMakeUrl(inet_ntoa(addr->sin_addr), ntohs(addr->sin_port), proto, isServer);
}

const std::string XSocket::makeUrl(XSockProto proto,const std::string& ip, int port, bool isServer){
    return XSockMakeUrl(ip.c_str(), port, proto, isServer);
}

/*
 *  proto: SOCK_DGRAM ,  SOCK_STREAM
 */
static evutil_socket_t bind_socket(const struct sockaddr_in * my_addr, int proto, int reuseable){
    evutil_socket_t sock;
    sock = ::socket(PF_INET, proto, 0);
    if (sock < 0) {
//        perror("socket");
        set_last_error(errno, strerror(errno));
        return -1;
    }
    
    if(reuseable){
        int on=1;
        if((setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0){
            set_last_error(errno, strerror(errno));
//            perror("setsockopt SO_REUSEADDR");
        }
    }

    if (::bind(sock, (struct sockaddr*)my_addr, sizeof(struct sockaddr))<0) {
        set_last_error(errno, strerror(errno));
//        perror("bind");
        return -1;
    }
    evutil_make_socket_nonblocking(sock);
    return sock;
}

//static evutil_socket_t bind_socket(const char * ip, int port, int proto){
//    struct sockaddr_in my_addr;
//    socket_addrin_init(ip, port, &my_addr);
//    return bind_socket(&my_addr, proto);
//}


class XSocketImpl : public XSocket {
    XSocketFactory *            factory_ = NULL;
    struct event_base *         ev_base_ = NULL;
    XSockProto                  proto_ = XSOCK_NONE;
    bool                        isListen_ = false;
    XSockAddressImpl            localAddr_;
    std::string                 localUrl_;
    XSockAddressImpl            remoteAddr_;
    std::string                 remoteUrl_;
    bool                        alloc_ = false;
    XSocketCallback *           callback_ = NULL;
    bool                        isKickIO_ = false;
    bool                        isConnected_ = false;
    
    evutil_socket_t             fd_ = -1;
    struct evconnlistener *     ev_listener_ = NULL;
    struct bufferevent *        ev_tcp_ = NULL;
    struct event *              ev_udp_ = NULL;
    bool                        connecting_ = false;
    bool                        isBound_ = false;
    bool                        naglesEnable_ = false;
    

    
    void extractAddress(){
        struct sockaddr_in boundAddr;
        ev_socklen_t socklen = sizeof(boundAddr);
        getsockname(this->fd_, (struct sockaddr *)&boundAddr, &socklen);
        this->localAddr_.setAddress(&boundAddr);
        this->localUrl_ = XSockMakeUrl(&boundAddr, this->proto_, isListen_);
        
        socklen = sizeof(boundAddr);
        int err = getpeername(this->fd_, (struct sockaddr *) &boundAddr, &socklen);
        if (err == 0) {
            this->remoteAddr_.setAddress(&boundAddr);
            this->remoteUrl_ = XSockMakeUrl(&boundAddr, this->proto_, isListen_);
            this->isConnected_ = true;
        }
    }
    
    int kickListen(struct event_base * ev_base){
        int ret = -1;
        do{
            const int flags = LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_EXEC; // |LEV_OPT_CLOSE_ON_FREE;
            this->ev_listener_ = evconnlistener_new(ev_base, NULL, NULL,
                                                   flags,
                                                   0, /* Backlog is '0' because we already said 'listen' */
                                                   this->fd_);
            if (!this->ev_listener_){
                ret = -1;
                break;
            }
            evconnlistener_set_cb(this->ev_listener_, on_accept_socket_event, this);
            ret = 0;
        }while(0);
        return ret;
    }
    
    int kickTCP(struct event_base * ev_base){
        int ret = 0;
        this->ev_tcp_ = bufferevent_socket_new(ev_base, this->fd_, 0/*|BEV_OPT_CLOSE_ON_FREE*/);
        bufferevent_setcb(this->ev_tcp_, on_tcp_read_cb, on_tcp_write_cb, on_tcp_event_cb, this);
        bufferevent_enable(this->ev_tcp_, EV_WRITE);
        bufferevent_enable(this->ev_tcp_, EV_READ);
        if(!this->isConnected_ && this->remoteAddr_.isValid()){
            this->connecting_ = true;
            int socklen = sizeof(struct sockaddr_in);
            ret = bufferevent_socket_connect(this->ev_tcp_, (struct sockaddr *)this->remoteAddr_.getNativeAddrIn(), socklen);
        }
        return ret;
    }
    
    int kickUDP(struct event_base * ev_base){
        this->ev_udp_ = event_new(ev_base, this->fd_, EV_READ|EV_PERSIST, on_udp_event, this);
        event_add(this->ev_udp_, NULL);
        return 0;
    }
    
    virtual int bind(struct event_base * ev_base, evutil_socket_t fd){
        this->fd_ = fd;
        int type;
        socklen_t length = sizeof( int );
        ::getsockopt( this->fd_, SOL_SOCKET, SO_TYPE, &type, &length);
        
        if (type == SOCK_STREAM){
            this->proto_ = XSOCK_TCP;
        }else if(type == SOCK_DGRAM){
            this->proto_ = XSOCK_UDP;
        }else{
            return -1;
        }
        
        if(this->proto_ == XSOCK_TCP){
            int val;
            socklen_t vallen = sizeof(val);
            if (getsockopt(this->fd_, SOL_SOCKET, SO_ACCEPTCONN, &val, &vallen) < 0){
                // TODO: MAC always error here, check linux
                // perror("getsockopt SOL_SOCKET, SO_ACCEPTCONN");
                // return -1;
            }else{
                if(val){
                    this->isListen_ = true;
                }
            }
            
        }
        
        this->ev_base_ = ev_base;
        this->extractAddress();
        this->checkNagles();
        return 0;
    }
    
//    virtual int bind(struct event_base * ev_base,  const std::string& localIp, int localPort, const std::string& remoteIp, int remotePort, XSockProto proto){
//        int ret = this->bind(ev_base, localIp, localPort, proto, false);
//        if(ret) return ret;
//        this->remoteAddr_.setAddress(remoteIp, remotePort);
//        return ret;
//    }
//
//    virtual int bind(struct event_base * ev_base,  const std::string& localIp, int localPort, XSockProto proto, bool isListen){
//        struct sockaddr_in addr;
//        socket_addrin_init(localIp.c_str(), localPort, &addr);
//        return bind(ev_base, &addr, proto, isListen);
//    }
    
    virtual int bind(struct event_base * ev_base, const sockaddr_in * addr, XSockProto proto, bool isListen){
        int ret = -1;
        do{
            this->ev_base_ = ev_base;
            this->fd_ = bind_socket(addr, proto, proto==XSOCK_TCP&&isListen);
            if(this->fd_ < 0){
                this->localUrl_ = XSockMakeUrl(addr, proto, isListen);
                // dbge("fail to bind [%s]", this->localUrl_.c_str());
                ret = -1;
                break;
            }
            this->proto_ = proto;
            
            if(this->proto_ == XSOCK_TCP && isListen){
                if (::listen(this->fd_, 128) < 0) {
//                    perror("listen");
                    set_last_error(errno, strerror(errno));
                    ret = -1;
                    break;
                }
                this->isListen_ = true;
            }
            
            this->ev_base_ = ev_base;
            this->extractAddress();
            this->checkNagles();
            
            ret = 0;
        }while(0);
        return ret;
    }
    
    int checkBind(){
        if(isBound_) return 0;
        int ret = -1;
        do {
            if(fd_ < 0){
                ret = this->bind(ev_base_, localAddr_.getNativeAddrIn(), proto_, isListen_);
                if(ret) break;
            }else{
                ret = this->bind(ev_base_, fd_);
            }
            isBound_ = (ret==0);
        } while (0);
        return ret;
    }
    
    void checkNagles(){
        if(fd_ >= 0 && proto_ == XSOCK_TCP){
            int kOne = naglesEnable_?0:1;
            int err = setsockopt(fd_, SOL_SOCKET, TCP_NODELAY, &kOne, sizeof(kOne));
//            int err = setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &kOne, sizeof(kOne));
//            dbgi("[%s] TCP_NODELAY=%d, err=[%d-%s]", this->getLocalUrl().c_str(), kOne, err, (err < 0 ? strerror(err) : ""));
            (void) err;
        }
    }
    
public:
    XSocketImpl(XSocketFactory * factory
                , struct event_base * ev_base
                , const std::string& localIp
                , int localPort
                , const std::string& remoteIp
                , int remotePort
                , XSockProto proto
                , bool isListen)
    : factory_(factory)
    , ev_base_(ev_base)
    , proto_(proto)
    , isListen_(isListen){
        this->localAddr_.setAddress(localIp, localPort);
        this->remoteAddr_.setAddress(remoteIp, remotePort);
        this->localUrl_ = XSockMakeUrl(this->localAddr_.getNativeAddrIn(), proto, isListen_);
//        checkBind();
    }
    
    XSocketImpl(XSocketFactory * factory
                , struct event_base * ev_base
                , evutil_socket_t fd)
    : factory_(factory)
    , ev_base_(ev_base)
    , fd_(fd){
        if(fd_ >= 0){
            this->extractAddress();
        }
    }
    
    virtual ~XSocketImpl(){
        this->close();
    }
    
    virtual void close() override{
        if(this->ev_tcp_){
            bufferevent_free(this->ev_tcp_);
            this->ev_tcp_ = NULL;
        }
        if(this->ev_listener_){
            evconnlistener_free(this->ev_listener_);
            this->ev_listener_ = NULL;
        }
        if(this->ev_udp_){
            event_free(this->ev_udp_);
            this->ev_udp_ = NULL;
        }
        if(this->fd_ >= 0){
            ::close(this->fd_);
            this->fd_ = -1;
        }
    }
    

    
    virtual void registerCallback(XSocketCallback * callback) override{
        this->callback_ = callback;
    }
    
    virtual XSockAddress * getLocalAddress() override{
        return &this->localAddr_;
    }
    
    virtual XSockAddress * getRemoteAddress() override{
        return &this->remoteAddr_;
    }
    
    virtual const std::string& getLocalUrl() override{
        return this->localUrl_;
    }
    
    virtual int bind() override{
        return checkBind();
    }
    
    virtual int kickIO() override{
        if(isKickIO_) return 0;
        
        int ret = 0;
        ret = checkBind();
        if(ret) return ret;
        
        if(this->proto_ == XSOCK_TCP){
            if(this->isListen_){
                ret = this->kickListen(ev_base_);
            }else{
                ret = this->kickTCP(ev_base_);
            }
        }else{
            ret = this->kickUDP(ev_base_);
        }
        if(ret == 0){
            isKickIO_ = true;
        }
        return ret;
    }
    
    virtual int recv(void * buf, int bufSize) override{
        // TODO:
        if(this->ev_tcp_){
            struct evbuffer *input = bufferevent_get_input(this->ev_tcp_);
            size_t len = evbuffer_get_length(input);
            if (len <= 0){
                return 0;
            }
            int pos = 0;
            while (1){
                int remain_sz = bufSize - pos;
                uint8_t * data = (uint8_t *)buf+pos;
                int n = evbuffer_remove(input, data, remain_sz);
                if(n <= 0) break;
                pos += n;
            }
            return pos;
        }else if(this->ev_udp_){
            remoteAddr_.string_.clear();
            socklen_t addrsz = sizeof(struct sockaddr_in);
            ssize_t sock_ret = recvfrom(fd_, buf, bufSize, 0, (struct sockaddr*)&remoteAddr_.nativeAddr.addr_in, &addrsz);
            return (int)sock_ret;
        }
        
        return -1;
    }
    
    virtual int send(const void * data, int length, const XSockAddress * remoteAddress) override{
        if(this->ev_tcp_){
            bufferevent_write(this->ev_tcp_, data, length);
            return length;
        }else if(this->ev_udp_){
            const struct sockaddr_in * paddr = remoteAddress->getNativeAddrIn();
            ssize_t sock_ret = sendto(fd_, data, length, 0, (struct sockaddr*)paddr, sizeof(struct sockaddr_in));
            return (int)sock_ret;
        }
        return -1;
    }
    
    virtual void enableNagles(bool enabled) override{
        naglesEnable_ = enabled;
        checkNagles();
    }
    
    static void on_accept_socket_event(struct evconnlistener *listener, evutil_socket_t nfd, struct sockaddr *peer_sa, int peer_socklen, void *arg);
    static void on_udp_event(evutil_socket_t fd, short what, void *arg);
    static void on_tcp_read_cb(struct bufferevent *bev, void * user_data);
    static void on_tcp_write_cb(struct bufferevent *bev, void *user_data);
    static void on_tcp_event_cb(struct bufferevent *bev, short events, void *user_data);
    
};

void XSocketImpl::on_accept_socket_event(struct evconnlistener *listener, evutil_socket_t nfd, struct sockaddr *peer_sa, int peer_socklen, void *arg){
    XSocketImpl * impl = (XSocketImpl*)arg;
//    dbgi("on_accept_socket_event: xsock=%p, nfd=%d", impl, nfd);
    if(impl->callback_){
        XSocket * newsock = impl->factory_->newSocket(nfd);
        newsock->enableNagles(impl->naglesEnable_);
        impl->callback_->onAccept(impl, newsock);
    }else{
        ::close(nfd);
    }
}

void XSocketImpl::on_udp_event(evutil_socket_t fd, short what, void *arg){
    XSocketImpl * impl = (XSocketImpl*)arg;
//    dbgi("on_udp_event: what=0x%04X", what);
    if(what&EV_READ){
        if(impl->callback_){
            impl->callback_->onSocketRecvDataReady(impl);
        }
    }
}

void XSocketImpl::on_tcp_read_cb(struct bufferevent *bev, void * user_data){
    XSocketImpl * impl = (XSocketImpl*)user_data;
    if(impl->callback_){
        impl->callback_->onSocketRecvDataReady(impl);
    }
}

void XSocketImpl::on_tcp_write_cb(struct bufferevent *bev, void *user_data){
//    XSocketImpl * impl = (XSocketImpl*)user_data;
//    dbgi("on_tcp_write_cb: impl=%p", impl);
    struct evbuffer *output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0) {
    }
}

void XSocketImpl::on_tcp_event_cb(struct bufferevent *bev, short events, void *user_data){
    XSocketImpl * impl = (XSocketImpl*)user_data;

//    std::string eventStr;
//    eventStr += (events & BEV_EVENT_READING) ?      " rd" : "";
//    eventStr += (events & BEV_EVENT_WRITING) ?      " wr" : "";
//    eventStr += (events & BEV_EVENT_EOF) ?          " eof" : "";
//    eventStr += (events & BEV_EVENT_ERROR) ?        " err" : "";
//    eventStr += (events & BEV_EVENT_TIMEOUT) ?      " to"  : "";
//    eventStr += (events & BEV_EVENT_CONNECTED) ?    " con" : "";
//    dbgi("on_tcp_event_cb: impl=%p, events=0x%04X-[%s], ", impl, events, eventStr.c_str());
    
    if ((events & BEV_EVENT_EOF) || (events & BEV_EVENT_ERROR)){        
        // must bufferevent_free here, for avoid BEV_EVENT_CONNECTED  when connect fail
        if(events & BEV_EVENT_ERROR){
            set_last_error(errno, strerror(errno));
        }
        
        if(impl->ev_tcp_){
            bufferevent_free(impl->ev_tcp_);
            impl->ev_tcp_ = NULL;
        }
        
        if(impl->callback_){
            impl->callback_->onDisconnect(impl, impl->connecting_);
            // don't touch impl anymore for impl may be freed
        }
    }else {
        if(events & BEV_EVENT_CONNECTED){
            impl->connecting_ = false;
            impl->extractAddress();
            if(impl->callback_){
                impl->callback_->onConnected(impl);
                // don't touch impl anymore for impl may be freed
            }
        }
    }
}


class XSocketFactoryImpl : public XSocketFactory{
    struct event_base * ev_base_ = NULL;

public:
    XSocketFactoryImpl(struct event_base * ev_base):ev_base_(ev_base){
    }
    virtual ~XSocketFactoryImpl(){
        // TODO: clean
    }
    
    virtual XSocket * newSocket(int fd){
        XSocketImpl * xsock = new XSocketImpl(this, ev_base_, fd);
        return xsock;
    }
    
    virtual XSocket * newListenTCPSocket(const std::string& localIp, int localPort){
        XSocketImpl * xsock = new XSocketImpl(this, ev_base_, localIp, localPort, "", 0, XSOCK_TCP, true);
        return xsock;
    }
    
    virtual XSocket * newTCPSocket(const std::string& localIp, int localPort, const std::string& remoteIp, int remotePort){
        XSocketImpl * xsock = new XSocketImpl(this, ev_base_, localIp, localPort, remoteIp, remotePort, XSOCK_TCP, false);
        return xsock;
    }
    
    virtual XSocket * newUDPSocket(const std::string& localIp, int localPort, const std::string& remoteIp, int remotePort){
        XSocketImpl * xsock = new XSocketImpl(this, ev_base_, localIp, localPort, remoteIp, remotePort, XSOCK_UDP, false);
        return xsock;
    }
    
};

XSocketFactory * XSocketFactory::newFactory(struct event_base * ev_base){
    return new XSocketFactoryImpl(ev_base);
}







