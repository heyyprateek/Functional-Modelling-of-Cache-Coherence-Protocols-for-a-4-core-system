# Cache Coherence Protocols
The purpose of this project is to study how parallel architecture designs are evaluated and how to interpret performance data. In this project, I created a simulator that will allowed me to compare different coherence protocol optimizations. I simulated a 4-processor system with Modified MSI, MESI, MOESI and DragonProtocol. I created and instantiate these peer caches and to maintain coherenceacross them by applying coherence protocols. My simulator produces output as various statistics, such as the number of cache hits, misses, memory transactions, issued coherence signals, etc. The simulator is functional, i.e., the number of cycles and data transfer are not underconsideration.

# Introduction
In this section, the provided infrastructure is described. Particular tasks are described in Parts 1 and 2.
## Organization
- src/ - starter source code
- cache.cc
- cache.h
- main.cc
- Makefile
- trace/ - memory access traces
- val/ - validation outputs for traces

Class Cache simulates the behavior of a single cache. It provides implementation of all basic cache methods and cache line states. Actions that should be performed during cache access are described in the Access() method.

## Trace format
The trace reading routine is provided in main.cc. Each line in the trace file is one memory transaction by one of the processors. Each transaction consists of three elements: processor(0-3) operation(r,w) address(in hex). For example, if you read the line 5 w 0xabcd from the trace file, processor 5 is writing to the address “0xabcd” in its local cache. The simulator propagates this request down to cache 5, and cache 5 takes care of that request (maintaining coherence at the same time).

## Cache parameters
Size: 8192B, associativity: 8, block size: 64B

## Command line arguments
./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file>
