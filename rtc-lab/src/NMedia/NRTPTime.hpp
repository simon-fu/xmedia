
#ifndef NRTPTime_hpp
#define NRTPTime_hpp

#include <stdio.h>
#include <string>
#include <sstream>
#include <iomanip>

class NNtp{
public:
    static const uint32_t kNtpJan1970 = 2208988800UL; // seconds from 1900/1/1 to 1970/1/1
    static const uint64_t kMagicNtpFractionalUnit = 4294967296ULL;
    
    inline static
    uint64_t Milliseconds2NTP (uint64_t time_ms) {
        uint64_t seconds = (time_ms / 1000) + kNtpJan1970;
        uint64_t fractions = time_ms % 1000;
        fractions =  kMagicNtpFractionalUnit * fractions / 1000;
        uint64_t ntp = (seconds << 32) | fractions;
        return ntp;
    }
    
    inline static
    int64_t DeltaMilliseconds2NTP (int64_t delta_time_ms) {
        uint64_t abs_delta_time_ms = std::abs(delta_time_ms);
        int64_t ntp = Milliseconds2NTP(abs_delta_time_ms);
        return delta_time_ms >= 0 ? ntp : -ntp;
    }
    
    inline static
    uint64_t NTP2Milliseconds(uint64_t ntp){
        uint64_t seconds = ntp >> 32;
        uint64_t fractions = ntp & 0x0FFFFFFFFULL;
        fractions = 1000  * fractions / kMagicNtpFractionalUnit;
        uint64_t time_ms = seconds * 1000+ fractions;
        return time_ms;
    }
    
    inline static
    int64_t DeltaNTP2Milliseconds(int64_t ntp_delta){
        uint64_t abs_ntp_delta = std::abs(ntp_delta);
        int64_t time_ms = NTP2Milliseconds(abs_ntp_delta);
        return ntp_delta >= 0 ? time_ms : -time_ms;
    }
    
    inline static
    int64_t DeltaTimestamp2NTP (int32_t delta_ts, uint32_t clockrate) {
        uint64_t abs_delta_ts = std::abs(delta_ts);
        uint64_t delta_ts_sec = abs_delta_ts / clockrate;
        uint64_t delta_ts_frac =  abs_delta_ts % clockrate;
        uint64_t delta_ntp_frac = kMagicNtpFractionalUnit * delta_ts_frac/clockrate;
        int64_t delta_ntp = (delta_ts_sec << 32) | delta_ntp_frac;
        return delta_ts >= 0 ? delta_ntp : -delta_ntp;
    }
    
    inline static
    std::string toString(uint64_t ntpts){
        uint32_t seconds = (uint32_t)(ntpts >> 32);
        time_t in_time_t = ntpts > 0 ? seconds - kNtpJan1970 : 0;
        auto ms = NTP2Milliseconds(ntpts)%1000;
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X") << "." << ms;
        return ss.str();
    }
    
    inline static
    std::string toShortString(uint64_t ntpts){
        uint32_t seconds = (uint32_t)(ntpts >> 32);
        time_t in_time_t = ntpts > 0 ? seconds - kNtpJan1970 : 0;
        auto ms = NTP2Milliseconds(ntpts)%1000;
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%X") << "." << ms;
        return ss.str();
    }
    
    inline static
    std::string toNum2String(uint64_t ntpts){
        uint32_t seconds = (uint32_t)(ntpts >> 32);
        uint32_t frac =(uint32_t)(ntpts & 0x0FFFFFFFFULL);
        std::stringstream ss;
        ss << seconds << "," << frac;
        return ss.str();
    }
    
    inline static
    std::string toHuman2String(uint64_t ntpts){
        uint64_t time_ms = NTP2Milliseconds(ntpts);
        uint64_t seconds = (time_ms/1000)%60;
        uint64_t milli = time_ms%1000;;
        std::stringstream ss;
        ss << seconds << "." << milli;
        return ss.str();
    }
    
};



class NRTPTime{
public:
    class RemoteNTP{
    public:
        void updateNTP(uint32_t ntp_sec, uint32_t ntp_frac, uint32_t rtp_timestamp){
            ntp_ = ntp_sec;
            ntp_ = ntp_ << 32 | (uint64_t)ntp_frac;
            timestamp_ = rtp_timestamp;
            ntpValid_ = true;
        }
        
        uint64_t convert(uint32_t timestamp, uint32_t clockrate) const{
            if(!ntpValid_){
                return 0;
            }
            int32_t delta_ts = (int32_t)(timestamp - timestamp_);
            uint64_t ntp = ntp_ + NNtp::DeltaTimestamp2NTP(delta_ts, clockrate);
            return ntp;
        }
        
        uint64_t ntpts() const {
            return ntp_;
        }
        
        uint32_t rtpts() const {
            return timestamp_;
        }
        
    private:
        bool ntpValid_          = false;
        uint64_t ntp_           = 0;
        uint32_t timestamp_     = 0;
    };
    
    class LocalNTP{
    public:
        uint64_t input(int64_t now_ms, uint32_t timestamp, uint32_t clockrate){
            if(lastNTP_ == 0){
                lastNTP_ = NNtp::Milliseconds2NTP(now_ms);
                lastTS_ = timestamp;
            }else{
                int32_t delta_ts = (int32_t)(timestamp - lastTS_);
                lastNTP_ += NNtp::DeltaTimestamp2NTP(delta_ts, clockrate);
                lastTS_ = timestamp;
            }
            return lastNTP_;
        }
    public:
        uint64_t lastNTP_ = 0;
        uint32_t lastTS_ = 0;
    };
    
public:
    NRTPTime()
    : localNTP_(local_.lastNTP_){}
    
    uint64_t input(int64_t now_ms, uint32_t timestamp, uint32_t clockrate, uint64_t remote_ntp){
        local_.input(now_ms, timestamp, clockrate);
        remoteNTP_ = remote_ntp;
        return localNTP_;
    }
    
    uint64_t estimate(const NRTPTime& other){
        int64_t diff = 0;
        return estimate(other, diff);;
    }
    
    uint64_t estimate(const NRTPTime& other, int64_t &diff){
        if(remoteNTP_ > 0 && other.remoteNTP_ > 0){
            int64_t remote_distance = (int64_t) (remoteNTP_ - other.remoteNTP_);
            int64_t local_distance  = (int64_t) (localNTP_ - other.localNTP_);
            diff = (remote_distance - local_distance);
            localNTP_ = localNTP_ + diff/4;
        }else{
            diff = 0;
        }
        return localNTP_;
    }
    
    uint64_t localNTP() const{
        return local_.lastNTP_;
    }
    
    uint64_t remoteNTP() const{
        return remoteNTP_;
    }
    
    uint32_t rtpTimestamp() const{
        return local_.lastTS_;
    }
    
private:
    LocalNTP local_;
    uint64_t remoteNTP_ = 0;
    uint64_t& localNTP_;
};

#endif /* NRTPTime_hpp */
