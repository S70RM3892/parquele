// Phase 1 用のグレーボックスコース。仕様書「レベル設計メトリクス」の標準寸法で各アクションを1つずつ試せる。
// 空のレベルでも ParkourGameMode が自動で配置する。レベルに置けばエディタ上でも形が見える。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrayboxCourse.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class PARQUELE_API AGrayboxCourse : public AActor
{
	GENERATED_BODY()

public:
	AGrayboxCourse();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** プレイヤーの開始位置（ワールド座標） */
	FTransform GetStartTransform() const;

	UPROPERTY(VisibleAnywhere, Category = "Course")
	TObjectPtr<UInstancedStaticMeshComponent> Blocks;

	UPROPERTY(EditAnywhere, Category = "Course")
	bool bSpawnCheckpoints = true;

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	void AddBlock(const FVector& Center, const FVector& Size);
};
