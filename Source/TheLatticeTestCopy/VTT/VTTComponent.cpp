#include "VTT/VTTComponent.h"

#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "VTT/VTTSubsystem.h"

UVTTComponent::UVTTComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	CanCharacterStepUpOn = ECB_No;
	SetHiddenInGame(true);

	InitBoxExtent(FVector(50.f));
	ShapeColor = FColor(80, 255, 210);
}

void UVTTComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UVTTSubsystem* Subsystem = UVTTSubsystem::Get(this))
	{
		Subsystem->RegisterComponent(this);
	}
}

void UVTTComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVTTSubsystem* Subsystem = UVTTSubsystem::Get(this))
	{
		Subsystem->UnregisterComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

bool UVTTComponent::ShowVTT(FName EntryId)
{
	UVTTSubsystem* Subsystem = UVTTSubsystem::Get(this);
	return Subsystem && Subsystem->RequestCallout(this, EntryId, FVTTRequestParams());
}

void UVTTComponent::SetVTTEnabled(bool bEnabled)
{
	bVTTEnabled = bEnabled;
}

FBox UVTTComponent::GetVTTWorldBounds() const
{
	if (BoundsMode == EVTTBoundsMode::OwnerBounds)
	{
		FBox Result(ForceInit);
		if (const AActor* Owner = GetOwner())
		{
			Owner->ForEachComponent<UPrimitiveComponent>(false, [this, &Result](const UPrimitiveComponent* Primitive)
			{
				// ShouldRender() covers visibility and hidden-in-game, so trigger boxes etc. are skipped.
				if (Primitive != this && Primitive->IsRegistered() && Primitive->ShouldRender())
				{
					Result += Primitive->Bounds.GetBox();
				}
			});
		}
		if (Result.IsValid)
		{
			return Result;
		}
		// Nothing visible on the owner: fall back to our own box.
	}

	return CalcBounds(GetComponentTransform()).GetBox();
}

int32 UVTTComponent::FindEntryIndex(FName EntryId) const
{
	return Entries.IndexOfByPredicate([EntryId](const FVTTEntry& Entry) { return Entry.Id == EntryId; });
}

bool UVTTComponent::IsEntryAmbientEligible(int32 Index) const
{
	const FVTTEntry& Entry = Entries[Index];
	return Entry.bAmbient && Entry.Weight > 0.f && !(Entry.bAmbientOnlyOnce && SeenEntries.Contains(Index));
}

int32 UVTTComponent::PickEntry(bool bForAmbient) const
{
	if (Entries.Num() == 0)
	{
		return INDEX_NONE;
	}

	if (!bForAmbient)
	{
		// Scripted with no Id: first entry not seen yet, otherwise any.
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			if (!SeenEntries.Contains(i))
			{
				return i;
			}
		}
		return FMath::RandRange(0, Entries.Num() - 1);
	}

	// Ambient: weighted random, slightly favouring entries not seen yet.
	float TotalWeight = 0.f;
	TArray<TPair<int32, float>, TInlineAllocator<8>> Candidates;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (IsEntryAmbientEligible(i))
		{
			const float Weight = Entries[i].Weight * (SeenEntries.Contains(i) ? 1.f : 2.f);
			Candidates.Emplace(i, Weight);
			TotalWeight += Weight;
		}
	}

	float Roll = FMath::FRand() * TotalWeight;
	for (const TPair<int32, float>& Candidate : Candidates)
	{
		Roll -= Candidate.Value;
		if (Roll <= 0.f)
		{
			return Candidate.Key;
		}
	}
	return Candidates.Num() > 0 ? Candidates.Last().Key : INDEX_NONE;
}

bool UVTTComponent::IsOnAmbientCooldown(float WorldTime) const
{
	return WorldTime - LastShownTime < AmbientCooldown;
}

void UVTTComponent::NotifyEntryShown(int32 EntryIndex, float WorldTime)
{
	SeenEntries.Add(EntryIndex);
	LastShownTime = WorldTime;

	if (Entries.IsValidIndex(EntryIndex))
	{
		OnVTTShown.Broadcast(this, Entries[EntryIndex].Id);
	}
}
