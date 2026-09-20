// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceWindField.generated.h"

class FRHITexture;

/**
 * Per-instance data passed from game thread → render thread proxy each tick.
 */
struct FNDIWindFieldInstanceData_GameThread
{
	TWeakObjectPtr<UWorld> WeakWorld;
};

/**
 * Render-thread snapshot of the wind field scene extension state for this instance.
 * Populated in ProvidePerInstanceDataForRenderThread.
 */
struct FNDIWindFieldInstanceData_RenderThread
{
	FTextureRHIRef WindFieldTexture;
	FVector3f      Origin = FVector3f::ZeroVector;
	FVector3f      InvExtent = FVector3f::OneVector;
	FVector3f      DirectionalDirection = FVector3f::ZeroVector;
	float          DirectionalStrength = 0.0f;
};

/**
 * Proxy: owns render-thread instance data, keyed by NiagaraSystem instance id.
 */
struct FNDIWindFieldProxy : public FNiagaraDataInterfaceProxy
{
	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override;
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override;

	TMap<FNiagaraSystemInstanceID, FNDIWindFieldInstanceData_RenderThread> SystemInstancesToProxyData;
};

/**
 * Niagara Data Interface that samples the WindSystem 3D velocity field
 * produced by FWindFieldSceneExtension.
 *
 * HLSL functions exposed to Emitter / Particle Update:
 *   SampleWindVelocity(float3 WorldPos, out float3 OutVelocity)
 *   GetDirectionalWind(out float3 OutDirection, out float OutStrength)
 */
UCLASS(EditInlineNew, Category = "Wind", meta = (DisplayName = "Wind Field"))
class WINDSYSTEMNIAGARA_API UNiagaraDataInterfaceWindField : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	//~ UNiagaraDataInterface interface
	virtual void PostInitProperties() override;

	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual int32 PerInstanceDataSize() const override { return sizeof(FNDIWindFieldInstanceData_GameThread); }
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override { return false; }
	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override;

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	// CPU VM
#if WITH_EDITORONLY_DATA
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
#endif
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;

	// GPU
#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
#endif
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override { return true; }

private:
	// CPU VM handlers
	void VMSampleWindVelocity(FVectorVMExternalFunctionContext& Context);
	void VMGetDirectionalWind(FVectorVMExternalFunctionContext& Context);
};
