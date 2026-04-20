// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldShaders.h"

IMPLEMENT_GLOBAL_SHADER(
	FWindFieldAdvectionCS,
	"/Plugin/WindSystem/Private/WindFieldAdvection.usf",
	"MainCS",
	SF_Compute);

IMPLEMENT_GLOBAL_SHADER(
	FWindFieldForceCS,
	"/Plugin/WindSystem/Private/WindFieldForce.usf",
	"MainCS",
	SF_Compute);

IMPLEMENT_GLOBAL_SHADER(
	FWindFieldDiffusionCS,
	"/Plugin/WindSystem/Private/WindFieldDiffusion.usf",
	"MainCS",
	SF_Compute);

IMPLEMENT_GLOBAL_SHADER(
	FWindFieldComposeCS,
	"/Plugin/WindSystem/Private/WindFieldCompose.usf",
	"MainCS",
	SF_Compute);

IMPLEMENT_GLOBAL_SHADER(
	FWindFieldDebugSliceCS,
	"/Plugin/WindSystem/Private/WindFieldDebugSlice.usf",
	"MainCS",
	SF_Compute);

IMPLEMENT_GLOBAL_SHADER(
	FWindFieldScrollCS,
	"/Plugin/WindSystem/Private/WindFieldScroll.usf",
	"ScrollVolumeCS",
	SF_Compute);
