#include "architecture.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <format>
#include <thread>

namespace Archi = Architecture;

namespace {
void parseInstructionsFromFile(const std::string file, std::vector<Architecture::Instruction>& instructions, bool& success) {
  success = false; // start with fail, set to true if all done
  std::ifstream fileStream(file);
  if (!fileStream.is_open()) {
    std::fprintf(stderr, "Failed to open file %s\n", file.c_str());
    return;
  }

  std::string label, value;
  Archi::INSTRUCTION_TYPE type;
  int intValue;
  while (!fileStream.eof()) {
    fileStream >> label >> value;
    // Parse label
    try {
      type = static_cast<Archi::INSTRUCTION_TYPE>(std::stoi(label));
    } catch (const std::exception& e) {
      std::fprintf(stderr, "Failed to parse label %s: %s\n", label.c_str(), e.what());
      return;
    }

    if (!(type == Archi::LOAD || type == Archi::STORE || type == Archi::COMPUTE)) {
      std::fprintf(stderr, "Invalid label %s\n", label.c_str());
      return;
    }

    // Parse value
    try {
      intValue = std::stoi(value, nullptr, 16);
    } catch (const std::exception& e) {
      std::fprintf(stderr, "Failed to parse value %s: %s\n", value.c_str(), e.what());
      return;
    }

    instructions.emplace_back(type, intValue);
  }
  success = true; // successfully parsed
}
} // anonymous namespace

namespace Architecture {
int GlobalCycleCounter::counter = 0;
int GlobalReport::overallExecutionCycles = 0;
std::array<int, Architecture::NUM_CORES> GlobalReport::numComputeInstructions;
std::array<int, Architecture::NUM_CORES> GlobalReport::computeCycles;
std::array<int, Architecture::NUM_CORES> GlobalReport::numLoadStoreInstructions;
std::array<int, Architecture::NUM_CORES> GlobalReport::idleCycles;
std::array<int, Architecture::NUM_CORES> GlobalReport::numCacheHits;
std::array<int, Architecture::NUM_CORES> GlobalReport::numCacheMisses;
int GlobalReport::busDataTrafficBytes = 0;
int GlobalReport::busInvalidationsOrUpdates = 0;
int GlobalReport::numPrivateAccess = 0;
int GlobalReport::numSharedAccess = 0;

void GlobalReport::clearReport() {
  overallExecutionCycles = 0;
  numComputeInstructions.fill(0);
  computeCycles.fill(0);
  numLoadStoreInstructions.fill(0);
  idleCycles.fill(0);
  numCacheHits.fill(0);
  numCacheMisses.fill(0);
  busDataTrafficBytes = 0;
  busInvalidationsOrUpdates = 0;
  numPrivateAccess = 0;
  numSharedAccess = 0;
}

std::ostream& printGlobalReport(std::ostream& os) {
  os.precision(5);
  os << "Report:\nOverall Execution Cycles: " << GlobalReport::overallExecutionCycles << '\n';
  for (int coreNum = 0; coreNum < NUM_CORES ; ++coreNum) {
    os << "Core " << coreNum << '\n';
    os << "\tTotal Instructions: " << GlobalReport::numComputeInstructions[coreNum] + GlobalReport::numLoadStoreInstructions[coreNum] << '\n';
    os << "\t\tNum Compute Inst: " << GlobalReport::numComputeInstructions[coreNum] << '\n';
    os << "\t\tNum Load Store Inst: " << GlobalReport::numLoadStoreInstructions[coreNum] << '\n';

    os << "\tTotal Execution Cycles: " << GlobalReport::computeCycles[coreNum] + GlobalReport::idleCycles[coreNum] << '\n';
    os << "\t\tCompute Cycles: " << GlobalReport::computeCycles[coreNum] << '\n';
    os << "\t\tIdle Cycles: " << GlobalReport::idleCycles[coreNum] << '\n';

    os << "\tCache Hit Rate: " << float(GlobalReport::numCacheHits[coreNum]) / float(GlobalReport::numCacheHits[coreNum] + GlobalReport::numCacheMisses[coreNum]) << '\n';
    os << "\t\tNum Cache Hits: " << GlobalReport::numCacheHits[coreNum] << '\n';
    os << "\t\tNum Cache Misses: " << GlobalReport::numCacheMisses[coreNum] << '\n';
  }
  os << '\n';
  os << "Total Bus Data Traffic (Bytes): " << GlobalReport::busDataTrafficBytes << '\n';
  os << "Total Bus Invalidations/Updates: " << GlobalReport::busInvalidationsOrUpdates << '\n';  
  os << "Total Private Data Access: " << GlobalReport::numPrivateAccess << '\n';
  os << "Total Shared Data Access: " << GlobalReport::numSharedAccess << '\n';
  float totalDataAccess = GlobalReport::numPrivateAccess + GlobalReport::numSharedAccess;
  os << "Private Data Access Rate: " << float(GlobalReport::numPrivateAccess) / totalDataAccess << '\n';
  os << "Shared Data Access Rate: " << float(GlobalReport::numSharedAccess) / totalDataAccess;
  
  return os;
}


bool loadInstructionsFromFiles(const std::filesystem::path& directory, const std::string& fileName, std::array<std::vector<Architecture::Instruction>, NUM_CORES>& instructionsByCore)  {
  std::cout << "Loading Instructions...\n";
  std::array<std::string, NUM_CORES>paths;
  std::array<std::thread, NUM_CORES> loadThreads;
  std::array<bool, NUM_CORES> successes;
  successes.fill(false);
  for (int coreNum = 0; coreNum < NUM_CORES; ++coreNum) {
    std::filesystem::path filePath = directory / std::format("{}_{}.data", fileName, coreNum);
    paths[coreNum] = filePath.string(); 
    loadThreads[coreNum] = std::move(std::thread(parseInstructionsFromFile, paths[coreNum], std::ref(instructionsByCore[coreNum]), std::ref(successes[coreNum])));
  }

  bool success = true;
  for (int coreNum = 0; coreNum < NUM_CORES; ++coreNum) {
    loadThreads[coreNum].join();
    if (successes[coreNum]) {
      std::cout << "Core " << coreNum << " loaded " << instructionsByCore[coreNum].size() << " instructions from " << paths[coreNum] << '\n';
    }
    else {
      std::cout << "Core " << coreNum << " failed to load instructions from " << paths[coreNum] << '\n';
    }
    success &= successes[coreNum];
  }
  return success;
}

} // namespace
