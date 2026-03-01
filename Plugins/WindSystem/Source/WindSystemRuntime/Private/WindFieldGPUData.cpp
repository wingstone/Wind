// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindFieldGPUData.h"

// ============================================================================
// FWindFieldProxy
// ============================================================================

void FWindFieldProxy::UpdateSources_GameThread(
	TArray<FGPUWindSourceData>&& InSources,
	const FVector3f& InFieldCenter,
	float InTime,
	const FWindFieldConfig& InConfig)
{
	FScopeLock Lock(&DataLock);
	PendingSources = MoveTemp(InSources);
	PendingFieldCenter = InFieldCenter;
	PendingTime = InTime;
	PendingConfig = InConfig;
	bHasPendingUpdate.store(true, std::memory_order_release);
}

bool FWindFieldProxy::FetchUpdate_RenderThread(
	TArray<FGPUWindSourceData>& OutSources,
	FVector3f& OutFieldCenter,
	float& OutTime,
	FWindFieldConfig& OutConfig)
{
	if (!bHasPendingUpdate.load(std::memory_order_acquire))
	{
		return false;
	}

	FScopeLock Lock(&DataLock);
	OutSources = MoveTemp(PendingSources);
	OutFieldCenter = PendingFieldCenter;
	OutTime = PendingTime;
	OutConfig = PendingConfig;
	bHasPendingUpdate.store(false, std::memory_order_release);
	return true;
}

// ============================================================================
// FWindFieldProxyRegistry
// ============================================================================

FWindFieldProxyRegistry& FWindFieldProxyRegistry::Get()
{
	static FWindFieldProxyRegistry Instance;
	return Instance;
}

void FWindFieldProxyRegistry::Register(const UWorld* World, TSharedPtr<FWindFieldProxy> Proxy)
{
	FScopeLock Lock(&RegistryLock);
	Proxies.Add(World, Proxy);
}

void FWindFieldProxyRegistry::Unregister(const UWorld* World)
{
	FScopeLock Lock(&RegistryLock);
	Proxies.Remove(World);
}

TSharedPtr<FWindFieldProxy> FWindFieldProxyRegistry::Find(const UWorld* World) const
{
	FScopeLock Lock(&RegistryLock);
	const TSharedPtr<FWindFieldProxy>* Found = Proxies.Find(World);
	return Found ? *Found : nullptr;
}
