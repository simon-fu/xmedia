

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



int lab060_PushTLV_main0(int argc, char* argv[]){
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




















class FFPCMWriter{
public:
    FFPCMWriter(const std::string& name):logger_(NLogger::Get(name)){
    }
    
    virtual ~FFPCMWriter(){
        close();
    }
    
    int open(const FFSampleConfig& srccfg, const std::string& path_name_prefix){
        int ret = 0;
        do {
            char fname[256];
            sprintf(fname, "%s%dHz_%dch_%sle.pcm",
                    path_name_prefix.c_str(),
                    srccfg.getSampleRate(),
                    srccfg.getChannels(),
                    av_get_sample_fmt_name(srccfg.getFormat()));
            
            outfp_ = fopen(fname, "wb");
            if(!outfp_){
                logger_->error("fail to open [{}]", fname);
                ret = -1;
                break;
            }
            logger_->info("opened [{}]", fname);
        } while (0);
        return ret;
    }
    
    void close(){
        if(outfp_){
            fclose(outfp_);
            outfp_ = nullptr;
        }
    }
    
    int write(const AVFrame * avframe){
        if(!outfp_) return -1;
        
        int ret = (int)fwrite(avframe->data[0], sizeof(char), avframe->pkt_size, outfp_);
        if(ret != avframe->pkt_size){
            logger_->error("fail to write file, ret=[%d]", ret);
            return -1;
        }
        return 0;
    }
    
private:
    NLogger::shared logger_;
    FILE * outfp_ = nullptr;
    //FFSampleConverter000 converter_;
};

class FFPCMReader{
public:
    FFPCMReader(const std::string& name):logger_(NLogger::Get(name)){
    }
    
    virtual ~FFPCMReader(){
        close();
    }
    
    int open(const FFSampleConfig& srccfg, const std::string& filename){
        int ret = 0;
        do {
            fp_ = fopen(filename.c_str(), "rb");
            if(!fp_){
                logger_->error("fail to open [{}]", filename);
                ret = -1;
                break;
            }
            logger_->info("opened [{}]", filename);
            cfg_ = srccfg;
            if(cfg_.getFrameSize() <= 0){
                cfg_.setFrameSize(1024);
            }
            frame_ = av_frame_alloc();
            ptsconv_.setSrcBase((AVRational){1, srccfg.getSampleRate()});
            ptsconv_.setDstBase((AVRational){1, srccfg.getSampleRate()});
            readSamples_ = 0;
        } while (0);
        return ret;
    }
    
    void setOutTimebase(const AVRational &dst){
        ptsconv_.setDstBase(dst);
    }
    
    const AVRational& getOutTimebase()const{
        return ptsconv_.dst();
    }
    
    void close(){
        if(fp_){
            fclose(fp_);
            fp_ = nullptr;
        }
        if(frame_){
            av_frame_free(&frame_);
        }
    }
    
    int read(const FFFrameFunc& func){
        if(!fp_) return -1;
        
        frame_->sample_rate = cfg_.getSampleRate();
        frame_->format = cfg_.getFormat();
        frame_->channel_layout = cfg_.getChLayout();
        frame_->channels = av_get_channel_layout_nb_channels(frame_->channel_layout);
        
        int ret = 0;
        frame_->nb_samples = cfg_.getFrameSize();
        av_frame_get_buffer(frame_, 0);
        frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                      frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
        
        ret = (int)fread(frame_->data[0], sizeof(char), frame_->pkt_size, fp_);
        if(ret != frame_->pkt_size){
            if (!feof(fp_)) {
                logger_->error("fail to read file, ret=[{}]", ret);
                return -1;
            }
            logger_->info("reach file end");
            return 0;
        }
        int bytes = ret;
        
        frame_->pts = ptsconv_.convert(10000 + readSamples_);;
        readSamples_ += frame_->nb_samples;
        func(frame_);
        av_frame_unref(frame_);
        return bytes;
    }
    
private:
    NLogger::shared logger_;
    FILE * fp_ = nullptr;
    FFSampleConfig cfg_;
    AVFrame * frame_ = nullptr;
    FFPTSConverter ptsconv_;
    int64_t readSamples_ = 0;
};




class FFFramesizeConverter000{
public:
    FFFramesizeConverter000():outTimebase_({1, 1000}){
        
    }
    
    virtual ~FFFramesizeConverter000(){
        close();
    }
    
    int open(const AVRational& timebase, int framesize){
        close();
        outTimebase_ = timebase;
        dstCfg_.setFrameSize(framesize);
        return 0;
    }
    
    void close(){
        if(outFrame_){
            av_frame_free(&outFrame_);
            outFrame_ = nullptr;
        }
        dstCfg_.reset();
    }
    
    int convert(const AVFrame * srcframe, const FFFrameFunc& func){
        int ret = 0;
        if(!srcframe){
            if(outFrame_ && outFrame_->nb_samples > 0){
                // fflush remain samples
                ret = func(outFrame_);
            }
            return ret;
        }
        
        if(!outFrame_){
            if(srcframe->nb_samples == dstCfg_.getFrameSize() || dstCfg_.getFrameSize() <= 0){
                // same frame size, output directly
                ret = func(srcframe);
                return ret;
            }
            
            dstCfg_.assign(srcframe);
            outFrame_ = av_frame_alloc();
        }
        
        outFrame_->pts = srcframe->pts - samples2Duration(outFrame_->nb_samples);
        
        int dst_offset = outFrame_->nb_samples;
        int src_offset = 0;
        while(src_offset < srcframe->nb_samples){
            checkAssignFrame();
            int remain_samples = srcframe->nb_samples-src_offset;
            int min = std::min(remain_samples+dst_offset, dstCfg_.getFrameSize());
            int num_copy = min - dst_offset;
            
            av_samples_copy(outFrame_->data, srcframe->data,
                            dst_offset,
                            src_offset,
                            num_copy, outFrame_->channels, (AVSampleFormat)outFrame_->format);
            dst_offset += num_copy;
            src_offset += num_copy;
            outFrame_->nb_samples = dst_offset;
            if(outFrame_->nb_samples == dstCfg_.getFrameSize()){
                outFrame_->pkt_size = av_samples_get_buffer_size(outFrame_->linesize, outFrame_->channels,
                                                                 outFrame_->nb_samples, (AVSampleFormat)outFrame_->format,
                                                                 1);
                ret = func(outFrame_);
                av_frame_unref(outFrame_);
                if(ret != 0){
                    return ret;
                }
                outFrame_->pts = outFrame_->pts + samples2Duration(outFrame_->nb_samples);
                outFrame_->nb_samples = 0;
                dst_offset = 0;
            }
        }
        return 0;
    }
    
private:
    int64_t samples2Duration(int out_samples){
        int64_t duration = out_samples * outTimebase_.den / dstCfg_.getSampleRate() / outTimebase_.num;
        return duration;
    }
    
    void checkAssignFrame(){
        if(!outFrame_->buf[0]){
            assignFrame();
            outFrame_->nb_samples = dstCfg_.getFrameSize();
            if(outFrame_->nb_samples > 0){
                av_frame_get_buffer(outFrame_, 0);
            }
            outFrame_->nb_samples = 0;
        }
    }
    
    void assignFrame(){
        outFrame_->sample_rate = dstCfg_.getSampleRate();
        outFrame_->format = dstCfg_.getFormat();
        outFrame_->channel_layout = dstCfg_.getChLayout();
        outFrame_->channels = av_get_channel_layout_nb_channels(outFrame_->channel_layout);
    }
    
private:
    //int framesize_ = 0;
    AVRational outTimebase_;
    AVFrame * outFrame_ = nullptr;
    FFSampleConfig dstCfg_;
};




class FFSampleConverter000{
public:
    FFSampleConverter000():timebase_({1, 1000}){
        
    }
    
    virtual ~FFSampleConverter000(){
        close();
    }
    
    int open(const FFSampleConfig& srccfg, const FFSampleConfig& dstcfg, const AVRational& timebase){
        close();
        
        srccfg_ = srccfg;
        dstcfg_ = dstcfg;
        timebase_ = timebase;
        
        int ret = 0;
        if(!srccfg.equalBase(dstcfg)){
            
            swrctx_ = swr_alloc_set_opts(
                                         NULL,
                                         dstcfg.getChLayout(),
                                         dstcfg.getFormat(),
                                         dstcfg.getSampleRate(),
                                         srccfg.getChLayout(),
                                         srccfg.getFormat(),
                                         srccfg.getSampleRate(),
                                         0, NULL);
            if(!swrctx_){
                NERROR_FMT_SET(ret, "fail to swr_alloc_set_opts");
                return -61;
            }
            
            ret = swr_init(swrctx_);
            if(ret != 0){
                NERROR_FMT_SET(ret, "fail to swr_init , err=[{}]", av_err2str(ret));
                return ret;
            }
            
            // alloc buffer
            //reallocBuffer(dstcfg_.framesize);
            
            frame_ = av_frame_alloc();
            
        }else{
            ret = sizeConverter_.open(timebase_, dstcfg_.getFrameSize());
        }
        return ret;
    }
    
    void close(){
        if(swrctx_){
            if(swr_is_initialized(swrctx_)){
                swr_close(swrctx_);
            }
            swr_free(&swrctx_);
        }
        
        if(frame_){
            av_frame_free(&frame_);
            frame_ = nullptr;
        }
        
        sizeConverter_.close();
        realDstFramesize_ = 0;
    }
    
    int convert(const AVFrame * srcframe, const FFFrameFunc& func){
        if(!swrctx_){
            return sizeConverter_.convert(srcframe, func);
        }
        int ret = 0;
        if(!srcframe){
            if(remainDstSamples_ > 0){
                // pull remain samples
                checkAssignFrame();
                frame_->pts = nextDstPTS_;
                ret = swr_convert_frame(swrctx_, frame_, NULL);
                if(ret != 0){
                    av_frame_unref(frame_);
                    NERROR_FMT_SET(ret, "fail to swr_convert_frame 1st, err=[{}]", av_err2str(ret));
                    return ret;
                }
                frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                              frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
                if(frame_->nb_samples > 0){
                    ret = func(frame_);
                }
                av_frame_unref(frame_);
            }
            return ret;
        }
        
        int out_samples = swr_get_out_samples(swrctx_, srcframe->nb_samples);
        if(out_samples < realDstFramesize_){
            // not enough output samples
            ret = swr_convert_frame(swrctx_, NULL, srcframe);
            if(ret != 0){
                NERROR_FMT_SET(ret, "fail to swr_convert_frame 2nd, err=[{}]", av_err2str(ret));
                return ret;
            }
            return 0;
        }
        
        //        if(dstcfg_.framesize <= 0){
        //            reallocBuffer(srcframe->nb_samples);
        //        }
        
        checkAssignFrame();
        
        int64_t dst_pts = srcframe->pts;
        frame_->pts = dst_pts - dstDuration(remainDstSamples_);
        frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                      frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
        
        ret = swr_convert_frame(swrctx_, frame_, srcframe);
        if(ret != 0){
            av_frame_unref(frame_);
            NERROR_FMT_SET(ret, "fail to swr_convert_frame 3rd, err=[{}]", av_err2str(ret));
            return ret;
        }
        
        frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                      frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
        
        // TODO:
        //        static Int64Relative srcRelpts_;
        //        const AVFrame * frame = frame_;
        //        odbgd("src frame0: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
        //              , frame->pts, srcRelpts_.offset(frame->pts), srcRelpts_.delta(frame->pts)
        //              , frame->channels, frame->nb_samples, frame->pkt_size
        //              , av_get_sample_fmt_name((AVSampleFormat)frame->format)
        //              , frame->sample_rate);
        
        ret = func(frame_);
        
        
        if(ret){
            av_frame_unref(frame_);
            return ret;
        }else{
            out_samples -= frame_->nb_samples;
            frame_->pts += dstDuration(frame_->nb_samples);
            nextDstPTS_ = frame_->pts;
            av_frame_unref(frame_);
        }
        
        while(realDstFramesize_>0 && out_samples >= realDstFramesize_){
            checkAssignFrame();
            //frame_->pts = nextPTS(srcframe->pts, out_samples);
            ret = swr_convert_frame(swrctx_, frame_, NULL);
            if(ret != 0){
                av_frame_unref(frame_);
                NERROR_FMT_SET(ret, "fail to swr_convert_frame 4th, err=[{}]", av_err2str(ret));
                return ret;
            }
            frame_->pkt_size = av_samples_get_buffer_size(frame_->linesize, frame_->channels,
                                                          frame_->nb_samples, (AVSampleFormat)frame_->format, 1);
            
            //            // TODO:
            //            const AVFrame * frame = frame_;
            //            odbgd("src frame1: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
            //                  , frame->pts, srcRelpts_.offset(frame->pts), srcRelpts_.delta(frame->pts)
            //                  , frame->channels, frame->nb_samples, frame->pkt_size
            //                  , av_get_sample_fmt_name((AVSampleFormat)frame->format)
            //                  , frame->sample_rate);
            
            ret = func(frame_);
            av_frame_unref(frame_);
            if(ret){
                return ret;
            }
            
            out_samples -= frame_->nb_samples;
            frame_->pts += dstDuration(frame_->nb_samples);
            nextDstPTS_ = frame_->pts;
            //out_samples = swr_get_out_samples(swrctx_, 0);
        }
        
        remainDstSamples_ = out_samples;
        
        return 0;
    }
    
private:
    int64_t dstDuration(int out_samples){
        int64_t duration = out_samples * timebase_.den / dstcfg_.getSampleRate() / timebase_.num;
        return duration;
    }
    
    int checkAssignFrame(){
        frame_->sample_rate = dstcfg_.getSampleRate();
        frame_->format = dstcfg_.getFormat();
        frame_->channel_layout = dstcfg_.getChLayout();
        frame_->channels = av_get_channel_layout_nb_channels(frame_->channel_layout);
        
        int ret = 0;
        if(dstcfg_.getFrameSize() > 0){
            frame_->nb_samples = dstcfg_.getFrameSize();
            ret = av_frame_get_buffer(frame_, 0);
        }
        return ret;
    }
    
    //    int reallocBuffer(int frameSize){
    //        // alloc buffer
    //        int ret = 0;
    //        if(frameSize > 0 && frameSize > realDstFramesize_){
    //            if(!frame_){
    //                frame_ = av_frame_alloc();
    //                frame_->sample_rate = dstcfg_.samplerate;
    //                frame_->format = dstcfg_.samplefmt;
    //                frame_->channel_layout = dstcfg_.channellayout;
    //                frame_->channels = av_get_channel_layout_nb_channels(frame_->channel_layout);
    //            }
    //
    //            if(frame_ && realDstFramesize_ > 0){
    //                av_frame_unref(frame_);
    //                realDstFramesize_ = 0;
    //            }
    //
    //            frame_->nb_samples = frameSize;
    //            ret = av_frame_get_buffer(frame_, 0);
    //            if(ret == 0){
    //                realDstFramesize_ = frameSize;
    //            }
    //        }
    //        return ret;
    //    }
    
private:
    FFSampleConfig srccfg_;
    FFSampleConfig dstcfg_;
    AVRational timebase_;
    SwrContext * swrctx_ = nullptr;
    AVFrame * frame_ = nullptr;
    int realDstFramesize_ = -1;
    int64_t nextDstPTS_ = 0;
    int remainDstSamples_ = 0;
    FFFramesizeConverter000 sizeConverter_;
};




class FFAudioEncoder000{
public:
    //static const AVCodecID DEFAULT_CODEC_ID = AV_CODEC_ID_AAC;
    //static const int DEFAULT_CLOCKRATE = 44100;
    //static const int DEFAULT_CHANNELS = 2;
    static const int64_t DEFAULT_BITRATE = 24000;
    
public:
    FFAudioEncoder000():timebase_((AVRational){1, 1000}),
    srcWriter_("srcwriter"),
    encWriter_("encwriter")
    {
        
    }
    
    virtual ~FFAudioEncoder000(){
        close();
    }
    
    void setCodecName(const std::string& codec_name){
        codec_ = avcodec_find_encoder_by_name(codec_name.c_str());
        if(codec_){
            codecId_ = codec_->id;
        }else{
            codecId_ = AV_CODEC_ID_NONE;
        }
    }
    
    void setCodecId(AVCodecID codec_id){
        codecId_ = codec_id;
        codec_ = avcodec_find_encoder(codec_id);
    }
    
    void setBitrate(int64_t bitrate){
        bitrate_ = bitrate;
    }
    
    void setSrcConfig(const FFSampleConfig& cfg){
        srcCfg_ = cfg;
    }
    
    void setTimebase(const AVRational& timebase){
        timebase_ = timebase;
    }
    
    void setStreamIndex(int index){
        streamIndex_ = index;
    }
    
    const FFSampleConfig& srcConfig(){
        return srcCfg_;
    }
    
    const FFSampleConfig& encodeConfig(){
        return encodeCfg_;
    }
    
    AVCodecID codecId() const{
        return codecId_;
    }
    
    int open(){
        int ret = 0;
        do{
            AVCodecID codec_id = (AVCodecID)codecId_;
            if(codec_id <= AV_CODEC_ID_NONE){
                ret = -1;
                break;
            }
            
            const AVCodec * codec = codec_;
            if(!codec){
                codec = avcodec_find_encoder(codec_id);
                if (!codec) {
                    ret = -1;
                    NERROR_FMT_SET(ret, "fail to avcodec_find_encoder audio codec id [{}]",
                                   (int)codec_id);
                    break;
                }
            }
            
            ctx_ = avcodec_alloc_context3(codec);
            if (!ctx_) {
                ret = -1;
                NERROR_FMT_SET(ret, "fail to avcodec_alloc_context3 audio codec [{}][{}]",
                               (int)codec->id, codec->name);
                break;
            }
            
            pkt_ = av_packet_alloc();
            if (!pkt_){
                NERROR_FMT_SET(ret, "fail to av_packet_alloc audio");
                ret = -1;
                break;
            }
            
            ctx_->bit_rate = bitrate_ > 0 ? bitrate_ : DEFAULT_BITRATE;
            ctx_->sample_fmt     = selectSampleFormat(codec, srcCfg_.getFormat());
            ctx_->sample_rate    = selectSampleRate(codec, srcCfg_.getSampleRate());
            ctx_->channel_layout = selectChannelLayout(codec, srcCfg_.getChLayout() );
            ctx_->channels       = av_get_channel_layout_nb_channels(ctx_->channel_layout);
            ctx_->time_base      = timebase_;
            //ctx_->time_base      = srcCfg_.timebase;
            
            ret = avcodec_open2(ctx_, codec, NULL);
            if (ret != 0) {
                NERROR_FMT_SET(ret, "fail to avcodec_open2 audio codec [{}][{}], err=[{}]",
                               (int)codec->id, codec->name, av_err2str(ret));
                break;
            }
            
            encodeCfg_.setFormat(ctx_->sample_fmt);
            encodeCfg_.setSampleRate(ctx_->sample_rate);
            encodeCfg_.setChLayout(ctx_->channel_layout);
            encodeCfg_.setFrameSize(ctx_->frame_size);
            //encodeCfg_.timebase = ctx_->time_base;
            
            srcCfg_.match(encodeCfg_);
            
            ret = converter_.open(srcCfg_, encodeCfg_, timebase_);
            if(ret != 0){
                break;
            }
            
            srcWriter_.open(srcCfg_,    "/tmp/out_src_");
            encWriter_.open(encodeCfg_, "/tmp/out_enc_");
            
            ret = 0;
        }while(0);
        
        if(ret){
            close();
        }
        
        return ret;
    }
    
    void close(){
        if(ctx_){
            avcodec_free_context(&ctx_);
            ctx_ = nullptr;
        }
        
        if(pkt_){
            av_packet_free(&pkt_);
            pkt_ = nullptr;
        }
        
        srcWriter_.close();
        encWriter_.close();
    }
    
    int encode(const AVFrame *frame, const FFPacketFunc& out_func){
        int ret = 0;
        if(frame){
            //            odbgd("audio frame: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
            //                  , frame->pts, srcRelpts_.offset(frame->pts), srcRelpts_.delta(frame->pts)
            //                  , frame->channels, frame->nb_samples, frame->pkt_size
            //                  , av_get_sample_fmt_name((AVSampleFormat)frame->format)
            //                  , frame->sample_rate);
            srcWriter_.write(frame);
        }
        
        ret = converter_.convert(frame, [this, out_func](const AVFrame * outframe)->int{
            return doEncode(outframe, out_func);
        });
        
        if(!frame && numInFrames_ > 0){
            ret = doEncode(nullptr, out_func);
        }
        return ret;
    }
    
private:
    int doEncode(const AVFrame *frame, const FFPacketFunc& out_func){
        if(frame){
            ++numInFrames_;
            //            odbgd("No.%lld encode audio: pts=%lld(%+lld,%+lld), ch=%d, nb_samples=%d, pkt_size=%d, fmt=[%s], samplerate=%d"
            //                  , numInFrames_
            //                  , frame->pts, encRelpts_.offset(frame->pts), encRelpts_.delta(frame->pts)
            //                  , frame->channels, frame->nb_samples, frame->pkt_size
            //                  , av_get_sample_fmt_name((AVSampleFormat)frame->format)
            //                  , frame->sample_rate);
            encWriter_.write(frame);
        }
        int ret = avcodec_send_frame(ctx_, frame);
        if (ret != 0) {
            NERROR_FMT_SET(ret, "fail to avcodec_send_frame audio, err=[{}]",
                           av_err2str(ret));
            return ret;
        }
        
        while (ret >= 0) {
            ret = avcodec_receive_packet(ctx_, pkt_);
            if (ret == AVERROR(EAGAIN)){
                return 0;
            }else if(ret == AVERROR_EOF){
                return 0;
            }else if (ret < 0) {
                NERROR_FMT_SET(ret, "fail to avcodec_receive_packet audio, err=[{}]",
                               av_err2str(ret));
                return ret;
            }
            ++numOutPackets_;
            //            odbgd("No.%lld audio pkt: pts=%lld(%+lld,%+lld), dts=%lld, size=%d",
            //                  numOutPackets_,
            //                  pkt_->pts,
            //                  dstRelpts_.offset(pkt_->pts),
            //                  dstRelpts_.delta(pkt_->pts),
            //                  pkt_->dts, pkt_->size);
            pkt_->stream_index = streamIndex_;
            ret = out_func(pkt_);
            av_packet_unref(pkt_);
            if(ret){
                return ret;
            }
        }
        return 0;
    }
    
    static int selectSampleRate(const AVCodec *codec, int prefer_samplerate){
        if(!codec->supported_samplerates || *codec->supported_samplerates == 0){
            // no supported samplerate
            if(prefer_samplerate > 0){
                // select prefer one
                return prefer_samplerate;
            }else{
                // select default one
                return 44100;
            }
        }
        
        int best_samplerate = 0;
        const int * p = codec->supported_samplerates;
        while (*p) {
            if (best_samplerate <= 0
                || (abs(prefer_samplerate - *p) < abs(prefer_samplerate - best_samplerate))){
                best_samplerate = *p;
            }
            p++;
        }
        return best_samplerate;
    }
    
    static uint64_t selectChannelLayout(const AVCodec *codec, uint64_t prefer_layout){
        if(!codec->channel_layouts || *codec->channel_layouts == 0){
            // no supported channel layout
            if(prefer_layout > 0){
                // select prefer one
                return prefer_layout; //  == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
            }else{
                // select default one
                return AV_CH_LAYOUT_MONO;
            }
        }
        
        if(prefer_layout == 0){
            // if no prefer, select first
            return *codec->channel_layouts;
        }
        
        int prefer_channels = av_get_channel_layout_nb_channels(prefer_layout);
        uint64_t best_ch_layout = 0;
        int best_nb_channels   = 0;
        const uint64_t *p = codec->channel_layouts;
        while (*p) {
            if(*p == prefer_layout){
                return *p;
            }
            
            int nb_channels = av_get_channel_layout_nb_channels(*p);
            if (best_nb_channels <= 0
                || (abs(prefer_channels - nb_channels) < abs(prefer_channels - best_nb_channels))){
                best_nb_channels = nb_channels;
            }
            p++;
        }
        return best_ch_layout;
    }
    
    static AVSampleFormat selectSampleFormat(const AVCodec *codec, AVSampleFormat prefer_fmt){
        if(!codec->sample_fmts || *codec->sample_fmts <= AV_SAMPLE_FMT_NONE){
            // no supported format
            if(prefer_fmt > AV_SAMPLE_FMT_NONE){
                // select prefer format
                return prefer_fmt;
            }else{
                // select default format
                return AV_SAMPLE_FMT_S16;
            }
        }
        
        const enum AVSampleFormat *p = codec->sample_fmts;
        while (*p != AV_SAMPLE_FMT_NONE) {
            if (*p == prefer_fmt){
                // match, select it
                return prefer_fmt;
            }
            p++;
        }
        // NOT match, select first supported format
        return *codec->sample_fmts;
    }
    
private:
    AVCodecID codecId_ = AV_CODEC_ID_NONE;
    FFSampleConfig  srcCfg_;
    FFSampleConfig  encodeCfg_;
    FFSampleConverter000 converter_;
    int64_t bitrate_ = -1;
    const AVCodec * codec_ = nullptr;
    AVCodecContext * ctx_ = nullptr;
    AVPacket * pkt_ = nullptr;
    AVRational timebase_;
    int64_t numInFrames_ = 0;
    int64_t numOutPackets_ = 0;
    Int64Relative srcRelpts_;
    Int64Relative encRelpts_;
    Int64Relative dstRelpts_;
    int streamIndex_ = -1;
    FFPCMWriter srcWriter_;
    FFPCMWriter encWriter_;
};





class AudioEncoderTester{
public:
    AudioEncoderTester(const std::string& name):logger_(NLogger::Get(name)){
        
    }
    
    virtual ~AudioEncoderTester(){
        
    }
    
    int test(){
        const std::string& filename = "/tmp/test_44100Hz_2ch_fltle.pcm";
        FFSampleConfig srccfg;
        srccfg.setFormat(AV_SAMPLE_FMT_FLT);
        srccfg.setSampleRate(44100);
        srccfg.setChLayout(av_get_default_channel_layout(2));
        srccfg.setFrameSize(512);
        
        FFPCMReader reader("pcmreader");
        int ret = 0;
        do{
            ret = reader.open(srccfg, filename);
            if(ret){
                logger_->error("fail to open file [{}], ret={}", filename, ret);
                break;
            }
            reader.setOutTimebase((AVRational){1, 1000000});
            
            audioEncoder_.setStreamIndex(0);
            audioEncoder_.setCodecName("libfdk_aac");
            audioEncoder_.setBitrate(32*1000);
            audioEncoder_.setTimebase(reader.getOutTimebase());
            audioEncoder_.setSrcConfig(srccfg);
            ret = audioEncoder_.open();
            if(ret){
                logger_->error("fail to open audio encoder, ret={}", ret);
                break;
            }
            
            while(1){
                ret = reader.read([this](const AVFrame * frame)->int{
                    logger_->info("got frame,  format={}, pts={}", frame->format, frame->pts);
                    audioEncoder_.encode(frame, [this](AVPacket * pkt)->int{
                        logger_->info("got packet, index={}, pts={}", pkt->stream_index, pkt->pts);
                        return 0;
                    });
                    return 0;
                });
                if(ret <= 0){
                    break;
                }
            }
        }while(0);
        
        reader.close();
        audioEncoder_.close();
        
        return ret;
    }
    
private:
    NLogger::shared logger_ ;
    FFAudioEncoder000 audioEncoder_;
};

int lab060_PushTLV_main(int argc, char* argv[]){
    av_register_all();
    avdevice_register_all();
    avcodec_register_all();
    
    {
        AudioEncoderTester tester("tester");
        tester.test();
    }
    
    
    return 0;
}

