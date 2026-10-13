// Copyright 2025 Andrew Laptev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SceneViewExtension.h"
#include "RenderGraphUtils.h"
#include "DatamoshBlueprintLibrary.h"
#include "DatamoshParameters.h"
#include "DatamoshSubsystem.h"

class UDatamoshSubsystem;

class FDatamoshViewExtension : public FWorldSceneViewExtension
{
public:
	FDatamoshViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld);

	static FDatamoshViewExtension* GetActiveInstance();

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	virtual int32 GetPriority() const override;

	void ResetRenderTarget_RenderThread();
	void SetInputDatamoshRenderTarget(UTextureRenderTarget2D* RenderTarget);
	void SetInputDatamoshMaskRenderTarget(UTextureRenderTarget2D* RenderTarget);

private:
	void CreatePooledRenderTarget_RenderThread(UTextureRenderTarget2D* InputRenderTargetResource, TRefCountPtr<IPooledRenderTarget>& InputRenderTargetPooled);
	static FDatamoshViewExtension* ActiveInstance;

	FRDGTextureRef ScreenSpaceDatamosh(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& SceneColor, FScreenPassTexture& VelocityTexture, FScreenPassTexture& DepthTexture, FRDGTextureRef& InputTexture, FRDGTextureRef& InputMaskTexture, const FIntRect& ViewRect);
	FRDGTextureRef CreateDatamoshMask(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& SceneColor, FScreenPassTexture& VelocityTexture, FScreenPassTexture& DepthTexture, FRDGTextureRef& InputTexture, FRDGTextureRef& InputMaskTexture, const FIntRect& ViewRect);
	FRDGTextureRef RenderTargetDatamosh(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& SceneColor, FScreenPassTexture& VelocityTexture, FScreenPassTexture& DepthTexture, FRDGTextureRef& InputTexture, FRDGTextureRef& RenderTargetTexture, FRDGTextureRef& MaskTexture, const FIntRect& ViewRect);

	/** Struct that contains all the shader values */
	FDatamoshParameters DatamoshParameters;
	
	/** Runtime value for effect activation.
	Needed for greater control, for example, render the effect even if it's not used for SceneColor.Texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Datamosh")
	bool bDatamoshActive = false;

	float MotionAttenuation = 10000;

	FIntPoint DefaultGroupSize = FIntPoint(16, 16);
	TWeakObjectPtr<UDatamoshSubsystem> DatamoshSubsystemWeak;

	TObjectPtr<UTextureRenderTarget2D> InputDatamoshRenderTarget = nullptr;
	TRefCountPtr<IPooledRenderTarget> InputDatamoshRenderTargetPooled;
	TObjectPtr<UTextureRenderTarget2D> InputDatamoshMaskRenderTarget = nullptr;
	TRefCountPtr<IPooledRenderTarget> InputDatamoshMaskRenderTargetPooled;

	FRDGTextureRef TemporalBufferTexture_ScreenSpace;
	FRDGTextureRef TemporalBufferTexture_RenderTarget;

	TRefCountPtr<IPooledRenderTarget> TemporalPRT;
	TRefCountPtr<IPooledRenderTarget> TemporalMaskPRT;
};