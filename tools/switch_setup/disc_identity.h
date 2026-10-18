#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

// Identify the boot serial in ISO9660 SYSTEM.CNF, independently of VER/hash.
static bool disc_is_scus97328(const std::filesystem::path &path)
{
    std::ifstream file(path,std::ios::binary | std::ios::ate);
    if(!file) return false;
    const auto size=file.tellg();
    auto read=[&](uint64_t offset,void *out,size_t length) {
        if(size<0 || offset>(uint64_t)size || length>(uint64_t)size-offset) return false;
        file.clear();file.seekg((std::streamoff)offset);
        return bool(file.read((char *)out,(std::streamsize)length));
    };
    auto le32=[](const unsigned char *p) {
        return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
    };
    unsigned char pvd[2048];
    if(!read(16*2048,pvd,sizeof pvd) || pvd[0]!=1 ||
        std::string((char *)pvd+1,5)!="CD001" || pvd[6]!=1) return false;
    uint32_t length=le32(pvd+166),sector=le32(pvd+158);
    if(!length || length>16*1024*1024) return false;
    std::vector<unsigned char> directory(length);
    if(!read(uint64_t(sector)*2048,directory.data(),length)) return false;
    for(size_t at=0;at<directory.size();) {
        const unsigned char *record=directory.data()+at;
        size_t bytes=record[0];
        if(!bytes) { at=(at/2048+1)*2048;continue; }
        if(bytes<34 || bytes>directory.size()-at || record[32]>bytes-33) return false;
        std::string name((char *)record+33,record[32]);
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return (char)std::toupper(c);});
        name=name.substr(0,name.find(';'));
        if(name=="SYSTEM.CNF") {
            uint32_t count=le32(record+10);
            if(!count || count>65536 || (record[25]&2)) return false;
            std::string cnf(count,'\0');
            if(!read(uint64_t(le32(record+2))*2048,cnf.data(),count)) return false;
            std::istringstream lines(cnf);std::string line;
            while(std::getline(lines,line)) {
                auto eq=line.find('=');if(eq==std::string::npos) continue;
                std::string key=line.substr(0,eq);
                key.erase(std::remove_if(key.begin(),key.end(),[](unsigned char c){return std::isspace(c);}),key.end());
                std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return (char)std::toupper(c);});
                if(key!="BOOT2") continue;
                std::string boot=line.substr(eq+1);
                auto slash=boot.find_last_of("\\/:");if(slash!=std::string::npos) boot=boot.substr(slash+1);
                boot=boot.substr(0,boot.find(';'));
                std::string serial;
                for(unsigned char c:boot) {
                    if(std::isspace(c) || c=='_' || c=='.' || c=='-') continue;
                    serial+=(char)std::toupper(c);
                }
                return serial=="SCUS97328";
            }
            return false;
        }
        at+=bytes;
    }
    return false;
}
