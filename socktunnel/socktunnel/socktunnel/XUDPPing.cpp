
#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <thread>
#include <chrono>
//#include <event2/bufferevent.h>
//#include <event2/buffer.h>
//#include <event2/listener.h>
//#include <event2/util.h>
//#include <event2/event.h>
//#include <event2/http.h>
//#include <netinet/in.h>
//#include <unistd.h>

// MAC
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>     // for TCP_NODELAY
#include <arpa/inet.h>       // for IPPROTO_TCP
#include <unistd.h>

#include <limits.h>
#include <float.h>
#include "XUDPPing.h"
//#include "XSocket.h"

#define dbgd(...) // do{  printf("<tunnel>[D] " __VA_ARGS__); printf("\n"); }while(0)
#define dbgi(...) do{  printf("<uping>[I] " __VA_ARGS__); printf("\n"); }while(0)
#define dbge(...) do{  printf("<uping>[E] " __VA_ARGS__); printf("\n"); }while(0)



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

int socket_bind(const char * localIP, int localPort, int proto, int reuseable){
//    int proto = SOCK_DGRAM;
//    int reuseable = 1;
    int sock = ::socket(PF_INET, proto, 0);
    if (sock < 0) {
        dbge("fail to create socket, err=[%d]-[%s]", errno, strerror(errno));
        return -1;
    }
    
    if(reuseable){
        int on=1;
        if((setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0){
            dbge("fail to SO_REUSEADDR, err=[%d]-[%s]", errno, strerror(errno));
            ::close(sock);
            return -2;
        }
    }
    
    struct sockaddr_in my_addr;
    socket_addrin_init(localIP, localPort, &my_addr);
    
    if (::bind(sock, (struct sockaddr*)&my_addr, sizeof(struct sockaddr))<0) {
        dbge("fail to bind, err=[%d]-[%s]", errno, strerror(errno));
        ::close(sock);
        return -3;
    }
    return sock;
}



#define     be_get_u32(xptr)   ((uint32_t) ((((uint32_t)(xptr)[0]) << 24) | ((xptr)[1] << 16) | ((xptr)[2] << 8) | ((xptr)[3] << 0)))        /* by simon */
#define     be_set_u32(xval, xptr)  \
do{\
(xptr)[0] = (uint8_t) (((xval) >> 24) & 0x000000FF);\
(xptr)[1] = (uint8_t) (((xval) >> 16) & 0x000000FF);\
(xptr)[2] = (uint8_t) (((xval) >> 8) & 0x000000FF);\
(xptr)[3] = (uint8_t) (((xval) >> 0) & 0x000000FF);\
}while(0)

#define PKT_TYPE_CTRL               0xfc
#define CTRL_EVENT_TEST_ECHO        101
struct PacketInfo{
    uint32_t sessionNo = 0;
    uint32_t slotIndex = 0;
    uint32_t seq = 0;
    int64_t timestamp = 0;
    int64_t delay = 0;
};

static
int build_serv_echo_packet(const PacketInfo * info, uint8_t buf[] )
{
    
    int offset = 0;
    buf[offset] = PKT_TYPE_CTRL; // command
    offset += 1;
    
    buf[offset] = CTRL_EVENT_TEST_ECHO; // msg type
    offset += 1;
    
    be_set_u32(info->sessionNo, &buf[offset]); // session id
    offset += 4;
    
    be_set_u32(info->slotIndex, &buf[offset]); // slot
    offset += 4;
    
    be_set_u32(info->seq, &buf[offset]); // seq
    offset += 4;
    
    uint32_t lo32 = info->timestamp&0xFFFFFFFFUL;
    uint32_t hi32 = (info->timestamp>>32)&0xFFFFFFFFUL;
    be_set_u32(lo32, &buf[offset]); // timestamp low 32
    offset += 4;
    be_set_u32(hi32, &buf[offset]); // timestamp high 32
    offset += 4;
    
    return offset;
}

static
int parse_echo_packet(uint8_t buf[], uint32_t len, PacketInfo * info)
{
    int ret = -1;
    do{
        if(len < 8){
            ret = -11;
            break;
        }
        
        int offset = 0;
        if(buf[offset] != PKT_TYPE_CTRL){
            ret = -15;
            break;
        }
        offset += 1;
        
        if(buf[offset] != CTRL_EVENT_TEST_ECHO){
            ret = -20;
            break;
        }
        offset += 1;
        
        
        info->sessionNo = be_get_u32(&buf[offset]);
        offset += 4;
        
        info->slotIndex = be_get_u32(&buf[offset]);
        offset += 4;
        
        info->seq = be_get_u32(&buf[offset]);
        offset += 4;
        
        uint32_t lo = be_get_u32(&buf[offset]); // timestamp low 32
        offset += 4;
        uint32_t hi = be_get_u32(&buf[offset]); // timestamp high 32
        offset += 4;
        info->timestamp = hi;
        info->timestamp <<= 32;
        info->timestamp |= lo;
        
        ret = 0;
        
    }while(0);
    return ret;
}

static
int64_t get_now_ms(){
    unsigned long milliseconds_since_epoch =
    std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();
    return milliseconds_since_epoch;
};

struct UPacket{
    int64_t sendTime = 0;
    int64_t delay = 0;
    uint32_t sendSeq = 0;
    uint32_t recvSeq = 0;
};

struct StatiItem{
    int64_t sum ;
    int64_t num ;
    int64_t min ;
    int64_t max ;
    int64_t average ;
    int64_t stddev ;
};

struct StatiItemOper {
    StatiItem stati_;
    StatiItemOper(){
        reset();
    }
    void reset(){
        stati_.sum = 0;
        stati_.num = 0;
        stati_.min = LONG_MAX;
        stati_.max = LONG_MIN;
        stati_.average = 0;
        stati_.stddev = 0;
    }
    void update(int64_t value){
        stati_.sum += value;
        ++stati_.num;
        if(stati_.min > value ){
            stati_.min = value;
        }
        if(stati_.max < value){
            stati_.max = value;
        }
    }
    void compute(){
        if(stati_.num > 0){
            stati_.average = stati_.sum / stati_.num;
            // TODO: compute stddev
        }
    }
};

#define HValueType float
//template <int HSize, HValueType HValueHi, HValueType HValueLo, HValueType HMinValue, HValueType HMaxValue, const char  HValueFmt[]>

class RangeHistogram{
    HValueType histogramUnit_ ;
    int        size_;
    HValueType valueHi_;
    HValueType valueLo_;
    std::string name_;
    HValueType minValue_ = FLT_MAX;
    HValueType maxValue_ = FLT_MIN;
    int indexOfMax_ = 0;
    int numValues_ = 0;
    int * histogram_;

public:
    RangeHistogram(int HSize, HValueType HValueHi, HValueType HValueLo, const std::string& name)
    : size_(HSize), valueHi_(HValueHi), valueLo_(HValueLo), name_(name)
    , histogramUnit_((HValueHi-HValueLo)/(HSize-1))
    , histogram_(new int[size_]){
        reset();
    }
    RangeHistogram(const RangeHistogram& other){
        name_ = other.name_;
        histogramUnit_ = other.histogramUnit_;
        size_ = other.size_;
        valueHi_ = other.valueHi_;
        valueLo_ = other.valueLo_;
        minValue_ = other.minValue_;
        maxValue_ = other.maxValue_;
        indexOfMax_ = other.indexOfMax_;
        numValues_ = other.numValues_;
        histogram_  = (new int[size_]);
        memcpy(histogram_, other.histogram_, sizeof(histogram_[0])*size_);
    }
    
    virtual ~RangeHistogram(){
        delete[] histogram_;
    }
    std::string getName()const{
        return this->name_;
    }
    HValueType getValueHi()const{
        return valueHi_;
    }
    HValueType getValueLo()const{
        return valueLo_;
    }
    HValueType getUnit()const{
        return histogramUnit_;
    }
    int getSize()const{
        return size_;
    }
    HValueType getMinValue()const{
        return minValue_;
    }
    HValueType getMaxValue()const{
        return maxValue_;
    }
    int getIndexOfMax()const{
        return indexOfMax_;
    }
    int getMax()const{
        return this->histogram_[indexOfMax_];
    }
    int getSum()const{
        return this->numValues_;
    }
    HValueType getValueAt(int index)const{
        HValueType v = valueLo_ + (index) * histogramUnit_;
        return v;
    }
    int getAt(int index)const{
        return this->histogram_[index];
    }
    void reset(){
        memset(histogram_, 0, sizeof(histogram_[0])*size_);
    }
    int input(HValueType value){
        if(value < minValue_){
            minValue_ = value;
        }
        if(value > maxValue_){
            maxValue_ = value;
        }
        int index = (int)(value/histogramUnit_);
        if(index >= size_){
            index = (index >= size_)-1;
        }
        ++histogram_[index];
        if(histogram_[index] > histogram_[indexOfMax_]){
            indexOfMax_ = index;
        }
        ++numValues_;
        return histogram_[index];
    }
};

class DelayHistogram : public RangeHistogram{
public:
    DelayHistogram():RangeHistogram(65, 2048.0, 0.0, "Delay"){
    }
};

class DropHistogram : public RangeHistogram{
public:
    DropHistogram():RangeHistogram(51, 100.0, 0.0, "Drop"){
    }
};

//const char int64_t_fmt[] = "%lld";
////typedef RangeHistogram<65, int64_t, 2048, 0, LONG_MIN, LONG_MAX, int64_t_fmt> DelayHistogram;
//typedef RangeHistogram<65, 2048.0, 0, -99999999.0, 99999999.0, int64_t_fmt> DelayHistogram;
//typedef RangeHistogram<51, 100.0, 0, -99999999.0, 99999999.0, int64_t_fmt> DropHistogram;



struct USlot{
    UPacket * packets = NULL;
    int maxPackets = 0;
    
    int numSentPackets = 0;
    int numRecvPackets = 0;     // total recv packets
    int numRecvUniPackets = 0;  // recv packets, not including duplicate packets
    int numRecvDupPackets = 0;  // duplicate packets
    uint32_t firstSeq = 0;
    uint64_t duration = 0;
    StatiItemOper delayStati;
    
    USlot(){
        reset();
    }
    void reset(){
        this->numSentPackets = 0;
        this->numRecvPackets = 0;
        this->numRecvUniPackets = 0;
        this->numRecvDupPackets = 0;
        this->firstSeq = 0;
        this->duration = 0;
        this->delayStati.reset();
    }
    
    UPacket * recvPacket(int64_t now_ms, PacketInfo& info){
        int pktIndex = info.seq - this->firstSeq;
        if(pktIndex >= this->maxPackets){
            return NULL;
        }
        UPacket * pkt = &this->packets[pktIndex];
        pkt->recvSeq = info.seq;
        pkt->delay = now_ms - info.timestamp;
        this->delayStati.update(pkt->delay);
        return pkt;
    }
};





struct PingSession{
protected:
    int bitrate_ = 600*1000;
    int packetSize_ = 1024;
    int maxSlots_ = 32;
    int slotCapacity_ = (((bitrate_+7/8)/packetSize_)+1) * 2;
    int64_t minSlotDuration_ = 1000;
    USlot * slots_ = NULL;
    PacketInfo * pktInfo_ = NULL;
    
    std::list<std::pair<int64_t, int64_t>> delayHistory_;
    std::list<std::pair<int64_t, int>> lostHistory_;
    int headSlotIndex_ = 0;
    DelayHistogram delayHistogram_;
    
    int incSlotIndex(int slotIndex){
        ++slotIndex;
        if(slotIndex >= maxSlots_){
            slotIndex = 0;
        }
        return slotIndex;
    }
    
    USlot * slotAt(int slotIndex){
        return &slots_[slotIndex];
    }
public:
    PingSession(){
        int64_t numbits =  minSlotDuration_ * bitrate_ / 1000;
        slotCapacity_ = (int)(((numbits+7/8)/packetSize_)+1) * 2;
        int maxpkts = slotCapacity_ * maxSlots_;
        dbgi("maxpkts=%d", maxpkts);
        slots_ = new USlot[maxSlots_];
        for(int i = 0; i < maxSlots_; ++i){
            slots_[i].maxPackets = slotCapacity_;
            slots_[i].packets = new UPacket[slotCapacity_];
        }
        pktInfo_ = new PacketInfo();
    }
    
    int getBitrate(){
        return bitrate_;
    }
    
    DelayHistogram getDelayHistogram(){
        return delayHistogram_;
    }
    
    void nextPacket4Send(int64_t timestamp, uint8_t buf[]){
//        int64_t timestamp = get_now_ms();
        ++pktInfo_->seq;
        if(pktInfo_->seq == 0){
            pktInfo_->seq = 1;
        }
        pktInfo_->timestamp = timestamp;
        build_serv_echo_packet(pktInfo_, buf);
        USlot * slot = slotAt(pktInfo_->slotIndex);
        if(slot->firstSeq == 0){
            slot->firstSeq = pktInfo_->seq;
        }
        ++slot->numSentPackets;
        int pktIndex = (int)(pktInfo_->seq - slot->firstSeq);
        UPacket * pkt = &slot->packets[pktIndex];
        pkt->sendSeq = pktInfo_->seq;
        pkt->sendTime = timestamp;
        UPacket * pkt0 = &slot->packets[0];
        int64_t duration = timestamp-pkt0->sendTime;
        if(duration >= minSlotDuration_){
            slot->duration = duration;
            dbgi("slot[%d]: sent-pkt=%d, duration=%lld", pktInfo_->slotIndex, slot->numSentPackets, slot->duration);
            pktInfo_->slotIndex  = incSlotIndex(pktInfo_->slotIndex);
            slot = slotAt(pktInfo_->slotIndex);
            slot->reset();
        }
    }
    
    int incomingPacket(int64_t now_ms, uint8_t buf[], int length, PacketInfo& pktinfo){
        int ret = -1;
        do{
            PacketInfo& info = pktinfo;
            ret = parse_echo_packet(buf, length, &info);
            if(ret < 0){
                break;
            }
            if(info.seq == 0){
                // ignore warm up packet
                ret= 0;
                break;
            }
            if(info.slotIndex >= maxSlots_){
                ret = -12;
                break;
            }
            USlot * slot = slotAt(info.slotIndex);
            if(info.seq < slot->firstSeq){
                ret = -15;
                break;
            }
            
//            int64_t now_ms = get_now_ms();
            UPacket * pkt = slot->recvPacket(now_ms, info);
            if(!pkt){
                ret = -20;
                break;
            }
            delayHistory_.push_back(std::pair<int64_t, int64_t>(now_ms, pkt->delay));
            delayHistogram_.input((float)pkt->delay);
            info.delay = pkt->delay;
            dbgi("slot[%d]: got pkt, seq=%u, delay=%lld", info.slotIndex, pkt->recvSeq, pkt->delay);
        }while(0);
        return ret;
    }
    
    void timeCheck(int64_t now_ms){
        USlot * tailSlot = slotAt(pktInfo_->slotIndex);
        if(tailSlot->firstSeq == 0){
            return;
        }
        int64_t elapsed = now_ms - tailSlot->packets[0].sendTime;
        int64_t range = elapsed/minSlotDuration_;
        if(range < 4){
            return;
        }

        USlot * headSlot = slotAt(headSlotIndex_);
    }
    
};

struct XUDPPing{
protected:
    int  socket_;
    struct sockaddr_in remoteAddr_;
    std::thread * sendThread_ = NULL;
    std::thread * recvThread_ = NULL;
    PingSession * session_ = NULL;
    std::atomic_bool isDone_;
    void timeToCheck(){

    }
    
    void sendLoop(){
        int bufSize = 2*1024;
        uint8_t buf[bufSize];
        memset(buf, 0, sizeof(buf));
        int dataLength = 1024;
        int bitrate = session_->getBitrate();
        int64_t sentBytes = 0;
        int64_t startTime = get_now_ms();
        for(int i = 0; i < 160; ++i){
            session_->nextPacket4Send(get_now_ms(), buf);
            ssize_t sock_ret = sendto(socket_, buf, dataLength, 0, (struct sockaddr*)&remoteAddr_, sizeof(struct sockaddr_in));
            if(sock_ret <= 0){
                break;
            }
            sentBytes += dataLength;
            int64_t stdElapsed = 1000*(sentBytes * 8)/bitrate;
            int64_t elapsed = get_now_ms() - startTime;
            if(stdElapsed > elapsed){
                int64_t ms = stdElapsed - elapsed;
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        isDone_ = true;
    }
    
    void recvLoop(){
        int bufSize = 2*1024;
        uint8_t buf[bufSize];
        struct sockaddr_in remoteAddr;
        PacketInfo pktinfo;
        while(1){
            socklen_t addrsz = sizeof(struct sockaddr_in);
            ssize_t sock_ret = recvfrom(socket_, buf, bufSize, 0, (struct sockaddr*)&remoteAddr, &addrsz);
            if(sock_ret <= 0){
                break;
            }
            int numBytes = (int)sock_ret;
//            dbgi("got %d bytes, %d", numBytes, buf[0]);
            session_->incomingPacket(get_now_ms(), buf, numBytes, pktinfo);
            dbgi("slot[%d]: got pkt, seq=%u, delay=%lld", pktinfo.slotIndex, pktinfo.seq, pktinfo.delay);
        }

    }
    
public:
    XUDPPing(int socket) : socket_(socket), isDone_(false){
    }
    virtual ~XUDPPing(){
        
    }
    
    int start(const char * remoteIP, int remotePort){

        session_ = new PingSession();
        socket_addrin_init(remoteIP, remotePort, &remoteAddr_);
        sendThread_ = new std::thread(&XUDPPing::sendLoop, this);
        recvThread_ = new std::thread(&XUDPPing::recvLoop, this);
        return 0;
    }
    
    bool done(){
        return isDone_;
    }

    DelayHistogram getDelayHistogram(){
        return session_->getDelayHistogram();
    }
};


XUDPPing * XUDPPingCreate(const char * localIP, int localPort){
    int proto = SOCK_DGRAM;
    int reuseable = 1;
    int sock = socket_bind(localIP, localPort, proto, reuseable);
    if(sock < 0){
        return NULL;
    }
    XUDPPing * obj = new XUDPPing(sock);
    return obj;
}

void XUDPPingDelete(XUDPPing * obj){
    if(obj){
        delete obj;
        obj = NULL;
    }
}

int XUDPPingStart(XUDPPing * obj, const char * remoteIP, int remotePort){
    return obj->start(remoteIP, remotePort);
}

template<typename T>
T XLimitValue(T val, T MIN, T MAX){
    if(val < MIN) return MIN;
    if(val > MAX) return MAX;
    return val;
}


class CharImage{
    int width_;
    int height_;
    char initPixel_;
    int widthSize_;
    int imageSize_;
    char ** image_;
    char& at(int w, int h){
//        int index = h*widthSize_+w;
//        return image_[index];
        return *(image_[h]+w);
    }
    void allocLine(int h){
        image_[h] = new char[widthSize_];
        memset(image_[h], initPixel_, widthSize_);
        image_[h][widthSize_-1] = '\0';
    }
public:
    CharImage(int width, int height, char initPixel=' ')
    :width_(width),height_(height),initPixel_(initPixel)
    ,widthSize_(width+1)
    ,imageSize_(width_*height_)
    ,image_(NULL){
        image_ = new char*[height_];
        for(int h = 0; h < height_; ++h){
            this->allocLine(h);
        }
    }
    CharImage(CharImage& other){
        this->width_ = other.width_;
        this->height_ = other.height_;
        this->initPixel_ = other.initPixel_;
        this->widthSize_ = other.widthSize_;
        this->imageSize_ = other.imageSize_;
        image_ = new char*[height_];
        for(int h = 0; h < height_; ++h){
            this->allocLine(h);
            memcpy(this->image_[h], other.image_, widthSize_);
        }
    }
    virtual ~CharImage(){
        if(image_){
            for(int h = 0; h < height_; ++h){
                delete[] image_[h];
            }
            delete[] image_;
        }
    }
    int getWidth()const{
        return width_;
    }
    int getHeight()const{
        return height_;
    }
    int getLines()const{
        return height_;
    }
    void complete(){
        for(int h = 0; h < height_; ++h){
            this->setChar(h, width_, '\0');
        }
    }
    const char *getLine(int h) const{
//        int first = h*widthSize_+0;
//        return &image_[first];
        return image_[h];
    }
    int setChar(int lineIndex, int offset, char c){
        char * line = image_[lineIndex];
        line[offset] = c;
        return 1;
    }
    int setStr(int lineIndex, int offset, const std::string& str){
        // TODO: handle wrap line
//        int first = line*widthSize_+offset;
//        memcpy(&image_[first], str.c_str(), str.size());
        char * line = image_[lineIndex];
        memcpy(&line[offset], str.c_str(), str.size());
        return (int)str.size();
    }
    void setLines(int numLines){
        if(numLines == height_){
            return;
        }
        char ** newimage = new char*[numLines];
        int copyLines = std::min(numLines, height_);
        for(int h = 0; h < copyLines; h++){
            newimage[h] = image_[h];
        }
        for(int h = copyLines; h < numLines; h++){
            this->allocLine(h);
        }
        
        for(int h = copyLines; h < height_; h++){
            delete[] image_[h];
        }
        image_ = newimage;
        height_ = numLines;
    }
};


CharImage * makeHistogramCharImage(int width, const DelayHistogram& histogram, bool fullMode = false){
    int height = histogram.getSize();;
    const char initPixel = ' ';
    CharImage * image  = new CharImage(width, height, initPixel);
    
    do{
        int sum = histogram.getSum() > 0 ? histogram.getSum() : 1;
        int miny = 0;
        int maxy = histogram.getMax();
        int numrange = maxy-miny;
        char buf[128];
        
        float percent = 100.0;
        

        std::string columnNames[3];
        int columnWidths[] = {0, 0, 0};
        columnWidths[0] = sprintf(buf, "%s", "Percent");   columnNames[0] = buf;
        columnWidths[1] = sprintf(buf, "%s", histogram.getName().c_str());  columnNames[1] = buf;
        columnWidths[2] = sprintf(buf, "%s", "Num");  columnNames[2] = buf;
        
        int colWidth = 0;
        colWidth = sprintf(buf, "%.1f", histogram.getValueHi()) + 1;
        columnWidths[1] = colWidth > columnWidths[1] ? colWidth : columnWidths[1];
        
        colWidth = sprintf(buf, "%d", histogram.getMax()) + 1;
        columnWidths[2] = colWidth > columnWidths[2] ? colWidth : columnWidths[2];
        
        
        char format[64];
        int formatlen = 0;
        formatlen += sprintf(format+formatlen, "|%%%d.1f%%%%|", columnWidths[0]-1);
        formatlen += sprintf(format+formatlen, "%%%d.1f|", columnWidths[1]);
        formatlen += sprintf(format+formatlen, "%%%dd|", columnWidths[2]);
        int prefixWidth = sprintf(buf, format, percent, histogram.getValueHi(), histogram.getMax());
        int graphWidth = width - prefixWidth;
        
        int line = 0;
        int offset = 0;
        offset += image->setStr(line, offset, "|");
        for(int i = 0; i < 3; ++i){
            std::string&name = columnNames[i];
            for(int n = 0; n < (columnWidths[i] - name.size()); ++n){
                offset += image->setChar(line, offset, initPixel);
            }
            offset += image->setStr(line, offset, name);
            offset += image->setStr(line, offset, "|");
        }
        ++line;
        
        int emptyLines = 0;
        for(int i = 0; i < histogram.getSize(); ++i){
            int num = histogram.getAt(i);
            float value = histogram.getValueAt(i);
            if(!fullMode){
                if(num == 0){
                    ++emptyLines;
                    if(emptyLines > 1){
                        continue;
                    }
                }else{
                    emptyLines = 0;
                }
            }
            
            
            percent = 100.0 * num/sum;
            sprintf(buf, format, percent, value, num);
            image->setStr(line, 0, buf);
            
            int x = (int) (graphWidth * num/numrange);
            x = XLimitValue<int>(x, 0, graphWidth);
            
            char c = initPixel;
            if(x > 0){
                c = '=';
            }else if(num > 0){
                c = '.';
                x = 1;
            }
            
            for(int n0 = 0; n0 < x; ++n0){
                image->setChar(line, prefixWidth+n0,  c);
            }
            ++line;
        }
        image->setLines(line);
        image->complete();
        

    }while(0);
    return image;
}

static
void consoleDrawHistogram(int width, const DelayHistogram& histogram, bool fullMode = false){
    CharImage * image = makeHistogramCharImage(width, histogram);
    for(int h = 0; h < image->getLines(); ++h){
        const char * str = image->getLine(h);
        dbgi("%s", str);
    }
    delete image;
}

int xudp_ping_main(int argc, const char * argv[]) {
    XUDPPing * obj = XUDPPingCreate("", 0);
    XUDPPingStart(obj, "121.196.226.37", 40000);
    while(!obj->done()){
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    DelayHistogram delayHisg = obj->getDelayHistogram();
    consoleDrawHistogram(80, delayHisg, false);
    
//    DelayHistogram delayHisg;
////    for(int i = 0; i < 4; ++i){
////        delayHisg.input(30);
////    }
//    for(int i = 0; i < 100; ++i){
//        delayHisg.input(50);
//    }
//    for(int i = 0; i < 2; ++i){
//        delayHisg.input(80);
//    }
//    for(int i = 0; i < 88; ++i){
//        delayHisg.input(1230);
//    }
//    for(int i = 0; i < 4; ++i){
//        delayHisg.input(1230+30);
//    }
//    consoleDrawHistogram(80, delayHisg, false);
    
    return 0;
}

