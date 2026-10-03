// チェックポイント。通過すると落下時の復帰先になる。bIsGoal ならゴール（タイム記録）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParkourCheckpoint.generated.h"

class UBoxComponent;

UCLASS()
class PARQUELE_API AParkourCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	AParkourCheckpoint();

	UPROPERTY(VisibleAnywhere, Category = "Checkpoint")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	bool bIsGoal = false;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
