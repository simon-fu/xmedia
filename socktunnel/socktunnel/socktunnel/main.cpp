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

// mac
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "XSocket.h"

#define dbgv(...) do{  printf("<tunnel>[D] " __VA_ARGS__); printf("\n"); }while(0)
#define dbgi(...) do{  printf("<tunnel>[I] " __VA_ARGS__); printf("\n"); }while(0)
#define dbge(...) do{  printf("<tunnel>[E] " __VA_ARGS__); printf("\n"); }while(0)

#define ZERO_ALLOC(o, type, sz) do{o=(type)malloc(sz); memset(o, 0, sz);}while(0)







static inline
const std::string socket_addrin_to_string (struct sockaddr_in * addr){
    char dst[64];
    sprintf(dst, "%s:%d", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    return std::string(dst);
}



// header ==>


struct XAddrDesc{
    XSockProto          proto = XSOCK_NONE;
    std::string         ip;
    int                 minPort = 0;
    int                 maxPort = 0;
    XAddrDesc(){}
    XAddrDesc(XSockProto proto_, const std::string& ip_, int port_)
    :proto(proto_), ip(ip_), minPort(port_), maxPort(port_){
    }
};

struct XAgentDesc{
    XAddrDesc           localAgentAddrd;
    XAddrDesc           localForwardAddrd;
    XAddrDesc           remoteCustomerAddrd;
};

struct XSockRule{
    XSockProto          proto;
    std::string         localIp;
    int                 localPort = 0;
    std::string         srcIp;
    int                 srcPort = 0;
    std::string         dstIp;
    int                 dstPort = 0;
    int                 index = -1;
};

struct TunnelConfig{
    XAddrDesc                tunnelAddr;
    std::vector<XAgentDesc>  agentDescs;
};

// header <==


#define is_xsockdesc_valid(x) ((x)->proto != XSOCK_NONE && (x)->minPort >= 0 && (x)->maxPort >= (x)->minPort)


class XTransport;
class XTransportRaw;
struct TunnelEntity;
struct XSockConnection;
struct XForwardRule;
struct XTunnel;
struct XForward;
typedef std::list<XSockConnection> ConnectionList;
typedef std::list<XTunnel> TunnelList;
typedef std::list<XForward> ForwardList;
typedef std::map <struct sockaddr_in, XSockConnection*, addr_less_fun> Addr2ConnMap;
typedef std::map <struct sockaddr_in, const XSockRule*, addr_less_fun> Addr2RuleMap;
typedef std::map <struct sockaddr_in, XForward, addr_less_fun> ForwardMap;


struct XStatistics{
    uint64_t bytes;
    uint64_t packets;
};

class XTransport{
public:
    virtual ~XTransport(){}
    virtual int sendData(uint8_t * data, int length) = 0;
};

class XTransportRaw{
    XSocket * xsock_;
    XSockAddress * remoteAddress_;
public:
    XTransportRaw(XSocket * xsock, XSockAddress * remoteAddress) : xsock_(xsock), remoteAddress_(remoteAddress) {
    }
    virtual ~XTransportRaw(){}
    int sendData(uint8_t * data, int length){
        return xsock_->send(data, length, remoteAddress_);
    }
};

class XTransportTunnel{
    XSocket * xsock_;
    XSockAddress * remoteAddress_;
public:
    XTransportTunnel(XSocket * xsock, XSockAddress * remoteAddress)
        : xsock_(xsock), remoteAddress_(remoteAddress) {
    }
    virtual ~XTransportTunnel(){}
    int sendData(uint8_t * data, int length){
        // TODO: wrap data in tunnel packet
        return 0;
    }
};

//struct XSockConnection{
//    XSocket *                   xsock;
//    XStatistics                 sentStati;
//    XStatistics                 recvStati;
//    unsigned char sendBuf[2*1024];
//    unsigned char recvBuf[2*1024];
//};
//
//struct XForwardRule{
//    int forwardIndex = -1;
//    XSockProto  proto = XSOCK_NONE;
//    XSockAddress agentAddr;
//    XSockAddress forwardAddr;
//    XSockAddress customerAddr;
//};
//
//struct XTunnel{
//    TunnelList::iterator        link;
//    TunnelEntity *              owner = NULL;
//    XSockConnection             conn;
//};
//
//struct XAgent{
//    TunnelEntity *              owner = NULL;
//    XForwardRule *              forwardRule = NULL;
//    XSockConnection             conn;
//};
//
//struct XForward{
//    TunnelEntity *              owner = NULL;
//    XSockConnection             conn;
//    XSockAddress                srcAddr;
//    XSockConnection   *         uplinkConn = NULL;
//    bool                        uplinkInTunnel = false;
//};
//
//
//struct TunnelEntity{
//    TunnelConfig                    config;
//    Addr2ConnMap                    src2ConnMap;
//    Addr2RuleMap                    src2RuleMap;
//    std::vector<const XSockRule*>   dstRules;
//
//    TunnelList                      tunnels;
//    std::vector<XSockConnection>    fowardConns;
//
//    TunnelList                      listenTunnels;
//    std::vector<XForwardRule>       forwardRules;
//    std::vector<XAgent>             agents;
//    ForwardMap                      forwardMap;
//
//    struct event_base *         ev_base = NULL;
//    struct event *              ev_signal = NULL;
//    struct evconnlistener *     ev_listener = NULL;
//
//    TunnelEntity(){
//
//    }
//};
//
//
//static
//void on_forward_udp_event(evutil_socket_t fd, short what, void *arg){
//    if(!(what&EV_READ)) return ;
//    XForward * forward = (XForward*)arg;
//    XSockConnection * conn = &forward->conn;
//    dbgi("on_forward_udp_event %p, %d", forward, fd);
//    struct sockaddr_in tempadd;
//    socklen_t len = sizeof(tempadd);
//    int bytes_to_recv = sizeof(conn->recvBuf);
//    while(1){
//        ssize_t sock_ret = recvfrom(conn->fd, conn->recvBuf, bytes_to_recv, 0, (struct sockaddr*)&tempadd, &len);
//        if(sock_ret <= 0){
//            break;
//        }
//        int udp_len = (int)sock_ret;
//        dbgi("got forward udp, len=%d", udp_len);
//        if(forward->uplinkConn){
//            XSockConnection * toConn = forward->uplinkConn;
//            memcpy(toConn->sendBuf, conn->recvBuf, udp_len);
//            tempadd = forward->srcAddr.nativeAddr.addr_in;
//            sock_ret = sendto(toConn->fd, toConn->sendBuf, udp_len, 0, (struct sockaddr*)&tempadd, sizeof(tempadd));
//            std::string toUrl = XSockMakeUrl(&tempadd, XSOCK_UDP, false);
//            dbgi("back to [%s]", toUrl.c_str());
//        }
//    }
//}
//
//
//static
//void on_agent_udp_event(evutil_socket_t fd, short what, void *arg){
//    if(!(what&EV_READ)) return ;
//    XAgent * agent = (XAgent*)arg;
//    XSockConnection * conn = &agent->conn;
//    dbgi("on_agent_udp_event %p", agent);
//
//
//    struct sockaddr_in tempadd;
//    socklen_t len = sizeof(tempadd);
//    int bytes_to_recv = sizeof(conn->recvBuf);
//    while(1){
//        ssize_t sock_ret = recvfrom(conn->fd, conn->recvBuf, bytes_to_recv, 0, (struct sockaddr*)&tempadd, &len);
//        if(sock_ret <= 0){
//            break;
//        }
//        int udp_len = (int)sock_ret;
//        dbgi("got agent udp, len=%d", udp_len);
//
//        XForward * forward= NULL;
//        auto it = agent->owner->forwardMap.find(tempadd);
//        if(it == agent->owner->forwardMap.end()) {
//            agent->owner->forwardMap[tempadd] =  XForward();
//            forward = &agent->owner->forwardMap[tempadd];
//            forward->owner = agent->owner;
//            forward->srcAddr.proto = conn->proto;
//            forward->srcAddr.nativeAddr.addr_in = tempadd;
//            forward->uplinkConn = conn;
//            forward->uplinkInTunnel = false;
//            sockaddr_in * addr = &(agent->forwardRule->forwardAddr.nativeAddr.addr_in);
//            XSockConnectionBind(forward->owner->ev_base,
//                                &forward->conn,
//                                addr,
//                                agent->forwardRule->forwardAddr.proto,
//                                false);
//            if(agent->forwardRule->forwardAddr.proto == XSOCK_TCP){
//                // TODO: connect to tunnel server
//            }else if(agent->forwardRule->forwardAddr.proto == XSOCK_UDP){
//                forward->conn.ev = event_new(forward->owner->ev_base, forward->conn.fd, EV_READ|EV_PERSIST, on_forward_udp_event, forward);
//                event_add(forward->conn.ev, NULL);
//                dbgi("new forward at [%s]", forward->conn.localUrl.c_str());
//            }
//        }else{
//            forward = &it->second;
//        }
//        if(forward){
//            // TODO: send tcp case
//            memcpy(forward->conn.sendBuf, conn->recvBuf, udp_len);
//            tempadd = agent->forwardRule->customerAddr.nativeAddr.addr_in;
//
//            sock_ret = sendto(forward->conn.fd, forward->conn.sendBuf, udp_len, 0, (struct sockaddr*)&tempadd, sizeof(tempadd));
//            std::string toUrl = XSockMakeUrl(&tempadd, XSOCK_UDP, false);
//            dbgi("forward to [%s]", toUrl.c_str());
//        }
//
//    }
//
//}
//
//
//
//static inline
//void XSockConnectionFree(XSockConnection * conn){
//    // TODO:
//}
//
//static inline
//void XSockAddressInit(XSockAddress * xaddr, XSockProto proto, const char * ip, int port){
//    xaddr->proto = proto;
//    socket_addrin_init(ip, port, &xaddr->nativeAddr.addr_in);
//}
//
//TunnelEntity * tunserver_create(const TunnelConfig * config)
//{
//    int                 ret = 0;
//    TunnelEntity *      obj = 0;
//    XTunnel *           tunnel = NULL;
//    XAgent *            agent = NULL;
//    do{
//        obj = new TunnelEntity();
//        obj->config = *config;
//
//        obj->ev_base = event_base_new();
//
//
//        if(is_xsockdesc_valid(&obj->config.tunnelAddr)){
//            XAddrDesc * addrd = &obj->config.tunnelAddr;
//            for(int port = addrd->minPort; port <= addrd->maxPort; port++){
//                XTunnel newTunnel;
//                newTunnel.link = obj->listenTunnels.insert(obj->listenTunnels.end(), newTunnel);
//                tunnel = &obj->listenTunnels.back();
//                tunnel->owner = obj;
//                ret = XSockConnectionBind(obj->ev_base, &tunnel->conn, addrd->ip.c_str(), port, addrd->proto, true);
//                if(ret){
//                    dbge("bind tunnel fail, addr=[%s]", tunnel->conn.localUrl.c_str());
//                    break;
//                }
//
//                if(addrd->proto == XSOCK_TCP && !tunnel->conn.ev_listener){
//                    // TODO: connect to tunnel server
//                }else{
//                    if(tunnel->conn.ev_listener){
//                        evconnlistener_set_cb(tunnel->conn.ev_listener, on_tunnel_accept_socket_event, tunnel);
//                    }else if(addrd->proto == XSOCK_UDP){
//                        tunnel->conn.ev = event_new(obj->ev_base, tunnel->conn.fd, EV_READ|EV_PERSIST, on_tunnel_udp_event, tunnel);
//                        event_add(tunnel->conn.ev, NULL);
//                    }
//                    dbgi("listen tunnel at [%s]", tunnel->conn.localUrl.c_str());
//                }
//                tunnel = NULL;
//            }
//        }
//
//        for(auto &o : obj->config.agentDescs){
//            // TODO: check port range consistency of agent, forward, cumstomer
//
//            bool isAgent = false;
//            int minPort = o.agentAddrd.minPort;
//            int maxPort = o.agentAddrd.maxPort;
//            for(int port = minPort; port <= maxPort; port++){
//                obj->forwardRules.push_back(XForwardRule());
//                XForwardRule * rule = &obj->forwardRules.back();
//                XSockAddressInit(&rule->customerAddr, o.customerAddrd.proto, o.customerAddrd.ip.c_str(), port);
//                XSockAddressInit(&rule->agentAddr, o.agentAddrd.proto, o.agentAddrd.ip.c_str(), port);
//                XSockAddressInit(&rule->forwardAddr, o.forwardAddrd.proto, o.forwardAddrd.ip.c_str(), 0); // TODO: specific forward port
//
//                XAddrDesc * addrd = &o.agentAddrd;
//                XAgent newAgent;
//                obj->agents.push_back(newAgent);
//                agent = &obj->agents.back();
//                agent->owner = obj;
//                agent->forwardRule = rule;
//                if(is_xsockdesc_valid(&o.agentAddrd)){
//                    isAgent = true;
//                    ret = XSockConnectionBind(obj->ev_base, &agent->conn, addrd->ip.c_str(), port, addrd->proto, true);
//                    if(ret){
//                        dbge("bind agent fail, addr=[%s]", agent->conn.localUrl.c_str());
//                        break;
//                    }
//                    if(agent->conn.ev_listener){
//                        // TODO:
//                        //evconnlistener_set_cb(agent->conn.ev_listener, , agent);
//                    }else if(addrd->proto == XSOCK_UDP){
//                        agent->conn.ev = event_new(obj->ev_base, agent->conn.fd, EV_READ|EV_PERSIST, on_agent_udp_event, agent);
//                        event_add(agent->conn.ev, NULL);
//                    }
//                    dbgi("agent at [%s]", agent->conn.localUrl.c_str());
//                    agent = NULL;
//                }
//                dbgi("forward rule [%s:%d-%d] -> [%s:%d-%d]", o.agentAddrd.ip.c_str(), o.agentAddrd.minPort, o.agentAddrd.maxPort
//                     , o.customerAddrd.ip.c_str(), o.customerAddrd.minPort, o.customerAddrd.maxPort);
//
//            }
//
//        }
//
//
////        plw_u32 try_ports = 1;
////
////
////        plw_port_t port = config->udp_port;
////        uret = 0;
////        for(plw_u32 ui = 0; ui < try_ports;ui++)
////        {
////            port = config->udp_port + ui;
////            uret = plw_udp_create(0, &port, config->sender_buf_size_mb*1024*1024, config->recver_buf_size_mb*1024*1024, &obj->udp);
////            if(uret)
////            {
////                dbge("fail to try bind udp at port %d\n", port);
////                obj->udp = 0;
////                //break;
////            }
////            else
////            {
////                obj->actual_udp_port = port;
////                dbgi("udp send buf size %d MB\n", config->sender_buf_size_mb);
////                dbgi("udp recv buf size %d MB\n", config->recver_buf_size_mb);
////                dbgi("bind udp port at %d\n", port);
////                break;
////            }
////        }
////
////        if(uret) break;
////
////        if(config->udp_port != port)
////        {
////            config->udp_port = port;
////        }
////
////        obj->udp_sock_pool = new udp_socket_pool;
////
////#if STRESS_TEST == 0
////        plw_u32 udp_start_port = 6000;
////        plw_u32 udp_port_count = 1;
////        dbgi("pre-alloc udp ports start=%d, count=%d ...\n", udp_start_port, udp_port_count);
////        uret = obj->udp_sock_pool->pre_alloc(udp_start_port, udp_port_count);
////        if (uret)
////        {
////            dbge("fail to pre-alloc udp ports start=%d, count=%d\n", udp_start_port, udp_port_count);
////            break;
////        }
////        dbgi("successfully pre-alloc udp ports start=%d, count=%d\n", udp_start_port, udp_port_count);
////
////#endif
////
////
////
////        obj->ev_base = event_base_new();
////        obj->ev_signal = evsignal_new(obj->ev_base, SIGINT, ev_signal_cb, (void *)obj);
////        event_add(obj->ev_signal, NULL);
////
////
////        plw_str_copy(obj->server_id, config->server_id.c_str());
////        //if (!config->server_id.empty())
////        //{
////        //    plw_str_copy(obj->server_id, config->server_id.c_str());
////        //}
////        //else
////        //{
////        //    plw_sprintf(obj->server_id, "serv_%s_%d", config->public_ip.c_str(), config->server_port);
////        //}
////
////        dbgi("server_id %s\n", obj->server_id);
////        obj->sender = new mdata_sender_impl(obj, obj->udp);
////        mconf_mgr_create(obj->sender, obj->server_id, obj->config, &obj->conf_mgr);
////        mconf_mgr_set_udp_pool(obj->conf_mgr, obj->udp_sock_pool);
////        mconf_mgr_set_key(obj->conf_mgr, config->secret_key);
////        mconf_mgr_set_evbase(obj->conf_mgr, obj->ev_base);
////
////        uret = 0;
////        for(plw_u32 ui = 0; ui < try_ports;ui++)
////        {
////            plw_u32 port = config->server_port+ui;
////            memset(&sin, 0, sizeof(sin));
////            sin.sin_family = AF_INET;
////            sin.sin_port = htons(port);
////
////            obj->ev_listener = evconnlistener_new_bind(obj->ev_base, listener_cb, (void *)obj,
////                                                       LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
////                                                       (struct sockaddr*)&sin,
////                                                       sizeof(sin));
////
////            if (!obj->ev_listener)
////            {
////                dbge("can't bind at port %d !!!\n", port);
////                uret = PLWS_ERROR;
////                //break;
////            }
////            else
////            {
////                obj->server_port = port;
////                uret = 0;
////                break;
////            }
////        }
////        if(uret) break;
////
////        if (config->server_port != obj->server_port)
////        {
////            config->server_port = obj->server_port;
////
////            char buf[128];
////            plw_sprintf(buf, "serv_%s_%d", config->public_ip.c_str(), config->server_port);
////            config->server_id = buf;
////        }
////
////
////        if(config->separate_udp_task)
////        {
////            uret = plw_mutex_create(&obj->lock);
////            if(uret) break;
////
////            uret = plw_task_create(udp_task, obj, 0, PLW_PRIOR_NORMAL, &obj->task);
////            if(uret) break;
////
////        }
////        else
////        {
////            evutil_socket_t * psock = (evutil_socket_t *)plw_udp_socket(obj->udp);
////            evutil_socket_t fd = *psock;
////            evutil_make_socket_nonblocking(fd);
////            obj->ev_udp = event_new(obj->ev_base, fd, EV_READ|EV_PERSIST, on_udp_event, obj);
////            event_add(obj->ev_udp, NULL);
////        }
////
////        obj->ev_timer = event_new(obj->ev_base, -1, EV_PERSIST, timer_event_handler, obj);
////        struct timeval tv;
////        tv.tv_sec = 1;
////        tv.tv_usec = 0;
////        evtimer_add(obj->ev_timer, &tv);
////
////        obj->ev_http = evhttp_new(obj->ev_base);
////        if (!obj->ev_http) {
////            dbge("can't create ev_http !!!\n");
////            uret = PLWS_ERROR;
////            break;
////        }
////
////        evhttp_set_cb(obj->ev_http, "/status", on_http_get_status, obj);
////        evhttp_set_cb(obj->ev_http, "/on_push", on_http_push_cb, obj);
////        evhttp_set_cb(obj->ev_http, "/on_push_done", on_http_push_done_cb, obj);
////
////        //        evhttp_set_cb(obj->ev_http, "/dump", on_http_push_cb, obj);
////        evhttp_set_gencb(obj->ev_http, on_http_gen_cb, obj);
////
////        int http_port = 8800;
////        //        evhttp_bind_socket_with_handle(obj->ev_http, "0.0.0.0", http_port);
////        int ret = evhttp_bind_socket(obj->ev_http, "0.0.0.0", http_port);
////        if (ret != 0) {
////            dbge("can't bind http at port %d !!!\n", http_port);
////            uret = PLWS_ERROR;
////            return 1;
////        }
////        dbgi("http port %d\n", http_port);
////
////        *pobj = obj;
//    } while (0);
//    if(ret){
//        if(tunnel){
//            XSockConnectionFree(&tunnel->conn);
//            obj->listenTunnels.erase(tunnel->link);
//            tunnel = NULL;
//        }
//        if(agent){
//            // TODO:
//        }
//        if(obj){
//            delete obj;
//            obj = NULL;
//        }
//    }
//    return obj;
//}
//
//void run_tunnel_server(){
//    TunnelConfig config;
//    config.tunnelAddr.proto = XSOCK_TCP;
//    config.tunnelAddr.minPort = config.tunnelAddr.maxPort = 8722;
//
//    XAgentDesc agentDesc;
//    agentDesc.agentAddrd.proto = XSOCK_UDP;
//    agentDesc.agentAddrd.minPort = agentDesc.agentAddrd.maxPort = 20000;
//
//    agentDesc.forwardAddrd.proto = XSOCK_UDP;
//
//    agentDesc.customerAddrd.proto = XSOCK_UDP;
//    agentDesc.customerAddrd.minPort = agentDesc.customerAddrd.maxPort = 20000;
//    agentDesc.customerAddrd.ip = "121.41.87.159";
//    config.agentDescs.push_back(agentDesc);
//
//    TunnelEntity * server = tunserver_create(&config);
//    if(!server) return;
//    event_base_dispatch(server->ev_base);
//
//}

class TCPForwardPair{
    class Leg : public XSocketCallback{
        uint8_t buf_[2*1024];
        int bufSize_ = sizeof(this->buf_);
        
    public:
        TCPForwardPair * owner_;
        XSocket *xsock_;
        XSocket *xother_;
        std::string name_;
        std::string infoStr_;
        Leg(TCPForwardPair * owner, XSocket *xsock, const std::string&  name, XSocket *other)
        :owner_(owner), xsock_(xsock), name_(name), xother_(other){
            xsock_->registerCallback(this);
            char buf[1024];
            sprintf(buf, "[%s]-[%p], local=[%s], remote=[%s]", name_.c_str(),  xsock_
                 , xsock->getLocalAddress()->getString().c_str()
                 , xsock->getRemoteAddress()->getString().c_str());
            infoStr_ = buf;
        }
        virtual ~Leg(){
            if(xsock_){
//                dbgi("close tcp leg %s", infoStr_.c_str());
                delete xsock_;
                xsock_ = NULL;
            }
        }
        virtual void onConnected(XSocket *xsock) override {
            dbgi("connected, tcp leg %s", infoStr_.c_str());
        }
        
        virtual void onDisconnect(XSocket *xsock, bool isConnecting) override {
            if(isConnecting){
                dbge("fail to connect, tcp leg %s, reason=[%s]", infoStr_.c_str(), XSocketFactory::getLastError(NULL).c_str());
            }else{
                dbgi("disconnected, tcp leg %s, reason=[%s]", infoStr_.c_str(), XSocketFactory::getLastError(NULL).c_str());
            }
            owner_->checkOnDisconn();
        }
        
        virtual void onSocketRecvDataReady(XSocket *xsock) override {
//            dbgi("data ready, tcp leg %s ", infoStr_.c_str());
            int bytesRecv = xsock->recv(buf_, bufSize_);
            if( bytesRecv > 0){
                XSocket * other = xother_;
                int bytesSent = 0;
                while(bytesSent < bytesRecv){
                    int n = other->send(buf_+bytesSent, bytesRecv-bytesSent, NULL);
                    if( n <= 0) break;
                    bytesSent += n;
                }
            }
        }
    };
    
    Leg leg1_;
    Leg leg2_;
    bool freeOnDisconn_;
    
    void checkOnDisconn(){
        if(this->freeOnDisconn_){
            delete this;
        }
    }
    
public:
    TCPForwardPair(XSocket *xsock1, XSocket *xsock2, bool freeOnDisconn = true)
    : leg1_(this, xsock1, "sock1", xsock2)
    , leg2_(this, xsock2, "sock2", xsock1)
    , freeOnDisconn_(freeOnDisconn){
    }
    
    virtual ~TCPForwardPair(){
        dbgi("close tcp pair %p", this);
    }
    
    int start(){
        dbgi("start tcp pair leg1 %p, %s", this, leg1_.infoStr_.c_str());
        dbgi("start tcp pair leg2 %p, %s", this, leg2_.infoStr_.c_str());
        int ret = 0;
        do{
            ret = leg1_.xsock_->kickIO();
            if(ret) break;
            ret = leg2_.xsock_->kickIO();
            if(ret) break;
            ret = 0;
        }while (0);
        if(ret){
            checkOnDisconn();
        }
        return ret;
    }
};


class TCPBridge : public XSocketCallback{
    XSocketFactory * factory_;
    std::string ip1_;
    int port1_ = -1;
    std::string ip2_;
    int port2_ = -1;
    XSocket * agentSock_ = NULL;
public:
    TCPBridge(XSocketFactory * factory, const std::string& ip1, int port1,  const std::string& ip2, int port2)
    :factory_(factory), ip1_(ip1), port1_(port1), ip2_(ip2), port2_(port2){
    }
    virtual ~TCPBridge(){
        if(agentSock_){
            delete agentSock_;
            agentSock_ = NULL;
        }
    }
    int startServer2Client(){
        int ret = -1;
        do {
            agentSock_ = factory_->newListenTCPSocket(ip1_, port1_);
            agentSock_->registerCallback(this);
            ret = agentSock_->kickIO();
            if(ret == 0){
                dbgi("bridge listen at [%s] -> [%s]", agentSock_->getLocalUrl().c_str(), XSocket::makeUrl(XSOCK_TCP, ip2_, port2_, false).c_str());
            }else{
                dbge("bridge faid to listen at [%s], reason=[%s]", agentSock_->getLocalUrl().c_str(), XSocketFactory::getLastError(NULL).c_str());
            }
        } while (0);
        return ret;
    }
    
    virtual void onAccept(XSocket *xsock, XSocket *newxsock) override {
        XSocket *xsock2 = factory_->newTCPSocket("", 0, ip2_, port2_);
        TCPForwardPair * pair = new TCPForwardPair(newxsock, xsock2);
        pair->start();
    }
};


#define FORWARD_MAX_SLOTS 8
class UDPBridge : public XSocketCallback{
    
    struct Forwarder : public XSocketCallback{
        UDPBridge * owner_ = NULL;
        uint8_t buf_[2*1024];
        int bufSize_ = sizeof(this->buf_);
        int64_t time_ = 0;
    public:
        XSockAddress * from_ = NULL;
        XSockAddress * to_ = NULL;
        XSocket * other_ = NULL;
        int slot_ = -1;
        Forwarder(){}
        Forwarder(UDPBridge * owner, XSocket * other) : owner_(owner) , other_(other){
        }
        virtual void onSocketRecvDataReady(XSocket *xsock) override {
            while(1) {
                int bytesRecv = xsock->recv(buf_, bufSize_);
                if(bytesRecv <= 0) break;
                if(xsock->getRemoteAddress()->equal(to_)){
                    // dbgi("udp forwarder got from [%s], %d byes", xsock->getRemoteAddress()->getString().c_str(), bytesRecv);
                    owner_->agentSock_->send(buf_, bytesRecv, from_);
                    time_ = 0;
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
    ForwarderMap  forwardMap_ ;
    ForwarderPtrMap forwardSlots_[FORWARD_MAX_SLOTS];
    int slotCounter_ = 0;
    int checkCounter_ = 0;
public:
    UDPBridge(XSocketFactory * factory, const std::string& ip1, int port1,  const std::string& ip2, int port2)
    :factory_(factory), ip1_(ip1), port1_(port1), ip2_(ip2), port2_(port2){
    }
    virtual ~UDPBridge(){
        while( !forwardMap_.empty() ) {
            this->freeFoward(forwardMap_.begin()->second, "BridgeFree");
        }
        
        if(agentSock_){
            delete agentSock_;
            agentSock_ = NULL;
        }
    }
    
    void freeFoward(Forwarder& o, const std::string& reason){
        if(o.other_ && o.from_){
            dbgi("free udp forward [%s] <-> [%s]-[%s] <-> [%s], reason[%s]"
                 , o.from_->getString().c_str()
                 , agentSock_->getLocalAddress()->getString().c_str()
                 , o.other_->getLocalAddress()->getString().c_str()
                 , o.other_->getRemoteAddress()->getString().c_str()
                 , reason.c_str());
        }
        
        if(o.other_ && o.other_ != agentSock_){
            delete o.other_;
            o.other_ = NULL;
        }
        if(o.from_)
        {
            const sockaddr_in& key = *o.from_->getNativeAddrIn();
            if(o.slot_ >= 0){
                ForwarderPtrMap &m = forwardSlots_[o.slot_];
                m.erase(key);
            }
            forwardMap_.erase(key);
            delete o.from_;
            o.from_ = NULL;
        }
        
        if(o.to_)
        {
            delete o.to_;
            o.to_ = NULL;
        }
    }
    
    void timeToCheck(){
        const int64_t timeout = 90;
        int64_t nowSecond = time(NULL);
        int slot = checkCounter_%FORWARD_MAX_SLOTS;
        ++checkCounter_;
        ForwarderPtrMap &m = forwardSlots_[slot];
        std::list<Forwarder *> timeoutList;
        for(auto& it : m){
            Forwarder * forward = it.second;
            if(forward->time_ > 0){
                if((nowSecond - forward->time_) > timeout){
                    timeoutList.push_back(forward);
                }
            }else{
                forward->time_ = nowSecond;
            }
        }
        for(auto& f : timeoutList){
            this->freeFoward(*f, "timeout");
        }
    }
    
    int start(){
        int ret = -1;
        do {
            agentSock_ = factory_->newUDPSocket(ip1_, port1_, "", 0);
            agentSock_->registerCallback(this);
            ret = agentSock_->kickIO();
            if(ret == 0){
                dbgi("bridge listen at [%s] -> [%s]", agentSock_->getLocalUrl().c_str(), XSocket::makeUrl(XSOCK_UDP, ip2_, port2_, false).c_str());
            }else{
                dbge("bridge faid to listen at [%s], reason=[%s]", agentSock_->getLocalUrl().c_str(), XSocketFactory::getLastError(NULL).c_str());
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
            const sockaddr_in& key = *xsock->getRemoteAddress()->getNativeAddrIn();
            auto it = forwardMap_.find(key);
            if(it == forwardMap_.end()) {
                XSocket * other = factory_->newUDPSocket("", 0, ip2_, port2_);
                forwardMap_[key] = Forwarder(this, other);
                forward = &forwardMap_[key];
                forward->from_ = XSockAddress::newAddress(xsock->getRemoteAddress());
                forward->to_ = XSockAddress::newAddress(other->getRemoteAddress());
                forward->other_->registerCallback(forward);
                ret = forward->other_->kickIO();
                
                if(ret){
                    dbge("bind forward udp fail, reason=[%s]", XSocketFactory::getLastError(NULL).c_str());
                    this->freeFoward(*forward,XSocketFactory::getLastError(NULL));
                    forward = NULL;
                    break;
                }
                forward->slot_ = slotCounter_%FORWARD_MAX_SLOTS;
                ForwarderPtrMap &m = forwardSlots_[forward->slot_];
                m[key] = forward;
                ++slotCounter_;
                dbgi("new udp forward [%s] <-> [%s]-[%s] <-> [%s]"
                     , agentSock_->getRemoteAddress()->getString().c_str()
                     , agentSock_->getLocalAddress()->getString().c_str()
                     , other->getLocalAddress()->getString().c_str()
                     , other->getRemoteAddress()->getString().c_str());
            }else{
                forward = &it->second;
            }
            forward->other_->send(buf_, bytesRecv, forward->other_->getRemoteAddress());
            forward->time_ = 0;
        }
    }
};

static
void ev_signal_cb(evutil_socket_t sig, short events, void *user_data)
{
    struct event_base * ev_base = (struct event_base *) user_data;
    long seconds = 0;
    struct timeval delay = { seconds, 1 };
    dbgi("*** caught signal (0x%08X) ***, stop after %ld seconds...", events, seconds);
    event_base_loopexit(ev_base, &delay);
}

static void timer_event_handler(evutil_socket_t fd, short what, void* arg){
    UDPBridge * udpBridge = (UDPBridge *) arg;
    udpBridge->timeToCheck();
}

void run_tunnel_tcppair(){
    struct event_base * ev_base = event_base_new();
    struct event *            ev_signal = NULL;
    struct event *            ev_timer = NULL;
    
    int ret = 0;
    XSocketFactory * factory = XSocketFactory::newFactory(ev_base);
    TCPBridge * tcpBridge = new TCPBridge(factory, "", 8711, "121.41.87.159", 8799);
    UDPBridge * udpBridge = new UDPBridge(factory, "", 8711, "121.41.87.159", 8799);
    do{
        ret = tcpBridge->startServer2Client();
        if(ret) break;
        
        ret = udpBridge->start();
        if(ret) break;
        
        ev_signal = evsignal_new(ev_base, SIGINT|SIGALRM, ev_signal_cb, (void *)ev_base);
        event_add(ev_signal, NULL);
        
        ev_timer = event_new(ev_base, -1, EV_PERSIST, timer_event_handler, udpBridge);
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
    
    if(ev_signal){
        event_free(ev_signal);
        ev_signal = 0;
    }
    if(tcpBridge){
        delete tcpBridge;
        tcpBridge = NULL;
    }
    if(udpBridge){
        delete udpBridge;
        udpBridge = NULL;
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

int main(int argc, const char * argv[]) {
//    run_tunnel_server();
    run_tunnel_tcppair();
    return 0;
}
