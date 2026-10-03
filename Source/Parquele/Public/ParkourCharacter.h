// 3人称パルクールキャラクター。入力・カメラ・復帰を担当し、移動は UParkourMovementComponent に任せる。
// 入力アセット（Input Action / Mapping Context）を割り当てなければ C++ で既定の割り当てを作る。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ParkourCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UParkourMovementComponent;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

UCLASS()
class PARQUELE_API AParkourCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AParkourCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UFUNCTION(BlueprintPure, Category = "Parkour")
	UParkourMovementComponent* GetParkourMovement() const;

	/** チェックポイント通過時に呼ばれる */
	void ReachCheckpoint(const FTransform& RespawnAt, bool bIsGoal);

	/** 直前のチェックポイント（bToCourseStart ならスタート地点）へ即座に戻す */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	void Respawn(bool bToCourseStart);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** マネキン導入までの仮の見た目。Skeletal Mesh を設定すると自動で隠れる */
	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderNose;

	/** 未設定なら下の Input Action と合わせて C++ で生成する */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookStickAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookMouseAction;

	/** 上アクション：ジャンプ／ヴォルト／マントル／壁走り／壁蹴り */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> UpAction;

	/** 下アクション：スライド／受け身／しゃがみ */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> DownAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SprintToggleAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> RestartAction;

	/** 右スティックの旋回速度 */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (Units = "deg/s"))
	float LookStickRate = 180.f;

	UPROPERTY(EditAnywhere, Category = "Input")
	float MouseSensitivity = 0.4f;

	/** これより下に落ちたら直前のチェックポイントへ戻す */
	UPROPERTY(EditAnywhere, Category = "Respawn", meta = (Units = "cm"))
	float RespawnKillZ = -1500.f;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowDebugHud = true;

private:
	void BuildDefaultInput();
	void Move(const FInputActionValue& Value);
	void MoveCompleted(const FInputActionValue& Value);
	void LookStick(const FInputActionValue& Value);
	void LookMouse(const FInputActionValue& Value);
	void UpPressed();
	void DownPressed();
	void DownReleased();
	void ToggleSprint();
	void RestartPressed();
	void UpdatePlaceholderForHalfHeight(float HalfHeight);

	FTransform CourseStartTransform;
	FTransform RespawnTransform;
	double RunStartTime = 0.0;
	double LastGoalTime = -1.0;
};
