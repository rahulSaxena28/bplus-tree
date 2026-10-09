# B+ Tree Implementation and Performance Analysis

## 1. Overview

This project implements and compares three B+ Tree variants using a dataset of 10 million records:

1. **Memory-only B+ Tree** — stores tree nodes in memory.
2. **Disk-only B+ Tree** — stores tree nodes in a disk-backed file.
3. **Hybrid B+ Tree** — uses disk storage with an LRU page cache to reduce disk I/O.

The project evaluates insertion time, search time, range-query time, memory usage, disk usage, tree height, and page I/O.

## 2. Dataset

- Total records: 10,000,000
- Indexing column: `key`
- Payload column: `payload`
- Key length: 8 characters
- Payload length: 64 characters
- CSV header: `key,payload`

The dataset is not included in this repository because of its large size.

## 3. Implementation Assumptions

| Parameter | Value |
|---|---|
| Page size | 4096 bytes (4 KiB) |
| Configured B+ Tree order | 50 |
| Key length | 8 characters |
| Payload length | 64 characters |
| Hybrid cache capacity | 256 pages for the reported benchmark |
| Disk storage | File-backed tree nodes |
| Hybrid cache policy | Least Recently Used (LRU) |

The actual number of entries per node depends on the node layout and page serialization.

## 4. Benchmark Results

The following measurements were obtained by running the benchmark on 10,000,000 records.

| Metric | Memory-only | Disk-only | Hybrid |
|---|---:|---:|---:|
| Records inserted | 10,000,000 | 10,000,000 | 10,000,000 |
| Insertion time (s) | 16.460 | 501.178 | 324.017 |
| Search time (µs) | 1.133 | 69.433 | 14.900 |
| Range-query time (ms) | 20.634 | 746.445 | 447.387 |
| Peak memory (MB) | 1731.24 | 12.86 | 14.05 |
| Disk usage (MiB) | 0.00 | 1153.63 | 1153.63 |
| Tree height | 5 | 5 | 5 |
| Total nodes | 295,329 | 295,329 | 295,329 |
| Page reads | 0 | 47,987,602 | 23,227,276 |
| Page writes | 0 | 48,252,770 | 10,537,540 |

### Consistency checks

All three implementations reported:

- Checksum: `75497688`
- Range rows in the tested interval: `1048576`

These matching values provide an initial consistency check. The benchmark's current checksum is based on key and payload lengths, so it is not a complete verification of the actual contents of every returned record.

## 5. Performance Analysis

### Memory-only B+ Tree

The memory-only implementation achieved the lowest insertion, search, and range-query times. Its main trade-off was peak memory usage of approximately 1.69 GiB.

### Disk-only B+ Tree

The disk-only implementation used the least reported peak memory, but insertion took approximately 501 seconds. Its high page-read and page-write counts reflect the cost of operating on disk-backed nodes.

### Hybrid B+ Tree

The hybrid implementation reduced insertion time from 501.178 seconds to 324.017 seconds compared with disk-only storage. Search time decreased from 69.433 µs to 14.900 µs. It also recorded fewer page reads and writes than the disk-only implementation.

The hybrid implementation provided a compromise between memory consumption and performance in this benchmark.

## 6. Conclusion

The experiment demonstrates the trade-off between memory consumption and performance in B+ Tree storage designs.

- **Memory-only** provided the best raw performance.
- **Disk-only** provided the lowest reported memory usage.
- **Hybrid** improved insertion and search performance over disk-only storage while retaining disk-backed storage.

The measurements are specific to the test machine, filesystem, implementation, and benchmark configuration. Further validation of individual search results and range-query contents would strengthen the correctness evaluation.

## 7. Build and Run

Compile from the project directory in an environment that supports the required C++ headers and Windows process-memory APIs:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -o bplus_benchmark.exe src/main.cpp -lpsapi
```

Run all three implementations:

```bash
./bplus_benchmark.exe --csv data/dataset.csv --mode all --n 10000000 --cache-pages 256
```

The dataset must be placed at `data/dataset.csv`.

## 8. Repository

Source code: https://github.com/rahulSaxena28/bplus-tree
