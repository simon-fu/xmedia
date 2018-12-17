

#include "NLogger.hpp"

class TestA : public NObjDumper::Dumpable{
public:
    TestA(int v):v_(v), d_(v){}
    friend inline std::ostream& operator<<(std::ostream& out, const TestA& o){
        NObjDumperOStream(out).dump(o);
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
    TestB(int v1)
    :int1_(v1), int2_(v1+1) ,a1_(v1+2), a2_(v1+3), a3_(v1+4){}
    friend inline std::ostream& operator<<(std::ostream& out, const TestB& o){
        NObjDumperOStream(out).dump(o);
        return out;
    }
    
    inline NObjDumper& dump(NObjDumper&& dumper) const{
        return dump(dumper);
    }
    
    inline NObjDumper& dump(NObjDumper& dumper) const{
        return dumper.objB().dump("a1", a1_).kv("int1", int1_).kv("int2", int2_).endl()
        .dump("a2", a1_).endl()
        .dump("a3", a2_).endl()
        .objE();
    }
    
private:
    int int1_;
    int int2_;
    TestA a1_;
    TestA a2_;
    TestA a3_;
};

class TestC{
public:
    TestC(int v1, int v2)
    :b1_(v1++), b2_(v2++){
        b3_.emplace_back(++v1);
        b3_.emplace_back(++v1);
        b3_.emplace_back(++v1);
        b3_.emplace_back(++v1);
        b3_.emplace_back(++v1);
        b3_.emplace_back(++v1);
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

int NObjDumperTester::test(){
    
    TestB b(1);
    TestC c(77,88);
    
    NObjDumperOStream(std::cout).dump("stream-b", b).endl();
    NObjDumperOStream(std::cout).dump("stream-c", c).endl();
    
    auto console = spdlog::stdout_logger_mt("console");
    NObjDumperLog(*console).dump("logger-b", b).endl();
    NObjDumperLog(*console).dump("logger-c", c).endl();
    
    return 0;
}



