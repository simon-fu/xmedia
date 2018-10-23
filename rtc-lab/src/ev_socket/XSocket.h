

#ifndef XSocket_hpp
#define XSocket_hpp

#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>

enum XSockProto{
    XSOCK_NONE = 0,
    XSOCK_TCP = SOCK_STREAM,
    XSOCK_UDP = SOCK_DGRAM
};


class XSockAddress;
class XSocket;

struct addr_less_fun{
    bool operator()(const sockaddr_in &addr1, const sockaddr_in &addr2) const;
};

class XSockAddress{
public:
    virtual ~XSockAddress(){}
    virtual const sockaddr_in* getNativeAddrIn()const = 0;
    virtual const std::string getLocalIp() = 0;
    virtual int getLocalPort() = 0;
    virtual const std::string getString() = 0;
    virtual bool equal(XSockAddress *other) = 0;
    static XSockAddress * newAddress(const std::string& ip, int port);
    static XSockAddress * newAddress(const XSockAddress * other);
};

class XSocketCallback{
public:
    virtual ~XSocketCallback(){}
    virtual void onSocketRecvDataReady(XSocket *xsock) {};
    virtual void onAccept(XSocket *xsock, XSocket *newxsock) {};
    virtual void onConnected(XSocket *xsock) {};
    virtual void onDisconnect(XSocket *xsock, bool isConnecting) {};
};

class XSocket{
public:
    virtual ~XSocket(){}
    virtual void setUserData(void * userData) = 0;
    virtual void * getUserData() = 0;
    virtual void registerCallback(XSocketCallback * callback) = 0;
    virtual XSockAddress * getLocalAddress() = 0;
    virtual XSockAddress * getRemoteAddress() = 0;
    virtual const std::string& getLocalUrl() = 0;
    virtual int bind() = 0;
    virtual int kickIO() = 0;
    virtual int recv(void * buf, int bufSize) = 0;
    virtual int send(const void * data, int length, const XSockAddress * remoteAddress) = 0;
    virtual void close() = 0;
    virtual void enableNagles(bool enabled) = 0;

    static const std::string makeUrl(XSockProto proto,const std::string& ip, int port, bool isServer);
};


class XSocketFactory{
public:
    virtual ~XSocketFactory(){}
    virtual XSocket * newSocket(int fd) = 0;
    virtual XSocket * newListenTCPSocket(const std::string& localIp, int localPort) = 0;
    virtual XSocket * newTCPSocket(const std::string& localIp, int localPort, const std::string& remoteIp, int remotePort) = 0;
    virtual XSocket * newUDPSocket(const std::string& localIp, int localPort, const std::string& remoteIp, int remotePort) = 0;
    
    static XSocketFactory * newFactory(struct event_base * ev_base);
    static const std::string& getLastError(int * error);
};



#endif /* XSocket_hpp */
