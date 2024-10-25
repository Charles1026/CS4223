#pragma once

#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace Architecture {

constexpr int ADDRESS_SPACE_BIT_SIZE = 32;
constexpr int WORD_SIZE_BYTES = 4;
constexpr char DATA_FOLDER[] = "data";
constexpr int NUM_CORES = 4;

class GlobalCycleCounter {   
public:
  static void initialiseCounter() {counter = 0;}
  static void incrementCounter() {++counter;}
  static int getCounter() {return counter;}

  GlobalCycleCounter() = delete;
private:
  static int counter;
};

struct GlobalReport {
  static int overallExecutionCycles;
  static std::array<int, Architecture::NUM_CORES> numComputeInstructions;
  static std::array<int, Architecture::NUM_CORES> computeCycles;
  static std::array<int, Architecture::NUM_CORES> numLoadStoreInstructions;
  static std::array<int, Architecture::NUM_CORES> idleCycles;
  static std::array<int, Architecture::NUM_CORES> numCacheHits;
  static std::array<int, Architecture::NUM_CORES> numCacheMisses;
  static int busDataTrafficBytes;
  static int busInvalidationsOrUpdates;
  static int numPrivateAccess;
  static int numSharedAccess;

  static void clearReport();
};
std::ostream& printGlobalReport(std::ostream& os);

enum INSTRUCTION_TYPE {
  LOAD = 0,
  STORE = 1,
  COMPUTE = 2
};

struct Instruction {
  const INSTRUCTION_TYPE instType;
  const uint32_t dataAddress; // For Load/Store instructions
  const int computeCycles; // For compute instructions
  int executionCycles = 0;

  Instruction(INSTRUCTION_TYPE type, int value) : instType(type), dataAddress((type == LOAD || type == STORE) ? value : 0), computeCycles((type == COMPUTE) ? value : 0) {}
};

bool loadInstructionsFromFiles(const std::string& fileName, std::array<std::vector<Architecture::Instruction>, NUM_CORES>& instructionsByCore);
} // namespce