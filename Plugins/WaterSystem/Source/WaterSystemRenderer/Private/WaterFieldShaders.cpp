// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterFieldShaders.h"
#include "ShaderCore.h"

// Shallow Water Equations Solver
IMPLEMENT_GLOBAL_SHADER(FShallowWaterSolverCS, "/Plugin/WaterSystem/Private/ShallowWaterSolver.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FWaterInteractionApplicationCS, "/Plugin/WaterSystem/Private/WaterInteractionApplication.usf", "MainCS", SF_Compute);

// Navier-Stokes Solver (5 steps)
IMPLEMENT_GLOBAL_SHADER(FNSAdvectionCS, "/Plugin/WaterSystem/Private/NavierStokesSolver2D.usf", "AdvectionCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNSDiffusionCS, "/Plugin/WaterSystem/Private/NavierStokesSolver2D.usf", "DiffusionCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNSComputeDivergenceCS, "/Plugin/WaterSystem/Private/NavierStokesSolver2D.usf", "ComputeDivergenceCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNSPressureSolveCS, "/Plugin/WaterSystem/Private/NavierStokesSolver2D.usf", "PressureSolveCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNSProjectionCS, "/Plugin/WaterSystem/Private/NavierStokesSolver2D.usf", "ProjectionCS", SF_Compute);
