#include "processor.h"
#include "architecture.h"
#include "cache.h"

#include <memory>


namespace Processor {

CPU::CPU(std::array<std::vector<Architecture::Instruction>, Architecture::NUM_CORES>&& instructionsByCore, Cache::COHERENCE_PROTOCOL protocol) {
  for (int i = 0; i < Architecture::NUM_CORES; ++i) {
    m_cores[i].instructions.swap(instructionsByCore[i]);
  }
  if (protocol == Cache::MESI) {
    m_memorySystemPtr = std::make_unique<Cache::MesiMemorySystem>();
  } else if (protocol == Cache::DRAGON) {
    m_memorySystemPtr = std::make_unique<Cache::DragonMemorySystem>();
  }
}

bool CPU::isFinishedExecuting() const {
  for (const Core& core : m_cores) {
    if (core.state != COMPLETED) {
      return false;
    }
  }
  return true;
}

void CPU::simulate() {
  Architecture::GlobalCycleCounter::initialiseCounter();
  std::vector<Cache::MemoryRequest> pendingMemoryRequests;
  std::vector<Cache::MemoryRequest> completedMemoryRequests;
  while (!isFinishedExecuting()) {
    // Reset vectors
    pendingMemoryRequests.clear();
    completedMemoryRequests.clear();
    // Update Instructions
    for (int coreIdx = 0; coreIdx < Architecture::NUM_CORES; ++coreIdx) {
      Core& core = m_cores[coreIdx];
      if (core.state == COMPLETED) {
        continue; // do nothing if already completed
      }

      if (core.state == LOADING) {
        // core has finished executing, set to completed and continue
        if (core.currInst >= core.instructions.size()) {
          core.state = COMPLETED;
          continue;
        }
        if (core.instructions[core.currInst].instType == Architecture::COMPUTE) {
          ++Architecture::GlobalReport::numComputeInstructions[coreIdx];
          core.state = EXECUTING;
        } else if (core.instructions[core.currInst].instType == Architecture::LOAD || core.instructions[core.currInst].instType == Architecture::STORE) {
          ++Architecture::GlobalReport::numLoadStoreInstructions[coreIdx];
          core.state = BLOCKED;
        }
      }

      Architecture::Instruction& instruction = core.instructions[core.currInst];
      ++instruction.executionCycles; // increment execution cycles of instruction
      if (core.state == EXECUTING) {
        if (instruction.executionCycles >= instruction.computeCycles) { // complete execution of compute
          Architecture::GlobalReport::computeCycles[coreIdx] += instruction.executionCycles;
          core.state = LOADING; // set state to be loading nxt instruction
          ++core.currInst;
        }
      }

      if (core.state == BLOCKED) {
        pendingMemoryRequests.emplace_back(coreIdx, instruction.instType, instruction.dataAddress);
      }    
    }

    m_memorySystemPtr->tickMemorySystem(pendingMemoryRequests, completedMemoryRequests);

    // Increment to next instruction for finished memory requests
    for (const Cache::MemoryRequest& request : completedMemoryRequests) {
      Core& core = m_cores[request.coreNum];
      // Report idle cycles
      {
        Architecture::GlobalReport::idleCycles[request.coreNum] += core.instructions[core.currInst].executionCycles;

      }
      core.state = LOADING;
      ++core.currInst;
    }
    Architecture::GlobalCycleCounter::incrementCounter(); // increment global cycle Counter
  }
  Architecture::GlobalReport::overallExecutionCycles = Architecture::GlobalCycleCounter::getCounter();
}

} //namespace