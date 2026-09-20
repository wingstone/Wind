// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceWindField.h"

#include "NiagaraCompileHashVisitor.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraTypes.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraDataInterfaceUtilities.h"

#include "RenderGraphBuilder.h"
#include "RHIStaticStates.h"
#include "SystemTextures.h"
#include "TextureResource.h"

#include "SceneInterface.h"
#include "ScenePrivate.h"
#include "WindFieldSceneExtension.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceWindField"

static const TCHAR* GWindFieldTemplateShader = TEXT("/Plugin/WindSystem/Private/NiagaraDataInterfaceWindField.ush");

// ---------------------------------------------------------------------------
// Shader parameter struct (one instance per DI usage)
// ---------------------------------------------------------------------------
BEGIN_SHADER_PARAMETER_STRUCT(FNDIWindFieldShaderParameters, )
	SHADER_PARAMETER_TEXTURE(Texture3D, WindFieldTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, WindFieldSampler)
	SHADER_PARAMETER(FVector3f, WindFieldOrigin)
	SHADER_PARAMETER(float, WindFieldDirectionalStrength)
	SHADER_PARAMETER(FVector3f, WindFieldInvExtent)
	SHADER_PARAMETER(float, Padding0)
	SHADER_PARAMETER(FVector3f, WindFieldDirectionalDirection)
	SHADER_PARAMETER(float, Padding1)
END_SHADER_PARAMETER_STRUCT()

// ---------------------------------------------------------------------------
// VM function name constants
// ---------------------------------------------------------------------------
static const FName NAME_SampleWindVelocity(TEXT("SampleWindVelocity"));
static const FName NAME_GetDirectionalWind(TEXT("GetDirectionalWind"));

// ===========================================================================
// UNiagaraDataInterfaceWindField
// ===========================================================================
UNiagaraDataInterfaceWindField::UNiagaraDataInterfaceWindField(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Proxy.Reset(new FNDIWindFieldProxy());
}

void UNiagaraDataInterfaceWindField::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

bool UNiagaraDataInterfaceWindField::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIWindFieldInstanceData_GameThread* Inst = new (PerInstanceData) FNDIWindFieldInstanceData_GameThread();
	Inst->WeakWorld = SystemInstance ? SystemInstance->GetWorld() : nullptr;
	return true;
}

void UNiagaraDataInterfaceWindField::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIWindFieldInstanceData_GameThread* Inst = static_cast<FNDIWindFieldInstanceData_GameThread*>(PerInstanceData);
	Inst->~FNDIWindFieldInstanceData_GameThread();

	// Remove render-thread mirror
	FNDIWindFieldProxy* ThisProxy = GetProxyAs<FNDIWindFieldProxy>();
	FNiagaraSystemInstanceID Id = SystemInstance ? SystemInstance->GetId() : FNiagaraSystemInstanceID();
	ENQUEUE_RENDER_COMMAND(NDIWindField_Destroy)(
		[ThisProxy, Id](FRHICommandListImmediate&)
		{
			ThisProxy->SystemInstancesToProxyData.Remove(Id);
		});
}

void UNiagaraDataInterfaceWindField::ProvidePerInstanceDataForRenderThread(
	void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance)
{
	FNDIWindFieldInstanceData_GameThread* Src = static_cast<FNDIWindFieldInstanceData_GameThread*>(PerInstanceData);
	FNDIWindFieldInstanceData_RenderThread* Dst = new (DataForRenderThread) FNDIWindFieldInstanceData_RenderThread();

	// Snapshot scene extension state on the game thread by capturing the FScene pointer,
	// then queue a render-thread fetch. Because ConsumePerInstanceDataFromGameThread runs
	// on the RT, we can just pass the scene pointer and read there.
	UWorld* World = Src->WeakWorld.Get();
	FScene* Scene = (World && World->Scene) ? World->Scene->GetRenderScene() : nullptr;

	// We stash the scene pointer into Origin.X as a hack? — no, use a member; pack into RT struct via lambda.
	// Simplest: capture into an out parameter that Consume reads. We use a small trampoline field:
	Dst->WindFieldTexture = nullptr;
	// Pack raw scene pointer into a trailing pointer stored via a static side-channel would be fragile;
	// instead we enqueue an RT command right here to fill the proxy entry, and leave Dst zero-initialized.
	FNDIWindFieldProxy* ThisProxy = GetProxyAs<FNDIWindFieldProxy>();

	ENQUEUE_RENDER_COMMAND(NDIWindField_Snapshot)(
		[ThisProxy, SystemInstance, Scene](FRHICommandListImmediate&)
		{
			FNDIWindFieldInstanceData_RenderThread& Entry = ThisProxy->SystemInstancesToProxyData.FindOrAdd(SystemInstance);
			Entry = FNDIWindFieldInstanceData_RenderThread();

			if (Scene)
			{
				if (FWindFieldSceneExtension* Ext = Scene->GetExtensionPtr<FWindFieldSceneExtension>())
				{
					FRHITexture* Tex = nullptr;
					Ext->GetRenderState_RenderThread(
						Tex, Entry.Origin, Entry.InvExtent,
						Entry.DirectionalDirection, Entry.DirectionalStrength);
					Entry.WindFieldTexture = Tex;
				}
			}
		});
}

bool UNiagaraDataInterfaceWindField::Equals(const UNiagaraDataInterface* Other) const
{
	return Super::Equals(Other);
}

// ---------------------------------------------------------------------------
// CPU VM
// ---------------------------------------------------------------------------
#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceWindField::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = NAME_SampleWindVelocity;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.bSupportsCPU = true;
		Sig.bSupportsGPU = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("WindField")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("WorldPosition")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = NAME_GetDirectionalWind;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.bSupportsCPU = true;
		Sig.bSupportsGPU = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("WindField")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Direction")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Strength")));
		OutFunctions.Add(Sig);
	}
}
#endif

void UNiagaraDataInterfaceWindField::GetVMExternalFunction(
	const FVMExternalFunctionBindingInfo& BindingInfo, void* /*InstanceData*/, FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == NAME_SampleWindVelocity)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceWindField::VMSampleWindVelocity);
	}
	else if (BindingInfo.Name == NAME_GetDirectionalWind)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceWindField::VMGetDirectionalWind);
	}
}

void UNiagaraDataInterfaceWindField::VMSampleWindVelocity(FVectorVMExternalFunctionContext& Context)
{
	// CPU path: we don't have a CPU mirror of the 3D volume — return zero (or directional-only fallback).
	VectorVM::FExternalFuncInputHandler<float> InX(Context);
	VectorVM::FExternalFuncInputHandler<float> InY(Context);
	VectorVM::FExternalFuncInputHandler<float> InZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutZ(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		InX.GetAndAdvance(); InY.GetAndAdvance(); InZ.GetAndAdvance();
		*OutX.GetDestAndAdvance() = 0.0f;
		*OutY.GetDestAndAdvance() = 0.0f;
		*OutZ.GetDestAndAdvance() = 0.0f;
	}
}

void UNiagaraDataInterfaceWindField::VMGetDirectionalWind(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<float> OutX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutS(Context);

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		*OutX.GetDestAndAdvance() = 0.0f;
		*OutY.GetDestAndAdvance() = 0.0f;
		*OutZ.GetDestAndAdvance() = 0.0f;
		*OutS.GetDestAndAdvance() = 0.0f;
	}
}

// ---------------------------------------------------------------------------
// GPU
// ---------------------------------------------------------------------------
#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceWindField::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	bool bSuccess = Super::AppendCompileHash(InVisitor);
	InVisitor->UpdateShaderFile(GWindFieldTemplateShader);
	InVisitor->UpdateShaderParameters<FNDIWindFieldShaderParameters>();
	return bSuccess;
}

void UNiagaraDataInterfaceWindField::GetParameterDefinitionHLSL(
	const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	const TMap<FString, FStringFormatArg> Args =
	{
		{TEXT("ParameterName"), ParamInfo.DataInterfaceHLSLSymbol},
	};
	OutHLSL += FString::Format(TEXT(
		"Texture3D    {ParameterName}_WindFieldTexture;\n"
		"SamplerState {ParameterName}_WindFieldSampler;\n"
		"float3       {ParameterName}_WindFieldOrigin;\n"
		"float        {ParameterName}_WindFieldDirectionalStrength;\n"
		"float3       {ParameterName}_WindFieldInvExtent;\n"
		"float        {ParameterName}_Padding0;\n"
		"float3       {ParameterName}_WindFieldDirectionalDirection;\n"
		"float        {ParameterName}_Padding1;\n\n"),
		Args);
}

bool UNiagaraDataInterfaceWindField::GetFunctionHLSL(
	const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
	const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
	int /*FunctionInstanceIndex*/, FString& OutHLSL)
{
	const TMap<FString, FStringFormatArg> Args =
	{
		{TEXT("ParameterName"), ParamInfo.DataInterfaceHLSLSymbol},
		{TEXT("FunctionName"), FunctionInfo.InstanceName},
	};

	if (FunctionInfo.DefinitionName == NAME_SampleWindVelocity)
	{
		OutHLSL += FString::Format(TEXT(
			"void {FunctionName}(float3 WorldPos, out float3 OutVelocity)\n"
			"{\n"
			"    float3 UVW = (WorldPos - {ParameterName}_WindFieldOrigin) * {ParameterName}_WindFieldInvExtent;\n"
			"    float3 Field = float3(0,0,0);\n"
			"    if (all(UVW >= 0.0) && all(UVW <= 1.0))\n"
			"    {\n"
			"        Field = {ParameterName}_WindFieldTexture.SampleLevel({ParameterName}_WindFieldSampler, UVW, 0).xyz;\n"
			"    }\n"
			"    OutVelocity = Field + {ParameterName}_WindFieldDirectionalDirection * {ParameterName}_WindFieldDirectionalStrength;\n"
			"}\n\n"), Args);
		return true;
	}
	if (FunctionInfo.DefinitionName == NAME_GetDirectionalWind)
	{
		OutHLSL += FString::Format(TEXT(
			"void {FunctionName}(out float3 OutDir, out float OutStrength)\n"
			"{\n"
			"    OutDir = {ParameterName}_WindFieldDirectionalDirection;\n"
			"    OutStrength = {ParameterName}_WindFieldDirectionalStrength;\n"
			"}\n\n"), Args);
		return true;
	}
	return false;
}
#endif // WITH_EDITORONLY_DATA

void UNiagaraDataInterfaceWindField::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<FNDIWindFieldShaderParameters>();
}

void UNiagaraDataInterfaceWindField::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	const FNDIWindFieldProxy& ThisProxy = Context.GetProxy<FNDIWindFieldProxy>();
	const FNDIWindFieldInstanceData_RenderThread* Entry = ThisProxy.SystemInstancesToProxyData.Find(Context.GetSystemInstanceID());

	FNDIWindFieldShaderParameters* Params = Context.GetParameterNestedStruct<FNDIWindFieldShaderParameters>();

	FRHITexture* Tex = Entry ? Entry->WindFieldTexture.GetReference() : nullptr;
	if (!Tex)
	{
		Tex = GBlackVolumeTexture->TextureRHI;
	}
	Params->WindFieldTexture = Tex;
	Params->WindFieldSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Params->WindFieldOrigin = Entry ? Entry->Origin : FVector3f::ZeroVector;
	Params->WindFieldInvExtent = Entry ? Entry->InvExtent : FVector3f::OneVector;
	Params->WindFieldDirectionalDirection = Entry ? Entry->DirectionalDirection : FVector3f::ZeroVector;
	Params->WindFieldDirectionalStrength = Entry ? Entry->DirectionalStrength : 0.0f;
	Params->Padding0 = 0.0f;
	Params->Padding1 = 0.0f;
}

// ===========================================================================
// Proxy
// ===========================================================================
void FNDIWindFieldProxy::ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& /*Instance*/)
{
	// Snapshot was already applied through an ENQUEUE_RENDER_COMMAND in ProvidePerInstanceDataForRenderThread.
	// Just destruct the transient struct.
	FNDIWindFieldInstanceData_RenderThread* Data = static_cast<FNDIWindFieldInstanceData_RenderThread*>(PerInstanceData);
	Data->~FNDIWindFieldInstanceData_RenderThread();
}

int32 FNDIWindFieldProxy::PerInstanceDataPassedToRenderThreadSize() const
{
	return sizeof(FNDIWindFieldInstanceData_RenderThread);
}

#undef LOCTEXT_NAMESPACE
