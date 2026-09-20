// Copyright TADemo. All Rights Reserved.

#include "Data/SeasonDataAsset.h"

void USeasonDataAsset::PostLoad()
{
	Super::PostLoad();
	NormalizeFrames();
}

#if WITH_EDITOR
void USeasonDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	NormalizeFrames();
}
#endif

void USeasonDataAsset::NormalizeFrames()
{
	Frames.Sort([](const FSeasonFrame& A, const FSeasonFrame& B) { return A.SeasonPhase < B.SeasonPhase; });
	for (FSeasonFrame& F : Frames)
	{
		F.Resolve(Bindings);
	}
}

void USeasonDataAsset::ResolveFrames()
{
	for (FSeasonFrame& F : Frames)
	{
		F.Resolve(Bindings);
	}
}
