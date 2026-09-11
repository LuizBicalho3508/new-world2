#pragma once

// UE 5.8 declares UPCGGraphInterface in PCGGraph.h.
// This forwarding header keeps the world manager include explicit and prevents
// accidental dependence on a non-existent engine header named PCGGraphInterface.h.
#include "PCGGraph.h"
