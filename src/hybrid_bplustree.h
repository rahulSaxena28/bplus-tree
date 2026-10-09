#ifndef HYBRID_BPLUSTREE_H
#define HYBRID_BPLUSTREE_H
#include "disk_bplustree.h"
#include <list>
#include <unordered_map>

class HybridBPlusTree : public DiskBPlusTree {
    struct CacheEntry { Node node; bool dirty=false; std::list<uint32_t>::iterator lruIt; };
    size_t capacity; std::list<uint32_t> lru; std::unordered_map<uint32_t,CacheEntry> cache;
    uint64_t hits=0,misses=0;
    Node load(uint32_t p) {
        auto it=cache.find(p); if(it!=cache.end()) { ++hits; lru.erase(it->second.lruIt); lru.push_front(p); it->second.lruIt=lru.begin(); return it->second.node; }
        ++misses; Node n=DiskBPlusTree::readNode(p); put(p,n,false); return n;
    }
    void put(uint32_t p,const Node& n,bool dirty) {
        auto it=cache.find(p); if(it!=cache.end()) { it->second.node=n; it->second.dirty=it->second.dirty||dirty; lru.erase(it->second.lruIt); lru.push_front(p); it->second.lruIt=lru.begin(); return; }
        if(capacity==0) { if(dirty) DiskBPlusTree::writeNode(p,n); return; }
        if(cache.size()>=capacity) evictOne(); lru.push_front(p); cache.emplace(p,CacheEntry{n,dirty,lru.begin()});
    }
    void evictOne() { if(lru.empty())return; uint32_t p=lru.back(); auto it=cache.find(p); if(it!=cache.end()) { if(it->second.dirty)DiskBPlusTree::writeNode(p,it->second.node); cache.erase(it); } lru.pop_back(); }
    void flushCache() { for(auto& kv:cache) { auto& e=kv.second; if(e.dirty)DiskBPlusTree::writeNode(kv.first,e.node); e.dirty=false; } file.flush(); }
    SplitResult insertHybrid(uint32_t p,const std::string& k,const std::string& v) {
        Node n=load(p);
        if(n.leaf) {
            size_t i=static_cast<size_t>(std::lower_bound(n.keys.begin(),n.keys.end(),k)-n.keys.begin());
            if(i<n.keys.size()&&n.keys[i]==k) { n.payloads[i]=v; put(p,n,true); return {}; }
            n.keys.insert(n.keys.begin()+i,k); n.payloads.insert(n.payloads.begin()+i,v); n.count=static_cast<uint32_t>(n.keys.size());
            if(n.count>ORDER) { uint32_t mid=n.count/2; Node r; r.leaf=true; r.next=n.next; for(uint32_t j=mid;j<n.count;++j) { r.keys.push_back(std::move(n.keys[j])); r.payloads.push_back(std::move(n.payloads[j])); }
                n.keys.resize(mid); n.payloads.resize(mid); n.count=static_cast<uint32_t>(n.keys.size()); uint32_t rp=allocatePage(); n.next=rp; r.count=static_cast<uint32_t>(r.keys.size()); put(p,n,true); put(rp,r,true); return {true,r.keys.front(),rp}; }
            put(p,n,true); return {};
        }
        size_t c=static_cast<size_t>(std::upper_bound(n.keys.begin(),n.keys.end(),k)-n.keys.begin()); SplitResult s=insertHybrid(n.children[c],k,v); if(!s.split) { put(p,n,false); return {}; }
        n.keys.insert(n.keys.begin()+c,s.separator); n.children.insert(n.children.begin()+c+1,s.rightPage); n.count=static_cast<uint32_t>(n.keys.size());
        if(n.count>ORDER) { uint32_t mid=n.count/2; std::string sep=n.keys[mid]; Node r; r.leaf=false; for(uint32_t j=mid+1;j<n.count;++j)r.keys.push_back(std::move(n.keys[j])); for(uint32_t j=mid+1;j<n.children.size();++j)r.children.push_back(n.children[j]); n.keys.resize(mid); n.children.resize(mid+1); n.count=static_cast<uint32_t>(n.keys.size()); r.count=static_cast<uint32_t>(r.keys.size()); uint32_t rp=allocatePage(); put(p,n,true); put(rp,r,true); return {true,sep,rp}; }
        put(p,n,true); return {};
    }
public:
    explicit HybridBPlusTree(const std::string& path,size_t cachePages=4096):DiskBPlusTree(path),capacity(cachePages) { cache.reserve(cachePages*2+1); }
    ~HybridBPlusTree() override { try { flushCache(); } catch(...){} }
    void insert(const std::string& k,const std::string& v) override { SplitResult s=insertHybrid(rootPage,k,v); if(s.split) { Node r; r.leaf=false; r.count=1; r.keys.push_back(s.separator); r.children={rootPage,s.rightPage}; uint32_t p=allocatePage(); put(p,r,true); rootPage=p; writeMeta(); } }
    std::string search(const std::string& k) override { uint32_t p=rootPage; while(true) { Node n=load(p); if(n.leaf) { auto it=std::lower_bound(n.keys.begin(),n.keys.end(),k); if(it==n.keys.end()||*it!=k)return {}; return n.payloads[static_cast<size_t>(it-n.keys.begin())]; } size_t c=static_cast<size_t>(std::upper_bound(n.keys.begin(),n.keys.end(),k)-n.keys.begin()); p=n.children[c]; } }
    uint64_t rangeCount(const std::string& a,const std::string& b,uint64_t& sum) override { uint32_t p=rootPage; while(true) { Node n=load(p); if(n.leaf)break; size_t c=static_cast<size_t>(std::upper_bound(n.keys.begin(),n.keys.end(),a)-n.keys.begin()); p=n.children[c]; } uint64_t count=0; while(p) { Node n=load(p); for(size_t i=0;i<n.keys.size();++i) { if(n.keys[i]<a)continue; if(n.keys[i]>b)return count; ++count; sum+=n.keys[i].size()+n.payloads[i].size(); } p=n.next; } return count; }
    uint64_t getCacheHits() const { return hits; } uint64_t getCacheMisses() const { return misses; }
    uint32_t getHeight() override { flushCache(); return DiskBPlusTree::getHeight(); }
    void finalize() { flushCache(); writeMeta(); }
};
#endif
