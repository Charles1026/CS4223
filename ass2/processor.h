#pragma once
#include <array>
#include <memory>
#include <vector>

#include "architecture.h"
#include "cache.h"

namespace Processor {
enum EXECUTION_STATE {
  LOADING,
  EXECUTING, // Compute Instruction
  BLOCKED, // Load/Store Instruction
  COMPLETED
};

struct Core {
  std::vector<Architecture::Instruction> instructions;
  int currInst = 0;
  EXECUTION_STATE state = LOADING;
};

class CPU {
public:
  CPU(std::array<std::vector<Architecture::Instruction>, Architecture::NUM_CORES>&& instructionsByCore, Cache::COHERENCE_PROTOCOL protocol);

  bool isFinishedExecuting() const;

  void simulate();


private:
  std::array<Core, Architecture::NUM_CORES> m_cores;
  std::unique_ptr<Cache::MemorySystem> m_memorySystemPtr;
};
} // Processor namespace