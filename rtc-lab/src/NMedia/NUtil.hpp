

#ifndef NUtil_hpp
#define NUtil_hpp

#include <stdio.h>
#include <string>
#include <stack>
#include <list>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <thread>
#include "spdlog/fmt/bundled/format.h"

#define DEFAULT_END_CHARS "\r\n"
class NLineParser{
    //static constexpr const char * DEFAULT_END_CHARS = "\r\n";
    
    const char * line_begin_;
    const char * line_end_;
    size_t line_length_;
    const char * curr_p_;
    const char * end_chars_;
private:
    bool checkEnd(const char *p){
        return *p == '\0' || p >= line_end_ || EndChar(*p);
    }
    
public:
    NLineParser(const char * line_begin)
    : NLineParser(line_begin, FindLineEnd(line_begin, DEFAULT_END_CHARS)){
        
    }
    
    NLineParser(const char * line_begin, size_t length)
    : NLineParser(line_begin, line_begin+length){
        
    }
    
    NLineParser(const std::string& str)
    : NLineParser(str.c_str(), str.length()){
        
    }
    
    NLineParser(const char * line_begin, const char * line_end)
    : NLineParser(line_begin, line_end, DEFAULT_END_CHARS){
        
    }
    
    NLineParser(const char * line_begin, const char * line_end, const char * end_chars)
    : line_begin_(line_begin)
    , line_end_(line_end)
    , line_length_(line_end-line_begin)
    , curr_p_(line_begin)
    , end_chars_(end_chars) {
        
    }
    
    
    //    static inline bool EndChar(char c){
    //        return  c == '\r' || c == '\n';
    //    }
    
    static inline bool EndChar(char c){
        return  EndChar(c, DEFAULT_END_CHARS);
    }
    
    static inline bool EndChar(char c, const char * end_chars){
        auto p = end_chars;
        for(; *p; ++p){
            if(*p == c){
                return true;
            }
        }
        return false;
    }
    
    static inline bool SpaceChar(char c){
        return (c == ' ' || c == '\t');
    }
    
    //    static
    //    const char * FindLineEnd(const char * p){
    //        while(*p && !EndChar(*p)){
    //            p++;
    //        }
    //        return p;
    //    }
    //
    //    static
    //    const char * FindLineEnd(const char * p, const char * end){
    //        while(*p && p < end && !EndChar(*p)){
    //            p++;
    //        }
    //        return p;
    //    }
    
    //    static
    //    const char * FindNoneLineEnd(const char * p){
    //        while(*p && EndChar(*p)){
    //            p++;
    //        }
    //        return p;
    //    }
    //
    //    static
    //    const char * FindNoneLineEnd(const char * p, const char * end){
    //        while(*p && p < end && EndChar(*p)){
    //            p++;
    //        }
    //        return p;
    //    }
    
    
    static
    const char * FindLineEnd(const char * p, const char * end_chars){
        while(*p && !EndChar(*p, end_chars)){
            p++;
        }
        return p;
    }
    
    static
    const char * FindLineEnd(const char * p, const char * end, const char * end_chars){
        while(*p && p < end && !EndChar(*p, end_chars)){
            p++;
        }
        return p;
    }
    
    static
    const char * FindNoneLineEnd(const char * p, const char * end_chars){
        while(*p && EndChar(*p, end_chars)){
            p++;
        }
        return p;
    }
    
    static
    const char * FindNoneLineEnd(const char * p, const char * end, const char * end_chars){
        while(*p && p < end && EndChar(*p, end_chars)){
            p++;
        }
        return p;
    }
    
    static
    uint32_t StrToU32(const std::string& str){
        uint32_t val = static_cast<uint32_t>(atol(str.c_str()));
        return val;
    }
    
    const char * begin(){
        return line_begin_;
    }
    
    const char * end(){
        return line_end_;
    }
    
    
    size_t length(){
        return line_length_;
    }
    
    size_t remains(){
        return (int)(line_end_ - curr_p_);
    }
    
    const char * findNoneSpace(const char *p, const char delimiter='\n'){
        while(!checkEnd(p)
              && (SpaceChar(*p) || *p ==delimiter)
              ){
            p++;
        }
        return p;
    }
    
    const char * findSpace(const char *p, const char delimiter){
        while(!checkEnd(p) && !SpaceChar(*p) && *p !=delimiter){
            p++;
        }
        return p;
    }
    
    bool nextWith(const std::string& prefix){
        const char * s = curr_p_;
        const char * d = prefix.c_str();
        while(!checkEnd(s) && *d && *s == *d ){
            ++s;
            ++d;
        }
        if(*d){
            return false;
        }
        curr_p_ = s;
        return true;
    }
    
    bool nextWordWith(const std::string& prefix){
        curr_p_ = findNoneSpace(curr_p_);
        return nextWith(prefix);
    }
    
    bool nextChar(char& c){
        if(!checkEnd(curr_p_)){
            c = *curr_p_++;
            return true;
        }else{
            return false;
        }
    }
    
    bool nextWord(std::string& word, const char delimiter='\n'){
        const char * next_word = findNoneSpace(curr_p_, delimiter);
        const char * next_sp = findSpace(next_word, delimiter);
        size_t len = (next_sp-next_word);
        if(len > 0){
            word = std::string(next_word, len);
            curr_p_ = next_sp;
            return true;
        }else{
            return false;
        }
    }
    
    bool nextU32(uint32_t& val, const char delimiter='\n'){
        std::string str;
        if(nextWord(str, delimiter)){
            //val = static_cast<uint32_t>(atol(str.c_str()));
            val = StrToU32(str);
            return true;
        }
        return false;
    }
    
    bool nextWord2End(std::string& word, const char delimiter='\n'){
        const char * next_word = findNoneSpace(curr_p_, delimiter);
        const char * next_sp = line_end_;
        size_t len = (next_sp-next_word);
        if(len > 0){
            word = std::string(next_word, len);
            curr_p_ = next_sp;
            return true;
        }else{
            return false;
        }
    }
    
};

class LinesBuffer{
    const char * begin_;
    const char * end_;
    const char * p_;
    NLineParser line_;
    size_t lineNum_ = 0;
    std::string endChars_;
public:
    LinesBuffer(const char * begin, const char * end)
    :begin_(begin), end_(end), p_(begin), line_(begin, begin), endChars_("\r\n"){
        
    }
    
    LinesBuffer(const char * begin, size_t size)
    :LinesBuffer(begin, begin+size){
        
    }
    
    LinesBuffer(const char * begin)
    :LinesBuffer(begin, strlen(begin)){
        
    }
    
    void setLineEndChars(const char * end_chars){
        endChars_ = end_chars;
    }
    
    size_t lineNum(){
        return lineNum_;
    }
    
    NLineParser& line(){
        return line_;
    }
    
    //    bool next(){
    //        p_ = line_.end();
    //        if(p_ != begin_){
    //            p_ = NLineParser::FindNoneLineEnd(p_, end_);
    //        }
    //        line_ = NLineParser(p_, NLineParser::FindLineEnd(p_, end_)-p_);
    //        bool b = *p_ && line_.begin() < end_;
    //        if(b){
    //            ++lineNum_;
    //        }
    //        return b;
    //    }
    
    bool next(){
        return next(DEFAULT_END_CHARS);
    }
    
    bool next(const char * end_chars){
        p_ = line_.end();
        if(p_ != begin_){
            p_ = NLineParser::FindNoneLineEnd(p_, end_, end_chars);
        }
        line_ = NLineParser(p_, NLineParser::FindLineEnd(p_, end_, end_chars)-p_);
        bool b = *p_ && line_.begin() < end_;
        if(b){
            ++lineNum_;
        }
        return b;
    }
};


class NUtil{
public:
    static inline int64_t get_ms_since_1970(){
        unsigned long milliseconds_since_epoch =
        std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
        return milliseconds_since_epoch;
    }
    
    static inline int64_t get_ms_relative_monotonic(){
        typedef std::chrono::milliseconds Milli;
        typedef std::chrono::steady_clock Time;
        static auto start = Time::now();
        auto elapsed = Time::now() - start;
        return std::chrono::duration_cast<Milli> (elapsed).count();
    }
    
    static inline int64_t get_ms_monotonic(){
        typedef std::chrono::milliseconds Milli;
        typedef std::chrono::steady_clock Time;
        return std::chrono::duration_cast<Milli> (Time::now().time_since_epoch()).count();
    }
    
    static inline int64_t get_now_ms(){
        static int64_t base = get_ms_since_1970()-get_ms_monotonic();
        return base + get_ms_monotonic();
    }
    
    static inline void sleep_ms(int64_t ms){
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    
    static inline uint8_t  get1(const uint8_t *data,size_t i) {
        return data[i];
    }
    
    static inline uint32_t get2(const uint8_t *data,size_t i) {
        return (uint32_t)(data[i+1]) | ((uint32_t)(data[i]))<<8;
    }
    
    static inline uint32_t get3(const uint8_t *data,size_t i){
        return (uint32_t)(data[i+2])
        | ((uint32_t)(data[i+1]))<<8
        | ((uint32_t)(data[i]))<<16;
    }
    
    static inline uint32_t get4(const uint8_t *data,size_t i) {
        return (uint32_t)(data[i+3])
        | ((uint32_t)(data[i+2]))<<8
        | ((uint32_t)(data[i+1]))<<16
        | ((uint32_t)(data[i]))<<24;
    }
    
    static inline uint64_t get8(const uint8_t *data,size_t i) {
        return ((uint64_t)get4(data,i))<<32 | get4(data,i+4);
    }
    
    static inline void set1(uint8_t *data,size_t i,uint8_t val){
        data[i] = val;
    }
    
    static inline void set2(uint8_t *data,size_t i,uint32_t val){
        data[i+1] = (uint8_t)(val);
        data[i]   = (uint8_t)(val>>8);
    }
    
    static inline void set3(uint8_t *data,size_t i,uint32_t val){
        data[i+2] = (uint8_t)(val);
        data[i+1] = (uint8_t)(val>>8);
        data[i]   = (uint8_t)(val>>16);
    }
    
    static inline void set4(uint8_t *data,size_t i,uint32_t val){
        data[i+3] = (uint8_t)(val);
        data[i+2] = (uint8_t)(val>>8);
        data[i+1] = (uint8_t)(val>>16);
        data[i]   = (uint8_t)(val>>24);
    }
    
    static inline void set6(uint8_t *data,size_t i,uint64_t val){
        data[i+5] = (uint8_t)(val);
        data[i+4] = (uint8_t)(val>>8);
        data[i+3] = (uint8_t)(val>>16);
        data[i+2] = (uint8_t)(val>>24);
        data[i+1] = (uint8_t)(val>>32);
        data[i]   = (uint8_t)(val>>40);
    }
    
    static inline void set8(uint8_t *data,size_t i,uint64_t val){
        data[i+7] = (uint8_t)(val);
        data[i+6] = (uint8_t)(val>>8);
        data[i+5] = (uint8_t)(val>>16);
        data[i+4] = (uint8_t)(val>>24);
        data[i+3] = (uint8_t)(val>>32);
        data[i+2] = (uint8_t)(val>>40);
        data[i+1] = (uint8_t)(val>>48);
        data[i]   = (uint8_t)(val>>56);
    }
    
    static inline uint16_t le_get2(const uint8_t *data,size_t i) {
        return (uint16_t)(data[i+0])
        | ((uint16_t)(data[i+1]))<<8;
    }
    
    static inline uint32_t le_get4(const uint8_t *data,size_t i) {
        return (uint32_t)(data[i+0])
        | ((uint32_t)(data[i+1]))<<8
        | ((uint32_t)(data[i+2]))<<16
        | ((uint32_t)(data[i+3]))<<24;
    }
    
    static inline uint64_t le_get8(const uint8_t *data,size_t i) {
        return (uint64_t)(data[i+0])
        | ((uint64_t)(data[i+1]))<<8
        | ((uint64_t)(data[i+2]))<<16
        | ((uint64_t)(data[i+3]))<<24
        | ((uint64_t)(data[i+4]))<<32
        | ((uint64_t)(data[i+5]))<<40
        | ((uint64_t)(data[i+6]))<<48
        | ((uint64_t)(data[i+7]))<<56;
    }
    
    static inline void le_set2(uint8_t *data,size_t i,uint32_t val){
        data[i]     = (uint8_t)(val);
        data[i+1]   = (uint8_t)(val>>8);
    }
    
    static inline void le_set3(uint8_t *data,size_t i,uint32_t val){
        data[i]     = (uint8_t)(val);
        data[i+1]   = (uint8_t)(val>>8);
        data[i+2]   = (uint8_t)(val>>16);
    }
    
    static inline void le_set4(uint8_t *data,size_t i,uint32_t val){
        data[i]     = (uint8_t)(val);
        data[i+1]   = (uint8_t)(val>>8);
        data[i+2]   = (uint8_t)(val>>16);
        data[i+3]   = (uint8_t)(val>>24);
    }
    
    static inline void le_set6(uint8_t *data,size_t i,uint64_t val){
        data[i]     = (uint8_t)(val);
        data[i+1]   = (uint8_t)(val>>8);
        data[i+2]   = (uint8_t)(val>>16);
        data[i+3]   = (uint8_t)(val>>24);
        data[i+4]   = (uint8_t)(val>>32);
        data[i+5]   = (uint8_t)(val>>40);
    }
    
    static inline void le_set8(uint8_t *data,size_t i,uint64_t val){
        data[i]     = (uint8_t)(val);
        data[i+1]   = (uint8_t)(val>>8);
        data[i+2]   = (uint8_t)(val>>16);
        data[i+3]   = (uint8_t)(val>>24);
        data[i+4]   = (uint8_t)(val>>32);
        data[i+5]   = (uint8_t)(val>>40);
        data[i+6]   = (uint8_t)(val>>48);
        data[i+7]   = (uint8_t)(val>>56);
    }
    
    template<typename T>
    static inline T Pad32(T size){
        auto mod = size % 4;
        if(mod > 0){
            size = size + 4 - mod;
        }
        return size;
    }
    
    // see https://stackoverflow.com/questions/4244274/how-do-i-count-the-number-of-zero-bits-in-an-integer
    // in the book "Hackers Delight"
    // see https://github.com/hanji/popcnt/blob/master/populationcount.cpp
    static inline uint32_t count_1bits(uint32_t x)
    {
        x = x - ((x >> 1) & 0x55555555);
        x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
        x = x + (x >> 8);
        x = x + (x >> 16);
        return x & 0x0000003F;
    }
    
    static inline uint32_t count_0bits(uint32_t x)
    {
        return 32 - count_1bits(x);
    }
    
    static inline uint16_t count_leading_0bits(uint64_t v){
        return std::__clz(v);
    }
    
    static int LoadFile(const std::string& fname, uint8_t **buf, size_t * buf_size);
    static int ReadTLV(FILE *fp, unsigned char *buf, size_t bufsize, size_t *plength, int *ptype);
    
    // see https://stackoverflow.com/questions/28828957/enum-to-string-in-modern-c11-c14-c17-and-future-c20/31362042#31362042
    static void buildEnumMap(const char * enum_string, std::map<int, std::string>& m){
        int index = 0;
        LinesBuffer lines(enum_string);
        while(lines.next(",\r\n")){
            NLineParser& line = lines.line();
            std::string enum_name;
            while(line.nextWord(enum_name, '=')){
                std::string enum_value;
                if(line.nextWord(enum_value, '=')){
                    //index = atoi(enum_value.c_str());
                    index = static_cast<int>(std::stoll(enum_value, 0, 0));
                }
                m.insert(std::make_pair(index, enum_name));
                ++index;
            }
        }
    }
};

#define _DECLARE_ENUM(S,E,...)   \
S const std::string& getNameFor##E(int index){  \
    static const std::string unknown = "Unknown"#E;  \
    static std::map<int, std::string> enum_map;  \
    if(enum_map.size() == 0){  \
        NUtil::buildEnumMap(#__VA_ARGS__, enum_map);  \
    }  \
    auto it = enum_map.find(index);  \
    if(it != enum_map.end()){  \
        return it->second;  \
    }else{  \
        return unknown;  \
    } \
} \
enum E                              \
{                                   \
    __VA_ARGS__                     \
}                                  

#define DECLARE_CLASS_ENUM(E,...)   _DECLARE_ENUM(static, E, __VA_ARGS__)
#define DECLARE_GLOBAL_ENUM(E,...)  _DECLARE_ENUM(, E, __VA_ARGS__)




// see https://swarminglogic.com/jotting/2015_05_smartpool
template <class T, class D = std::default_delete<T>>
class NObjectPool
{
private:
    using PoolType = NObjectPool<T, D>;
    struct ReturnToPool_Deleter {
        explicit ReturnToPool_Deleter(std::weak_ptr<PoolType* > pool)
        : pool_(pool) {}
        
        void operator()(T* ptr) {
            if (auto pool_ptr = pool_.lock()){
                (*pool_ptr.get())->put(std::unique_ptr<T, D>{ptr});
            }else{
                D{}(ptr);
            }
            
        }
    private:
        std::weak_ptr<PoolType* > pool_;
    };
    
public:
    using Unique = std::unique_ptr<T, ReturnToPool_Deleter >;
    using CreateType = std::function<T *()>;
    
    static inline Unique MakeNullPtr(){
        return Unique(nullptr,
                      ReturnToPool_Deleter{
                          std::weak_ptr<PoolType*>{std::shared_ptr<PoolType* >(nullptr)}});
    }
public:
    NObjectPool()
    : this_ptr_(new PoolType*(this)) {}
    NObjectPool(const CreateType& creator)
    : this_ptr_(new PoolType*(this)), creator_(creator) {}
    
    virtual ~NObjectPool(){}
    
    void put(std::unique_ptr<T, D> t) {
        pool_.push(std::move(t));
    }
    
    Unique get() {
        if (pool_.empty()){
            auto t = std::unique_ptr<T>(creator_());
            pool_.push(std::move(t));
        }
        
        Unique tmp(pool_.top().release(),
                   ReturnToPool_Deleter{
                       std::weak_ptr<PoolType*>{this_ptr_}});
        pool_.pop();
        return std::move(tmp);
    }
    
    bool empty() const {
        return pool_.empty();
    }
    
    size_t size() const {
        return pool_.size();
    }
    
private:
    std::shared_ptr<PoolType* > this_ptr_;
    std::stack<std::unique_ptr<T, D> > pool_;
    const CreateType creator_ = []()->T *{
        return new T();
    };
};

template <class T, class BaseType=T>
class NPool : public NObjectPool<BaseType>{
public:
    using Parent = NObjectPool<BaseType>;
public:
    NPool():NObjectPool<BaseType>([]()->BaseType*{
        return new T();
    }){};
};

class NError{
public:
    NError(int errcode, const char *errstr)
    : errcode_(errcode), errstr_(errstr){
    }
    NError(int errcode, const std::string &errstr)
    : errcode_(errcode), errstr_(errstr){
    }
    int code()const { return errcode_;}
    const std::string& str()const { return errstr_;}
private:
    int errcode_;
    const std::string errstr_;
};

#define NERROR_FMT_SET(code, ...)\
NThreadError::setError(code, fmt::format(__VA_ARGS__))
class NThreadError{
public:
    static const std::string& lastMsg() {
        return msg_;
    }
    
    static int lastCode() {
        return code_;
    }
    
    static void setError(int code, const std::string& msg){
        code_ = code;
        msg_ = msg;
    }
    
private:
    static thread_local int code_ ;
    static thread_local std::string msg_;
};



#endif /* NUtil_hpp */
