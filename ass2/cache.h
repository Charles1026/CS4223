#pragma once
#include <cstdint>
#include <queue>
#include <string>
#include <vector>

#include "architecture.h"

namespace Cache {
constexpr int L1_CACHE_HIT_CYCLES = 1;
constexpr int L1_CACHE_LOAD_FROM_MEM_CYCLES = 100;
constexpr int L1_CACHE_WRITE_BACK_CYCLES = 100;
constexpr int L1_CACHE_LOAD_FROM_BUS_PER_WORD_CYCLES = 2;
constexpr char MESI_STRING[] = "MESI";
constexpr char DRAGON_STRING[] = "DRAGON";
enum COHERENCE_PROTOCOL: uint8_t {
  MESI,
  DRAGON
};

enum CACHELINE_STATE {
  INVALID = 0,
  EXCLUSIVE = 1, // MESI
  SHARED = 2, // MESI
  MODIFIED = 3, // MESI
};

std::string toString(CACHELINE_STATE state);

struct CacheLine {
  uint32_t tag;
  int lastUsed = -1;
  CACHELINE_STATE state = INVALID;
};

struct MemoryRequest {
  int coreNum; 
  Architecture::INSTRUCTION_TYPE type; // only read or write
  uint32_t address;

  MemoryRequest(const int coreNum, const Architecture::INSTRUCTION_TYPE type, const uint32_t address) : coreNum(coreNum), type(type), address(address) {}
};

struct BusTransaction {
  enum TRANSACTION_TYPE {
    BUS_RD,
    BUS_RD_X
  };
  TRANSACTION_TYPE type;
  MemoryRequest request;
  uint32_t setIdx;
  int blockIdx;
  bool processed = false;
  int remainingCycles = 0;

  BusTransaction(TRANSACTION_TYPE type, const MemoryRequest& request, const uint32_t setIdx, const int blockIdx, const int remainingCycles = 0) 
      : type(type), request(request), setIdx(setIdx), blockIdx(blockIdx), remainingCycles(remainingCycles) {}
};


class MemorySystem {
public: // static
  // call this before creating any objects to ensure the correct number of cache lines etc are constructed
  static bool initialiseStaticCacheVariables(const int cacheSize, const int associativity, const int blockSize);

protected: // static
  static constexpr int INVALID_BLOCK_IDX = -1; 

  static COHERENCE_PROTOCOL protocol;
  static int cacheSize;
  static int associativity;
  static int blockSize;
  static int numBlocks;
  static int numSets;
  static int wordsPerBlock;
  static uint32_t blockOffsetMask;
  static uint32_t setIdxMask;
  static uint32_t tagMask;

public:
  MemorySystem();

  void tickMemorySystem(const std::vector<MemoryRequest>& incomingMemoryRequests, std::vector<MemoryRequest>& completedMemoryRequests);

protected:
  // If exists in cache returns {setIdx, blockIdx} else blockIdx = -1 
  std::pair<uint32_t, int> findInCache(int cacheNum, uint32_t address) const;
  // Finds the block index of the block to replace by LRU
  int findBlockIdxToReplace(const int coreNum, const uint32_t setIdx) const;

  // Resolves request if no need for bus transaction, else adds to the bus transaction queue
  virtual void handleIncomingRequest(const MemoryRequest& request, std::vector<MemoryRequest>& completedMemoryRequests) = 0;
  virtual void processBusTransaction(BusTransaction& transaction) = 0;

  // For Report
  int getAndLog_L1_CACHE_LOAD_FROM_MEM_CYCLES() {
    Architecture::GlobalReport::busDataTrafficBytes += blockSize;
    return L1_CACHE_LOAD_FROM_MEM_CYCLES;
  }

  int getAndLog_L1_CACHE_WRITE_BACK_CYCLES() {
    Architecture::GlobalReport::busDataTrafficBytes += blockSize;
    return L1_CACHE_WRITE_BACK_CYCLES;
  }

  int getAndLog_L1_CACHE_LOAD_WORD_FROM_BUS_CYCLES() {
    Architecture::GlobalReport::busDataTrafficBytes += Architecture::WORD_SIZE_BYTES;
    return L1_CACHE_LOAD_FROM_BUS_PER_WORD_CYCLES;
  }

  int getAndLog_L1_CACHE_LOAD_BLOCK_FROM_BUS_CYCLES() {
    Architecture::GlobalReport::busDataTrafficBytes += blockSize;
    return L1_CACHE_LOAD_FROM_BUS_PER_WORD_CYCLES * wordsPerBlock;
  }

protected:
  std::array<std::vector<std::vector<CacheLine>>, Architecture::NUM_CORES> l1Caches;
  std::queue<BusTransaction> queuedBusTransactions;
};

class MesiMemorySystem : public MemorySystem {
public:
  MesiMemorySystem() = default;

protected:
  void handleIncomingRequest(const MemoryRequest& request, std::vector<MemoryRequest>& completedMemoryRequests) override;
  void processBusTransaction(BusTransaction& transaction) override;

};

class DragonMemorySystem : public MemorySystem {
public:
  DragonMemorySystem() = default;

protected:
  void handleIncomingRequest(const MemoryRequest& request, std::vector<MemoryRequest>& completedMemoryRequests) override;
  void processBusTransaction(BusTransaction& transaction) override;

};

}