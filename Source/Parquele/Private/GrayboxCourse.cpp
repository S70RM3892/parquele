#include "GrayboxCourse.h"

#include "ParkourCheckpoint.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// コースの各区間（X が進行方向、Z=0 がスタート地点の床面。単位 cm）
	struct FCheckpointSpec
	{
		FVector Location;
		bool bIsGoal;
	};

	const FCheckpointSpec CheckpointSpecs[] = {
		{ FVector(2600.f, 0.f, 0.f), false },    // マントル台の後
		{ FVector(4800.f, 0.f, -200.f), false }, // 受け身用の落下の後
		{ FVector(8000.f, 0.f, -200.f), false }, // ウォールラン区間の後
		{ FVector(9500.f, 0.f, -200.f), true },  // ゴール
	};
}

AGrayboxCourse::AGrayboxCourse()
{
	PrimaryActorTick.bCanEverTick = false;

	Blocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Blocks"));
	RootComponent = Blocks;
	Blocks->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GridMaterial(TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	if (CubeMesh.Succeeded()) Blocks->SetStaticMesh(CubeMesh.Object);
	if (GridMaterial.Succeeded()) Blocks->SetMaterial(0, GridMaterial.Object);
}

void AGrayboxCourse::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

FTransform AGrayboxCourse::GetStartTransform() const
{
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(FVector(-300.f, 0.f, 100.f)));
}

void AGrayboxCourse::AddBlock(const FVector& Center, const FVector& Size)
{
	// BasicShapes の Cube は 100cm 角・中心が原点
	Blocks->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.f));
}

void AGrayboxCourse::Build()
{
	Blocks->ClearInstances();

	// 区間1：スタート床（上面 Z=0、X -500〜3900）
	AddBlock(FVector(1700.f, 0.f, -50.f), FVector(4400.f, 800.f, 100.f));
	// ヴォルト：標準（高さ100・奥行き60）と低め（高さ70）
	AddBlock(FVector(800.f, 0.f, 50.f), FVector(60.f, 400.f, 100.f));
	AddBlock(FVector(1200.f, 0.f, 35.f), FVector(60.f, 400.f, 70.f));
	// マントル：高さ200の台（X 1700〜2100）。降りるのは200cmの落下（受け身不要）
	AddBlock(FVector(1900.f, 0.f, 100.f), FVector(400.f, 800.f, 200.f));
	// ウォールクライム：高さ300の台（X 2800〜3400）
	AddBlock(FVector(3100.f, 0.f, 150.f), FVector(600.f, 800.f, 300.f));
	// 走り幅跳び：隙間500cm の先の高さ300の台（X 3900〜4500）。落ちても下の床からクライムで戻れる
	AddBlock(FVector(4200.f, 0.f, 150.f), FVector(600.f, 800.f, 300.f));

	// 区間2：500cm 下の床（上面 Z=-200、X 4500〜5900）。台から降りると受け身判定
	AddBlock(FVector(5200.f, 0.f, -250.f), FVector(1400.f, 800.f, 100.f));
	// スライド：床から110cm上に浮いたバー（X 5200）
	AddBlock(FVector(5200.f, 0.f, -70.f), FVector(40.f, 800.f, 40.f));

	// 区間3：穴の上のウォールラン（壁A：X 5800〜6700、壁B：X 6700〜7700、長さ800以上）
	// 壁Aに斜め（15〜60°）に入り、壁蹴りで壁Bへ繋ぐ
	AddBlock(FVector(6250.f, 250.f, 0.f), FVector(900.f, 40.f, 400.f));
	AddBlock(FVector(7200.f, -250.f, 0.f), FVector(1000.f, 40.f, 400.f));

	// 区間4：ゴール床（上面 Z=-200、X 7600〜10000）
	AddBlock(FVector(8800.f, 0.f, -250.f), FVector(2400.f, 800.f, 100.f));
	// ゴール標識
	AddBlock(FVector(9500.f, 420.f, 100.f), FVector(40.f, 40.f, 600.f));
	AddBlock(FVector(9500.f, -420.f, 100.f), FVector(40.f, 40.f, 600.f));
}

void AGrayboxCourse::BeginPlay()
{
	Super::BeginPlay();
	if (!bSpawnCheckpoints) return;

	for (const FCheckpointSpec& Spec : CheckpointSpecs)
	{
		// トリガーの底面を床面に合わせる
		const FVector Location = GetActorTransform().TransformPosition(Spec.Location + FVector(0.f, 0.f, 300.f));
		if (AParkourCheckpoint* Checkpoint = GetWorld()->SpawnActor<AParkourCheckpoint>(AParkourCheckpoint::StaticClass(), Location, GetActorRotation()))
		{
			Checkpoint->bIsGoal = Spec.bIsGoal;
		}
	}
}
