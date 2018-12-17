
#ifndef NRTCPPacket_hpp
#define NRTCPPacket_hpp

#include <stdio.h>
#include "NUtil.hpp"
#include "NLogger.hpp"

namespace  NRtcp{
    
    DECLARE_CLASS_ENUM(PacketType,
                       kFullIntraRequest        = 192,
                       kNACK                    = 193,
                       kExtendedJitterReport    = 195,
                       kSenderReport            = 200,
                       kReceiverReport          = 201,
                       kSDES                    = 202,
                       kBye                     = 203,
                       kApp                     = 204,
                       kRTPFeedback             = 205,
                       kPayloadFeedback         = 206
                       );
    
    /*
     RTCP common header
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |V=2|P|    RC   |   PT          |             length            |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */
    class Header{
    public:
        uint8_t  count       = 0;    /* varies by packet type :5*/
        bool     padding     = 0;    /* padding flag */
        uint8_t  version     = 2;    /* protocol version :2 */
        uint8_t  packetType  = 0;    /* RTCP packet type */
        uint16_t length      = 0;    /* pkt len in words, w/o this word */
        
        size_t Parse(const uint8_t* data, size_t size){
            if (size<4) return 0;
            
            version     = data[0] >> 6;
            padding     = (data[0] >> 4 ) & 0x01;
            count       = data[0] & 0x1F;
            packetType  = data[1];
            length      = (NUtil::get2(data,2)+1)*4;
            
            return 4; //Return size
        }
        
        size_t Serialize(uint8_t* data, size_t size) const{
            if (size<4) return 0;
            
            data[0] = (padding ? 0xA0 : 0x80) | (count & 0x1F);
            data[1] = packetType;
            NUtil::set2(data,2, (length/4)-1);
            
            return 4; //Return size
        }
    };
    
    class Report : public NObjDumper::Dumpable{
        
    public:
        uint32_t GetSSRC()            const { return NUtil::get4(buffer,0);  }
        uint8_t  GetFactionLost()        const { return NUtil::get1(buffer,4);  }
        uint32_t GetLostCount()        const { return NUtil::get3(buffer,5) & 0x7FFFFF;  }
        uint32_t GetLastSeqNum()        const { return NUtil::get4(buffer,8);  }
        uint32_t GetJitter()        const { return NUtil::get4(buffer,12); }
        uint32_t GetLastSR()        const { return NUtil::get4(buffer,16); }
        uint32_t GetDelaySinceLastSR()    const { return NUtil::get4(buffer,20); }
        uint32_t GetSize()            const { return 24;        }
        
        void SetSSRC(uint32_t ssrc)        { NUtil::set4(buffer,0,ssrc);        }
        void SetFractionLost(uint8_t fraction)    { NUtil::set1(buffer,4,fraction);    }
        void SetLostCount(uint32_t count)        { NUtil::set3(buffer,5,count & 0x7FFFFF);        }
        void SetLastSeqNum(uint32_t seq)        { NUtil::set4(buffer,8,seq);        }
        void SetLastJitter(uint32_t jitter)    { NUtil::set4(buffer,12,jitter);    }
        void SetLastSR(uint32_t last)        { NUtil::set4(buffer,16,last);        }
        void SetDelaySinceLastSR(uint32_t delay)    { NUtil::set4(buffer,20,delay);    }
        
        void SetDelaySinceLastSRMilis(uint32_t milis)
        {
            //calculate the delay, expressed in units of 1/65536 seconds
            uint32_t dlsr = (milis/1000) << 16 | (uint32_t)((milis%1000)*65.535);
            //Set it
            SetDelaySinceLastSR(dlsr);
        }
        
        uint32_t GetDelaySinceLastSRMilis() const
        {
            //Get the delay, expressed in units of 1/65536 seconds
            uint32_t dslr = GetDelaySinceLastSR();
            //Return in milis
            return (dslr>>16)*1000 + ((double)(dslr & 0xFFFF))/65.635;
        }
        
        
        size_t Serialize(uint8_t* data, size_t size) const
        {
            //Check size
            if (size<24)
                return 0;
            //Copy
            memcpy(data,buffer,24);
            //Return size
            return 24;
        }
        
        size_t Parse(const uint8_t* data, size_t size){
            if (size<24) return 0; // Check size
            
            memcpy(buffer,data,24); // Copy
            
            return 24; // Return size
        }
        
        virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
            auto& report = *this;
            return dumper.objB()
            .kv("ssrc", report.GetSSRC())
            .kv("lostf", (unsigned)report.GetFactionLost())
            .kv("lostc", (unsigned)report.GetLostCount())
            .kv("eseq", report.GetLastSeqNum())
            .kv("jit", report.GetJitter())
            .kv("LSR", report.GetLastSR())
            .kv("DLSR", report.GetDelaySinceLastSRMilis());
        }
        
    private:
        uint8_t buffer[24] = {0};
    }; // Report
    
    class Reports : public std::vector<Report>, public NObjDumper::Dumpable{
    public:
        size_t parse(const Header &header, const uint8_t* data, size_t size){
            size_t len = 0;
            // parse each block
            for(int i=0; i< header.count; i++){
                Report report ;
                auto sz = report.Parse(data+len,size-len);
                if (!sz){
                    return 0;
                }
                
                emplace_back(report);
                
                len += sz;
            }
            return len;
        }
        virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
            int n = 0;
            for(auto& report : *this){
                ++n;
                char str[64];
                sprintf(str, "report[%d/%zu]", n, this->size());
                dumper.dump(str, report);
            }
            return dumper;
        }
    };
    
    
    
    class Packet : public NObjDumper::Dumpable{
    protected:
        Packet(PacketType type){
            this->type = type;
        }
        
    public:
        // Must have virtual destructor to ensure child class's destructor is called
        virtual ~Packet(){};
        virtual PacketType GetType() const {return type; }
        virtual size_t GetSize() const = 0;
        virtual size_t Parse(const Header &header, const uint8_t* data, size_t size) = 0;
        virtual size_t Serialize(uint8_t* data, size_t size) const = 0;
        virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
            return dumper.objB().kv("typ", getNameForPacketType(type))
            .objE();
            return dumper;
        };
        
        PacketType type;
        
    }; // Packet

    class SenderReport : public Packet{
    public:
        SenderReport() : Packet(kSenderReport){}
        
        virtual ~SenderReport() = default;
        
        virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
            dumper.objB().kv("typ", "SR")
            .kv("ssrc", ssrc)
            .kv("ntpH", ntpSec)
            .kv("ntpL", ntpFrac)
            .kv("ts", rtpTimestamp)
            .kv("pkts", packetsSent)
            .kv("bytes", octectsSent);
            if(reports.size() > 0){
                dumper.endl();
                dumper.dump("reports", reports);
            }
            return dumper.objE();
        }
        
        virtual size_t GetSize() const override{
            return 4+24+24*reports.size();
        }
        
        virtual size_t Parse (const Header &header, const uint8_t* data, size_t size) override{
            if(size < (4+24+24*header.count)){
                return 0;
            }
               
            size_t len = 4;
            ssrc            = NUtil::get4(data, len+0);
            ntpSec          = NUtil::get4(data, len+4);
            ntpFrac         = NUtil::get4(data, len+8);
            rtpTimestamp    = NUtil::get4(data, len+12);
            packetsSent     = NUtil::get4(data, len+16);
            octectsSent     = NUtil::get4(data, len+20);
            len += 24;
            
            // parse each report
            reports.clear();
            if(header.count > 0){
                auto sz = reports.parse(header, data+len,size-len);
                if (!sz){
                    return 0;
                }
                len += sz;
            }

            return len; // Return total size
        }
        
        virtual size_t Serialize (uint8_t* data, size_t size) const override{

            auto packetSize = GetSize();

            if (size < packetSize){
                return 0;
            }
            
            // RTCP common header
            Header header;
            header.count      = reports.size();
            header.packetType = GetType();
            header.padding      = 0;
            header.length      = packetSize;
            auto len = header.Serialize(data, size);
            
            //Set values
            NUtil::set4(data, len, ssrc);
            NUtil::set4(data, len+4, ntpSec);
            NUtil::set4(data, len+8, ntpFrac);
            NUtil::set4(data, len+12, rtpTimestamp);
            NUtil::set4(data, len+16,  packetsSent);
            NUtil::set4(data, len+20,octectsSent);
            len += 24;
            
            // for each report
            for(int i=0;i<header.count;i++){
                len += reports[i].Serialize(data+len, size-len);
            }
            
            return len; // return size
        }
        
        uint64_t GetNTPTimestamp()  const       { return ((uint64_t)ntpSec)<<32 | ntpFrac;  }
        
        void  SetTimestamp(uint64_t time);
        
        uint64_t GetTimestamp() const;
        
    public:
        uint32_t ssrc           = 0; /* sender generating this report */
        uint32_t ntpSec         = 0; /* NTP timestamp */
        uint32_t ntpFrac        = 0;
        uint32_t rtpTimestamp   = 0; /* RTP timestamp */
        uint32_t packetsSent    = 0; /* packets sent */
        uint32_t octectsSent    = 0; /* octets sent */
        
        Reports    reports;
    };
    
    class ReceiverReport : public Packet
    {
    public:
        ReceiverReport() : Packet(kReceiverReport){}
        
        ReceiverReport(uint32_t ssrc) : ReceiverReport(){
            this->ssrc = ssrc;
        }
        
        virtual ~ReceiverReport() = default;
        
        virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
            dumper.objB().kv("typ", "RR")
            .kv("ssrc", ssrc);
            if(reports.size() > 0){
                dumper.dump("reports", reports);
            }
            return dumper.objE();
        }
        
        virtual size_t GetSize() const override{
            return 4+4+24*reports.size();
        }
        
        virtual size_t Parse(const Header &header, const uint8_t* data, size_t size) override{
            if(size < (4+4+24*header.count)){
                return 0;
            }
            
            size_t len = 4;
            this->ssrc = NUtil::get4(data,len);
            len += 4;
            
            // parse each block
            if(header.count > 0){
                auto sz = reports.parse(header, data+len,size-len);
                if (!sz){
                    return 0;
                }
                len += sz;
            }
            
            return len; // Return total size
        }
        
        virtual size_t Serialize(uint8_t* data, size_t size) const override{
            
            auto packetSize = GetSize(); //Get packet size
            
            if (size < packetSize) return 0; //Check size
            
            // RTCP common header
            Header header;
            header.count      = reports.size();
            header.packetType = GetType();
            header.padding      = 0;
            header.length      = packetSize;
            auto len = header.Serialize(data, size);
            
            NUtil::set4(data,len,ssrc);
            len += 4;
            
            // for each block
            for(int i=0;i<header.count;i++){
                len += reports[i].Serialize(data+len,size-len);
            }
            
            return len;
        }
        
    public:
        uint32_t ssrc    = 0;     /* receiver generating this report */
        Reports    reports;
    };
    
    class  SDES : public Packet{
    
        class Item : public NObjDumper::Dumpable{
        public:
            DECLARE_CLASS_ENUM(Type,
                               kCName = 1,
                               kName = 2,
                               kEmail = 3,
                               kPhone = 4,
                               kLocation = 5,
                               kTool = 6,
                               kNote = 7,
                               kPrivate = 8
                               );
        public:
            Item() = delete;
            Item(Item&&) = delete;
            
            Item(const Item& other)
            : Item(other.type, other.data, other.size){
                
            }
            
            Item(Type type, const uint8_t* data, size_t size){
                this->type = type;
                this->data = (uint8_t*)malloc(size);
                this->size = size;
                memcpy(this->data,data,size);
            }
            
            Item(Type type,const char* str){
                this->type = type;
                size = strlen(str);
                data = (uint8_t*)malloc(size);
                memcpy(data,(uint8_t*)str,size);
            }
            
            ~Item(){
                free(data);
            }
            Type  GetType() const { return type; }
            uint8_t* GetData() const { return data; }
            uint8_t  GetSize() const { return size; }
            
            virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
                return dumper.objB()
                .kv("typ", getNameForType(type))
                .kv("text", std::string((char*)data, size))
                .objE();
            }
            
        private:
            Type type;
            uint8_t* data;
            uint8_t size;
        };
        
        typedef std::vector<Item> Items;
        
        class Description : public NObjDumper::Dumpable{
        public:
            Description(){}
            Description(uint32_t ssrc){
                this->ssrc = ssrc;
            }
            
            ~Description() = default;
            
            virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
                dumper.objB().kv("ssrc", ssrc);
                if(items.size() > 0){
                    dumper.endl();
                    dumper.dump("items", items);
                }
                return dumper.objE();
            }
            
            size_t GetSize() const{
                size_t len = 4;
                for (auto& o : items){
                    // add data size and header
                    len += o.GetSize()+2;
                }
                
                len+=1; // add end
                
                return NUtil::Pad32<size_t>(len);
            }
            
            size_t Parse(const uint8_t* data, size_t size){
                if (size < 4){
                    return 0;
                }

                ssrc = NUtil::get4(data, 0);
                
                size_t len = 4; //Skip ssrc
                //While we have
                while (size>(len+2) && data[len]){
                    //Get tvalues
                    Item::Type type = (Item::Type)data[len];
                    uint8_t length = data[len+1];
                    //Check size
                    if ((len+2+length)>size){
                        return 0; //Error
                    }
                    items.emplace_back(type, data+len+2, length);
                    len += length+2;
                }
                //Skip last
                len++;
                //Return consumed len
                return NUtil::Pad32<size_t>(len);
            }
            
            size_t Serialize(uint8_t* data, size_t size) const{
                //Get packet size
                size_t packetSize = GetSize();
                //Check size
                if (size<packetSize)
                    //error
                    return 0;
                //Set ssrc
                NUtil::set4(data,0,ssrc);
                //Skip headder
                size_t len = 4;
                //For each item
                for (auto& item : items)
                {
                    //Serilize it
                    data[len++] = item.GetType();
                    data[len++] = item.GetSize();
                    //Copy data
                    memcpy(data+len,item.GetData(),item.GetSize());
                    //Move
                    len += item.GetSize();
                }
                //Add null item
                data[len++] = 0;
                //Append nulls till pading
                memset(data+len,0, NUtil::Pad32(len)-len);
                //Return padded size
                return NUtil::Pad32(len);
            }

        public:
            
            uint32_t ssrc;
            Items items;
        };
    public:
        
    public:
        SDES() : Packet(kSDES){
        }
        
        virtual ~SDES() = default;
        
        virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
            dumper.objB().kv("typ", "SDES");
            
            if(descriptions.size() > 0){
                if(descriptions.size() == 1
                   && descriptions[0].items.size() == 1){
                    dumper.kv("ssrc", descriptions[0].ssrc);
                    dumper.dump("item", descriptions[0].items[0]);
                }else{
                    dumper.endl();
                    dumper.dump("descriptions", descriptions);
                }
            }
            return dumper.objE();
        }
        
        virtual size_t GetSize() const override{
            size_t len = 4; // common header
            for (auto& desc : descriptions){
                len += desc.GetSize();
            }
            return len;
        }
        
        virtual size_t Parse(const Header &header, const uint8_t* data, size_t size) override{
            descriptions.clear();
            
            //Get packet size
            size_t packetSize = header.length;
            
            //Check size
            if (size<packetSize)
                //Exit
                return 0;
            
            size_t len = 4;
            
            //Parse fields
            size_t i = 0;
            //While we have
            while (len < size && i<header.count){
                descriptions.emplace_back();
                Description& desc = descriptions.back();
                //Parse field
                size_t parsed = desc.Parse(data+len,size-len);
                //If not parsed
                if (!parsed)
                    //Error
                    return 0;
                
                //Skip
                len += parsed;
            }
            //Return consumed len
            return len;
        }
        
        virtual size_t Serialize(uint8_t* data, size_t size) const override{
            
            //Get packet size
            size_t packetSize = GetSize();
            //Check size
            if (size<packetSize)
                //error
                return 0;
            
            // RTCP common header
            Header header;
            //Set values
            header.count      = descriptions.size();
            header.packetType = GetType();
            header.padding      = 0;
            header.length      = packetSize;
            //Serialize
            size_t len = header.Serialize(data,size);
            
            //For each field
            for (auto& desc : descriptions){
                len += desc.Serialize(data+len,size-len);
            }
            
            return len;
        }
        
        typedef std::vector<Description> Descriptions;
        Descriptions descriptions;
    };

    static bool IsRTCP(const uint8_t *data, size_t size){
        
        if (size<4) return false; // Check size
        
        if ((data[0]>>6)!=2) return false; // Check version
        
        if (data[1]<200 ||  data[1] > 206) return false; //Check type
        
        return true;
    }
    
    
    class Parser{
        SenderReport        sr_;
        ReceiverReport      rr_;
        SDES                sdes_;
    public:
        DECLARE_CLASS_ENUM (State,
                            kNoError = 0,
                            kNotRTCP = -1,
                            kWrongHeader,
                            kWrongSize,
                            kWrongData,
                            );
        
        static State DumpPackets(const uint8_t *data, size_t size, NObjDumper& dumper){
            dumper.indent();
            Parser parser;
            State state = parser.Parse(data, size, [&dumper](Packet &packet){
                dumper.dump("RTCP", packet).endl();
            });
            dumper.uindent();
            return state;
        }
        
    public:
        typedef std::function<void(Packet &packet)> OnRtcpFuncT;
        State Parse(const uint8_t *data, size_t size, const OnRtcpFuncT &func){
            if (!IsRTCP(data,size)){
                return kNotRTCP;
            }
            
            Header header;
            
            const uint8_t *buffer = data;
            size_t bufferLen = size;
            while (bufferLen > 0){
                
                auto sz = header.Parse(buffer,bufferLen);
                if (sz == 0){
                    return kWrongHeader;
                }
                
                if (header.length > bufferLen){
                    return kWrongSize;
                }
                
                Packet * pkt = nullptr;
                switch (header.packetType){
                    case kSenderReport:
                        pkt = &sr_;
                        break;
                    case kReceiverReport:
                        pkt = &rr_;
                        break;
                    case kSDES:
                        pkt = &sdes_;
                        break;
                    case kBye:
                        
                        break;
                    case kApp:
                        
                        break;
                    case kRTPFeedback:
                        
                        break;
                    case kPayloadFeedback:
                        
                        break;
                    case kFullIntraRequest:
                        
                        break;
                    case kNACK:
                        
                        break;
                    case kExtendedJitterReport:
                        
                        break;
                    default:
                        // skip unknown
                        pkt = nullptr;
                        
                }
                
                if (pkt){
                    sz = pkt->Parse(header, buffer, header.length);
                    if(sz == 0){
                        return kWrongData;
                    }
                    func(*pkt);
                }
                
                bufferLen -= header.length;
                buffer    += header.length;
            }
            
            return kNoError;
        }
    };
    
    
}; // NRtcp


#endif /* NRTCPPacket_hpp */
