// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WindFieldTypes.h"
#include <atomic>

/**
 * Thread-safe bridge for transferring wind source data from game thread to render thread.
 * The game thread pushes updates, the render thread consumes them.
 */
class WINDSYSTEMRUNTIME_API FWindFieldProxy : public TSharedFromThis<FWindFieldProxy>
{
public:
	FWindFieldProxy() = default;
	~FWindFieldProxy() = default;

	// Non-copyable
	FWindFieldProxy(const FWindFieldProxy&) = delete;
	FWindFieldProxy& operator=(const FWindFieldProxy&) = delete;

	/** Called from game thread: push new wind source data for GPU upload */
	void UpdateSources_GameThread(
		TArray<FGPUWindSourceData>&& InSources,
		const FVector3f& InFieldCenter,
		float InTime,
		const FWindFieldConfig& InConfig);

	/**
	 * Called from render thread: consume pending update.
	 * Returns true if new data was available.
	 */
	bool FetchUpdate_RenderThread(
		TArray<FGPUWindSourceData>& OutSources,
		FVector3f& OutFieldCenter,
		float& OutTime,
		FWindFieldConfig& OutConfig);

private:
	FCriticalSection DataLock;
	TArray<FGPUWindSourceData> PendingSources;
	FVector3f PendingFieldCenter = FVector3f::ZeroVector;
	float PendingTime = 0.0f;
	FWindFieldConfig PendingConfig;
	std::atomic<bool> bHasPendingUpdate{ false };
};

/**
 * Global registry connecting UWorld instances to their wind field proxies.
 * Used by both game-thread subsystems and render-thread scene extensions.
 */
class WINDSYSTEMRUNTIME_API FWindFieldProxyRegistry
{
public:
	static FWindFieldProxyRegistry& Get();

	void Register(const UWorld* World, TSharedPtr<FWindFieldProxy> Proxy);
	void Unregister(const UWorld* World);
	TSharedPtr<FWindFieldProxy> Find(const UWorld* World) const;

private:
	mutable FCriticalSection RegistryLock;
	TMap<const UWorld*, TSharedPtr<FWindFieldProxy>> Proxies;
};
