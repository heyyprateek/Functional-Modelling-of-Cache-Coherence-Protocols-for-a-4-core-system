/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

/****add new states, based on the protocol****/
enum {
   INVALID = 0,
   VALID,
   DIRTY
};

class cacheLine 
{
protected:
   ulong tag;
   ulong Flags;   // 0:invalid, 1:valid, 2:dirty 
   ulong seq;
   // coherence state variables
   int coherenceState;
 
public:
   cacheLine()                         { tag = 0; Flags = 0; }
   ulong getTag()                      { return tag; }
   ulong getFlags()                    { return Flags;}
   ulong getSeq()                      { return seq; }
   int getCoherenceState()             { return coherenceState; }
   void setSeq(ulong Seq)              { seq = Seq;}
   void setFlags(ulong flags)          {  Flags = flags; }
   void setTag(ulong a)                { tag = a; }
   void setCoherenceState(int state)   { coherenceState = state; }
   void invalidate()                   { tag = 0; Flags = INVALID; } //useful function
   bool isValid()                      { return ((Flags) != INVALID); }
};

class Cache
{
protected:
   // Cache configuration parameters
   ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;

   // Cache counters
   ulong reads, readMisses, writes, writeMisses, writeBacks;

   //******///
   //add coherence counters here///
   //******///
   ulong memTxCnt, invalidationCnt, flushCnt, busRdXCnt, interventionCnt, busUpdCnt;

   /*
   Modified MSI protocol (protocol 0) -> I:001, C:010, M:100
   Dragon protocol (protocol 1) -> M:0001, Sc:0010, Sm:0100, E:1000
   */

   // Cache data strcuture
   cacheLine **cache;

   // functions to calculate tag, index and 
   ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
   ulong calcIndex(ulong addr)   { return ((addr >> log2Blk) & tagMask);}
   ulong calcAddr4Tag(ulong tag) { return (tag << (log2Blk));}
   
public:
    ulong currentCycle;  
     
   // Constructor
   Cache(int,int,int,ulong);
   // Destructor
   ~Cache() { delete cache;}
   
   // Cache operations
   cacheLine *findLineToReplace(ulong addr);
   cacheLine *fillLine(ulong addr);
   cacheLine * findLine(ulong addr);
   cacheLine * getLRU(ulong);
   
   // Getter for cache statistics
   ulong getRM()                 {return readMisses;} 
   ulong getWM()                 {return writeMisses;} 
   ulong getReads()              {return reads;}       
   ulong getWrites()             {return writes;}
   ulong getWB()                 {return writeBacks;}
   ulong getMemTx()              {return memTxCnt;}
   ulong getInvalidations()      {return invalidationCnt;}
   ulong getInterventions()      {return interventionCnt;}
   ulong getFlushes()            {return flushCnt;}
   ulong getBusRdX()             {return busRdXCnt;}
   ulong getBusUpd()             {return busUpdCnt;}
   double getMissRate()          {return 100.00*(double(getRM()+getWM()))/double(getReads()+getWrites());}
   
   // Writeback operation
   void writeBack(ulong) {writeBacks++;}

   // Increment counters
   void MemoryTxInc()         {memTxCnt++;}
   void InvalidateInc()       {invalidationCnt++;}
   void InterventionInc()     {interventionCnt++;}
   void FlushInc()            {flushCnt++;}
   void BusRdXInc()           {busRdXCnt++;}
   void BusUpdInc()           {busUpdCnt++;}

   // Main access function
   int AccessMCI(ulong,uchar,ulong);
   int AccessDGN(ulong,uchar,ulong,bool);

   // Print cache statistics
   void printStats(ulong,ulong);

   // Update LRU information
   void updateLRU(cacheLine *);

   //******///
   //add other functions to handle bus transactions///
   //******///
   void SnoopMCI(ulong,uchar,ulong,int);
   void SnoopDGN(ulong,uchar,ulong,int);

};

#endif
