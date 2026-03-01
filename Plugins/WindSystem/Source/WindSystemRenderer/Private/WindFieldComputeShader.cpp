// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldComputeShader.h"

// Register the global compute shader with the engine.
// The virtual shader path maps to: Plugins/WindSystem/Shaders/Private/WindFieldCompute.usf
IMPLEMENT_GLOBAL_SHADER(
	FWindFieldComputeCS,
	"/Plugin/WindSystem/Private/WindFieldCompute.usf",
	"MainCS",
	SF_Compute);
