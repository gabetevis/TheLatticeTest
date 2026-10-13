// Copyright 2025 Andrew Laptev. All Rights Reserved.

#include "DatamoshViewExtension.h"
#include "PostProcess/PostProcessing.h"
#include "RenderGraph.h"
#include "RenderTargetPool.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderResource.h"
#include "TextureResource.h"
#include "ScreenPass.h"
#include "FXRenderingUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
#include "DataDrivenShaderPlatformInfo.h"
#endif
#endif

namespace
{
	TAutoConsoleVariable<int32> CVarDatamosh(
		TEXT("r.Datamosh"),
		1,
		TEXT("Set Datamosh post process effect mode\n")
		TEXT(" 0: Disabled;\n")
		TEXT(" 1: Enabled. (Default). Plugin will render the effect into render target objects located in the /Textures within the plugin folder\n")
		TEXT(" 2: Enabled Screen Rendering. Plugin will inject the shader directly to the SceneColor. Better performance and compability, but less control over the effect if you're not familiar with HLSL"),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable <float> CVarDatamoshVelocityCompensation(
		TEXT("r.Datamosh.VelocityCompensation"),
		0.45,
		TEXT("SHADER VALUE. Compensation for the Velocity pass: a workaround for the issue with edge rendering of dynamic objects in Velocity Pass for Render Target Mode. Default: 0.45."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarDatamoshDebugFloat(
		TEXT("r.Datamosh.DebugFloat"),
		0.5,
		TEXT("Debug shader value"),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int> CVarDatamoshStencil(
		TEXT("r.Datamosh.MaskStencil"),
		0,
		TEXT("SHADER VALUE. The Stencil value that will be used to select objects from the Stencil buffer when rendering the Datamosh mask.")
		TEXT("Example: If the object's Stencil value does not match this value, the object will not be rendered for the mask effect applied to the trail feature.")
		TEXT("0 by default. (Stencil test for mask is disabled, only Custom Depth)"),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarDatamoshPriority(
		TEXT("r.Datamosh.Priority"),
		0,
		TEXT("Datamosh SceneViewExtension priority. 0 by default."));

	FAutoConsoleCommand CVarDatamoshResetRT(
		TEXT("r.Datamosh.Reset"),
		TEXT("Resets the render target and calls CreatePooledRenderTarget_RenderThread"),
		FConsoleCommandDelegate::CreateLambda([]()
			{
				if (FDatamoshViewExtension* Extension = FDatamoshViewExtension::GetActiveInstance())
				{
					Extension->ResetRenderTarget_RenderThread();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("DatamoshExtension: No active instance found to reset render targets."));
				}

			})
	);

	// Shader declarations
	// Struct to include common shader parameters
	BEGIN_SHADER_PARAMETER_STRUCT(FDatamoshCommonParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)

		SHADER_PARAMETER(FIntRect, ViewportRect)
		SHADER_PARAMETER(FVector2f, ViewportInvSize)
		SHADER_PARAMETER(FVector2f, UVScale)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PreviousTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)

		SHADER_PARAMETER(float, DatamoshFactor)
		SHADER_PARAMETER(float, DatamoshTrailFactor)
		SHADER_PARAMETER(int, DatamoshNoise)
		SHADER_PARAMETER(float, DatamoshNoiseIntensity)
		SHADER_PARAMETER(float, DatamoshNoiseSize)
		SHADER_PARAMETER(FVector2f, DatamoshMotion)
		SHADER_PARAMETER(int, DatamoshStyle)
		SHADER_PARAMETER(float, DatamoshStyleIntensity)
		SHADER_PARAMETER(int, DatamoshTrail)
		SHADER_PARAMETER(float, DatamoshVelocityCompensation)
		SHADER_PARAMETER(float, DatamoshDebugFloat)
		SHADER_PARAMETER(float, DatamoshSharpness)
		SHADER_PARAMETER(float, DatamoshDistance)


	END_SHADER_PARAMETER_STRUCT()

	class FDatamoshCreateMaskCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FDatamoshCreateMaskCS);
		SHADER_USE_PARAMETER_STRUCT(FDatamoshCreateMaskCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_INCLUDE(FDatamoshCommonParameters, CommonParameters)

			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CustomDepthTexture)
			SHADER_PARAMETER(int, StencilValue)
			SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint2>, StencilTexture)
			SHADER_PARAMETER(FVector2f, StencilTextureSize)

			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		END_SHADER_PARAMETER_STRUCT()

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}	
#endif
#endif
	};

	class FDatamoshScreenSpaceCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FDatamoshScreenSpaceCS);
		SHADER_USE_PARAMETER_STRUCT(FDatamoshScreenSpaceCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_INCLUDE(FDatamoshCommonParameters, CommonParameters)

			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScreenSpaceMaskTexture)
			SHADER_PARAMETER(FVector2f, SceneColorTextureSize)


			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		END_SHADER_PARAMETER_STRUCT()

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
#endif
#endif
	};

	class FDatamoshRTCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FDatamoshRTCS);
		SHADER_USE_PARAMETER_STRUCT(FDatamoshRTCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_INCLUDE(FDatamoshCommonParameters, CommonParameters)

			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RenderTargetMaskTexture)

			SHADER_PARAMETER_SAMPLER(SamplerState, RenderTargetSampler)
			SHADER_PARAMETER(FVector2f, RenderTargetTextureSize)
			SHADER_PARAMETER(FVector2f, RenderTargetUVScale)

			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		END_SHADER_PARAMETER_STRUCT()

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 3
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
#endif
#endif
	};

	IMPLEMENT_GLOBAL_SHADER(FDatamoshCreateMaskCS, "/Plugins/DatamoshEffect/DatamoshMaskCS.usf", "DatamoshCreateMaskCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FDatamoshScreenSpaceCS, "/Plugins/DatamoshEffect/DatamoshCS.usf", "DatamoshScreenSpaceCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FDatamoshRTCS, "/Plugins/DatamoshEffect/DatamoshCS.usf", "DatamoshWriteRenderTargetCS", SF_Compute);

}
DECLARE_GPU_STAT_NAMED(DatamoshEffect, TEXT("Datamosh Effect"));
FDatamoshViewExtension* FDatamoshViewExtension::ActiveInstance = nullptr;

FDatamoshViewExtension::FDatamoshViewExtension(
	const FAutoRegister& AutoRegister,
	UWorld* InWorld)
	: FWorldSceneViewExtension(AutoRegister, InWorld)
{
	UE_LOG(LogTemp, VeryVerbose, TEXT("DatamoshExtension: Registered for world: %s"), *InWorld->GetName());
	DatamoshSubsystemWeak = GEngine->GetEngineSubsystem<UDatamoshSubsystem>();
	ActiveInstance = this;
}

void FDatamoshViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (UDatamoshSubsystem* Subsystem = DatamoshSubsystemWeak.Get())
	{
		Subsystem->Update(GetWorld(), InViewFamily);
	}
}

// Render effect at the start of the post processing pipeline (before depth of field) ensuring that it's compatible with any other default post process effects
void FDatamoshViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	FWorldSceneViewExtension::PrePostProcessPass_RenderThread(GraphBuilder, View, Inputs);

	if (InputDatamoshRenderTarget == nullptr || InputDatamoshMaskRenderTarget == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("DatamoshExtension: One or more render targets is missing."));
		return;
	}

	if (!InputDatamoshRenderTargetPooled.IsValid() && !InputDatamoshMaskRenderTargetPooled.IsValid())
	{
		CreatePooledRenderTarget_RenderThread(InputDatamoshRenderTarget.Get(), InputDatamoshRenderTargetPooled);
		UE_LOG(LogTemp, Log, TEXT("DatamoshExtension: Datamosh Pooled RT created"));
		CreatePooledRenderTarget_RenderThread(InputDatamoshMaskRenderTarget.Get(), InputDatamoshMaskRenderTargetPooled);
		UE_LOG(LogTemp, Log, TEXT("DatamoshExtension: Datamosh Mask Pooled RT created"));
	}

	const ERHIFeatureLevel::Type FeatureLevel = View.GetFeatureLevel();
	const FIntRect Viewport = UE::FXRenderingUtils::GetRawViewRectUnsafe(View);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);

	// The texture where SceneColor from the camera will be written, will later be processed in the shader, and the result will be written to the secondary final texture.
	FRDGTextureRef DatamoshRenderTargetTexture = GraphBuilder.RegisterExternalTexture(InputDatamoshRenderTargetPooled, TEXT("DE_Input_RT_Texture"));
	FRDGTextureRef DatamoshMaskRenderTargetTexture = GraphBuilder.RegisterExternalTexture(InputDatamoshMaskRenderTargetPooled, TEXT("DE_Input_RTMask_Texture"));

	FScreenPassTexture SceneColor((*Inputs.SceneTextures)->SceneColorTexture, Viewport);

	FRDGTextureDesc MaskDesc = FRDGTextureDesc::Create2D(
		SceneColor.Texture->Desc.Extent,              // Size (Scene Color size for the Screen Rendering)
		PF_G16R16F,									  // Format (Only red and green channels)
		FClearValueBinding::None,                     // Clear Value
		TexCreate_ShaderResource | TexCreate_UAV      // Flags
	);

	//// Datamosh Mask Texture
	//FRDGTextureRef TempMaskTexture = GraphBuilder.CreateTexture(MaskDesc, TEXT("DatamoshTempMask"), ERDGTextureFlags::MultiFrame);

	GRenderTargetPool.FindFreeElement(
		FRHICommandListExecutor::GetImmediateCommandList(),
		MaskDesc,
		TemporalMaskPRT,
		TEXT("DE_Mask_Temp"));
	FRDGTextureRef TempMaskTexture = GraphBuilder.RegisterExternalTexture(TemporalMaskPRT, TEXT("DatamoshTempMask"), ERDGTextureFlags::MultiFrame);

	bool bEffectEnabled = CVarDatamosh.GetValueOnRenderThread() != 0;
	DatamoshParameters = DatamoshSubsystemWeak->GetDatamoshParameters();

	if (SceneColor.IsValid() && bEffectEnabled)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "DatamoshEffect", Viewport.Width(), Viewport.Height());

		FScreenPassTexture SceneDepth((*Inputs.SceneTextures)->SceneDepthTexture, Viewport);
		FScreenPassTexture VelocityTexture((*Inputs.SceneTextures)->GBufferVelocityTexture, Viewport);
		FScreenPassTexture CustomDepth((*Inputs.SceneTextures)->CustomDepthTexture, Viewport);

		RDG_GPU_STAT_SCOPE(GraphBuilder, DatamoshEffect)

			switch (CVarDatamosh.GetValueOnRenderThread())
			{
			case 1:
			{
				// Render Target Mode

				FRDGTextureRef DatamoshMaskTexture = CreateDatamoshMask(GraphBuilder, ViewInfo, SceneColor, VelocityTexture, SceneDepth, DatamoshMaskRenderTargetTexture, CustomDepth.Texture, Viewport);
				FRDGTextureRef DatamoshTexture = RenderTargetDatamosh(GraphBuilder, ViewInfo, SceneColor, VelocityTexture, SceneDepth, SceneColor.Texture, DatamoshRenderTargetTexture, DatamoshMaskTexture, Viewport);

				AddCopyTexturePass(GraphBuilder, DatamoshTexture, DatamoshRenderTargetTexture);
				AddCopyTexturePass(GraphBuilder, DatamoshMaskTexture, DatamoshMaskRenderTargetTexture);
				break;
			}

			case 2:
			{
				// Screen Rendering Mode

				FRDGTextureRef DatamoshMaskTexture = CreateDatamoshMask(
					GraphBuilder, ViewInfo, SceneColor, VelocityTexture, SceneDepth,
					TempMaskTexture, CustomDepth.Texture, Viewport);

				FRDGTextureRef DatamoshTexture = ScreenSpaceDatamosh(
					GraphBuilder, ViewInfo, SceneColor, VelocityTexture, SceneDepth,
					SceneColor.Texture, DatamoshMaskTexture, Viewport);

				bool bFinalDatamoshActive = DatamoshSubsystemWeak->GetDatamoshActive(); // Default to active state.

				// Choose the correct texture based on the final active state.
				FRDGTextureRef SourceTexture = bFinalDatamoshActive ? DatamoshTexture : SceneColor.Texture;

				AddCopyTexturePass(GraphBuilder, SourceTexture, SceneColor.Texture);
				break;
			}
			}
	}
}

bool FDatamoshViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	bool bIsActive = FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
	if (!bIsActive)
	{
		return false;
	}

#if WITH_EDITOR
	if (GEditor)
	{
		if (GEditor->IsSimulatingInEditor())
		{
			if (Context.GetWorld()->WorldType == EWorldType::Editor)
			{
				return true;
			}
		}

		if (GEditor->PlayWorld)
		{
			bIsActive = Context.GetWorld()->WorldType == EWorldType::PIE;
		}
		else
		{
			bIsActive = Context.GetWorld()->WorldType == EWorldType::Editor || GetWorld()->WorldType == EWorldType::Game;
		}
	}
#endif
	// @todo: It would be better to use events instead, this is not cheap to check every tick
	return bIsActive;
}

int32 FDatamoshViewExtension::GetPriority() const
{
	return CVarDatamoshPriority.GetValueOnGameThread();
}

FRDGTextureRef FDatamoshViewExtension::ScreenSpaceDatamosh(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, FScreenPassTexture& SceneColor,
	FScreenPassTexture& VelocityTexture,
	FScreenPassTexture& DepthTexture,
	FRDGTextureRef& InputTexture,
	FRDGTextureRef& InputMaskTexture,
	const FIntRect& ViewRect)
{
	// Setup all the descriptors to create a target texture
	FRDGTextureDesc OutputDesc = InputTexture->Desc;
	OutputDesc.Extent = ViewRect.Size();
	OutputDesc.ClearValue = FClearValueBinding(FLinearColor::Black);
	OutputDesc.Flags = TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV;

	// Shaders access point
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	TShaderMapRef<FDatamoshScreenSpaceCS> ComputeShader(GlobalShaderMap);

	// Set the shader parameters
	FDatamoshScreenSpaceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDatamoshScreenSpaceCS::FParameters>();
	FIntRect PassViewSize = SceneColor.ViewRect;
	FIntPoint SrcTextureSize = InputTexture->Desc.Extent;

	// Create target texture
	FRDGTextureRef const OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("ScreenDatamosh"));
	FRDGTextureRef const CurrentTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("DE_SS_CurrentTexture"));
	
	////// 
	// Temporal Texture Buffer
	// Since in versions above 5.2 a significant part of the RenderGraphBuilder was rewritten, the resource lifetime in the graph when using the ConvertToExternalTexture method is insufficient.
	// So we have to register the texture separately to ensure its resource reaches the end of the graph.
	GRenderTargetPool.FindFreeElement(
		FRHICommandListExecutor::GetImmediateCommandList(),
		OutputDesc,
		TemporalPRT,
		TEXT("DE_Local_SS_PreviousTexture"));
	FRDGTextureRef TemporalBufferTexture = GraphBuilder.RegisterExternalTexture(TemporalPRT, TEXT("DatamoshTemporal"));
	//
	//////

	AddCopyTexturePass(GraphBuilder, SceneColor.Texture, CurrentTexture);

	// Get the input sizes (do note that viewport visible area might not be the full extent of the SceneColor texture
	// Method to setup common parameters, we use this to pass ViewUniformBuffer data
	// There is a lot of useful stuff in the ViewUniformBuffer, do note that when getting this from the SceneView, a lot them seem to be unpopulated
	PassParameters->CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->CommonParameters.ViewportRect = PassViewSize;
	PassParameters->CommonParameters.ViewportInvSize = FVector2f(1.0f / PassViewSize.Width(), 1.0f / PassViewSize.Height());
	// Conversion from the full texture to the actual used size
	// Refer to Screenpass.h to see how UE handles scaling of the different viewport sizes
	PassParameters->CommonParameters.UVScale = FVector2f(float(PassViewSize.Width()) / float(SrcTextureSize.X), float(PassViewSize.Height()) / float(SrcTextureSize.Y));
	// Input is the SceneColor from PostProcess Material Inputs
	PassParameters->CommonParameters.InputTexture = InputTexture;
	PassParameters->CommonParameters.PreviousTexture = TemporalBufferTexture ? TemporalBufferTexture : CurrentTexture;
	PassParameters->CommonParameters.VelocityTexture = VelocityTexture.Texture;
	PassParameters->CommonParameters.DepthTexture = DepthTexture.Texture;
	PassParameters->CommonParameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();
	PassParameters->CommonParameters.DatamoshStyle = static_cast<int32>(DatamoshParameters.DatamoshStyle);
	PassParameters->CommonParameters.DatamoshDebugFloat = CVarDatamoshDebugFloat.GetValueOnRenderThread();
	PassParameters->CommonParameters.DatamoshFactor = DatamoshParameters.DatamoshFactor;
	PassParameters->CommonParameters.DatamoshTrailFactor = DatamoshParameters.DatamoshTrailFactor;
	PassParameters->CommonParameters.DatamoshNoise = static_cast<int32>(DatamoshParameters.DatamoshNoise);
	PassParameters->CommonParameters.DatamoshNoiseIntensity = DatamoshParameters.DatamoshNoiseIntensity;
	PassParameters->CommonParameters.DatamoshNoiseSize = DatamoshParameters.DatamoshNoiseSize;
	PassParameters->CommonParameters.DatamoshMotion = DatamoshParameters.DatamoshMotion / MotionAttenuation;
	PassParameters->CommonParameters.DatamoshTrail = static_cast<int32>(DatamoshParameters.DatamoshTrail);
	PassParameters->CommonParameters.DatamoshSharpness = DatamoshParameters.DatamoshSharpness;
	PassParameters->CommonParameters.DatamoshStyleIntensity = DatamoshParameters.DatamoshStyleIntensity;
	PassParameters->CommonParameters.DatamoshVelocityCompensation = CVarDatamoshVelocityCompensation.GetValueOnRenderThread();
	PassParameters->CommonParameters.DatamoshDistance = DatamoshParameters.DatamoshDistance;

	PassParameters->ScreenSpaceMaskTexture = InputMaskTexture;
	PassParameters->SceneColorTextureSize = InputTexture->Desc.Extent;

	// Create UAV from Out Texture
	PassParameters->Output = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutputTexture));

	FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ViewRect.Size(), DefaultGroupSize);

	// Set and execute compute shader
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("Datamosh Screen Space %dx%d", PassViewSize.Width(), PassViewSize.Height()),
		ComputeShader,
		PassParameters,
		GroupCount);

	GraphBuilder.QueueTextureExtraction(TemporalBufferTexture, &TemporalPRT);
	AddCopyTexturePass(GraphBuilder, OutputTexture, TemporalBufferTexture);

	return OutputTexture;
}

FRDGTextureRef FDatamoshViewExtension::RenderTargetDatamosh(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FScreenPassTexture& SceneColor,
	FScreenPassTexture& VelocityTexture,
	FScreenPassTexture& SceneDepth,
	FRDGTextureRef& InputTexture,
	FRDGTextureRef& RenderTargetTexture,
	FRDGTextureRef& MaskTexture,
	const FIntRect& ViewRect)
{
	// Render Target object size
	const FIntRect RenderViewport = FIntRect(0, 0, RenderTargetTexture->Desc.Extent.X, RenderTargetTexture->Desc.Extent.Y);

	// Setup all the descriptors to create a temp texture
	FRDGTextureDesc RTDesc = RenderTargetTexture->Desc;
	RTDesc.Reset();
	RTDesc.Flags |= TexCreate_UAV;
	RTDesc.Flags |= TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_Dynamic | TexCreate_NoFastClearFinalize;
	//RTDesc.Extent = RenderViewport.Size();
	RTDesc.ClearValue = FClearValueBinding(FLinearColor::Black);

	FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(RTDesc, TEXT("RTDatamosh"));

	////// 
	// Temporal Texture Buffer
	// Since in versions above 5.2 a significant part of the RenderGraphBuilder was rewritten, the resource lifetime in the graph when using the ConvertToExternalTexture method is insufficient.
	// So we have to register the texture separately to ensure its resource reaches the end of the graph.
	GRenderTargetPool.FindFreeElement(
		FRHICommandListExecutor::GetImmediateCommandList(),
		RTDesc,
		TemporalPRT,
		TEXT("DE_Local_RT_PreviousTexture"));
	FRDGTextureRef TemporalBufferTexture = GraphBuilder.RegisterExternalTexture(TemporalPRT, TEXT("DatamoshTemporal"));
	//
	//////

	FIntRect PassViewSize = ViewRect;
	FIntPoint SrcTextureSize = SceneColor.Texture->Desc.Extent;

	FDatamoshRTCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDatamoshRTCS::FParameters>();
	// Get the input sizes (do note that viewport visible area might not be the full extent of the SceneColor texture
	// Method to setup common parameters, we use this to pass ViewUniformBuffer data
	// There is a lot of useful stuff in the ViewUniformBuffer, do note that when getting this from the SceneView, a lot them seem to be unpopulated
	PassParameters->CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->CommonParameters.ViewportRect = PassViewSize;
	PassParameters->CommonParameters.ViewportInvSize = FVector2f(1.0f / PassViewSize.Width(), 1.0f / PassViewSize.Height());
	// Conversion from the full texture to the actual used size
	// Refer to Screenpass.h to see how UE handles scaling of the different viewport sizes
	PassParameters->CommonParameters.UVScale = FVector2f(float(PassViewSize.Width()) / float(SrcTextureSize.X), float(PassViewSize.Height()) / float(SrcTextureSize.Y));
	// Input is the SceneColor from PostProcess Material Inputs
	PassParameters->CommonParameters.InputTexture = InputTexture;
	PassParameters->CommonParameters.PreviousTexture = TemporalBufferTexture ? TemporalBufferTexture : OutputTexture;
	PassParameters->CommonParameters.VelocityTexture = VelocityTexture.Texture;
	PassParameters->CommonParameters.DepthTexture = SceneDepth.Texture;
	PassParameters->CommonParameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();
	PassParameters->CommonParameters.DatamoshStyle = static_cast<int32>(DatamoshParameters.DatamoshStyle);
	PassParameters->CommonParameters.DatamoshDebugFloat = CVarDatamoshDebugFloat.GetValueOnRenderThread();
	PassParameters->CommonParameters.DatamoshFactor = DatamoshParameters.DatamoshFactor;
	PassParameters->CommonParameters.DatamoshTrailFactor = DatamoshParameters.DatamoshTrailFactor;
	PassParameters->CommonParameters.DatamoshNoise = static_cast<int32>(DatamoshParameters.DatamoshNoise);
	PassParameters->CommonParameters.DatamoshNoiseIntensity = DatamoshParameters.DatamoshNoiseIntensity;
	PassParameters->CommonParameters.DatamoshNoiseSize = DatamoshParameters.DatamoshNoiseSize;
	PassParameters->CommonParameters.DatamoshMotion = DatamoshParameters.DatamoshMotion / MotionAttenuation;
	PassParameters->CommonParameters.DatamoshTrail = static_cast<int32>(DatamoshParameters.DatamoshTrail);
	PassParameters->CommonParameters.DatamoshStyleIntensity = DatamoshParameters.DatamoshStyleIntensity;
	PassParameters->CommonParameters.DatamoshVelocityCompensation = CVarDatamoshVelocityCompensation.GetValueOnRenderThread();
	PassParameters->CommonParameters.DatamoshSharpness = DatamoshParameters.DatamoshSharpness;
	PassParameters->CommonParameters.DatamoshDistance = DatamoshParameters.DatamoshDistance;


	PassParameters->RenderTargetTextureSize = RenderTargetTexture->Desc.Extent;
	PassParameters->RenderTargetMaskTexture = MaskTexture;
	PassParameters->RenderTargetSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->RenderTargetUVScale = SrcTextureSize;

	PassParameters->Output = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutputTexture));

	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FDatamoshRTCS> ComputeShader(GlobalShaderMap);

	const FIntPoint ThreadCount = RenderViewport.Size();
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCount, DefaultGroupSize);

	// Set and execute compute shader
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("Datamosh Render Target %dx%d", RenderViewport.Width(), RenderViewport.Height()),
		ComputeShader,
		PassParameters,
		GroupCount
	);

	GraphBuilder.QueueTextureExtraction(TemporalBufferTexture, &TemporalPRT);
	AddCopyTexturePass(GraphBuilder, OutputTexture, TemporalBufferTexture);

	return OutputTexture;
}

/*A separate method for a configured shader that essentially allows one crucial thing: converting PF_CustomStencil into other formats. 
PF_CustomStencil does not support creating UAVs, which means it’s impossible to create a texture storing the previous frame from a texture with this format. 
Besides conversion, we can also customize the texture, where we generate the mask for the trail feature*/
FRDGTextureRef FDatamoshViewExtension::CreateDatamoshMask(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& SceneColor, FScreenPassTexture& VelocityTexture, FScreenPassTexture& DepthTexture, FRDGTextureRef& InputTexture, FRDGTextureRef& InputMaskTexture, const FIntRect& ViewRect)
{
	const FIntRect RenderViewport = FIntRect(0, 0, InputTexture->Desc.Extent.X, InputTexture->Desc.Extent.Y);

	// Setup all the descriptors to create a target texture
	FRDGTextureDesc OutputDesc = InputTexture->Desc;
	OutputDesc.Reset();
	OutputDesc.Flags |= TexCreate_UAV;
	OutputDesc.Flags |= TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_Dynamic | TexCreate_NoFastClearFinalize;
	OutputDesc.Extent = InputTexture->Desc.Extent;
	OutputDesc.ClearValue = FClearValueBinding(FLinearColor::Black);

	FRDGTextureRef const OutTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("DatamoshMask"));
	FRDGTextureRef const CurrentTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("DE_Mask_CurrentTexture"));

	FRDGTextureRef const TemporalBufferTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("DatamoshMaskTemporal"), ERDGTextureFlags::MultiFrame);

	FIntRect PassViewSize = ViewRect;
	FIntPoint SrcTextureSize = SceneColor.Texture->Desc.Extent;

	FDatamoshCreateMaskCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDatamoshCreateMaskCS::FParameters>();

	// Get the input sizes (do note that viewport visible area might not be the full extent of the SceneColor texture
	// Method to setup common parameters, we use this to pass ViewUniformBuffer data
	// There is a lot of useful stuff in the ViewUniformBuffer, do note that when getting this from the SceneView, a lot them seem to be unpopulated
	PassParameters->CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->CommonParameters.ViewportRect = PassViewSize;
	PassParameters->CommonParameters.ViewportInvSize = InputTexture->Desc.Extent;//FVector2f(1.0f / PassViewSize.Width(), 1.0f / PassViewSize.Height());
	// Conversion from the full texture to the actual used size
	// Refer to Screenpass.h to see how UE handles scaling of the different viewport sizes
	PassParameters->CommonParameters.UVScale = FVector2f(float(PassViewSize.Width()) / float(SrcTextureSize.X), float(PassViewSize.Height()) / float(SrcTextureSize.Y));
	PassParameters->CommonParameters.InputTexture = InputTexture;
	PassParameters->CommonParameters.PreviousTexture = TemporalBufferTexture ? TemporalBufferTexture : CurrentTexture;
	PassParameters->CommonParameters.VelocityTexture = VelocityTexture.Texture;
	PassParameters->CommonParameters.DepthTexture = DepthTexture.Texture;

	PassParameters->CommonParameters.DatamoshStyle = static_cast<int32>(DatamoshParameters.DatamoshStyle);
	PassParameters->CommonParameters.DatamoshDebugFloat = CVarDatamoshDebugFloat.GetValueOnRenderThread();
	PassParameters->CommonParameters.DatamoshFactor = DatamoshParameters.DatamoshFactor;
	PassParameters->CommonParameters.DatamoshTrailFactor = DatamoshParameters.DatamoshTrailFactor;
	PassParameters->CommonParameters.DatamoshNoise = static_cast<int32>(DatamoshParameters.DatamoshNoise);
	PassParameters->CommonParameters.DatamoshNoiseIntensity = DatamoshParameters.DatamoshNoiseIntensity;
	PassParameters->CommonParameters.DatamoshNoiseSize = DatamoshParameters.DatamoshNoiseSize;
	PassParameters->CommonParameters.DatamoshMotion = DatamoshParameters.DatamoshMotion / MotionAttenuation;
	PassParameters->CommonParameters.DatamoshTrail = static_cast<int32>(DatamoshParameters.DatamoshTrail);
	PassParameters->CommonParameters.DatamoshStyleIntensity = DatamoshParameters.DatamoshStyleIntensity;
	PassParameters->CommonParameters.DatamoshVelocityCompensation = CVarDatamoshVelocityCompensation.GetValueOnRenderThread();
	PassParameters->CommonParameters.DatamoshSharpness = DatamoshParameters.DatamoshSharpness;
	PassParameters->CommonParameters.DatamoshDistance = DatamoshParameters.DatamoshDistance;


	GraphBuilder.ConvertToExternalTexture(TemporalBufferTexture);

	PassParameters->CustomDepthTexture = InputMaskTexture;
	PassParameters->StencilValue = CVarDatamoshStencil.GetValueOnRenderThread();

	const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(&View);
	const FSceneTextures& SceneTextures = ViewInfo->GetSceneTextures();

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	PassParameters->StencilTextureSize = SceneTextures.Stencil->Desc.Texture->Desc.Extent;
	PassParameters->StencilTexture = SystemTextures.StencilDummySRV;

	if (InputMaskTexture->Desc.Format == PF_DepthStencil)
	{
		PassParameters->StencilTexture = SceneTextures.CustomDepth.Stencil;
	}

	// Create UAVs from the textures
	PassParameters->Output = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutTexture));

	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FDatamoshCreateMaskCS> ComputeShader(GlobalShaderMap);

	const FIntPoint ThreadCount = RenderViewport.Size();
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCount, DefaultGroupSize);

	// Set and execute compute shader
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("Datamosh Mask %dx%d", RenderViewport.Width(), RenderViewport.Height()),
		ComputeShader,
		PassParameters,
		GroupCount
	);

	//GraphBuilder.QueueTextureExtraction(TemporalBufferTexture, &TemporalPRT);
	AddCopyTexturePass(GraphBuilder, OutTexture, TemporalBufferTexture);

	return OutTexture;
}

void FDatamoshViewExtension::CreatePooledRenderTarget_RenderThread(UTextureRenderTarget2D* InputRenderTarget, TRefCountPtr<IPooledRenderTarget>& InputRenderTargetPooled)
{
	checkf(IsInRenderingThread() || IsInRHIThread(), TEXT("DatamoshExtension: Cannot create PRT from outside the rendering thread."));

	if (!InputRenderTarget || !InputRenderTarget->GetRenderTargetResource())
	{
		UE_LOG(LogTemp, Error, TEXT("DatamoshExtension: Invalid InputRenderTarget or it's resource."));
		return;
	}

	const FTextureRenderTargetResource* RenderTargetResource = InputRenderTarget->GetRenderTargetResource();
	const FTextureRHIRef RenderTargetRHI = RenderTargetResource->GetRenderTargetTexture();

	if (!RenderTargetRHI.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("DatamoshExtension: Render Target RHI is null"));
		return;
	}

	if (InputRenderTargetPooled.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("DatamoshExtension: Releasing previous Pooled Render Target. Resource: %p"), InputRenderTargetPooled.GetReference());
		InputRenderTargetPooled.SafeRelease();
	}

	FSceneRenderTargetItem RenderTargetItem;
	RenderTargetItem.TargetableTexture = RenderTargetRHI;
	RenderTargetItem.ShaderResourceTexture = RenderTargetRHI;

	FPooledRenderTargetDesc RenderTargetDesc = FPooledRenderTargetDesc::Create2DDesc(
		RenderTargetResource->GetSizeXY(),
		RenderTargetRHI->GetDesc().Format,
		FClearValueBinding::Black,
		TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV,
		TexCreate_None,
		false
	);

	GRenderTargetPool.CreateUntrackedElement(RenderTargetDesc, InputRenderTargetPooled, RenderTargetItem);

	UE_LOG(LogTemp, Log, TEXT("DatamoshExtension: Untracked PRT resource created. Resource: %p, RHI: %p"),
		RenderTargetResource, RenderTargetRHI.GetReference());
}

void FDatamoshViewExtension::ResetRenderTarget_RenderThread()
{
	ENQUEUE_RENDER_COMMAND(ResetRTCommand)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			if (InputDatamoshRenderTargetPooled.IsValid() && InputDatamoshMaskRenderTargetPooled.IsValid())
			{
				InputDatamoshRenderTargetPooled.SafeRelease();
				InputDatamoshMaskRenderTargetPooled.SafeRelease();
				UE_LOG(LogTemp, Log, TEXT("DatamoshExtension: Render Target has been reset."));
				CreatePooledRenderTarget_RenderThread(InputDatamoshRenderTarget, InputDatamoshRenderTargetPooled);
				CreatePooledRenderTarget_RenderThread(InputDatamoshMaskRenderTarget, InputDatamoshMaskRenderTargetPooled);
			}
		}
		);
}

FDatamoshViewExtension* FDatamoshViewExtension::GetActiveInstance()
{
	return ActiveInstance;
}

void FDatamoshViewExtension::SetInputDatamoshRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
	InputDatamoshRenderTarget = RenderTarget;
}

void FDatamoshViewExtension::SetInputDatamoshMaskRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
	InputDatamoshMaskRenderTarget = RenderTarget;
}