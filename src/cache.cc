/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;

// Bus Transactions
struct BusTransactions {
   // Common transactions
   int BusRd;

   // Modified MSI transactions
   int BusRdX;

   // Dragon protocol transactions
   int Flush;
   int BusUpd;
   int BusRdBusUpd;
};

BusTransactions busTxMCI = {0b0001, 0b0010, 0, 0, 0};
BusTransactions busTxDGN = {0b1001, 0, 0b1010, 0b1100, 0b1101};

// Coherence States
struct CoherenceStates {
   // Common states
   int M;

   // Only MCI states
   int C;
   int I;

   // Dragon protocol States
   int E;
   int Sm;
   int Sc;
};
CoherenceStates MCIStates = {0b0100, 0b0010, 0b0001, 0,0,0};
CoherenceStates DGNStates = {0b11000, 0, 0, 0b10100, 0b10010, 0b10001};

const char* state2String(ulong state) {
   switch (state) {
      case 0b11000: return "MODIFIED";
      case 0b10100: return "EXCLUSIVE";
      case 0b10010: return "SHARED_MODIFIED";
      case 0b10001: return "SHARED_CLEAN";
      default:      return "INVALID";
   }
}

const char* busTx2String(int busTx) {
   switch (busTx) {
      case 0b1001: return "BusRd";
      case 0b1010: return "Flush";
      case 0b1100: return "BusUpd";
      case 0b1101: return "BusRdBusUpd";
      default:     return "NONE";
   }
}

Cache::Cache(int s,int a,int b, ulong protocol)
{
   ulong i, j;
   reads = readMisses = writes = 0; 
   writeMisses = writeBacks = currentCycle = 0;

   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);   
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b)); 
  
   //*******************//
   //initialize your counters here//
   //*******************//
   busRdXCnt = memTxCnt = flushCnt = interventionCnt = busUpdCnt = 0;
 
   tagMask = 0;
   for(i=0;i<log2Sets;i++)
   {
      tagMask <<= 1;
      tagMask |= 1;
   }
   
   /**create a two dimentional cache, sized as cache[sets][assoc]**/ 
   cache = new cacheLine*[sets];
   for(i=0; i<sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for(j=0; j<assoc; j++) 
      {
         cache[i][j].invalidate();
         if      (protocol == 0) {cache[i][j].setCoherenceState(MCIStates.I);}
         else if (protocol == 1) {cache[i][j].setCoherenceState(0);}
      }
   }      
   
}

/**you might add other parameters to Access()
since this function is an entry point 
to the memory hierarchy (i.e. caches)**/
int Cache::AccessMCI(ulong addr, uchar op, ulong protocol)
{
   currentCycle++;/*per cache global counter to maintain LRU order 
                    among cache ways, updated on every cache access*/

   int signal = 0;
   #ifdef _DEBUG
      printf("\t\ttag:%lu index:%lu\n", calcTag(addr), calcIndex(addr));
   #endif
         
   if(op == 'w') writes++;
   else          reads++;
   cacheLine * line = findLine(addr);
   if(line == NULL)/*miss*/
   {
      // Allocate a cache line
      cacheLine *newline = fillLine(addr);
      #ifdef _DEBUG
         printf("\t\t***Cache miss***\n");
         printf("\t\tsignal: %d current State: %d\n", signal, newline->getCoherenceState());
      #endif
      if (op == 'w') { // PrWr operation
         #ifdef _DEBUG
            printf("\t\t***Cache read miss***\n");
         #endif
         writeMisses++;
         MemoryTxInc();
         newline->setFlags(DIRTY);
         // Move to M state
         newline->setCoherenceState(MCIStates.M);
         // Broadcast BusRdX signal
         signal = busTxMCI.BusRdX;
         BusRdXInc();
         #ifdef _DEBUG
            printf("\t\tsignal: BusRdX; current State moved to: %d\n", newline->getCoherenceState());
         #endif
      }
      if (op == 'r'){ // PrRd operation
         readMisses++;
         MemoryTxInc();
         // Move to C state
         newline->setCoherenceState(MCIStates.C);
         // Broadcast BusRd Signal
         signal = busTxMCI.BusRd;
         #ifdef _DEBUG
            printf("\t\tsignal: BusRd; current State moved to: %d\n", newline->getCoherenceState());
         #endif
      }
   }
   else
   {
      #ifdef _DEBUG
         printf("\t\t***Cache hit***\n");
         printf("\t\tsignal: %d current State: %d\n", signal, line->getCoherenceState());
      #endif
      // Fetch the current coherence State. It could be either M or C
      // Depends on whether the current line is dirty or not
      if (line->getFlags() == DIRTY)         line->setCoherenceState(MCIStates.M);
      else if (line->getFlags() == VALID)    line->setCoherenceState(MCIStates.C);
      else if (line->getFlags() == INVALID)  line->setCoherenceState(MCIStates.I);
      /**since it's a hit, update LRU and update dirty flag**/
      updateLRU(line);
      if(op == 'w') { // PrWr operation
         line->setFlags(DIRTY);
         // Move to M state if the current state was C in case of PrWr operation
         if (line->getCoherenceState() == MCIStates.C) {
            line->setCoherenceState(MCIStates.M);
         }
         #ifdef _DEBUG
            printf("\t\tsignal: BusRdX; current State moved to: %d\n", line->getCoherenceState());
         #endif
      }
      if(op == 'r') { //PrRd operation
         // Remain in the same state for PrRD operation
         line->setCoherenceState(line->getCoherenceState());
         #ifdef _DEBUG
            printf("\t\tsignal: none; current State moved to: %d\n", line->getCoherenceState());
         #endif
      }
   }
   return signal;
}

int Cache::AccessDGN(ulong addr, uchar op, ulong protocol, bool C)
{
   currentCycle++;/*per cache global counter to maintain LRU order 
                    among cache ways, updated on every cache access*/
   int signal = 0;
   #ifdef _DEBUG
      printf("\t\ttag:%lu index:%lu\n", calcTag(addr), calcIndex(addr));
   #endif
   if(op == 'w') writes++;
   else          reads++;
   
   cacheLine * line = findLine(addr);
   if(line == NULL)/*miss*/
   {
      // Allocate a cache line
      cacheLine *newline = fillLine(addr);
      #ifdef _DEBUG
         printf("\t\t***Cache miss***\n");
         printf("\t\tsignal: %d current State: %d C:%d\n", signal, newline->getCoherenceState(), C);
      #endif
      if(op == 'w') { // PrWrMiss;
         writeMisses++;
         MemoryTxInc();
         newline->setFlags(DIRTY);
         if(!C) {
            // Move to M state if there are no other copies
            newline->setCoherenceState(DGNStates.M);
            // Broadcast BusRd signal
            signal = busTxDGN.BusRd;
         }
         else {
            // Move to Sm state if there are other copies
            newline->setCoherenceState(DGNStates.Sm);
            // Broadcast BusRd signal followed by BusUpd signal
            signal = busTxDGN.BusRdBusUpd;
            BusUpdInc();
         }
         #ifdef _DEBUG
            printf("\t\tsignal: BusRdX; current State moved to: %d\n", newline->getCoherenceState());
         #endif
      }
      else if (op == 'r') { // PrRdMiss
         readMisses++;
         MemoryTxInc();
         if(!C) {
            // Move to E state if there are no other copies
            newline->setCoherenceState(DGNStates.E);
            signal = busTxDGN.BusRd;
         }
         else {
            newline->setCoherenceState(DGNStates.Sc);
            signal = busTxDGN.BusRd;
         }
         #ifdef _DEBUG
            printf("\t\tsignal: BusRd; current State moved to: %d\n", newline->getCoherenceState());
         #endif
      }     
   }
   else
   {
      #ifdef _DEBUG
         printf("\t\t***Cache hit***\n");
         printf("\t\tsignal: %d current State: %d\n", signal, line->getCoherenceState());
      #endif
      /**since it's a hit, update LRU and update dirty flag**/
      updateLRU(line);
      if(op == 'w') { // PrWr operation
         line->setFlags(DIRTY);
         if (line->getCoherenceState() == DGNStates.M) { // Modified state
            line->setCoherenceState(DGNStates.M);
         }
         else if (line->getCoherenceState() == DGNStates.Sc) { // Shared clean state
            signal = busTxDGN.BusUpd;
            BusUpdInc();
            if (!C) { line->setCoherenceState(DGNStates.M); }
            else    { line->setCoherenceState(DGNStates.Sm);}
            #ifdef _DEBUG
               printf("\t\tC signal: BusUpd; current State moved to: %d\n", line->getCoherenceState());
            #endif
         }
         else if (line->getCoherenceState() == DGNStates.Sm) { // Shared modified state
            signal = busTxDGN.BusUpd;
            BusUpdInc();
            if(!C) {line->setCoherenceState(DGNStates.M); }
            else   {line->setCoherenceState(DGNStates.Sm);}
            #ifdef _DEBUG
               printf("\t\t!C signal: BusUpd; current State moved to: %d\n", line->getCoherenceState());
            #endif
         }
         else if(line->getCoherenceState() == DGNStates.E) { // Exclusive state
            line->setCoherenceState(DGNStates.M);
            #ifdef _DEBUG
               printf("\t\tsignal: 0; current State moved to: %d\n", line->getCoherenceState());
            #endif
         }
      }
      else if(op == 'r') { // PrRd operation
         // In this case the state does not change
         line->setCoherenceState(line->getCoherenceState());
         #ifdef _DEBUG
            printf("\t\tsignal: 0; current State moved to: %d\n", line->getCoherenceState());
         #endif
      }
   }
   
   return signal;
}

/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
   for(j=0; j<assoc; j++)
      if(cache[i][j].isValid()) {
         if(cache[i][j].getTag() == tag)
         {
            pos = j; 
            break; 
         }
   }
   if(pos == assoc) {
      return NULL;
   }
   else {
      return &(cache[i][pos]); 
   }
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
   line->setSeq(currentCycle);  
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) { 
         return &(cache[i][j]); 
      }   
   }

   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].getSeq() <= min) { 
         victim = j; 
         min = cache[i][j].getSeq();}
   } 

   assert(victim != assoc);
   
   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{ 
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);
   
   // if(victim->getFlags() == DIRTY) {
   if(victim->getCoherenceState() == MCIStates.M || victim->getCoherenceState() == DGNStates.Sm || victim->getCoherenceState() == DGNStates.M) {
      writeBack(addr);
      MemoryTxInc();
   }
      
   tag = calcTag(addr);   
   victim->setTag(tag);
   victim->setFlags(VALID);    // check if this has to be done here or outside
   /**note that this cache line has been already 
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

void Cache::SnoopMCI(ulong addr, uchar op, ulong protocol, int signal)
{
   cacheLine * line = findLine(addr);
   #ifdef _DEBUG
      ulong tag = calcTag(addr);
      ulong index = calcIndex(addr);
      printf("\t\ttag:%lu index:%lu\n", tag, index);
   #endif
   if(!(line == NULL)) { // Either M or C state
      #ifdef _DEBUG
         printf("\t\tsignal: %d current State: %d\n", signal, line->getCoherenceState());
      #endif
      // Based on whether M or C state respond to the signal
      if (line->getCoherenceState() == MCIStates.M) {
         #ifdef _DEBUG
            printf("\t\tFLAG:%lu\n", line->getFlags());
         #endif
         if (signal == busTxMCI.BusRd || signal == busTxMCI.BusRdX) {
            line->setCoherenceState(MCIStates.I);
            writeBack(addr);
            // Increment counters
            // flush and increase memory transaction counter
            FlushInc();
            MemoryTxInc();
            // invalidate and increase invalidation counter
            InvalidateInc();
            line->invalidate();
         }
         #ifdef _DEBUG
            printf("\t\tcurrent State moved to: %d\n", line->getCoherenceState());
         #endif
      }
      if (line->getCoherenceState() == MCIStates.C) {
         #ifdef _DEBUG
            printf("\t\tsignal: %d current State: %d\n", signal, line->getCoherenceState());
            printf("\t\tFLAG:%lu\n", line->getFlags());
         #endif
         if (signal == busTxMCI.BusRd || signal == busTxMCI.BusRdX) {
            line->setCoherenceState(MCIStates.I);
            // Increment counters
            // invalidate and increase invalidation counter
            InvalidateInc();
            line->invalidate();
         }
         #ifdef _DEBUG
            printf("\t\tcurrent State moved to: %d\n", line->getCoherenceState());
         #endif
      }
   }
   else {
      #ifdef _DEBUG
         printf("\t\tinitial stage current State: %d\n", 1);
      #endif
   }
}

void Cache::SnoopDGN(ulong addr, uchar op, ulong protocol, int signal)
{
   cacheLine * line = findLine(addr);
   #ifdef _DEBUG
      ulong tag = calcTag(addr);
      ulong index = calcIndex(addr);
      printf("\t\ttag:%lu index:%lu\n", tag, index);
   #endif
   if(line != NULL) {
      #ifdef _DEBUG
         printf("\t\tsignal: %d current State: %d\n", signal, line->getCoherenceState());
      #endif
      if (line->getCoherenceState() == DGNStates.E) { // Exclusive state
         // only BusRd can be snooped. BusUpd can't be snooped as there are no other copies
         if (signal == busTxDGN.BusRd) {
            // another processor might've suffered a read miss
            // cache line will be supplied to the requestor by main memory
            // Move to Sc State
            line->setCoherenceState(DGNStates.Sc);
            InterventionInc();
            #ifdef _DEBUG
               printf("\t\tcurrent State moved to: %d\n", line->getCoherenceState());
               printf("\t\tIntervention encountered %lu\n", getInterventions());
            #endif
         }
      }
      else if (line->getCoherenceState() == DGNStates.Sc) { // Shared Clean State
         if (signal == busTxDGN.BusRd) {
            // another processor suffered a read miss
            // block will be supplied by the main memory or the owner
            // remain in the same state
            line->setCoherenceState(DGNStates.Sc);
            #ifdef _DEBUG
               printf("\t\tcurrent State moved to: %d\n", line->getCoherenceState());
            #endif
         }
         else if (signal == busTxDGN.BusUpd) {
            // Update the cache
            // State remains the same
            line->setCoherenceState(DGNStates.Sc);
            #ifdef _DEBUG
               printf("\t\tcurrent State moved to: %d\n", line->getCoherenceState());
               printf("\t\tBusUpdate encountered; Cache line updated\n");
            #endif
         }
      }
      else if (line->getCoherenceState() == DGNStates.Sm) { // Shared modified state
         // owner of the line/block
         if (signal == busTxDGN.BusRd) {
            line->setCoherenceState(DGNStates.Sm);
            FlushInc();
            writeBack(addr);
            MemoryTxInc();
            #ifdef _DEBUG
               printf("\t\tcurrent State moved to: %d\n", line->getCoherenceState());
               printf("\t\tFlush to the bus %lu\n", getFlushes());
            #endif
         }
         else if (signal == busTxDGN.BusUpd) {
            line->setCoherenceState(DGNStates.Sc);
            // Merge the update on the cache line
            #ifdef _DEBUG
               printf("\t\tcurrent State moved to: %d\n", line->getCoherenceState());
               printf("\t\tBusUpdate encountered; Cache line updated\n");
            #endif
         }
      }
      else if (line->getCoherenceState() == DGNStates.M) { // Modified state
         // this copy is the only valid copy; only BusRd can be snooped
         if (signal == busTxDGN.BusRd) {
            line->setCoherenceState(DGNStates.Sm);
            InterventionInc();
            FlushInc();
            writeBack(addr);
            MemoryTxInc();
            #ifdef _DEBUG
               printf("\t\tcurrent State moved to: %d\n", line->getCoherenceState());
               printf("\t\tIntervention encountered %lu\n", getInterventions());
               printf("\t\tFlush to the bus %lu\n", getFlushes());
            #endif
         }
      }
   }
   else {
      // cache does not have the line; ignore any snoop transaction
      #ifdef _DEBUG
         printf("\t\tNo cache line found, Snoop has nothing to do.\n");
      #endif
   }
}

void Cache::printStats(ulong proc, ulong protocol)
{ 
   printf("============ Simulation results (Cache %lu) ============\n", proc);
   /****print out the rest of statistics here.****/
   /****follow the ouput file format**************/
   printf("01. number of reads:                            %lu\n", getReads());
   printf("02. number of read misses:                      %lu\n", getRM());
   printf("03. number of writes:                           %lu\n", getWrites());
   printf("04. number of write misses:                     %lu\n", getWM());
   printf("05. total miss rate:                            %.2f%%\n", getMissRate());
   printf("06. number of writebacks:                       %lu\n", getWB());
   printf("07. number of memory transactions:              %lu\n", getMemTx());
   if (protocol == 0) {
      printf("08. number of invalidations:                    %lu\n", getInvalidations());
   }
   else if (protocol == 1) {
      printf("08. number of interventions:                    %lu\n", getInterventions());
   }

   printf("09. number of flushes:                          %lu\n", getFlushes());

   if (protocol == 0) {
      printf("10. number of BusRdX:                           %lu\n", getBusRdX());
   }
   else if (protocol == 1) {
      printf("10. number of Bus Transactions(BusUpd):         %lu\n", getBusUpd());
   }
}
