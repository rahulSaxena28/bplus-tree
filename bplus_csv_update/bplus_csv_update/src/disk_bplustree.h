#ifndef DISK_BPLUSTREE_H
#define DISK_BPLUSTREE_H
#include "common.h"
#include <array>
#include <cstring>
#include <stdexcept>

class DiskBPlusTree {
protected:
    struct Node { bool leaf=true; uint32_t count=0,next=0; std::vector<std::string> keys,payloads; std::vector<uint32_t> children; };
    static constexpr uint32_t FIRST_NODE_PAGE=1, MAGIC=0x42505432;
    std::fstream file; std::string filename; uint32_t rootPage=FIRST_NODE_PAGE,nextPage=2;
    uint64_t nodeCount=1,reads=0,writes=0;
    virtual Node readNode(uint32_t id) {
        Node n; std::array<char,PAGE_SIZE> b{}; file.clear(); file.seekg(static_cast<std::streamoff>(id)*PAGE_SIZE); file.read(b.data(),PAGE_SIZE);
        if(file.gcount()!=PAGE_SIZE) throw std::runtime_error("Failed to read B+ Tree page");
        uint32_t magic=0,leaf=0; std::memcpy(&magic,b.data(),4); if(magic!=MAGIC) throw std::runtime_error("Invalid B+ Tree page (wrong format or corrupt file)");
        std::memcpy(&leaf,b.data()+4,4); std::memcpy(&n.count,b.data()+8,4); std::memcpy(&n.next,b.data()+12,4); n.leaf=leaf!=0;
        if(n.count>ORDER) throw std::runtime_error("Corrupt page: key count exceeds ORDER");
        size_t off=16; n.keys.resize(n.count);
        for(uint32_t i=0;i<n.count;++i) { char t[KEY_SIZE]{}; std::memcpy(t,b.data()+off,KEY_SIZE); n.keys[i]=t; off+=KEY_SIZE; }
        if(n.leaf) { n.payloads.resize(n.count); for(uint32_t i=0;i<n.count;++i) { char t[PAYLOAD_SIZE]{}; std::memcpy(t,b.data()+off,PAYLOAD_SIZE); n.payloads[i]=t; off+=PAYLOAD_SIZE; } }
        else { n.children.resize(n.count+1); for(uint32_t i=0;i<=n.count;++i) { std::memcpy(&n.children[i],b.data()+off,4); off+=4; } }
        ++reads; return n;
    }
    virtual void writeNode(uint32_t id,const Node& n) {
        if(n.count>ORDER) throw std::runtime_error("Refusing to write node exceeding ORDER");
        std::array<char,PAGE_SIZE> b{}; uint32_t leaf=n.leaf?1u:0u;
        std::memcpy(b.data(),&MAGIC,4); std::memcpy(b.data()+4,&leaf,4); std::memcpy(b.data()+8,&n.count,4); std::memcpy(b.data()+12,&n.next,4);
        size_t off=16;
        for(uint32_t i=0;i<n.count;++i) { if(n.keys[i].size()>=KEY_SIZE) throw std::runtime_error("Key too long for page slot"); std::memcpy(b.data()+off,n.keys[i].c_str(),n.keys[i].size()); off+=KEY_SIZE; }
        if(n.leaf) { for(uint32_t i=0;i<n.count;++i) { if(n.payloads[i].size()>=PAYLOAD_SIZE) throw std::runtime_error("Payload too long for page slot"); std::memcpy(b.data()+off,n.payloads[i].c_str(),n.payloads[i].size()); off+=PAYLOAD_SIZE; } }
        else { for(uint32_t i=0;i<=n.count;++i) { std::memcpy(b.data()+off,&n.children[i],4); off+=4; } }
        if(off>PAGE_SIZE) throw std::runtime_error("Page serialization overflow");
        file.clear(); file.seekp(static_cast<std::streamoff>(id)*PAGE_SIZE); file.write(b.data(),PAGE_SIZE); if(!file) throw std::runtime_error("Failed to write B+ Tree page"); ++writes;
    }
    void writeMeta() { std::array<char,PAGE_SIZE>b{}; uint32_t order=ORDER; std::memcpy(b.data(),&MAGIC,4); std::memcpy(b.data()+4,&rootPage,4); std::memcpy(b.data()+8,&nextPage,4); std::memcpy(b.data()+12,&order,4); file.clear(); file.seekp(0); file.write(b.data(),PAGE_SIZE); file.flush(); }
    uint32_t allocatePage() { uint32_t p=nextPage++; ++nodeCount; return p; }
    struct SplitResult { bool split=false; std::string separator; uint32_t rightPage=0; };
    SplitResult splitLeaf(uint32_t id,Node& n) {
        Node r; r.leaf=true; r.next=n.next; uint32_t mid=n.count/2;
        for(uint32_t i=mid;i<n.count;++i) { r.keys.push_back(std::move(n.keys[i])); r.payloads.push_back(std::move(n.payloads[i])); }
        n.keys.resize(mid); n.payloads.resize(mid); n.count=static_cast<uint32_t>(n.keys.size()); r.count=static_cast<uint32_t>(r.keys.size());
        uint32_t rp=allocatePage(); n.next=rp; writeNode(id,n); writeNode(rp,r); return {true,r.keys.front(),rp};
    }
    SplitResult splitInternal(uint32_t id,Node& n) {
        Node r; r.leaf=false; uint32_t mid=n.count/2; std::string sep=n.keys[mid];
        for(uint32_t i=mid+1;i<n.count;++i) r.keys.push_back(std::move(n.keys[i]));
        for(uint32_t i=mid+1;i<n.children.size();++i) r.children.push_back(n.children[i]);
        n.keys.resize(mid); n.children.resize(mid+1); n.count=static_cast<uint32_t>(n.keys.size()); r.count=static_cast<uint32_t>(r.keys.size());
        uint32_t rp=allocatePage(); writeNode(id,n); writeNode(rp,r); return {true,sep,rp};
    }
    virtual SplitResult insertRecursive(uint32_t id,const std::string& k,const std::string& v) {
        Node n=readNode(id);
        if(n.leaf) { size_t i=static_cast<size_t>(std::lower_bound(n.keys.begin(),n.keys.end(),k)-n.keys.begin()); if(i<n.keys.size()&&n.keys[i]==k) { n.payloads[i]=v; writeNode(id,n); return {}; }
            n.keys.insert(n.keys.begin()+i,k); n.payloads.insert(n.payloads.begin()+i,v); n.count=static_cast<uint32_t>(n.keys.size()); if(n.count>ORDER) return splitLeaf(id,n); writeNode(id,n); return {}; }
        size_t c=static_cast<size_t>(std::upper_bound(n.keys.begin(),n.keys.end(),k)-n.keys.begin()); SplitResult s=insertRecursive(n.children[c],k,v); if(!s.split) { writeNode(id,n); return {}; }
        n.keys.insert(n.keys.begin()+c,s.separator); n.children.insert(n.children.begin()+c+1,s.rightPage); n.count=static_cast<uint32_t>(n.keys.size()); if(n.count>ORDER) return splitInternal(id,n); writeNode(id,n); return {};
    }
    void createEmptyFile() { file.open(filename,std::ios::in|std::ios::out|std::ios::binary|std::ios::trunc); if(!file) throw std::runtime_error("Cannot create tree file: "+filename); std::array<char,PAGE_SIZE> zero{}; file.write(zero.data(),PAGE_SIZE); Node r; r.leaf=true; r.count=0; writeNode(FIRST_NODE_PAGE,r); writeMeta(); }
public:
    explicit DiskBPlusTree(const std::string& path):filename(path) { createEmptyFile(); }
    virtual ~DiskBPlusTree() { if(file.is_open()) { try { writeMeta(); } catch(...){} file.close(); } }
    virtual void insert(const std::string& k,const std::string& v) { SplitResult s=insertRecursive(rootPage,k,v); if(s.split) { Node r; r.leaf=false; r.count=1; r.keys.push_back(s.separator); r.children={rootPage,s.rightPage}; uint32_t p=allocatePage(); writeNode(p,r); rootPage=p; writeMeta(); } }
    virtual std::string search(const std::string& k) { uint32_t p=rootPage; while(true) { Node n=readNode(p); if(n.leaf) { auto it=std::lower_bound(n.keys.begin(),n.keys.end(),k); if(it==n.keys.end()||*it!=k)return {}; return n.payloads[static_cast<size_t>(it-n.keys.begin())]; } size_t c=static_cast<size_t>(std::upper_bound(n.keys.begin(),n.keys.end(),k)-n.keys.begin()); p=n.children[c]; } }
    virtual uint64_t rangeCount(const std::string& a,const std::string& b,uint64_t& sum) { uint32_t p=rootPage; while(true) { Node n=readNode(p); if(n.leaf)break; size_t c=static_cast<size_t>(std::upper_bound(n.keys.begin(),n.keys.end(),a)-n.keys.begin()); p=n.children[c]; } uint64_t count=0; while(p) { Node n=readNode(p); for(size_t i=0;i<n.keys.size();++i) { if(n.keys[i]<a)continue; if(n.keys[i]>b)return count; ++count; sum+=n.keys[i].size()+n.payloads[i].size(); } p=n.next; } return count; }
    uint64_t getNodeCount() const { return nodeCount; }
    virtual uint32_t getHeight() { uint32_t h=1,p=rootPage; while(true) { Node n=readNode(p); if(n.leaf)return h; ++h;p=n.children.front(); } }
    uint64_t getReads() const { return reads; } uint64_t getWrites() const { return writes; }
    uint64_t fileSizeBytes() { file.flush(); return std::filesystem::file_size(filename); }
};
#endif
