CSV-backed B+ Tree benchmark update
==================================

Files in this package replace these project files:
  src/common.h
  src/memory_bplustree.h
  src/disk_bplustree.h
  src/hybrid_bplustree.h
  src/main.cpp

What changed:
- Reads actual key,payload records from data/dataset.csv (not generated records).
- Validates each key is 8 characters and each payload is 64 characters.
- Stores the complete payload in disk pages (65-byte slot includes NUL).
- Uses ORDER=50 so the leaf page fits in 4096 bytes: 16 + 50*(16+65) = 4066.
- Re-reads the CSV for each mode, preserving CSV insertion order without an extra 10M-row vector.
- Supports --n for small correctness tests and --mode memory|disk|hybrid|all.
- The disk file page magic changes so old files are not mistaken for this page layout; benchmark output files are recreated each run.

Install steps in MSYS2 UCRT64 (from project root):
1. Back up the current src directory before replacing these files.
2. Copy the five files from this package's src/ directory into your project's src/ directory.
3. Compile:
   g++ -std=c++17 -O2 -Wall -Wextra -o bplus_benchmark.exe src/main.cpp -lpsapi
4. Test a small prefix first:
   ./bplus_benchmark.exe --csv data/dataset.csv --mode all --n 1000 --cache-pages 256
5. If all three checksums match and searches pass, run the full dataset:
   ./bplus_benchmark.exe --csv data/dataset.csv --mode all --n 10000000 --cache-pages 4096

Notes:
- ORDER is now 50, not 64, to fit full payloads in 4 KiB pages. If the assignment strictly requires order 64, the page format needs a different design.
- The selected range query is [K0000000, K00FFFFFF]. Its result count depends on the actual keys.
- Disk-only mode can be slow on 10M rows because it writes/reads fixed-size pages for operations.
- Keep your CSV backup; this package does not modify the dataset.
