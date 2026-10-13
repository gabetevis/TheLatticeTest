// Copyright 2025 Andrew Laptev. All Rights Reserved.

#include "DatamoshSubsystem.h"
#include "DatamoshViewExtension.h"
#include "DatamoshDeveloperSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SceneViewExtension.h"

void UDatamoshSubsystem::Update(UWorld* InWorld, FSceneViewFamily& InViewFamily)
{
	if (!bDoUpdates)
	{
		return;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("DatamoshSubsystem::Update World: %s, Num. Views: %u"), *InWorld->GetName(), InViewFamily.Views.Num());
	//if (UDatamoshWorldSubsystem* Subsystem = InWorld->GetSubsystem<UDatamoshWorldSubsystem>())
	//{
	//	//DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDatamoshSubsystem::Update"), STAT_DatamoshEffect_UpdateAll, STATGROUP_DatamoshEffect);
	//	int32 ViewIndex = 0;
	//	for (const FSceneView*& View : InViewFamily.Views)
	//	{
	//		FSceneView* MutableSceneView = const_cast<FSceneView*>(View);
	//		++ViewIndex;
	//	}
	//}
}

void UDatamoshSubsystem::ToggleUpdate(const TOptional<bool>& bInShouldUpdate)
{
	// New value should be provided by user, or the inverse of the existing value
	bool bShouldUpdate = bInShouldUpdate.Get(!bDoUpdates);
	if (bDoUpdates != bShouldUpdate)
	{
		bDoUpdates = bShouldUpdate;
	}
}

TSharedPtr<FDatamoshViewExtension> UDatamoshWorldSubsystem::GetDatamoshViewExtension()
{
	return DatamoshViewExtension;
}

void UDatamoshWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();

	DatamoshViewExtension = FSceneViewExtensions::NewExtension<FDatamoshViewExtension>(World);

	UE_LOG(LogTemp, Log, TEXT("DatamoshSubsystem: Subsystem initialized & SceneViewExtension created"));

	FString PathDatamoshRenderTarget = "RT_Datamosh'/DatamoshEffect/Textures/RT_Datamosh.RT_Datamosh'";
	FString PathDatamoshMaskRenderTarget = "RT_DatamoshMask'/DatamoshEffect/Textures/RT_DatamoshMask.RT_DatamoshMask'";

	if (UTextureRenderTarget2D* RenderTargetDatamosh = LoadObject<UTextureRenderTarget2D>(nullptr, *PathDatamoshRenderTarget))
	{
		DatamoshViewExtension->SetInputDatamoshRenderTarget(RenderTargetDatamosh);
	}

	if (UTextureRenderTarget2D* RenderTargetDatamoshMask = LoadObject<UTextureRenderTarget2D>(nullptr, *PathDatamoshMaskRenderTarget))
	{
		DatamoshViewExtension->SetInputDatamoshMaskRenderTarget(RenderTargetDatamoshMask);
	}
}

void UDatamoshWorldSubsystem::Deinitialize()
{
	if (UDatamoshSubsystem* EngineSubsystem = GEngine->GetEngineSubsystem<UDatamoshSubsystem>())
	{
		EngineSubsystem->OnWorldDestroyed(GetWorld());
	}

	// This has to be done on render thread as otherwise there could be a race condition when new level is loaded. 
	ENQUEUE_RENDER_COMMAND(ReleaseSVE)([this](FRHICommandListImmediate& RHICmdList)
		{
			// Prevent this SVE from being gathered, in case it is kept alive by a strong reference somewhere else.
			{
				DatamoshViewExtension->IsActiveThisFrameFunctions.Empty();

				FSceneViewExtensionIsActiveFunctor IsActiveFunctor;

				IsActiveFunctor.IsActiveFunction = [](const ISceneViewExtension* InSceneViewExtension, const FSceneViewExtensionContext& InContext)
					{
						return TOptional<bool>(false);
					};

				DatamoshViewExtension->IsActiveThisFrameFunctions.Add(IsActiveFunctor);
			}

			DatamoshViewExtension.Reset();
		});

	Super::Deinitialize();
}

bool UDatamoshSubsystem::GetDatamoshActive()
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Datamosh.Preview"));
	bool bPreview = CVar && CVar->GetInt() == 1;

	if (bPreview)
	{
		bDatamoshActive = true;
	}

	return bDatamoshActive;
}

void UDatamoshSubsystem::SetDatamoshActive(bool bActive)
{
	bDatamoshActive = bActive;
}

void UDatamoshSubsystem::SetDatamoshParameters(FDatamoshParameters Params)
{
	ShaderParameters = Params;
}

FDatamoshParameters UDatamoshSubsystem::GetDatamoshParameters()
{
	return ShaderParameters;
}

void UDatamoshSubsystem::OnWorldDestroyed(UWorld* InWorld)
{
	// Custom implementation
}
