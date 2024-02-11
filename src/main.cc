/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
using namespace std;

#include "cache.h"
void printPersonalInfo()
{
    printf("===== 506 Personal information =====\n");
    printf("Name\n");
    printf("unity\n");
    printf("ECE406 Students? NO\n");
}
int main(int argc, char *argv[])
{
    // print personal info as required
    printPersonalInfo();
    ifstream fin;
    FILE * pFile;

    if(argv[1] == NULL){
         printf("input format: ");
         printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
         exit(0);
        }

    ulong cache_size     = atoi(argv[1]);
    ulong cache_assoc    = atoi(argv[2]);
    ulong blk_size       = atoi(argv[3]);
    ulong num_processors = atoi(argv[4]);
    ulong protocol       = atoi(argv[5]); /* 0:MODIFIED_MSI 1:DRAGON*/
    char *fname        = (char *) malloc(20);
    fname              = argv[6]; // trace_file

    printf("===== 506 SMP Simulator configuration =====\n");
    // print out simulator configuration here
    printf("L1_SIZE:                %lu\n", cache_size);
    printf("L1_ASSOC:               %lu\n", cache_assoc);
    printf("L1_BLOCKSIZE:           %lu\n", blk_size);
    printf("NUMBER OF PROCESSORS:   %lu\n", num_processors);
    if     (protocol == 0) printf("COHERENCE PROTOCOL:     MSI\n");
    else if(protocol == 1) printf("COHERENCE PROTOCOL:     Dragon\n");
    printf("TRACE FILE:             %s\n",fname);
    
    // Using pointers so that we can use inheritance */
    // Create an array to store objects of cache class, using dynamic 
    Cache** cacheArray = (Cache **) malloc(num_processors * sizeof(Cache));
    for(ulong i = 0; i < num_processors; i++) {
        // if(protocol == 0) {
        // Create cache objects and insert into cacheArray
        cacheArray[i] = new Cache(cache_size, cache_assoc, blk_size, protocol);
        // }
    }

    // Open trace file
    pFile = fopen (fname,"r");
    if(pFile == 0)
    {   
        printf("Trace file problem\n");
        exit(0);
    }
    
    ulong proc; // Processor number
    char op; // Operation (r, w)
    ulong addr; // Address at which the operation is being performed

    int line = 1;
    while(fscanf(pFile, "%lu %c %lx", &proc, &op, &addr) != EOF)
    {
#ifdef _DEBUG
    printf("%d: Protocol:%lu Core:%lu Operation:%c Addr:%lx\n", line, protocol, proc, op, addr);
#endif
        // propagate request down through memory hierarchy
        // by calling cachesArray[processor#]->Access(...)
        int brdcastSig = 0;
        if (protocol == 0) {
            #ifdef _DEBUG
                printf("\tBroadcast core %lu:\n", proc);
            #endif
            brdcastSig = cacheArray[proc]->AccessMCI(addr, op, protocol);
            for (ulong i=0; i < num_processors; i++) {
                if (i != proc) {
                    #ifdef _DEBUG
                        printf("\tSnooping core %lu:\n", i);
                    #endif
                    cacheArray[i]->SnoopMCI(addr, op, protocol, brdcastSig);
                }
            }
        }
        else if (protocol == 1) {
            #ifdef _DEBUG
                printf("\tBroadcast core %lu:\n", proc);
            #endif
            bool C = false;
            cacheLine * line = NULL;
            for (ulong i=0; i < num_processors; i++) {
                if (i != proc) {
                    line = cacheArray[i]->findLine(addr);
                    if(line != NULL) {
                        C = true;
                        break;
                    }
                }
            }
            brdcastSig = cacheArray[proc]->AccessDGN(addr, op, protocol, C);
            for (ulong i=0; i < num_processors; i++) {
                if (i != proc) {
                    #ifdef _DEBUG
                        printf("\tSnooping core %lu:\n", i);
                    #endif
                    if (brdcastSig == 0b1101) { // case of BusRdBusUpd signal
                    #ifdef _DEBUG
                        printf("\t\t^^^ Bus read followed by bus upgrade encountered ^^^\n");
                    #endif
                        cacheArray[i]->SnoopDGN(addr, op, protocol, 0b1001);
                        cacheArray[i]->SnoopDGN(addr, op, protocol, 0b1100);
                    }
                    else {
                        cacheArray[i]->SnoopDGN(addr, op, protocol, brdcastSig);
                    }
                }
            }
        }
        line++;
    }

    fclose(pFile);

    //********************************//
    //print out all caches' statistics //
    //********************************//
    for (ulong i=0; i < num_processors; i++) {
        cacheArray[i]->printStats(i, protocol);
    }
    // Free all the dynamically allocated variables/memory
    // Use delete for allocation using new
    // Use free for allocation using free
}
