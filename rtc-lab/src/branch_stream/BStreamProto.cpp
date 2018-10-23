
#include <stdio.h>
#include <string>
#include <vector>
#include <sstream>
#include "BStreamProto.h"

# pragma mark - proto definition


static const char * proto_type_names[] = {
    "kNullType",
    "kFalseType",
    "kTrueType",
    "kObjectType",
    "kArrayType",
    "kStringType",
    "kNumberType"
};


bool XProtoBase::parseField(const Value &doc_, XProtoField* f, std::ostringstream& oss_err){
    f->isNull_ = true;
    f->iter_  = doc_.FindMember(f->name_.c_str());
    if(f->iter_ != doc_.MemberEnd() && f->iter_->value.GetType() == kNullType){
        f->iter_ = doc_.MemberEnd();
    }
    
    if(f->iter_ == doc_.MemberEnd()){
        if(f->mandatory_){
            oss_err << "non-exist field [" << name_ << "]->["<< f->name_ << "]";
            return false;
        }
        return true;
    }
    
    if(!(MAKE_TYPE_FLAG(f->iter_->value.GetType()) & f->typflag_)){
        oss_err << "unexpectd field type [" << name_ << "]->["<< f->name_ << "]: ["
        << proto_type_names[f->iter_->value.GetType()] << "]";
        return false;
    }
    
    f->isNull_ = false;
    if(f->iter_->value.IsString() ){
        f->strvalue_ = MK_ITER_CSTR(f->iter_);
    }else if(f->iter_->value.IsInt()){
        f->intvalue_ = MK_ITER_INT(f->iter_);
    }else if(f->iter_->value.IsBool()){
        f->intvalue_ = f->iter_->value.GetBool()?1:0;
    }
    return true;
}

int XProtoBase::parse(const Value &doc_, std::ostringstream& oss_err){
    XProtoBase * proto = this;
    std::vector<XProtoField*>* fields = proto->getFields();
    for(auto f : *fields){
        if(!parseField(doc_, f, oss_err)){
            return -1;
        }
    }
    return 0;
}

int XProtoBase::parse(const char * json, int json_len, std::ostringstream& oss_err){
    doc_.Parse(json);
    if(!doc_.IsObject()){
        oss_err << "json is NOT object [" << name_ << "]";
        return -1;
    }
    return parse(doc_, oss_err);
}

int XProtoBase::parse(const std::string& str, std::ostringstream& oss_err){
    return parse(str.c_str(), (int)str.length(), oss_err);
}

int XProtoBase::parse(XProtoField& field, std::ostringstream& oss_err){
    const Value& strval = field.iter_->value;
    if(strval.IsString()){
        return parse(strval.GetString(), strval.GetStringLength(), oss_err);
    }else if(strval.IsObject()){
        return parse(strval, oss_err);
    }else{
        oss_err << "NOT string and NOT object [" << name_ << "]->["<< field.name_ << "]";
        return -1;
    }
}





