// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fluid2DFieldShaders.h"
#include "ShaderCore.h"

// Fluid2D Scroll
IMPLEMENT_GLOBAL_SHADER(FFluid2DScrollHeightCS, "/Plugin/Fluid2DSystem/Private/Fluid2DScroll.usf", "ScrollHeightCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FFluid2DScrollVelocityCS, "/Plugin/Fluid2DSystem/Private/Fluid2DScroll.usf", "ScrollVelocityCS", SF_Compute);

// Shallow Water Equations Solver
IMPLEMENT_GLOBAL_SHADER(FSWHeightAdvectionCS, "/Plugin/Fluid2DSystem/Private/ShallowWaterSolver.usf", "HeightAdvectionCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSWVelocityAdvectionCS, "/Plugin/Fluid2DSystem/Private/ShallowWaterSolver.usf", "VelocityAdvectionCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSWDiffusionCS, "/Plugin/Fluid2DSystem/Private/ShallowWaterSolver.usf", "DiffusionCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FFluid2DHeightToNormalCS, "/Plugin/Fluid2DSystem/Private/ShallowWaterSolver.usf", "HeightToNormalCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FFluid2DInteractionApplicationCS, "/Plugin/Fluid2DSystem/Private/Fluid2DInteractionApplication.usf", "MainCS", SF_Compute);

// Navier-Stokes Solver (5 steps)
IMPLEMENT_GLOBAL_SHADER(FNSAdvectionCS, "/Plugin/Fluid2DSystem/Private/NavierStokesSolver2D.usf", "AdvectionCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNSDiffusionCS, "/Plugin/Fluid2DSystem/Private/NavierStokesSolver2D.usf", "DiffusionCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNSComputeDivergenceCS, "/Plugin/Fluid2DSystem/Private/NavierStokesSolver2D.usf", "ComputeDivergenceCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNSPressureSolveCS, "/Plugin/Fluid2DSystem/Private/NavierStokesSolver2D.usf", "PressureSolveCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNSProjectionCS, "/Plugin/Fluid2DSystem/Private/NavierStokesSolver2D.usf", "ProjectionCS", SF_Compute);

// Navier-Stokes Vorticity Confinement
IMPLEMENT_GLOBAL_SHADER(FNSComputeVorticityCS, "/Plugin/Fluid2DSystem/Private/NavierStokesSolver2D.usf", "ComputeVorticityCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FNSVorticityConfinementCS, "/Plugin/Fluid2DSystem/Private/NavierStokesSolver2D.usf", "VorticityConfinementCS", SF_Compute);

// Global Flow
IMPLEMENT_GLOBAL_SHADER(FFluid2DGlobalFlowCS, "/Plugin/Fluid2DSystem/Private/Fluid2DGlobalFlow.usf", "GlobalFlowCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FFluid2DComposeVelocityCS, "/Plugin/Fluid2DSystem/Private/Fluid2DGlobalFlow.usf", "ComposeVelocityCS", SF_Compute);
