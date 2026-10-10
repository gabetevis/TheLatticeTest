#include "Core/MegaGameMode.h"

#include "Player/MegaCharacter.h"
#include "Player/MegaPlayerController.h"

AMegaGameMode::AMegaGameMode()
{
	DefaultPawnClass = AMegaCharacter::StaticClass();
	PlayerControllerClass = AMegaPlayerController::StaticClass();
}
