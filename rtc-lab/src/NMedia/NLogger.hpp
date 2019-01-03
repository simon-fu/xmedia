

#ifndef NLogger_hpp
#define NLogger_hpp

#include <stdio.h>
#include <iostream>
#define FMT_HEADER_ONLY
#define FMT_STRING_ALIAS 1
#include <spdlog/fmt/bundled/format.h>
#include "spdlog/fmt/bundled/ostream.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/fmt/bin_to_hex.h"


class NObjDumper{
#define EQUSTR  "="
#define OBJBSTR  "{{"
#define OBJESTR  "}}"
    
    static const char kDIV = ',';
    static const int  kIndent = 2;
    
public:
    class Dumpable{
    public:
        virtual NObjDumper& dump(NObjDumper&& dumper) const {
            return dump(dumper);
        }
        
        virtual NObjDumper& dump(NObjDumper& dumper) const = 0;
    };
    
public:
    inline NObjDumper(const std::string prefix)
    :prefix_(prefix){
        
    }
    
    virtual ~NObjDumper(){
        
    }
    
    inline void checkChar(char c){
        if(c) fmt::format_to(mbuffer_, "{}", c);
    }
    
    template <class T>
    inline NObjDumper& kv(const std::string& name, const T &value) {
        checkIndent();
        checkChar(div_);
        fmt::format_to(mbuffer_, "{}" EQUSTR "{}", name, value);
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
        fmt::format_to(mbuffer_, "{}" EQUSTR, name);
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
        fmt::format_to(mbuffer_, "{}" EQUSTR OBJBSTR, name);
        div_ = '\0';
        NObjDumper& dumper = *this;
        if(v.size() > 0){
            newlineIndent_ +=kIndent;
            endl();
            dump(v);
            newlineIndent_ -=kIndent;
        }
        //os_ << kOBJE;
        fmt::format_to(mbuffer_, OBJESTR);
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
    NObjDumper& v(const T &value){
        checkIndent();
        fmt::format_to(mbuffer_, "{}", value);
        return *this;
    }
    
    inline NObjDumper& objB(){
        checkIndent();
        fmt::format_to(mbuffer_, OBJBSTR);
        return *this;
    }
    
    inline NObjDumper& objE(){
        checkIndent();
        fmt::format_to(mbuffer_, OBJESTR);
        return *this;
    }
    
    virtual inline void onEndl() = 0;
    
    inline NObjDumper& endl(){
        checkChar(div_);
        static char c = '\0';
        mbuffer_.append(&c, (&c)+1);
        onEndl();
        mbuffer_.clear();
        
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
            fmt::format_to(mbuffer_, "{}", prefix_);
            prefixPos_ = prefix_.length();
        }
        
        for(; indentPos_ < indent_; ++indentPos_){
            fmt::format_to(mbuffer_, " ");
        }
        return *this;
    }
    
protected:
    std::string prefix_;
    size_t prefixPos_ = 0;
    //std::ostream& os_;
    char div_ = '\0';
    int indent_ = 0;
    int indentPos_ = 0;
    int newlineIndent_ = 0;
    fmt::memory_buffer mbuffer_;
};

class NObjDumperOStream : public NObjDumper{
public:
    inline NObjDumperOStream(std::ostream& os, const std::string prefix = "")
    :os_(os), NObjDumper(prefix){
        
    }
    
    virtual ~NObjDumperOStream(){
        
    }
    
    virtual inline void onEndl() override{
        os_ << mbuffer_.data() << std::endl;
    }
    
protected:
    std::ostream& os_;
};

class NObjDumperLog : public NObjDumper{
public:
    inline NObjDumperLog(spdlog::logger &logger
                         , spdlog::level::level_enum level = spdlog::level::info
                         , const std::string prefix = "")
    :logger_(logger), level_(level), NObjDumper(prefix){
        
    }
    
    virtual ~NObjDumperLog(){
        if(mbuffer_.size() > 0){
            endl();
        }
    }
    
    virtual inline void onEndl() override{
        // TODO: optimize
        logger_.log(level_, "{}", mbuffer_.data());
    }
    
protected:
    spdlog::logger &logger_;
    spdlog::level::level_enum level_;
};


class NLogger{
public:
    typedef std::shared_ptr<spdlog::logger> shared;
    typedef spdlog::level::level_enum Level;
    static shared Get(const std::string name){
        auto logger =  spdlog::get(name);
        if(!logger){
            logger = spdlog::stdout_logger_mt(name);
        }
        return logger;
    }
};

class NObjDumperTester{
public:
    int test();
};

#endif /* NLogger_hpp */
