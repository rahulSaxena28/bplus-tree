#ifndef MEMORY_BPLUSTREE_H
#define MEMORY_BPLUSTREE_H
#include "common.h"
#include <memory>

class MemoryBPlusTree {
    struct Node {
        bool leaf=true;
        std::vector<std::string> keys, payloads;
        std::vector<Node*> children;
        Node* next=nullptr;
    };
    struct Split { bool split=false; std::string separator; Node* right=nullptr; };
    std::vector<std::unique_ptr<Node>> storage;
    Node* root;
    Node* makeNode(bool leaf) {
        auto p=std::make_unique<Node>(); p->leaf=leaf;
        Node* raw=p.get(); storage.push_back(std::move(p)); return raw;
    }
    Split insertRec(Node* n,const std::string& k,const std::string& v) {
        if(n->leaf) {
            auto it=std::lower_bound(n->keys.begin(),n->keys.end(),k);
            size_t i=static_cast<size_t>(it-n->keys.begin());
            if(it!=n->keys.end() && *it==k) { n->payloads[i]=v; return {}; }
            n->keys.insert(it,k); n->payloads.insert(n->payloads.begin()+i,v);
            if(n->keys.size()<=ORDER) return {};
            size_t mid=n->keys.size()/2; Node* r=makeNode(true);
            r->keys.assign(std::make_move_iterator(n->keys.begin()+mid),std::make_move_iterator(n->keys.end()));
            r->payloads.assign(std::make_move_iterator(n->payloads.begin()+mid),std::make_move_iterator(n->payloads.end()));
            n->keys.resize(mid); n->payloads.resize(mid); r->next=n->next; n->next=r;
            return {true,r->keys.front(),r};
        }
        size_t i=static_cast<size_t>(std::upper_bound(n->keys.begin(),n->keys.end(),k)-n->keys.begin());
        Split s=insertRec(n->children[i],k,v); if(!s.split) return {};
        n->keys.insert(n->keys.begin()+i,s.separator); n->children.insert(n->children.begin()+i+1,s.right);
        if(n->keys.size()<=ORDER) return {};
        size_t mid=n->keys.size()/2; std::string sep=n->keys[mid]; Node* r=makeNode(false);
        r->keys.assign(std::make_move_iterator(n->keys.begin()+mid+1),std::make_move_iterator(n->keys.end()));
        r->children.assign(n->children.begin()+mid+1,n->children.end());
        n->keys.resize(mid); n->children.resize(mid+1);
        return {true,sep,r};
    }
public:
    MemoryBPlusTree() { root=makeNode(true); }
    void insert(const std::string& k,const std::string& v) {
        Split s=insertRec(root,k,v); if(s.split) { Node* r=makeNode(false); r->keys.push_back(s.separator); r->children={root,s.right}; root=r; }
    }
    std::string search(const std::string& k) const {
        Node* n=root; while(!n->leaf) { size_t i=static_cast<size_t>(std::upper_bound(n->keys.begin(),n->keys.end(),k)-n->keys.begin()); n=n->children[i]; }
        auto it=std::lower_bound(n->keys.begin(),n->keys.end(),k); if(it==n->keys.end()||*it!=k) return {}; return n->payloads[static_cast<size_t>(it-n->keys.begin())];
    }
    uint64_t rangeCount(const std::string& a,const std::string& b,uint64_t& checksum) const {
        Node* n=root; while(!n->leaf) { size_t i=static_cast<size_t>(std::upper_bound(n->keys.begin(),n->keys.end(),a)-n->keys.begin()); n=n->children[i]; }
        uint64_t count=0; while(n) { for(size_t i=0;i<n->keys.size();++i) { if(n->keys[i]<a) continue; if(n->keys[i]>b) return count; ++count; checksum+=n->keys[i].size()+n->payloads[i].size(); } n=n->next; } return count;
    }
    uint64_t getNodeCount() const { return storage.size(); }
    uint32_t getHeight() const { uint32_t h=1; Node* n=root; while(!n->leaf) { ++h; n=n->children.front(); } return h; }
};
#endif
