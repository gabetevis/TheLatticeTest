#include "Player/MegaPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "VTT/VTTOverlayWidget.h"

AMegaPlayerController::AMegaPlayerController()
{
	VTTOverlayClass = UVTTOverlayWidget::StaticClass();
}

void AMegaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -89.f;
		PlayerCameraManager->ViewPitchMax = 89.f;
	}

	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);

	if (VTTOverlayClass)
	{
		VTTOverlay = CreateWidget<UVTTOverlayWidget>(this, VTTOverlayClass);
		if (VTTOverlay)
		{
			VTTOverlay->AddToViewport(VTTOverlayZOrder);
		}
	}
}
