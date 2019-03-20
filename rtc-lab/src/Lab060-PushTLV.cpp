

#include "Lab060-PushTLV.hpp"
#include "FFMpegFramework.hpp"
#include "NUtil.hpp"
#include "NLogger.hpp"
#include "XProtoRapidJson.hpp"
#include <sstream>

class XProtoFFBase : public XProtoBase{
public:
    XProtoFFBase(const std::string& name)
    : XProtoBase(name)
    , tracks_("tracks", false, MAKE_TYPE_FLAG(kArrayType))
    , codecId_("codecId", false, MAKE_TYPE_FLAG(kNumberType))
    , timebase_("timebase", false, MAKE_TYPE_FLAG(kObjectType))
    , index_("index", false, MAKE_TYPE_FLAG(kNumberType))
    , num_("num", false, MAKE_TYPE_FLAG(kNumberType))
    , den_("den", false, MAKE_TYPE_FLAG(kNumberType))
    , format_("format", false, MAKE_TYPE_FLAG(kNumberType))
    , chLayout_("chLayout", false, MAKE_TYPE_FLAG(kNumberType))
    , frameSize_("frameSize", false, MAKE_TYPE_FLAG(kNumberType))
    , sampleRate_("sampleRate", false, MAKE_TYPE_FLAG(kNumberType))
    , width_("width", false, MAKE_TYPE_FLAG(kNumberType))
    , height_("height", false, MAKE_TYPE_FLAG(kNumberType))
    , frameRate_("frameRate", false, MAKE_TYPE_FLAG(kNumberType))
    {
        fields_.push_back(&tracks_);
        fields_.push_back(&codecId_);
        fields_.push_back(&timebase_);
        fields_.push_back(&index_);
        fields_.push_back(&num_);
        fields_.push_back(&den_);
        fields_.push_back(&format_);
        fields_.push_back(&chLayout_);
        fields_.push_back(&frameSize_);
        fields_.push_back(&sampleRate_);
        fields_.push_back(&width_);
        fields_.push_back(&height_);
        fields_.push_back(&frameRate_);
    }
    XProtoField tracks_;
    XProtoField codecId_;
    XProtoField timebase_;
    XProtoField index_;
    XProtoField num_;
    XProtoField den_;
    XProtoField format_;
    XProtoField chLayout_;
    XProtoField frameSize_;
    XProtoField sampleRate_;
    XProtoField width_;
    XProtoField height_;
    XProtoField frameRate_;
};

class XProtoFFHeader : public XProtoFFBase{
public:
    XProtoFFHeader():XProtoFFBase("header"){
        tracks_.mandatory_ = true;
    }
};

class XProtoFFMediaCfg : public XProtoFFBase{
public:
    XProtoFFMediaCfg():XProtoFFBase("mcfg"){
        codecId_.mandatory_ = true;
        timebase_.mandatory_ = true;
        index_.mandatory_ = true;
    }
};

class XProtoFFTimeBase : public XProtoFFBase{
public:
    XProtoFFTimeBase():XProtoFFBase("timebase"){
        num_.mandatory_ = true;
        den_.mandatory_ = true;
    }
};

class XProtoFFAudioCfg : public XProtoFFBase{
public:
    XProtoFFAudioCfg():XProtoFFBase("audiocfg"){
        format_.mandatory_ = true;
        chLayout_.mandatory_ = true;
        frameSize_.mandatory_ = true;
        sampleRate_.mandatory_ = true;
    }
};

class XProtoFFVideoCfg : public XProtoFFBase{
public:
    XProtoFFVideoCfg():XProtoFFBase("audiocfg"){
        format_.mandatory_ = true;
        width_.mandatory_ = true;
        height_.mandatory_ = true;
        frameRate_.mandatory_ = true;
    }
};



#define TLV_TYPE_TIME_FF_HEADER     4000
#define TLV_TYPE_TIME_FF_PACKET     4001

class FFTLVFormat{
public:
    static const size_t TLV_PREFIX_LENGTH = 8; // type + length
    static const size_t TLV_MIN_LENGTH = TLV_PREFIX_LENGTH + 8; // type + length + ts
    
    typedef std::function<int(const uint8_t * data_ptr, size_t length)> DataFunc;
    
    struct Header{
        int64_t ts = -1;
        std::vector<FFMediaConfig> configs;
    };
    
    typedef std::function<int(int64_t ts, const Header& header)> OnHeaderFunc;
    typedef std::function<int(int64_t ts, const AVPacket * pkt)> OnPacketFunc;

    struct OnItemFuncs{
        OnHeaderFunc onHeader = nullptr;
        OnPacketFunc onPacket = nullptr;
    };
    
public:
    
    static int make(int typ, int64_t ts, const char * msg_data, size_t msg_len, const DataFunc& func){
        return make(typ, ts, (const uint8_t *)msg_data, msg_len, func);
    }
    
    static int make(int typ, int64_t ts, const uint8_t * data_ptr, size_t data_len, const DataFunc& func){
        int length = (int)(8 + data_len);
        uint8_t buf[TLV_MIN_LENGTH];
        NUtil::le_set4(buf, 0, typ);
        NUtil::le_set4(buf, 4, length);
        NUtil::le_set8(buf, 8, ts);
        
        int ret = func(buf, sizeof(buf));
        if(ret){
            return ret;
        }
        
        ret = func(data_ptr, data_len);
        if(ret){
            return ret;
        }
        
        return 0;
    }
    
    static int make(const std::vector<FFMediaConfig>& configs, const DataFunc& func){
        int ret = 0;
        StringBuffer sb;
        Writer<StringBuffer> writer(sb);
        writer.StartObject();
        
        writer.Key("tracks");
        writer.StartArray();
        for(auto& cfg : configs){
            writer.StartObject();
            
            writer.Key("codecId"); writer.Int(cfg.getCodecId());
            writer.Key("index"); writer.Int(cfg.getIndex());
            writer.Key("timebase");
            {
                writer.StartObject();
                writer.Key("num"); writer.Int64(cfg.getTimeBase().num);
                writer.Key("den"); writer.Int64(cfg.getTimeBase().den);
                writer.EndObject();
            }
            
            
            if(cfg.getCodecType() == AVMEDIA_TYPE_AUDIO){
                auto& m = cfg.audio;
                writer.Key("format");       writer.Int(m.getFormat());
                writer.Key("chLayout");     writer.Uint64(m.getChLayout());
                writer.Key("frameSize");    writer.Int64(m.getFrameSize());
                writer.Key("sampleRate");   writer.Int(m.getSampleRate());
            }else if(cfg.getCodecType() == AVMEDIA_TYPE_VIDEO){
                auto& m = cfg.video;
                writer.Key("format");       writer.Int(m.getFormat());
                writer.Key("width");        writer.Int(m.getWidth());
                writer.Key("height");       writer.Int(m.getHeight());
                writer.Key("frameRate");    writer.Int(m.getFrameRate());
            }
            
            writer.EndObject();
        }
        writer.EndArray(); // end of tracks
        
        writer.EndObject();
        
        ret = make(TLV_TYPE_TIME_FF_HEADER, NUtil::get_now_ms(), sb.GetString(), sb.GetSize(), func);
        return ret;
    }
    
    static int parse(const uint8_t * data_ptr,
                     size_t data_len,
                     std::ostringstream& oss_err,
                     const OnItemFuncs& funcs){
        int typ;
        size_t length;
        
        int ret = 0;
        size_t offset = parsePrefix(data_ptr, data_len, typ, length);
        if(offset == 0){
            ret = -1;
            return ret;
        }
        
        if(data_len < (offset + length)){
            return -3;
        }
        
        int64_t ts = NUtil::le_get8(data_ptr, offset);   offset += 8;
        
        switch(typ){
            case TLV_TYPE_TIME_FF_HEADER:{
                FFTLVFormat::Header header;
                ret = parse(data_ptr+offset, data_len-offset, header, oss_err);
                if(ret == 0){
                    ret = funcs.onHeader(ts, header);
                }
                break;
            }
                
            case TLV_TYPE_TIME_FF_PACKET:{
                AVPacket packet;
                //av_init_packet(&packet);
                ret = parse(data_ptr+offset,
                            data_len-offset,
                            &packet,
                            oss_err);
                if(ret == 0){
                    ret = funcs.onPacket(ts, &packet);
                }
                av_packet_unref(&packet);
                break;
            }
                
            default:{
                ret = -1;
                break;
            }
        } // switch
        
        return ret;
    }
    
    static size_t parsePrefix(const uint8_t * data_ptr,
                           size_t data_len,
                           int& typ,
                           size_t& length){
        if(data_len < TLV_PREFIX_LENGTH){
            return 0;
        }
        size_t offset = 0;
        typ = NUtil::le_get4(data_ptr, offset);         offset += 4;
        length = NUtil::le_get4(data_ptr, offset);   offset += 4;
        return offset;
    }
    
    static int parse(const uint8_t * data_ptr,
                     size_t data_len,
                     Header& header,
                     std::ostringstream& oss_err){
        int ret = 0;
        size_t offset = 0;
        std::string json_str((const char *)(data_ptr+offset), data_len-offset);
        offset += (data_len-offset);
        
        Document doc;
        do{
            doc.Parse(json_str.c_str());
            if(!doc.IsObject()){
                ret = -1;
                break;
            }
            
            XProtoFFHeader header_proto;
            ret = header_proto.parse(doc, oss_err);
            if(ret){
                break;
            }
            
            Value::ConstArray cfgarr = header_proto.tracks_.iter_->value.GetArray();
            for(int num = 0; num < cfgarr.Size(); ++num){
                auto& val = cfgarr[num];
                header.configs.emplace_back();
                auto& mcfg = header.configs.back();
                
                XProtoFFMediaCfg mcfg_proto;
                ret = mcfg_proto.parse(val, oss_err);
                if(ret){
                    break;
                }
                
                XProtoFFTimeBase timebase_proto;
                ret = timebase_proto.parse(mcfg_proto.timebase_.iter_->value, oss_err);
                if(ret){
                    break;
                }
                
                mcfg.setCodecId((AVCodecID)mcfg_proto.codecId_.getInt());
                mcfg.setTimeBase({timebase_proto.num_.getInt(), timebase_proto.den_.getInt()});
                mcfg.setIndex(mcfg_proto.index_.getInt());
                
                if(mcfg.getCodecType() == AVMEDIA_TYPE_AUDIO){
                    XProtoFFAudioCfg audiocfg_proto;
                    ret = audiocfg_proto.parse(val, oss_err);
                    if(ret){
                        break;
                    }
                    mcfg.audio.setFormat((AVSampleFormat)audiocfg_proto.format_.getInt());
                    mcfg.audio.setChLayout(audiocfg_proto.chLayout_.getUint64());
                    mcfg.audio.setFrameSize(audiocfg_proto.frameSize_.getInt64());
                    mcfg.audio.setSampleRate(audiocfg_proto.sampleRate_.getInt());
                }else if(mcfg.getCodecType() == AVMEDIA_TYPE_VIDEO){
                    XProtoFFVideoCfg videocfg_proto;
                    ret = videocfg_proto.parse(val, oss_err);
                    if(ret){
                        break;
                    }
                    mcfg.video.setFormat((AVPixelFormat)videocfg_proto.format_.getInt());
                    mcfg.video.setWidth(videocfg_proto.width_.getInt());
                    mcfg.video.setHeight(videocfg_proto.height_.getInt());
                    mcfg.video.setFrameRate(videocfg_proto.frameRate_.getInt());
                }
            } // for
            if(ret){
                break;
            }
            
            
            ret = 0;
        }while(0);
            
        return ret;
    }
    
    static size_t parsePktFixed(const uint8_t * data_ptr,
                     size_t data_len,
                     AVPacket* pkt,
                     std::ostringstream& oss_err){
        size_t offset = 0;
        pkt->size = NUtil::le_get4(data_ptr, offset);     offset += 4;
        pkt->pts = NUtil::le_get8(data_ptr, offset); offset += 8;
        pkt->dts = NUtil::le_get8(data_ptr, offset); offset += 8;
        pkt->stream_index = NUtil::le_get4(data_ptr, offset); offset += 4;
        pkt->flags = NUtil::le_get4(data_ptr, offset);        offset += 4;
        pkt->side_data_elems = NUtil::le_get4(data_ptr, offset);  offset += 4;
        pkt->duration = NUtil::le_get8(data_ptr, offset); offset += 8;
        pkt->pos = NUtil::le_get8(data_ptr, offset);     offset += 8;
        return offset;
    }
    
    static size_t parsePktSideData(const uint8_t * data_ptr,
                                   size_t data_len,
                                   int elems,
                                   AVPacket* pkt,
                                   std::ostringstream& oss_err){
        size_t offset = 0;
        pkt->side_data_elems = elems;
        if(pkt->side_data_elems > 0){
            pkt->side_data = (AVPacketSideData *)av_malloc(pkt->side_data_elems * sizeof(AVPacketSideData));
            for(int i = 0; i < pkt->side_data_elems; ++i){
                AVPacketSideData& sd = pkt->side_data[i];
                
                if(data_len < (offset + 8)){
                    return -1;
                }
                
                offset = 0;
                sd.type = (AVPacketSideDataType)NUtil::le_get4(data_ptr, offset);   offset += 4;
                sd.size = NUtil::le_get4(data_ptr, offset);   offset += 4;
                
                if(sd.size > 0){
                    if(data_len < (offset + sd.size)){
                        return -1;
                    }
                    
                    memcpy(sd.data, data_ptr+offset, sd.size);
                    offset += sd.size;
                }
            }
        }
        return 0;
    }
    
    static int parse(const uint8_t * data_ptr,
                     size_t data_len,
                     AVPacket* pkt,
                     std::ostringstream& oss_err){
        size_t offset = 0;
        if(data_len < (offset + getFixedSizeOf(pkt))){
            return -1;
        }
        
        pkt->size = NUtil::le_get4(data_ptr, offset);
        av_new_packet(pkt, pkt->size);
        
        size_t sz = parsePktFixed(data_ptr+offset, data_len-offset, pkt, oss_err);
        if(sz == 0){
            return -1;
        }
        offset += sz;
        
        if(data_len < (offset + pkt->size)){
            return -1;
        }
        
        if(pkt->size > 0){
            memcpy(pkt->data, data_ptr + offset, pkt->size);
            offset += pkt->size;
        }
        
        if(pkt->side_data_elems > 0){
            sz = parsePktSideData(data_ptr + offset,
                                  data_len - offset,
                                  pkt->side_data_elems,
                                  pkt,
                                  oss_err);
            
            if(sz == 0){
                return -1;
            }
            offset += sz;
        }
        
        return 0;
    }
    
    static int make(const AVPacket * pkt, const DataFunc& func){
        int typ = TLV_TYPE_TIME_FF_PACKET;
        int64_t ts = NUtil::get_now_ms();
        size_t fixed_length = TLV_MIN_LENGTH + getFixedSizeOf(pkt);
        int length = (int)(getSizeOf(pkt)-TLV_PREFIX_LENGTH);
        uint8_t buf[fixed_length];
        
        size_t offset = 0;
        NUtil::le_set4(buf, offset, typ);       offset += 4;
        NUtil::le_set4(buf, offset, length);    offset += 4;
        NUtil::le_set8(buf, offset, ts);        offset += 8;
        
        NUtil::le_set4(buf, offset, pkt->size);     offset += 4;
        NUtil::le_set8(buf, offset, pkt->pts);  offset += 8;
        NUtil::le_set8(buf, offset, pkt->dts);  offset += 8;
        NUtil::le_set4(buf, offset, pkt->stream_index); offset += 4;
        NUtil::le_set4(buf, offset, pkt->flags);        offset += 4;
        NUtil::le_set4(buf, offset, pkt->side_data_elems);  offset += 4;
        NUtil::le_set8(buf, offset, pkt->duration); offset += 8;
        NUtil::le_set8(buf, offset, pkt->pos);      offset += 8;
        
        if(offset != sizeof(buf)){
            return -1;
        }
        
        int ret = 0;
        ret = func(buf, offset);
        if(ret){
            return ret;
        }
        
        ret = func(pkt->data, pkt->size);
        if(ret){
            return ret;
        }
        
        for(int i = 0; i < pkt->side_data_elems; ++i){
            AVPacketSideData& sd = pkt->side_data[i];
            
            offset = 0;
            NUtil::le_set4(buf, offset, sd.type);   offset += 4;
            NUtil::le_set4(buf, offset, sd.size);   offset += 4;
            
            ret = func(buf, offset);
            if(ret){
                return ret;
            }
            
            ret = func(sd.data, sd.size);
            if(ret){
                return ret;
            }
        }
        
        return 0;
    }
    
    static size_t getFixedSizeOf(const AVPacket * ){
        size_t fixed_length = 0; //TLV_MIN_LENGTH;
        fixed_length += 8; // pts
        fixed_length += 8; // dts
        fixed_length += 4; // stream_index
        fixed_length += 4; // flags
        fixed_length += 4; // side_data_elems
        fixed_length += 8; // duration
        fixed_length += 8; // pos
        fixed_length += 4; // size
        return fixed_length;
    }
    
    static size_t getSizeOf(const AVPacket * pkt){
        size_t fixed_length = TLV_MIN_LENGTH + getFixedSizeOf(pkt);
        size_t length = fixed_length + pkt->size;
        for(int i = 0; i < pkt->side_data_elems; ++i){
            AVPacketSideData& sd = pkt->side_data[i];
            length += 4; // type
            length += 4; // size
            length += sd.size; // data
        }
        return length;
    }
    
};


class FFTLVWriter{
public:
    virtual ~FFTLVWriter(){
        close();
    }
    
    int open(const std::string& filename){
        close();
        
        int ret = 0;
        do {
            fp_ = fopen(filename.c_str(), "wb");
            if(!fp_){
                ret = -1;
                NERROR_FMT_SET(ret, "fail to open [{}]", filename);
                break;
            }
            ret = 0;
        } while (0);
        return ret;
    }
    
    void close(){
        if(fp_){
            fclose(fp_);
            fp_ = nullptr;
        }
    }
    
    
    int write(const std::vector<FFMediaConfig>& configs){
        int ret = FFTLVFormat::make(configs,
                                  [this](const uint8_t * data_ptr, size_t length)->int{
                                      size_t bytes = fwrite(data_ptr, 1, length, fp_);
                                      int ret = (bytes == length) ? 0 : -1;
                                      return ret;
                                  });
        return ret;
    }
    
    int write(const AVPacket * pkt){
        int ret = FFTLVFormat::make(pkt,
                                  [this](const uint8_t * data_ptr, size_t length)->int{
            size_t bytes = fwrite(data_ptr, 1, length, fp_);
            int ret = (bytes == length) ? 0 : -1;
            return ret;
        });
        return ret;
    }
    
private:
    FILE * fp_ = nullptr;
};

class FFTLVReader{
public:
    FFTLVReader(){
    }
    
    virtual ~FFTLVReader(){
        close();
    }
    
    int open(const std::string& filename){
        close();
        
        int ret = 0;
        do {
            fp_ = fopen(filename.c_str(), "rb");
            if(!fp_){
                ret = -1;
                NERROR_FMT_SET(ret, "fail to open [{}]", filename);
                break;
            }
            ensureBuf(32*1024);
            
            ret = 0;
        } while (0);
        return ret;
    }
    
    void close(){
        if(fp_){
            fclose(fp_);
            fp_ = nullptr;
        }
        
        if(buf_){
            delete[] buf_;
            buf_ = nullptr;
        }
        bufsz_  =0;
    }
    
    int read(const FFTLVFormat::OnItemFuncs& funcs){
        size_t offset = 0;
        int ret = read(buf_, FFTLVFormat::TLV_PREFIX_LENGTH, offset);
        if(ret < 0){
            return ret;
        }
        
        int typ;
        size_t length;
        size_t bytes = FFTLVFormat::parsePrefix(buf_, offset, typ, length);
        if(bytes == 0){
            ret = -1;
            return ret;
        }
        offset = bytes;
        
        size_t data_size = offset + length;
        ensureBuf(data_size);
        
        ret = read(buf_, length, offset);
        if(ret < 0){
            return ret;
        }
        
        std::ostringstream oss;
        ret = FFTLVFormat::parse(buf_, data_size, oss, funcs);
        if(ret){
            NERROR_FMT_SET(ret, "parse fail with [{}]", oss.str());
        }
        
        return ret;
    }
    
    bool reachEnd()const{
        return (fp_ && feof(fp_));
    }
    
    int read(uint8_t * buf, size_t len, size_t& offset){
        size_t bytes = fread(buf+offset, sizeof(char), len, fp_);
        if(bytes != len){
            return -1;
        }
        offset += len;
        return 0;
    }
    
    int write(const std::vector<FFMediaConfig>& configs){
        int ret = FFTLVFormat::make(configs,
                                  [this](const uint8_t * data_ptr, size_t length)->int{
                                      size_t bytes = fwrite(data_ptr, 1, length, fp_);
                                      int ret = (bytes == length) ? 0 : -1;
                                      return ret;
                                  });
        return ret;
    }
    
    int write(const AVPacket * pkt){
        int ret = FFTLVFormat::make(pkt,
                                  [this](const uint8_t * data_ptr, size_t length)->int{
                                      size_t bytes = fwrite(data_ptr, 1, length, fp_);
                                      int ret = (bytes == length) ? 0 : -1;
                                      return ret;
                                  });
        return ret;
    }
    
private:
    void ensureBuf(size_t sz){
        if(sz <= bufsz_){
            return ;
        }
        
        if(buf_){
            delete[] buf_;
        }
        buf_ = new uint8_t[sz];
        bufsz_ = sz;
    }
    
private:
    FILE * fp_ = nullptr;
    size_t bufsz_ = 0;
    uint8_t * buf_ = nullptr;
};

class MediaFile2TLV{
public:
    MediaFile2TLV(const std::string& name):logger_(NLogger::Get(name)){
        
    }
    
    int transfmt(const std::string& media_file, const std::string& tlv_file){
        FFReader reader;
        FFTLVWriter writer;
        int ret = 0;
        do{
            const std::string containerFormat = "";
            ret = reader.open(containerFormat, media_file);
            if(ret){
                logger_->error("fail to open [{}]", media_file);
                break;
            }
            logger_->info("media file opened [{}]", media_file);
            
            ret = writer.open(tlv_file);
            if(ret){
                logger_->error("fail to open [{}]", tlv_file);
                break;
            }
            logger_->info("tlv file opened [{}]", tlv_file);
            
            
            std::vector<FFMediaConfig> configs(reader.getNumStreams());
            for(int i = 0; i < reader.getNumStreams(); ++i){
                const AVStream* stream = reader.getStream(i);
                configs[i].assign(stream);
            }
            
            ret = writer.write(configs);
            if(ret){
                logger_->error("fail to write FF configs, ret=[{}]", ret);
                break;
            }
            
            while(1){
                FFPacket packet;
                ret = reader.read(packet);
                if(ret){
                    if(ret == AVERROR_EOF){
                        ret = 0;
                    }
                    break;
                }
                
                ret = writer.write(packet.avpkt);
                if(ret){
                    logger_->error("fail to write packet, ret=[{}]", ret);
                    break;
                }
            }
            
            logger_->info("transfmt media -> tlv done");
        }while (0);
        reader.close();
        return ret;
    }
    
private:
    NLogger::shared logger_;
};


class TLVFile2Media{
public:
    TLVFile2Media(const std::string& name):logger_(NLogger::Get(name)){
        
    }
    
    int transfmt(const std::string& tlv_file, const std::string& media_file){
        FFTLVReader reader;
        FFWriter writer;
        int ret = 0;
        do{
            ret = reader.open(tlv_file);
            if(ret){
                logger_->error("fail to open [{}]", tlv_file);
                break;
            }
            logger_->info("input file opened [{}]", tlv_file);
            
            std::vector<FFMediaConfig> configs;
            FFTLVFormat::OnItemFuncs on_funcs;
            on_funcs.onHeader = [&writer, &media_file, &configs, this](int64_t ts, const FFTLVFormat::Header& header)->int{
                if(writer.isOpened()){
                    logger_->error("got header, but writer already opened");
                    return -1;
                }
                
                configs = header.configs;
                sort(configs.begin(), configs.end(), FFMediaConfigLessIndex);
                for(auto& cfg : configs){
                    int ret = writer.addTrack(cfg);
                    if(ret<0){
                        logger_->error("fail to add track");
                        return ret;
                    }else{
                        cfg.setIndex(ret);
                    }
                }
                
                int ret = writer.open(media_file);
                if(ret){
                    logger_->error("fail to open [{}]", media_file);
                    return ret;
                }
                logger_->info("output file opened [{}]", media_file);
                
                return 0;
            };
            
            on_funcs.onPacket = [this, &writer, &configs](int64_t ts, const AVPacket * pkt)->int{
                if(!writer.isOpened()){
                    logger_->error("got packet, but writer NOT opened");
                    return -1;
                }
                if(pkt->stream_index >= configs.size()){
                    logger_->error("packet index exceed range");
                    return -1;
                }
                
                int index = configs[pkt->stream_index].getIndex();
                int ret = writer.write(index, pkt);
                return ret;
            };
            
            while(1){
                ret = reader.read(on_funcs);
                if(ret){
                    if(reader.reachEnd()){
                        ret = 0;
                        logger_->info("reach tlv end");
                    }else{
                        logger_->error("read tlv fail, [{}]", NThreadError::lastMsg());
                    }
                    break;
                }
            }
            if(ret) break;
            logger_->info("transfmt OK");
        }while (0);
        reader.close();
        logger_->info("transfmt tlv -> media done");
        return ret;
    }
private:
    NLogger::shared logger_;
};



int lab060_PushTLV_main(int argc, char* argv[]){
    av_register_all();
    avdevice_register_all();
    avcodec_register_all();
    
//    {
//        //const std::string src_media_file = "/Users/simon/Desktop/simon/projects/easemob/src/xmedia/resource/test.mp4";
//        const std::string src_media_file = "/tmp/tt.mp4";
//        const std::string dst_tlv_file = "/tmp/out.tlv";
//        int ret =  MediaFile2TLV("main").transfmt(src_media_file, dst_tlv_file);
//        if(ret){
//            return ret;
//        }
//    }
//    
//    {
//        const std::string src_tlv_file = "/tmp/out.tlv";
//        const std::string dst_media_file = "/tmp/out.mp4";
//        int ret = TLVFile2Media("main").transfmt(src_tlv_file, dst_media_file);
//        if(ret){
//            return ret;
//        }
//    }
    

    
    return 0;
}
