#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MegaPlayerController.generated.h"

class UVTTOverlayWidget;

/** Owns player-facing UI. The VTT overlay is the game's only HUD. */
UCLASS()
class THELATTICETESTCOPY_API AMegaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMegaPlayerController();

	UFUNCTION(BlueprintPure, Category="Mega|UI")
	UVTTOverlayWidget* GetVTTOverlay() const { return VTTOverlay; }

protected:
	virtual void BeginPlay() override;

	/** Widget class used for the VTT overlay. Works as-is; subclass only to override drawing. */
	UPROPERTY(EditDefaultsOnly, Category="Mega|UI")
	TSubclassOf<UVTTOverlayWidget> VTTOverlayClass;

	UPROPERTY(EditDefaultsOnly, Category="Mega|UI")
	int32 VTTOverlayZOrder = 10;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVTTOverlayWidget> VTTOverlay;
};
