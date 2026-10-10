// Attach to any actor (prop, enemy, landmark) to give it VTT text.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "VTT/VTTTypes.h"
#include "VTTComponent.generated.h"

UENUM(BlueprintType)
enum class EVTTBoundsMode : uint8
{
	/** Frame the owner's visible geometry. Tracks animated meshes (enemies) automatically. */
	OwnerBounds,
	/** Frame this component's box. Place and scale it in the viewport to frame a specific detail. */
	CustomBox
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVTTComponentShownSignature, UVTTComponent*, Component, FName, EntryId);

/**
 * Marks an actor as VTT-readable and holds the text it can display.
 *
 * It is a box component so the frame can be placed visually in the editor (CustomBox mode);
 * collision is disabled and it is hidden in game. Registers itself with UVTTSubsystem on BeginPlay.
 */
UCLASS(ClassGroup=(VTT), meta=(BlueprintSpawnableComponent, DisplayName="VTT"),
	HideCategories=(Collision, Physics, Navigation, Cooking, HLOD, Mobile, LOD, Lighting, Rendering, Tags, AssetUserData, Activation))
class THELATTICETESTCOPY_API UVTTComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UVTTComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VTT", meta=(TitleProperty="Label"))
	TArray<FVTTEntry> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VTT")
	bool bVTTEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VTT")
	EVTTBoundsMode BoundsMode = EVTTBoundsMode::OwnerBounds;

	/** Max distance from the camera for this object to be readable. 0 = project default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VTT|Visibility", meta=(ClampMin="0", ForceUnits="cm"))
	float MaxDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VTT|Visibility")
	bool bRequireLineOfSight = true;

	/** After this object shows anything, it won't show an ambient pop-up again for this long. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VTT|Ambient", meta=(ClampMin="0", ForceUnits="s"))
	float AmbientCooldown = 45.f;

	/** Relative chance of this object being picked over other visible VTT objects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VTT|Ambient", meta=(ClampMin="0"))
	float AmbientWeight = 1.f;

	UPROPERTY(BlueprintAssignable, Category="VTT")
	FVTTComponentShownSignature OnVTTShown;

	/**
	 * Scripted request: show the entry with this Id (None = first unseen entry, else random).
	 * Waits until the object is visible, using default request params.
	 */
	UFUNCTION(BlueprintCallable, Category="VTT")
	bool ShowVTT(FName EntryId = NAME_None);

	UFUNCTION(BlueprintCallable, Category="VTT")
	void SetVTTEnabled(bool bEnabled);

	/** World-space box the VTT frame is drawn around. Invalid box if nothing to frame. */
	FBox GetVTTWorldBounds() const;

	int32 FindEntryIndex(FName EntryId) const;

	/** Picks an entry for display. Returns INDEX_NONE if none qualifies. */
	int32 PickEntry(bool bForAmbient) const;

	bool IsOnAmbientCooldown(float WorldTime) const;

	/** Called by the subsystem when one of this component's entries is shown. */
	void NotifyEntryShown(int32 EntryIndex, float WorldTime);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool IsEntryAmbientEligible(int32 Index) const;

	TSet<int32> SeenEntries;
	float LastShownTime = -1.e9f;
};
