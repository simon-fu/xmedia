
/*
 * branch stream protocol 
 */

#ifndef BStream_Proto_hpp
#define BStream_Proto_hpp

#include <stdio.h>
#include <map>
#include "XProtoRapidJson.hpp"



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
