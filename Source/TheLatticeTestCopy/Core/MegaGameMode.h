#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MegaGameMode.generated.h"

/** Default game mode: spawns AMegaCharacter driven by AMegaPlayerController. */
UCLASS()
class THELATTICETESTCOPY_API AMegaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMegaGameMode();
};
