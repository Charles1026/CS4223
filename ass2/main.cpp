#include "cache.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "architecture.h"
#include "cache.h"
#include "processor.h"

namespace {
  inline bool parseStringToInt(char str[], int& i) {
    try {
      i = std::stoi(str);
    } catch (const std::exception& e) {
      return false;
    }
    return true;
  }
}


int main(int argc, char *argv[]) {
  if (argc < 6) {
    std::fprintf(stderr, "Invalid Usage, please input ./coherence <protocol> <input_file> <cache_size> <associativity> <block_size> [data_folder]\n");
    return 1;
  }

  // Parse protocol
  // TODO case insensitive parsing
  Cache::COHERENCE_PROTOCOL protocol;
  if (!std::strcmp(argv[1], Cache::MESI_STRING)) {
    protocol = Cache::MESI;
  } else if (!std::strcmp(argv[1], Cache::DRAGON_STRING)) {
    protocol = Cache::DRAGON;
  } else {
    std::fprintf(stderr, "Error: Only %s or %s protocols allowed\n", Cache::MESI_STRING, Cache::DRAGON_STRING);
    return 1;
  }

  // Parse cache size
  int cacheSize;
  if (!parseStringToInt(argv[3], cacheSize)) {
    std::fprintf(stderr, "Error: Failed to parse %s into cache size\n", argv[3]);
    return 1;
  }

  // Associativity
  int associativity;
  if (!parseStringToInt(argv[4], associativity)) {
    std::fprintf(stderr, "Error: Failed to parse %s into associativty\n", argv[4]);
    return 1;
  }

  // Block Size
  int blockSize;
  if (!parseStringToInt(argv[5], blockSize)) {
    std::fprintf(stderr, "Error: Failed to parse %s into block size\n", argv[5]);
    return 1;
  }

  Cache::MemorySystem::initialiseStaticCacheVariables(cacheSize, associativity, blockSize); // initialise static cache sizing variables, needed in l1 cache constructor

  // Get data folder if applicable
  std::filesystem::path dataFolder;
  if (argc >= 7) {
    dataFolder = argv[6];
  } else {
    dataFolder = std::filesystem::current_path() / Architecture::DEFAULT_DATA_FOLDER;
  }

  // Parse input file
  std::string inputFileName = argv[2];
  std::array<std::vector<Architecture::Instruction>, Architecture::NUM_CORES> instructionsByCore;
  if (!Architecture::loadInstructionsFromFiles(dataFolder, inputFileName, instructionsByCore)) {
    std::fprintf(stderr, "Error: Failed to parse input file(s) %s\n", argv[2]);
    return 1;
  }

  Processor::CPU cpu(std::move(instructionsByCore), protocol);

  std::cout << "Simulating" << std::endl;
  cpu.simulate();

  std::cout << Architecture::printGlobalReport << std::endl;
}