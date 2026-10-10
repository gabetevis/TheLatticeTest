#include "VTT/VTTTriggerVolume.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "VTT/VTTComponent.h"
#include "VTT/VTTSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogVTTTrigger, Log, All);

AVTTTriggerVolume::AVTTTriggerVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->InitBoxExtent(FVector(200.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->ShapeColor = FColor(255, 200, 60);
	SetRootComponent(TriggerBox);
}

void AVTTTriggerVolume::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AVTTTriggerVolume::OnTriggerBeginOverlap);
}

void AVTTTriggerVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled() || (bTriggerOnce && bHasTriggered))
	{
		return;
	}

	UVTTComponent* Target = TargetActor ? TargetActor->FindComponentByClass<UVTTComponent>() : nullptr;
	if (!Target)
	{
		UE_LOG(LogVTTTrigger, Warning, TEXT("%s: Target Actor is missing or has no VTT component."), *GetName());
		return;
	}

	if (UVTTSubsystem* Subsystem = UVTTSubsystem::Get(this))
	{
		bHasTriggered = Subsystem->RequestCallout(Target, EntryId, RequestParams) || bHasTriggered;
	}
}
