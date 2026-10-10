// Level-placed scripted VTT: when the player enters, show a specific entry on a target actor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VTT/VTTTypes.h"
#include "VTTTriggerVolume.generated.h"

class UBoxComponent;

/**
 * Drop in a level, scale the box, pick a Target Actor that has a VTT component, and set an
 * Entry Id. When the player walks in, the request is queued and (by default) shown the moment
 * the target is actually on screen — e.g. "the next time they look at the statue".
 */
UCLASS(meta=(DisplayName="VTT Trigger Volume"))
class THELATTICETESTCOPY_API AVTTTriggerVolume : public AActor
{
	GENERATED_BODY()

public:
	AVTTTriggerVolume();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VTT")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Actor whose VTT component should speak. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="VTT")
	TObjectPtr<AActor> TargetActor;

	/** Entry to show. None = the target's first unseen entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT")
	FName EntryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT")
	FVTTRequestParams RequestParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VTT")
	bool bTriggerOnce = true;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool bHasTriggered = false;
};
