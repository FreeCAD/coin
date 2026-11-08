#ifndef COIN_SOIRCOVERAGEP_H
#define COIN_SOIRCOVERAGEP_H

#ifndef COIN_INTERNAL
#error this is a private header file
#endif

#include <Inventor/SbName.h>

#include <cstddef>
#include <cstdint>
#include <vector>

enum class SoIRApplicationCoverageClass : uint8_t {
  PENDING,
  ADMITTED_COMMANDS,
  EXPLICIT_NON_DRAWING,
  FALLBACK_REQUIRED
};

enum class SoIRCoverageProducerKind : uint8_t {
  APPLICATION_NODE,
  BUILT_IN_SHAPE
};

struct SoIRApplicationCoverageRecord {
  uint64_t nodeId = 0;
  SbName typeName;
  int firstCommand = 0;
  int commandCount = 0;
  SoIRCoverageProducerKind producerKind =
    SoIRCoverageProducerKind::APPLICATION_NODE;
  SoIRApplicationCoverageClass classification =
    SoIRApplicationCoverageClass::PENDING;
};

struct SoIRApplicationCoverageReceipt {
  uint32_t drawListGeneration = 0;
  uint64_t drawListContentRevision = 0;
  uint64_t viewInputRevision = 0;
  uint64_t cameraNodeId = 0;
  size_t recordLimit = 4096;
  bool sealed = false;
  bool overflow = false;
  bool invalid = false;
  std::vector<SoIRApplicationCoverageRecord> records;
};

struct SoIRApplicationCoveragePatchCheckpoint {
  uint32_t drawListGeneration = 0;
  uint64_t drawListContentRevision = 0;
  uint64_t viewInputRevision = 0;
  uint64_t cameraNodeId = 0;
  size_t recordLimit = 0;
  size_t recordCount = 0;
  size_t stackDepth = 0;
  int commandCount = 0;
  bool current = false;
  bool sealed = false;
  bool overflow = false;
  bool invalid = false;
};

#endif // COIN_SOIRCOVERAGEP_H
