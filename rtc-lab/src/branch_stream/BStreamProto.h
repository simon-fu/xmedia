
/*
 * branch stream protocol 
 */

#ifndef BStream_Proto_hpp
#define BStream_Proto_hpp

#include <stdio.h>
#include <map>
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




class XProtoBranchBase : public XProtoBase{
public:
    XProtoBranchBase(const std::string& name)
    : XProtoBase(name)
    , op_("op", false, MAKE_TYPE_FLAG(kStringType))
    , confrId_("confrId", false, MAKE_TYPE_FLAG(kStringType))
    , memName_("memName", false, MAKE_TYPE_FLAG(kStringType))
    , streamId_("streamId", false, MAKE_TYPE_FLAG(kStringType))
    , pubRtcId_("pubRtcId", false, MAKE_TYPE_FLAG(kStringType))
    , subRtcId_("subRtcId", false, MAKE_TYPE_FLAG(kStringType))
    , peerAudioSSRCs_("peerAudioSSRCs", false, MAKE_TYPE_FLAG(kArrayType))
    , peerVideoSSRCs_("peerVideoSSRCs", false, MAKE_TYPE_FLAG(kArrayType))
    , select_("select", false, MAKE_TYPE_FLAG(kNumberType)|MAKE_TYPE_FLAG(kFalseType)|MAKE_TYPE_FLAG(kTrueType), 1)
    {
        fields_.push_back(&op_);
        fields_.push_back(&confrId_);
        fields_.push_back(&memName_);
        fields_.push_back(&streamId_);
        fields_.push_back(&pubRtcId_);
        fields_.push_back(&subRtcId_);
        fields_.push_back(&peerAudioSSRCs_);
        fields_.push_back(&peerVideoSSRCs_);
        fields_.push_back(&select_);
    }
    XProtoField op_;
    XProtoField confrId_;
    XProtoField memName_;
    XProtoField streamId_;
    XProtoField pubRtcId_;
    XProtoField subRtcId_;
    XProtoField peerAudioSSRCs_;
    XProtoField peerVideoSSRCs_;
    XProtoField select_;
};

class XProtoSubR : public XProtoBranchBase{
public:
    XProtoSubR():XProtoBranchBase("subR"){
        op_.mandatory_ = true;
        confrId_.mandatory_ = true;
        pubRtcId_.mandatory_ = true;
    }
};

class XProtoUsubR : public XProtoBranchBase{
public:
    XProtoUsubR():XProtoBranchBase("usubR"){
        op_.mandatory_ = true;
        subRtcId_.mandatory_ = true;
        pubRtcId_.mandatory_ = true;
    }
};


template<class keyT, class ValueT>
ValueT map_get(std::map<keyT, ValueT> * m, keyT uid, bool is_remove, ValueT nullvalue){
    auto it = m->find(uid);
    if(it == m->end()){
        return nullvalue;
    }
    ValueT v = it->second;
    if(is_remove){
        m->erase(it);
    }
    return v;
}


template<class ValueT>
ValueT* map_get_p_by_str(std::map<std::string, ValueT*> * m, const std::string& uid, bool is_remove){
    return map_get<std::string, ValueT*>(m, uid, is_remove, NULL);
}


#endif /* BStream_Proto_hpp */
