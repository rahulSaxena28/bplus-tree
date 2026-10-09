#ifndef COMMON_H
#define COMMON_H
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

constexpr uint32_t NUM_RECORDS = 10'000'000;
// 16-byte header + 50*(16-byte key slot + 65-byte payload slot) = 4066 bytes.
constexpr uint32_t ORDER = 50;
constexpr uint32_t PAGE_SIZE = 4096;
constexpr uint32_t KEY_SIZE = 16;
constexpr uint32_t PAYLOAD_SIZE = 65; // 64 characters plus NUL terminator
static_assert(16 + ORDER * (KEY_SIZE + PAYLOAD_SIZE) <= PAGE_SIZE,
              "Leaf node layout exceeds page size");
static_assert(16 + ORDER * KEY_SIZE + (ORDER + 1) * sizeof(uint32_t) <= PAGE_SIZE,
              "Internal node layout exceeds page size");

struct Record { std::string key, payload; };
struct Stats {
    double insertionSeconds=0, searchMicroseconds=0, rangeMicroseconds=0;
    uint64_t checksum=0, nodes=0, pageReads=0, pageWrites=0;
    uint64_t cacheHits=0, cacheMisses=0, diskBytes=0;
    uint32_t height=0;
    double peakMemoryMB=0;
};
inline double elapsedSeconds(std::chrono::steady_clock::time_point a,
                              std::chrono::steady_clock::time_point b) {
    return std::chrono::duration<double>(b-a).count();
}
inline double elapsedMicroseconds(std::chrono::steady_clock::time_point a,
                                  std::chrono::steady_clock::time_point b) {
    return std::chrono::duration<double,std::micro>(b-a).count();
}
inline void printStats(const std::string& name, const Stats& s, uint32_t records) {
    std::cout << "\n========================================\n" << name
              << "\n========================================\n"
              << "Records inserted : " << records << "\n"
              << std::fixed << std::setprecision(3)
              << "Insertion time   : " << s.insertionSeconds << " seconds\n"
              << "Tree height      : " << s.height << "\n"
              << "Total nodes      : " << s.nodes << "\n"
              << std::setprecision(2) << "Peak memory      : " << s.peakMemoryMB << " MB\n"
              << std::setprecision(3) << "Search time      : " << s.searchMicroseconds << " us\n"
              << "Range time       : " << s.rangeMicroseconds << " us\n"
              << "Checksum         : " << s.checksum << "\n"
              << "Page reads       : " << s.pageReads << "\n"
              << "Page writes      : " << s.pageWrites << "\n"
              << "Cache hits       : " << s.cacheHits << "\n"
              << "Cache misses     : " << s.cacheMisses << "\n"
              << "Disk usage       : " << std::setprecision(2)
              << (s.diskBytes / (1024.0*1024.0)) << " MiB\n";
}
#endif
