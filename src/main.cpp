#include <bits/stdc++.h>
#include "common.h"
#include "memory_bplustree.h"
#include "disk_bplustree.h"
#include "hybrid_bplustree.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
static double processMemoryMB() { PROCESS_MEMORY_COUNTERS_EX pm{}; if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pm),sizeof(pm))) return pm.WorkingSetSize/(1024.0*1024.0); return 0; }
#else
#include <sys/resource.h>
static double processMemoryMB() { rusage u{}; if(getrusage(RUSAGE_SELF,&u)==0) {
#ifdef __APPLE__
 return u.ru_maxrss/(1024.0*1024.0);
#else
 return u.ru_maxrss/1024.0;
#endif
 } return 0; }
#endif

struct CsvOptions { std::string path="data/dataset.csv", mode="all"; uint32_t n=NUM_RECORDS; size_t cachePages=4096; };
static CsvOptions parseArgs(int argc,char** argv) {
    CsvOptions o; for(int i=1;i<argc;++i) { std::string a=argv[i]; auto val=[&](){ if(i+1>=argc)throw std::runtime_error("Missing value for "+a); return std::string(argv[++i]); };
        if(a=="--csv")o.path=val(); else if(a=="--mode")o.mode=val(); else if(a=="--n")o.n=static_cast<uint32_t>(std::stoul(val())); else if(a=="--cache-pages")o.cachePages=static_cast<size_t>(std::stoull(val())); else if(a=="--help") { std::cout<<"Usage: ./bplus_benchmark.exe [--csv data/dataset.csv] [--mode memory|disk|hybrid|all] [--n N] [--cache-pages N]\n"; std::exit(0); } else throw std::runtime_error("Unknown argument: "+a); }
    if(o.n==0)throw std::runtime_error("--n must be greater than zero"); return o;
}
static bool readCsvRecord(std::ifstream& in,Record& r,uint64_t lineNo) {
    std::string line; if(!std::getline(in,line))return false; if(!line.empty()&&line.back()=='\r')line.pop_back();
    size_t comma=line.find(','); if(comma==std::string::npos||line.find(',',comma+1)!=std::string::npos) throw std::runtime_error("Malformed CSV row at line "+std::to_string(lineNo));
    r.key=line.substr(0,comma); r.payload=line.substr(comma+1);
    if(r.key.size()!=8||r.payload.size()!=64)throw std::runtime_error("Unexpected key/payload length at CSV line "+std::to_string(lineNo));
    return true;
}
static void openCsv(std::ifstream& in,const std::string& path) {
    in.open(path); if(!in)throw std::runtime_error("Cannot open CSV: "+path);
    std::string header; if(!std::getline(in,header))throw std::runtime_error("CSV is empty"); if(!header.empty()&&header.back()=='\r')header.pop_back(); if(header!="key,payload")throw std::runtime_error("Expected CSV header: key,payload");
}
template<class Tree> static Stats benchmark(Tree& tree,const CsvOptions& o,const std::string& label) {
    std::ifstream in; openCsv(in,o.path); Record rec; std::array<std::string,3> q{}; uint32_t inserted=0;
    const uint32_t q1=0, q2=o.n/2, q3=o.n-1;
    auto t0=std::chrono::steady_clock::now();
    while(inserted<o.n&&readCsvRecord(in,rec,static_cast<uint64_t>(inserted)+2)) {
        if(inserted==q1)q[0]=rec.key; if(inserted==q2)q[1]=rec.key; if(inserted==q3)q[2]=rec.key;
        tree.insert(rec.key,rec.payload); ++inserted;
    }
    auto t1=std::chrono::steady_clock::now(); if(inserted==0)throw std::runtime_error("No CSV data records found");
    Stats s; s.insertionSeconds=elapsedSeconds(t0,t1); s.nodes=tree.getNodeCount(); s.height=tree.getHeight(); s.peakMemoryMB=processMemoryMB();
    uint64_t searchChecksum=0; auto ts=std::chrono::steady_clock::now();
    for(const auto& k:q) { std::string v=tree.search(k); if(v.empty())throw std::runtime_error(label+": search failed for existing key "+k); searchChecksum+=k.size()+v.size(); }
    auto te=std::chrono::steady_clock::now(); s.searchMicroseconds=elapsedMicroseconds(ts,te)/q.size(); s.checksum=searchChecksum;
    // The keys are K + seven hexadecimal characters. This interval is a small, repeatable real-key range.
    uint64_t rangeChecksum=0; auto tr0=std::chrono::steady_clock::now(); uint64_t rangeN=tree.rangeCount("K0000000","K00FFFFFF",rangeChecksum); auto tr1=std::chrono::steady_clock::now();
    s.rangeMicroseconds=elapsedMicroseconds(tr0,tr1); s.checksum+=rangeChecksum; std::cout<<"Range rows in [K0000000, K00FFFFFF]: "<<rangeN<<"\n";
    if constexpr(std::is_same_v<Tree,DiskBPlusTree>) { s.pageReads=tree.getReads(); s.pageWrites=tree.getWrites(); s.diskBytes=tree.fileSizeBytes(); }
    if constexpr(std::is_same_v<Tree,HybridBPlusTree>) { s.pageReads=tree.getReads(); s.pageWrites=tree.getWrites(); s.cacheHits=tree.getCacheHits(); s.cacheMisses=tree.getCacheMisses(); s.diskBytes=tree.fileSizeBytes(); }
    printStats(label,s,inserted); return s;
}
static void runMemory(const CsvOptions& o) { MemoryBPlusTree t; benchmark(t,o,"Memory-only B+ Tree"); }
static void runDisk(const CsvOptions& o) { const std::string p="disk_bplustree.bin"; DiskBPlusTree t(p); benchmark(t,o,"Disk-only B+ Tree"); }
static void runHybrid(const CsvOptions& o) { const std::string p="hybrid_bplustree.bin"; HybridBPlusTree t(p,o.cachePages); benchmark(t,o,"Hybrid B+ Tree"); t.finalize(); }
int main(int argc,char** argv) {
    try { CsvOptions o=parseArgs(argc,argv); std::cout<<"CSV: "<<o.path<<"\nRequested records: "<<o.n<<"\nORDER: "<<ORDER<<", page size: "<<PAGE_SIZE<<" bytes\n";
        if(o.mode=="memory")runMemory(o); else if(o.mode=="disk")runDisk(o); else if(o.mode=="hybrid")runHybrid(o); else if(o.mode=="all") { runMemory(o); runDisk(o); runHybrid(o); } else throw std::runtime_error("Invalid --mode; use memory, disk, hybrid, or all");
    } catch(const std::exception& e) { std::cerr<<"ERROR: "<<e.what()<<"\n"; return 1; } return 0;
}
