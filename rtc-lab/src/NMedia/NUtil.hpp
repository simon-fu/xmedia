

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
    
    static inline uint32_t le_get4(const uint8_t *data,size_t i) {
        return (uint32_t)(data[i+0])
        | ((uint32_t)(data[i+1]))<<8
        | ((uint32_t)(data[i+2]))<<16
        | ((uint32_t)(data[i+3]))<<24;
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


// see https://embeddedartistry.com/blog/2017/4/6/circular-buffers-in-cc#c-
// see https://github.com/embeddedartistry/embedded-resources/blob/master/examples/cpp/circular_buffer.cpp
// see https://github.com/rlogiacco/CircularBuffer/blob/master/CircularBuffer.tpp

typedef size_t NCircularBufferSizeT ;
template<typename T>
class NCircularBuffer {
public:
    typedef T                                       value_type;
    typedef value_type&                             reference;
    typedef const value_type&                       const_reference;
    typedef NCircularBufferSizeT                    size_type;
public:
    
    NCircularBuffer(T *buf, NCircularBufferSizeT capacity)
    :buffer_(buf), capacity_(capacity)
    , head_(buffer_), tail_(buffer_), count_(0){

    }
    
    virtual ~NCircularBuffer(){
        
    }
    
    /**
     * Adds an element to the beginning of buffer;
     * returns `false` if the addition caused overwriting an existing element.
     */
    bool pushFront(value_type&& value) {
        bool empty_slot = true;
        empty_slot = unshiftFront();
        front() = value;
        return empty_slot;
    }
    
    bool unshiftFront() {
        if (head_ == buffer_) {
            head_ = buffer_ + capacity_;
        }
        --head_;
        if (count_ == capacity_) {
            if (tail_-- == buffer_) {
                tail_ = buffer_ + capacity_ - 1;
            }
            return  false;
        } else {
            if (count_++ == 0) {
                tail_ = head_;
            }
            return true;
        }
    }
    
    bool unshiftFront(size_type num, const_reference value) {
        bool empty_slot = true;
        for(size_type n = 0; n < num; ++n){
            empty_slot = pushFront(value);
        }
        return empty_slot;
    }
    
    bool unshiftFront(size_type num) {
        bool empty_slot = true;
        for(size_type n = 0; n < num; ++n){
            empty_slot = unshiftFront();
        }
        return empty_slot;
    }
    
    
    /**
     * Adds an element to the end of buffer;
     * returns `false` if the addition caused overwriting an existing element.
     */
    size_type pushBack(const_reference value){
        auto over_num = shiftBack();
        back() = value;
        return over_num;
    }
    

    size_type shiftBack(){
        if (++tail_ == buffer_ + capacity_) {
            tail_ = buffer_;
        }

        if (count_ == capacity_) {
            if (++head_ == buffer_ + capacity_) {
                head_ = buffer_;
            }
            return 1;
        } else {
            if (count_++ == 0) {
                head_ = tail_;
            }
            return 0;
        }
    }
    
    size_type shiftBack(size_type num, const_reference value) {
        size_type over_num = 0;
        for(size_type n = 0; n < num; ++n){
            over_num += pushBack(value);
        }
        return over_num;
    }
    
    size_type shiftBack(size_type num) {
        size_type over_num = 0;
        for(size_type n = 0; n < num; ++n){
            over_num += shiftBack();
        }
        return over_num;
    }
    
    
    /**
     * Removes an element from the beginning of the buffer.
     */
    void popFront(){
        shiftFront();
    }
    
    void popFront(size_type num){
        for(size_type n = 0; n < num; ++n){
            shiftFront();
        }
    }
    
    reference shiftFront(){
        reference ref = *head_;
        if (count_ > 0) {
            head_++;
            if (head_ >= buffer_ + capacity_) {
                head_ = buffer_;
            }
            count_--;
        }else{
            //void(* crash) (void) = 0;
            //crash();
        }
        return ref;
    }
    
    
    /**
     * Removes an element from the end of the buffer.
     */
    void popBack(){
        unshiftBack();
    }
    
    void popBack(size_type num){
        for(size_type n = 0; n < num; ++n){
            unshiftBack();
        }
    }
    
    reference unshiftBack(){
        reference ref = *tail_;
        if (count_ > 0) {
            tail_--;
            if (tail_ < buffer_) {
                tail_ = buffer_ + capacity_ - 1;
            }
            count_--;
        }else{
            //void(* crash) (void) = 0;
            //crash();
        }
        return ref;
    }
    
    /**
     * Returns the element at the beginning of the buffer.
     */
    reference inline front() {
        return *head_;
    }
    
    const_reference inline front() const{
        return *head_;
    }
    
    /**
     * Returns the element at the end of the buffer.
     */
    reference inline back() {
        return *tail_;
    }
    
    const_reference inline back() const{
        return *tail_;
    }
    
    /**
     * Array-like access to buffer
     */
    inline
    reference operator [] (NCircularBufferSizeT index) {
        return *(buffer_ + ((head_ - buffer_ + index) % capacity_));
    }
    
    inline
    const_reference operator [] (NCircularBufferSizeT index) const{
        return *(buffer_ + ((head_ - buffer_ + index) % capacity_));
    }
    
    /**
     * Returns how many elements are actually stored in the buffer.
     */
    NCircularBufferSizeT inline size() const{
        return count_;
    }
    
    /**
     * Returns how many elements can be safely pushed into the buffer.
     */
    NCircularBufferSizeT inline available() const{
        return capacity_ - count_;
    }
    
    /**
     * Returns how many elements can be potentially stored into the buffer.
     */
    NCircularBufferSizeT inline capacity() const{
        return capacity_;
    }
    
    /**
     * Returns `true` if no elements can be removed from the buffer.
     */
    bool inline isEmpty() const{
        return count_ == 0;
    }
    
    /**
     * Returns `true` if no elements can be added to the buffer without overwriting existing elements.
     */
    bool inline isFull() const{
        return count_ == capacity_;
    }
    
    /**
     * Resets the buffer to a clean status, making all buffer positions available.
     */
    void inline clear() {
        head_ = tail_ = buffer_;
        count_ = 0;
    }
    
    typedef  std::function<int (NCircularBufferSizeT , reference )> TraverseFuncT;
    inline int traverse (NCircularBufferSizeT startIndex
                        , NCircularBufferSizeT num
                        , const TraverseFuncT& func) {
        
        for(NCircularBufferSizeT i = startIndex, n = 0
            ; i < size() && n < num
            ; ++i, ++n){
            int state = func(n, (*this)[i]);
            if(state != 0){
                return state;
            }
        }
        return 0;
    }
    
    inline int traverse (NCircularBufferSizeT startIndex
                  , const TraverseFuncT& func) {
        
        for(NCircularBufferSizeT i = startIndex, n = 0
            ; i < size()
            ; ++i, ++n){
            int state = func(n, (*this)[i]);
            if(state != 0){
                return state;
            }
        }
        return 0;
    }
    
protected:
    T *buffer_;
    T *head_;
    T *tail_;
    NCircularBufferSizeT capacity_;
    NCircularBufferSizeT count_;
    
};

template<typename T>
class NCircularVector : public NCircularBuffer<T> {
public:
    NCircularVector(NCircularBufferSizeT capacity)
    :NCircularBuffer<T>(new T[capacity], capacity){
        
    }
    
    NCircularVector(const NCircularVector&other)
    :NCircularVector(other.capacity_){
        
    }
    
    virtual ~NCircularVector(){
        delete[] this->buffer_;
    }
};

template<typename T, NCircularBufferSizeT S>
class NCircularArray : public NCircularBuffer<T> {
public:
    NCircularArray():NCircularBuffer<T>(buffer_, S){
        
    }
private:
    T buffer_[S];
};


class NObjDumper{
public:
    class Dumpable{
    public:
        virtual NObjDumper& dump(NObjDumper&& dumper) const {
            return dump(dumper);
        }
        
        virtual NObjDumper& dump(NObjDumper& dumper) const = 0;
    };
    
public:
    inline NObjDumper(std::ostream& os):NObjDumper(os, ""){
        
    }
    
    inline NObjDumper(std::ostream& os, const std::string prefix)
    :os_(os), prefix_(prefix){
        
    }
    
    
    
    ~NObjDumper(){
        
    }
    
    inline void checkChar(char c){
        if(c) os_ << c;
    }
    
    template <class T>
    inline NObjDumper& kv(const std::string& name, const T &value) {
        checkIndent();
        checkChar(div_);
        os_ << name << kEQU << value;
        div_ = kDIV;
        return *this;
    }
    
    template <class T>
    inline NObjDumper& dump(const std::string& name, const T &value){
        return dump(name, value, kIndent);
    }
    
    template <class T>
    inline NObjDumper& dump(const std::string& name, const T &value, int newlint_indent){
        checkIndent();
        checkChar(div_);
        os_ << name << kEQU;
        div_ = '\0';
        newlineIndent_ +=newlint_indent;
        auto& o = value.dump(*this);
        newlineIndent_ -=newlint_indent;
        o.div_ = kDIV;
        return o;
    }
    
    template <class T>
    inline NObjDumper& dump(const std::string& name, std::vector<T> v){
        checkIndent();
        checkChar(div_);
        os_ << name << kEQU;
        div_ = '\0';
        os_ << kOBJB;
        NObjDumper& dumper = *this;
        if(v.size() > 0){
            newlineIndent_ +=kIndent;
            endl();
            dump(v);
            newlineIndent_ -=kIndent;
        }
        os_ << kOBJE;
        div_ = kDIV;
        return dumper;
    }
    
    template <class T>
    inline NObjDumper& dump(std::vector<T> v){
        NObjDumper& dumper = *this;
        int n = 0;
        for(auto& o : v){
            ++n;
            char str[64];
            sprintf(str, "No[%d/%zu]", n, v.size());
            dumper.dump(str, o).endl();
        }
        return dumper;
    }
    
    template <class T>
    NObjDumper& dump(const T &value){
        checkIndent();
        return value.dump(*this);
    }
    
    
    template <class T>
    NObjDumper v(const T &value){
        checkIndent();
        os_ << value;
        return *this;
    }
    
    inline NObjDumper& objB(){
        checkIndent();
        os_ << kOBJB;
        return *this;
    }
    
    inline NObjDumper& objE(){
        checkIndent();
        os_ << kOBJE;
        return *this;
    }
    
    inline NObjDumper& endl(){
        checkChar(div_);
        os_ << std::endl;
        div_ = '\0';
        indentPos_ = 0;
        prefixPos_ = 0;
        if(newlineIndent_ > 0){
            indent_ += newlineIndent_;
            checkIndent();
            indent_ -= newlineIndent_;
        }
        return *this;
    }
    
    NObjDumper& indent(int n = kIndent){
        indent_ += n;
        return *this;
    }
    
    NObjDumper& uindent(int n = kIndent){
        indent_ -= n;
        return *this;
    }
    
    inline NObjDumper& checkIndent(){
        if(prefixPos_ < prefix_.length()){
            os_ << prefix_;
            prefixPos_ = prefix_.length();
        }
        
        for(; indentPos_ < indent_; ++indentPos_){
            os_ << " ";
        }
        return *this;
    }
    
private:
    static const char kOBJB = '{';
    static const char kOBJE = '}';
    static const char kDIV = ',';
    static const char kEQU = '=';
    static const int  kIndent = 2;
    std::string prefix_;
    size_t prefixPos_ = 0;
    std::ostream& os_;
    char div_ = '\0';
    int indent_ = 0;
    int indentPos_ = 0;
    int newlineIndent_ = 0;
};

class NObjDumperTester{
public:
    class TestA : public NObjDumper::Dumpable{
    public:
        TestA(int v):v_(v), d_(v){}
        friend inline std::ostream& operator<<(std::ostream& out, const TestA& o){
            NObjDumper(out).dump(o);
            return out;
        }
        
        virtual inline NObjDumper& dump(NObjDumper&& dumper) const override{
            return dump(dumper);
        }
        
        virtual inline NObjDumper& dump(NObjDumper& dumper) const override{
            dumper.objB().kv("v", v_).kv("d", d_).objE();
            return dumper;
        }
        
    private:
        int v_;
        int d_;
    };
    
    class TestB{
    public:
        TestB(int v1, int v2)
        :int1_(v1), int2_(v2) ,a1_(v1), a2_(v2){}
        friend inline std::ostream& operator<<(std::ostream& out, const TestB& o){
            NObjDumper(out).dump(o);
            return out;
        }
        
        inline NObjDumper& dump(NObjDumper&& dumper) const{
            return dump(dumper);
        }
        
        inline NObjDumper& dump(NObjDumper& dumper) const{
            return dumper.objB().dump("a1", a1_).kv("int1", int1_).kv("int2", int2_).endl()
            .dump("a1", a1_).endl()
            .dump("a2", a2_).endl()
            .objE();
        }
        
    private:
        int int1_;
        int int2_;
        TestA a1_;
        TestA a2_;
    };
    
    class TestC{
    public:
        TestC(int v1, int v2)
        :b1_(v1, 1), b2_(v2, 2){
            b3_.emplace_back(++v1, ++v2);
            b3_.emplace_back(++v1, ++v2);
            b3_.emplace_back(++v1, ++v2);
            b3_.emplace_back(++v1, ++v2);
            b3_.emplace_back(++v1, ++v2);
            b3_.emplace_back(++v1, ++v2);
        }
        
        inline NObjDumper& dump(NObjDumper&& dumper) const{
            return dump(dumper);
        }
        
        inline NObjDumper& dump(NObjDumper& dumper) const{
            dumper.objB().endl();
            dumper.dump("b1", b1_).endl();
            dumper.dump("b2", b2_).endl();
            dumper.dump("b3", b3_).endl();
            dumper.objE();
            return dumper;
        }
        
    private:
        TestB b1_;
        TestB b2_;
        std::vector<TestB> b3_;
    };
    
    int test(){
        std::ostream &out = std::cout;
        TestB b(1,2);
        
        NObjDumper(out).dump("bbb1", b).endl();
        NObjDumper(out).dump("bbb-2", b).endl();
        
        TestC c(77,88);
        NObjDumper(out).dump("cccc1", c).endl();
        NObjDumper(out).dump("cccc2", c).endl();
        return 0;
    }
};



//class NDumper{
//public:
//    //virtual inline NDumper& indent(int n) = 0;
//    virtual inline std::ostream& begin() = 0;
//    virtual inline std::ostream& begin(const char *str) = 0;
//    virtual inline void end() = 0;
//};
//
//
//class NCoutDumper : public NDumper{
//public:
//    NCoutDumper(const char * prefix){
//        prefix_ = prefix;
//    }
//
////    virtual inline NDumper& indent(int n) override{
////        indent_ += n;
////        return *this;
////    }
//
////    virtual inline NDumper& indentAs(const char *str) {
////        std::cout << str;
////        indent_ += strlen(str);
////        return *this;
////    }
//
//    virtual inline std::ostream& begin() override{
//        std::cout << prefix_;
//        for(int i = 0; i < indent_; ++i){
//            std::cout << " ";
//        }
//        return std::cout;
//    }
//
//    virtual inline std::ostream& begin(const char *str) override{
//        auto& os = begin();
//        return os;
//    }
//
//    virtual inline void end() override{
//        std::cout << std::endl;
//    }
//
//    static NCoutDumper& get(const char * prefix){
//        static NCoutDumper dumper("");
//        dumper.prefix_ = prefix;
//        return dumper;
//        //return NCoutDumper(prefix);
//    }
//private:
//    int indent_ = 0;
//    const char * prefix_ = "";
//};



//template <class T>
//class NCircularBuffer1 {
//public:
//    explicit NCircularBuffer1(size_t size) :
//    buf_(new T[size]),
//    max_size_(size){
//
//    }
//
//    virtual ~NCircularBuffer1(){
//        delete[] buf_;
//    }
//
//
//    void put(T item){
//
//        buf_[head_] = item;
//
//        if(full_)
//        {
//            tail_ = (tail_ + 1) % max_size_;
//        }
//
//        head_ = (head_ + 1) % max_size_;
//
//        full_ = head_ == tail_;
//    }
//
//    T get()
//    {
//
//        if(empty())
//        {
//            return T();
//        }
//
//        //Read data and advance the tail (we now have a free space)
//        auto val = buf_[tail_];
//        full_ = false;
//        tail_ = (tail_ + 1) % max_size_;
//
//        return val;
//    }
//
//    void reset()
//    {
//        head_ = tail_;
//        full_ = false;
//    }
//
//    bool empty() const
//    {
//        //if head and tail are equal, we are empty
//        return (!full_ && (head_ == tail_));
//    }
//
//    bool full() const
//    {
//        //If tail is ahead the head by 1, we are full
//        return full_;
//    }
//
//    size_t capacity() const
//    {
//        return max_size_;
//    }
//
//    size_t size() const
//    {
//        size_t size = max_size_;
//
//        if(!full_)
//        {
//            if(head_ >= tail_)
//            {
//                size = head_ - tail_;
//            }
//            else
//            {
//                size = max_size_ + head_ - tail_;
//            }
//        }
//
//        return size;
//    }
//
//private:
//    T * buf_;
//    size_t head_ = 0;
//    size_t tail_ = 0;
//    const size_t max_size_;
//    bool full_ = 0;
//};
//
//
//
//
//
//template <class T, class D = std::default_delete<T>>
//class SmartObjectPool
//{
//private:
//    struct ReturnToPool_Deleter {
//        explicit ReturnToPool_Deleter(std::weak_ptr<SmartObjectPool<T, D>* > pool)
//        : pool_(pool) {}
//
//        void operator()(T* ptr) {
//            if (auto pool_ptr = pool_.lock()){
//                (*pool_ptr.get())->add(std::unique_ptr<T, D>{ptr});
//            }else{
//                D{}(ptr);
//            }
//
//        }
//    private:
//        std::weak_ptr<SmartObjectPool<T, D>* > pool_;
//    };
//
//public:
//    using ptr_type = std::unique_ptr<T, ReturnToPool_Deleter >;
//
//    SmartObjectPool() : this_ptr_(new SmartObjectPool<T, D>*(this)) {}
//    virtual ~SmartObjectPool(){}
//
//    void add(std::unique_ptr<T, D> t) {
//        pool_.push(std::move(t));
//    }
//
//    ptr_type acquire() {
//        //        if (pool_.empty()){
//        //            throw std::out_of_range("Cannot acquire object from an empty pool.");
//        //        }
//
//        if (pool_.empty()){
//            auto t = std::unique_ptr<T>(new T());
//            pool_.push(std::move(t));
//        }
//
//
//        ptr_type tmp(pool_.top().release(),
//                     ReturnToPool_Deleter{
//                         std::weak_ptr<SmartObjectPool<T, D>*>{this_ptr_}});
//        pool_.pop();
//        return std::move(tmp);
//    }
//
//    bool empty() const {
//        return pool_.empty();
//    }
//
//    size_t size() const {
//        return pool_.size();
//    }
//
//private:
//    std::shared_ptr<SmartObjectPool<T, D>* > this_ptr_;
//    std::stack<std::unique_ptr<T, D> > pool_;
//};

#endif /* NUtil_hpp */
