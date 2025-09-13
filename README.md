# General

The purpose of this project is to learn about high performance computing systems (HPC) and strategies they use to process millions of events in short periods of time.

# Parralog

Parralog is the C++ implementation of a high performance telemetry analytics engine that processes structured event logs at scale. It demonstrates how memory mapping, multithreading, and approximate quantile algorithms can be combined to achieve throughput suitable for large scale telemetry workloads.

# Telemetry Engine

The telemetry engine produces and simulates synthetic telemetry log files in newline-delimited JSON (NDJSON) format. Each entry contains a service identifier, a status code, and a measured latency. These files simulate and randomize high volume telemetry streams and (aimed to) provide realistic input for the Parralog analytics engine. I tried to design the engine to process tens or hundreds of millions of events efficiently on any hardware.

# System Design Diagram

# Optimization Strategies

## Multithreading and Parallelism
The engine uses a thread pool to distribute work across all available CPU cores. The input file is divided into chunks aligned to newline boundaries so that each thread can process its region independently. Each thread accumulates results locally, avoiding contention. At the end, results are merged into a single report.

## Buffered Batching of Large Writes
The log generator produces synthetic telemetry logs. It uses buffered output so that large amounts of data are flushed to disk in fewer system calls. This reduces I/O overhead and allows the generator to create very large log files quickly.

## Memory Mapping
The telemetry engine uses memory mapping to access input files. This allows the operating system to page data into memory on demand, eliminating the need for explicit buffered reads and avoiding the cost of copying data between kernel and user space. This technique scales better to very large files while keeping memory usage efficient.

## Quantile Sketch
The engine uses the P² quantile sketch algorithm to estimate percentiles such as p50, p95, and p99. This algorithm does not require storing and sorting all samples in memory. It runs in constant space and provides accurate approximations of latency distributions at scale.

Algorithm Inspiration Credits: https://github.com/FooBarWidget/p2

## Platform-Concurrency Awareness
The engine queries the system for the number of available hardware threads using `std::thread::hardware_concurrency()`. On macOS this reflects the logical CPU cores available through the system scheduler. This allows the program to scale its thread pool to the hardware it is running on, ensuring efficient utilization of available resources.

# Running Instructions

Ensure CMake version 3.16 or later is installed. From the project root create a build directory, configure, and build:

```
mkdir build
cd build
cmake ..
make
```

This will produce two executables. The log generator `telemetry_engine` creates large synthetic telemetry log files. The analytics engine `parralog` processes a given file and prints a metrics summary report.

Example workflow:

```
./telemetry_engine ../data/log_100M.ndjson 100000000
./parralog ../data/log_100M.ndjson
```

The first command generates a telemetry log file. The second command runs the engine on that file.

On macOS the build uses the system’s reported concurrency level to select an appropriate number of threads. This ensures the engine takes advantage of available CPU cores while remaining portable across different hardware.