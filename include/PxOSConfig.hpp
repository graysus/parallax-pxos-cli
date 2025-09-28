
#include <PxConfig.hpp>
#include <PxState.hpp>
#include <PxResult.hpp>

#ifndef PXOSCONF
#define PXOSCONF

namespace PxOSConfig {
    class conf {
    private:
        std::string path;
    public:
        std::string root1;
        std::string root2;
        std::string data;
        std::string current;

        conf(std::string path) : path(path) {}
        
        PxResult::Result<void> readConf() {
            auto rescnf = PxConfig::ReadConfig(path);
            PXASSERTM(rescnf, "PxOSConfig::conf::readConf");
            auto cnf = rescnf.assert();
            root1 = cnf.QuickRead("ROOT1");
            root2 = cnf.QuickRead("ROOT2");
            data = cnf.QuickRead("DATA");
            current = cnf.QuickRead("CURRENT");
            return PxResult::Null;
        }
        PxResult::Result<void> writeConf() {
            auto res = PxState::fput(path, ""
                "ROOT1="+root1+"\n"
                "ROOT2="+root2+"\n"
                "DATA="+data+"\n"
                "CURRENT="+current
            );
            PXASSERTM(res, "PxOSConfig::conf::writeConf");
            return PxResult::Null;
        }
        std::string &curPart() {
            return current == "1" ? root1 : root2;
        }
        std::string &oppositePart() {
            return current == "1" ? root2 : root1;
        }
        void switchCurrent() {
            current = current == "1" ? "2" : "1";
        }
    };

    PxResult::Result<void> InitializeNew(conf &cfg);

    struct OSConfig {
        std::string repo;
        std::string branch;
    };
}
#endif