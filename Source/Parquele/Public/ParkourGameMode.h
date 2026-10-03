// 既定のゲームモード。プレイヤーは AParkourCharacter。
// レベルにコース・PlayerStart・ライトが無ければ自動で置くので、空のレベルからすぐ試せる。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ParkourGameMode.generated.h"

UCLASS()
class PARQUELE_API AParkourGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AParkourGameMode();

	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** レベルに AGrayboxCourse が無ければ原点に置く */
	UPROPERTY(EditAnywhere, Category = "Parkour")
	bool bSpawnGrayboxIfMissing = true;

private:
	void EnsureTestEnvironment();
	bool bEnvironmentChecked = false;
};
