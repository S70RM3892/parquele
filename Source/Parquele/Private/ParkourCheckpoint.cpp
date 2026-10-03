#include "ParkourCheckpoint.h"

#include "ParkourCharacter.h"
#include "Components/BoxComponent.h"

AParkourCheckpoint::AParkourCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->SetBoxExtent(FVector(50.f, 400.f, 300.f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
}

void AParkourCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AParkourCheckpoint::HandleOverlap);
}

void AParkourCheckpoint::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AParkourCharacter* Character = Cast<AParkourCharacter>(OtherActor))
	{
		// 復帰位置はトリガー底面から少し上（カプセル中心の高さ）
		const FVector Location = GetActorLocation() - FVector(0.f, 0.f, Trigger->GetScaledBoxExtent().Z - 100.f);
		Character->ReachCheckpoint(FTransform(GetActorRotation(), Location), bIsGoal);
	}
}
