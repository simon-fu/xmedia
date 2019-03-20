

#ifndef XProtoRapidJson_hpp
#define XProtoRapidJson_hpp

#include <stdio.h>
#include <string>
#include <vector>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
#define MAKE_TYPE_FLAG(t) (1 << (t))
#define MK_ITER_CSTR(iter) std::string((iter)->value.GetString(), (iter)->value.GetStringLength())
#define MK_ITER_INT(iter) (iter)->value.GetInt()

class XProtoField{
public:
    XProtoField(const std::string& name, bool mandatory, uint32_t typflag)
    : XProtoField(name, mandatory, typflag, -1, "")
    { }
    XProtoField(const std::string& name, bool mandatory, uint32_t typflag, int intvalue)
    : XProtoField(name, mandatory, typflag, intvalue, "")
    { }
    XProtoField(const std::string& name, bool mandatory, uint32_t typflag, const std::string& strvalue)
    : XProtoField(name, mandatory, typflag, -1, strvalue)
    { }
    XProtoField(const std::string& name, bool mandatory, uint32_t typflag, int intvalue, const std::string& strvalue)
    : name_(name), mandatory_(mandatory), typflag_(typflag), intvalue_(intvalue), strvalue_(strvalue), isNull_(true)
    { }
    
    int getInt()const{
        return iter_->value.GetInt();
    }
    
    int getUint64()const{
        return iter_->value.GetUint64();
    }
    
    int getInt64()const{
        return iter_->value.GetInt64();
    }
    
    const std::string name_;
    bool mandatory_;
    const uint32_t typflag_;
    Value::ConstMemberIterator iter_;
    std::string strvalue_;
    int intvalue_;
    bool isNull_;
};

class XProtoBase{
public:
    XProtoBase(const std::string& name):name_(name){}
    virtual ~XProtoBase(){}
    virtual std::vector<XProtoField*>* getFields(){
        return &fields_;
    }
    virtual const std::string * getName(){
        return &name_;
    }
    Document& getDoc(){
        return doc_;
    }
    
    virtual bool parseField(const Value &doc_, XProtoField* f, std::ostringstream& oss_err);
    virtual int parse(const Value &doc_, std::ostringstream& oss_err);
    int parse(const char * json, int json_len, std::ostringstream& oss_err);
    int parse(const std::string& str, std::ostringstream& oss_err);
    int parse(XProtoField& field, std::ostringstream& oss_err);
    
protected:
    std::string name_;
    std::vector<XProtoField*>fields_;
    Document doc_;
};

#endif /* XProtoRapidJson_hpp */
