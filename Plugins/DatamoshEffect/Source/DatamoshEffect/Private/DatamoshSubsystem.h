// Copyright 2025 Andrew Laptev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "SceneView.h"
#include "DatamoshParameters.h"
#include "DatamoshSubsystem.generated.h"

 UCLASS(BlueprintType)
class DATAMOSHEFFECT_API UDatamoshSubsystem : public UEngineSubsystem
 {
 	GENERATED_BODY()
 public:

    void Update(UWorld* InWorld, FSceneViewFamily& InViewFamily);

    // Toggles if no arg given
    void ToggleUpdate(const TOptional<bool>& bInShouldUpdate = {});

    // The source render target texture asset
    UPROPERTY()
    TObjectPtr<UTextureRenderTarget2D> DatamoshRenderTargetSource = nullptr;
    TObjectPtr<UTextureRenderTarget2D> DatamoshMaskRenderTargetSource = nullptr;

    bool GetDatamoshActive();
    void SetDatamoshActive(bool bActive);

    void SetDatamoshParameters(FDatamoshParameters Params);
    FDatamoshParameters GetDatamoshParameters();

 private:
     friend class UDatamoshWorldSubsystem;

     void OnWorldDestroyed(UWorld* InWorld);

     std::atomic<bool> bDoUpdates = true;

 	TSharedPtr<class FDatamoshViewExtension, ESPMode::ThreadSafe> DatamoshViewExtension;

    FDatamoshParameters ShaderParameters;
    bool bDatamoshActive;
    bool bDatamoshPreview;
 };

 UCLASS()
     class DATAMOSHEFFECT_API UDatamoshWorldSubsystem : public UWorldSubsystem
 {
     GENERATED_BODY()
 public:
     TSharedPtr<FDatamoshViewExtension> GetDatamoshViewExtension();
 protected:
     // ~Begin USubsystem
     virtual void Initialize(FSubsystemCollectionBase& Collection) override;
     virtual void Deinitialize() override;
     // ~End USubsystem
 private:
     friend class UDatamoshSubsystem;
     TSharedPtr<FDatamoshViewExtension> DatamoshViewExtension;
 };