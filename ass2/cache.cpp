#include "cache.h"
#include "architecture.h"

#include <cmath>
#include <cstdio>
#include <limits>


namespace Cache {

// Define static variables
COHERENCE_PROTOCOL protocol = MESI;
int MemorySystem::cacheSize = 0;
int MemorySystem::associativity = 0;
int MemorySystem::blockSize = 0;
int MemorySystem::numBlocks = 0;
int MemorySystem::numSets = 0;
int MemorySystem::wordsPerBlock = 0;
int MemorySystem::blockOffsetRShiftBits = 0;
uint32_t MemorySystem::blockOffsetMask = 0;
int MemorySystem::setIdxRShiftBits = 0;
uint32_t MemorySystem::setIdxMask = 0;
int MemorySystem::tagRShiftBits = 0;
uint32_t MemorySystem::tagMask = 0;

std::string toString(CACHELINE_STATE state) {
  switch (state) {
    case INVALID:
      return "Invalid";
    case EXCLUSIVE:
      return "Exclusive";
    case SHARED:
      return "Shared";
    case MODIFIED:
      return "Modified";
  }
  return "Unknown State";
}


bool MemorySystem::initialiseStaticCacheVariables(const int cacheSize, const int associativity, const int blockSize) {
  MemorySystem::cacheSize = cacheSize;
  MemorySystem::associativity = associativity;
  MemorySystem::blockSize = blockSize;
  wordsPerBlock = blockSize / Architecture::WORD_SIZE_BYTES;

  // Calculate cache Params
  if (cacheSize % blockSize != 0) {
    std::fprintf(stderr, "Error: Cache size(%d) must be a multiple of block size(%d)\n", cacheSize, blockSize);
    return false;
  }
  numBlocks = cacheSize / blockSize;

  if (numBlocks % associativity != 0) {
    std::fprintf(stderr, "Error: Number of Cache Lines(%d) must be a multiple of associativity(%d)\n", numBlocks, associativity);
    return false;
  }
  numSets = numBlocks / associativity;

  // Create Block Offset Mask
  const int numBlockOffsetBits = std::log2(blockSize);
  blockOffsetMask = 0;
  for (int i = 0; i < numBlockOffsetBits; ++i) {
    blockOffsetMask += std::pow(2, i);
  }
  blockOffsetRShiftBits = 0;

  // Create Set Idx Mask
  const int numSetIndexBits = std::log2(numSets);
  setIdxMask = 0;
  for (int i = numBlockOffsetBits; i < numBlockOffsetBits + numSetIndexBits; ++i) {
    setIdxMask += std::pow(2, i);
  }
  setIdxRShiftBits = numBlockOffsetBits;


  // Create Tag Mask
  tagMask = 0;
  for (int i = numBlockOffsetBits + numSetIndexBits; i < Architecture::ADDRESS_SPACE_BIT_SIZE; ++i) {
    tagMask += std::pow(2, i);
  }
  tagRShiftBits = numBlockOffsetBits + numSetIndexBits;

  return true;
}

inline uint32_t MemorySystem::getBlockOffset(const uint32_t address) {
  return (address & blockOffsetMask) >> blockOffsetRShiftBits;
}

inline uint32_t MemorySystem::getSetIdx(const uint32_t address) {
  return (address & setIdxMask) >> setIdxRShiftBits;

}

inline uint32_t MemorySystem::getTag(const uint32_t address) {
  return (address & tagMask) >> tagRShiftBits;
}

MemorySystem::MemorySystem() {
  // Initialise caches to the right size
  for (std::vector<std::vector<CacheLine>>& cache : m_l1Caches) {
    cache = std::vector<std::vector<CacheLine>>(numSets, std::vector<CacheLine>(associativity));
  }
  printf("Initialised %zu L1 Cache(s) of %d bytes with %d associativity, %d blocks of %d bytes or %d words, grouped into %d sets.\n", m_l1Caches.size(), cacheSize, associativity, numBlocks, blockSize, wordsPerBlock, numSets);
}

void MemorySystem::tickMemorySystem(const std::vector<MemoryRequest>& incomingMemoryRequests, std::vector<MemoryRequest>& completedMemoryRequests) {
  // Handle incoming requests
  for (const auto& request : incomingMemoryRequests) {
    handleIncomingRequest(request);
  }

  // Process Executing Non Bus Memory Requests
  if (!m_executingNonBusRequests.empty()) {
    std::vector<std::pair<MemoryRequest, int>> newExecuting;
    for (int i = 0; i < m_executingNonBusRequests.size(); ++i) {
      auto& [request, remainingCycles] = m_executingNonBusRequests[i];
      --remainingCycles;
      if (remainingCycles <= 0) {
        completedMemoryRequests.push_back(request);
      } else {
        newExecuting.emplace_back(request, remainingCycles);
      }
    }
    m_executingNonBusRequests = std::move(newExecuting); // override with new list
  }

  // Handle bus transaction
  if (!m_queuedBusTransactions.empty()) {
    BusTransaction& currBusTransaction = m_queuedBusTransactions.front();
    if (!currBusTransaction.processed) { // new bus transaction process it
      processBusTransaction(currBusTransaction);
    } 
  
    --currBusTransaction.remainingCycles; // execute 1 cycle of the curr bus transaction
  
    if (currBusTransaction.remainingCycles == 0) { // curr bus transaction completed add to completed and remove from queue
      completedMemoryRequests.push_back(currBusTransaction.request);
      m_queuedBusTransactions.pop();
    }
  }
}

std::pair<uint32_t, int> MemorySystem::findInCache(int cacheNum, uint32_t address) const {
  uint32_t setIdx = getSetIdx(address);
  uint32_t tag = getTag(address);
  const std::vector<CacheLine>& set = m_l1Caches[cacheNum][setIdx];
  for (int i = 0; i < set.size(); ++i) {
    if ((set[i].tag == tag) && (set[i].state != INVALID)) {
      return {setIdx, i};
    }
  }
  return {setIdx, INVALID_BLOCK_IDX};
}

int MemorySystem::findBlockIdxToReplace(const int coreNum, const uint32_t setIdx) const {
  int earliestLastUsed = std::numeric_limits<int>::max();
  int minIdx = -1;
  for (int blockIdx = 0; blockIdx < associativity; ++blockIdx) {
    const CacheLine& cacheLine = m_l1Caches[coreNum][setIdx][blockIdx];
    if (cacheLine.state == INVALID) { // return immediately if there is an invalid cache line
      return blockIdx;
    }
    if (cacheLine.lastUsed < earliestLastUsed) { // else if less than curr earliest used, replace it
      earliestLastUsed = cacheLine.lastUsed;
      minIdx = blockIdx;
    }
  }
  return minIdx; // return least recently used block
}

void MesiMemorySystem::handleIncomingRequest(const MemoryRequest& request) {
  auto [setIdx, blockIdx] = findInCache(request.coreNum, request.address);
  ////// in cache //////
  if (blockIdx != INVALID_BLOCK_IDX) {
    ++Architecture::GlobalReport::numCacheHits[request.coreNum];

    CacheLine& cacheLine = m_l1Caches[request.coreNum][setIdx][blockIdx];
    // Load Request: All loads from valid cache lines happen without bus transaction and state change
    if (request.type == Architecture::INSTRUCTION_TYPE::LOAD) {
      if (cacheLine.state == SHARED) {
        ++Architecture::GlobalReport::numSharedAccess;
      } else {
        ++Architecture::GlobalReport::numPrivateAccess;
      }

      cacheLine.lastUsed = Architecture::GlobalCycleCounter::getCounter(); // set last used to now
      m_executingNonBusRequests.emplace_back(request, L1_CACHE_HIT_CYCLES);
      return;
    }

    // Exclusive/Modified State Store Request: can write and return immediately
    if (cacheLine.state == EXCLUSIVE || cacheLine.state == MODIFIED) {
      ++Architecture::GlobalReport::numPrivateAccess;

      cacheLine.state = MODIFIED; // write changes exclusive to modified state, modified stays as modified
      cacheLine.lastUsed = Architecture::GlobalCycleCounter::getCounter(); // set last used to now
      m_executingNonBusRequests.emplace_back(request, L1_CACHE_HIT_CYCLES);
      return;
    }

    // Shared State Store Request: Need to invalidate all other cache lines through bus transaction, add to bus transaction queue
    ++Architecture::GlobalReport::numSharedAccess;

    m_queuedBusTransactions.emplace(BusTransaction::BUS_RD_X, request, setIdx, blockIdx);
    return;
  }

  ////// not in cache //////
  ++Architecture::GlobalReport::numCacheMisses[request.coreNum];

  blockIdx = findBlockIdxToReplace(request.coreNum, setIdx);
  CacheLine& cacheLine = m_l1Caches[request.coreNum][setIdx][blockIdx];
  // We evict the cache line and prepare to load in the new cache line through a bus transaction
  int startingCycles = 0;
  if (cacheLine.state == MODIFIED) { // if modified, we need to write back the dirty cache line first, so we add the cycles to the initial cycles needed
    startingCycles += getAndLog_L1_CACHE_WRITE_BACK_CYCLES();
  }
  cacheLine.tag = getTag(request.address); // set tag
  cacheLine.state = INVALID; // set state
  // Enqueue bus transaction
  m_queuedBusTransactions.emplace((request.type == Architecture::INSTRUCTION_TYPE::LOAD) ? BusTransaction::BUS_RD : BusTransaction::BUS_RD_X, request, setIdx, blockIdx, startingCycles);
}

void MesiMemorySystem::processBusTransaction(BusTransaction& transaction) {
  int initiatingCoreIdx = transaction.request.coreNum;
  CacheLine& cacheLine = m_l1Caches[initiatingCoreIdx][transaction.setIdx][transaction.blockIdx];
  
  // Bus Read Transaction only possible if loading from an invalid state
  if (transaction.type == BusTransaction::BUS_RD) {
    // check other caches for a valid copy of the cache line
    bool foundOtherCopy = false;
    for (int otherCoreIdx = 0; otherCoreIdx < Architecture::NUM_CORES; ++otherCoreIdx) {
      if (otherCoreIdx == initiatingCoreIdx) continue; // dont check self
      auto [otherSetIdx, otherBlockIdx] = findInCache(otherCoreIdx, transaction.request.address);
      if (otherBlockIdx == INVALID_BLOCK_IDX) continue; // not in the other cache, continue
      CacheLine& otherCacheLine = m_l1Caches[otherCoreIdx][otherSetIdx][otherBlockIdx]; // get other cache line

      // NOTE: FALLTHROUGHS HERE ARE INTENTIONAL FOR THE LOGIC 
      switch (otherCacheLine.state) {
      case MODIFIED: // we need to write back the dirty cache line
        transaction.remainingCycles += getAndLog_L1_CACHE_WRITE_BACK_CYCLES();

      case EXCLUSIVE:
      case SHARED:
        // All 3 states need to share their cache line with the requesting cache
        transaction.remainingCycles += getAndLog_L1_CACHE_LOAD_BLOCK_FROM_BUS_CYCLES() + L1_CACHE_HIT_CYCLES;
        break;

      default:
        fprintf(stderr, "Error at %s, %s: invalid other cache state reached!", __FILE__, __func__);
        break;
      }
      ++Architecture::GlobalReport::numSharedAccess;

      cacheLine.state = SHARED; // transition self state to shared
      otherCacheLine.state = SHARED; // the other cache line transitions to shared from any of modified/exclusive/shared
      foundOtherCopy = true;
      break; // if we reach here means we have obtained a copy from a cache already, no need to continue search 
    }

    // didnt find other copy, need to load from memory
    if (!foundOtherCopy) {
      ++Architecture::GlobalReport::numPrivateAccess;

      cacheLine.state = EXCLUSIVE; // transition self state to exclusive
      transaction.remainingCycles += getAndLog_L1_CACHE_LOAD_FROM_MEM_CYCLES() + L1_CACHE_HIT_CYCLES;
    }
  } 

  // Bus Read X Transaction possible through storing from invalid or shared state
  else {
    ++Architecture::GlobalReport::busInvalidationsOrUpdates;
    ++Architecture::GlobalReport::numPrivateAccess;

    // There could be other caches holding this line, need to invalidate all of them first
    bool foundOtherCopy = false;
    for (int otherCoreIdx = 0; otherCoreIdx < Architecture::NUM_CORES; ++otherCoreIdx) {
      if (otherCoreIdx == initiatingCoreIdx) continue; // dont check self
      auto [otherSetIdx, otherBlockIdx] = findInCache(otherCoreIdx, transaction.request.address);
      if (otherBlockIdx == INVALID_BLOCK_IDX) continue; // not in the other cache, continue
      CacheLine& otherCacheLine = m_l1Caches[otherCoreIdx][otherSetIdx][otherBlockIdx]; // get other cache line

      if (otherCacheLine.state == MODIFIED) { // The other cache line is dirty, we need to write it back to memory
        transaction.remainingCycles += getAndLog_L1_CACHE_WRITE_BACK_CYCLES();
      }
      otherCacheLine.state = INVALID; // invalidate other cache line
      foundOtherCopy = true;
    }

    // if our state is invalid we need to get the cache line either from memory or another cache
    if (cacheLine.state == INVALID) {
      transaction.remainingCycles += foundOtherCopy ? getAndLog_L1_CACHE_LOAD_BLOCK_FROM_BUS_CYCLES() : getAndLog_L1_CACHE_LOAD_FROM_MEM_CYCLES();
    }
    
    cacheLine.state = MODIFIED; // set self to modified state
    transaction.remainingCycles += L1_CACHE_HIT_CYCLES; // 1 cycle writing into cache
  }

  transaction.processed = true; // set to processed 
}

void DragonMemorySystem::handleIncomingRequest(const MemoryRequest& request) {

}

void DragonMemorySystem::processBusTransaction(BusTransaction& transaction) {

}

}