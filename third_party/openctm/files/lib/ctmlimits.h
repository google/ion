// Google-authored addition to openctm.

#ifndef __OPENCTM_CTMLIMITS_H_
#define __OPENCTM_CTMLIMITS_H_

#include <stdbool.h>

typedef struct {
  // If non-zero, any attempt to allocate more than maxAllocation bytes
  // during mesh loading will fail with CTM_OUT_OF_MEMORY.
  size_t maxAllocation;
  // If non-zero, any attempt to allocate more than maxFoo foos
  // during mesh loading will fail with CTM_LIMIT_VIOLATION.
  size_t maxVertices;
  size_t maxTriangles;
  size_t maxUVMaps;
  size_t maxAttributeMaps;
  size_t maxStringLength;

  // If a forbid limit is true, and a forbidden feature is present in a
  // serialized mesh, loading will fail with CTM_LIMIT_VIOLATION.
  bool forbidUVMaps;
  bool forbidAttributeMaps;
  bool forbidNormals;
  bool forbidComment;
} CTMlimits;

#endif
