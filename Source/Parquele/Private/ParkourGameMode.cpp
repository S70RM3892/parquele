#include "ParkourGameMode.h"

#include "GrayboxCourse.h"
#include "ParkourCharacter.h"
#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

AParkourGameMode::AParkourGameMode()
{
	DefaultPawnClass = AParkourCharacter::StaticClass();
}

void AParkourGameMode::StartPlay()
{
	EnsureTestEnvironment();
	Super::StartPlay();
}

AActor* AParkourGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// プレイヤーのスポーンはワールドの BeginPlay より先に来ることがあるので、ここでも用意する
	EnsureTestEnvironment();
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AParkourGameMode::EnsureTestEnvironment()
{
	if (bEnvironmentChecked) return;
	bEnvironmentChecked = true;

	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AGrayboxCourse* Course = nullptr;
	for (TActorIterator<AGrayboxCourse> It(World); It; ++It)
	{
		Course = *It;
		break;
	}
	if (!Course && bSpawnGrayboxIfMissing)
	{
		Course = World->SpawnActor<AGrayboxCourse>(AGrayboxCourse::StaticClass(), FTransform::Identity, Params);
	}

	bool bHasPlayerStart = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		bHasPlayerStart = true;
		break;
	}
	if (!bHasPlayerStart && Course)
	{
		World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Course->GetStartTransform(), Params);
	}

	bool bHasSun = false;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		bHasSun = true;
		break;
	}
	if (!bHasSun)
	{
		// 夕方の逆光を想定した低い太陽（コースの進行方向の正面奥）
		if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 1000.f), FRotator(-20.f, 180.f, 0.f), Params))
		{
			Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			Sun->GetLightComponent()->SetIntensity(6.f);
		}
	}
}
