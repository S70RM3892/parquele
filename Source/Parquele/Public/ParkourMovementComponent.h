// パルクール移動の本体。Character Movement Component にカスタム Movement Mode を足す。
// 移動はカプセル主導（アニメーションは移動を決めない）。Phase 2 でモンタージュ＋Motion Warping を
// OnParkourAction から再生する前提で、ここでは位置と速度だけを扱う。
// ソロ・オフライン前提のため、ネットワーク予測（SavedMove）は実装していない。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ParkourRules.h"
#include "ParkourMovementComponent.generated.h"

class UParkourMovementSettings;

/** MOVE_Custom のサブモード */
UENUM(BlueprintType)
enum class EParkourCustomMode : uint8
{
	None = 0 UMETA(Hidden),
	WallRun,
	/** ヴォルト・マントル・クライムなど、決めた経路に沿って動く間 */
	Traversal,
	LedgeHang,
};

/** 演出・アニメーションに通知するアクション */
UENUM(BlueprintType)
enum class EParkourAction : uint8
{
	None,
	Jump,
	Vault,
	Mantle,
	WallClimb,
	LedgeGrab,
	LedgeClimbUp,
	LedgeDrop,
	WallRun,
	WallJump,
	Slide,
	Roll,
	Stagger,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParkourAction, EParkourAction, Action);

/** 前方スキャンの結果 */
struct FParkourScan
{
	bool bHitWall = false;
	FVector WallPoint = FVector::ZeroVector;
	/** 水平化した壁の法線（壁から外向き） */
	FVector WallNormal = FVector::ZeroVector;
	TWeakObjectPtr<const UPrimitiveComponent> WallComponent;
	/** 進行方向と壁のなす角（90°=正対） */
	float AngleToWallDeg = 0.f;
	/** カプセル表面から壁までの水平距離 */
	float WallDistance = 0.f;
	/** 足元からの上面の高さ。登れないほど高い／上面が無いときは TNumericLimits<float>::Max() */
	float Height = 0.f;
	float Depth = 0.f;
	/** 壁面上の上端（縁） */
	FVector TopPoint = FVector::ZeroVector;
	bool bHasFarEdge = false;
	FVector FarEdge = FVector::ZeroVector;
	bool bHasLanding = false;
	FVector LandingPoint = FVector::ZeroVector;
	double Timestamp = -1.0;
};

UCLASS()
class PARQUELE_API UParkourMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UParkourMovementComponent();

	/** 調整値。未設定なら UParkourMovementSettings の既定値（仕様書の初期値）を使う */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour")
	TObjectPtr<UParkourMovementSettings> Settings;

	/** アクション開始の通知（アニメーション・音・振動・カメラ揺れ用） */
	UPROPERTY(BlueprintAssignable, Category = "Parkour")
	FOnParkourAction OnParkourAction;

	UPROPERTY(EditAnywhere, Category = "Parkour|Debug")
	bool bDrawDebug = false;

	const UParkourMovementSettings* GetSettings() const;

	/** Settings の値を CMC の各パラメータへ反映する。実行中に Settings を変えたら呼ぶ */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	void ApplySettings();

	// ---- 入力（キャラクターから呼ぶ） ----
	void SetStickInput(float RawMagnitude) { StickRaw = RawMagnitude; }
	void UpActionPressed();
	void DownActionPressed();
	void DownActionReleased();
	void ToggleAutoSprint() { bAutoSprint = !bAutoSprint; }
	/** リスタート・復帰時に状態を全て戻す */
	void ResetParkourState();

	// ---- 状態 ----
	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsWallRunning() const { return IsCustomMode(EParkourCustomMode::WallRun); }

	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsTraversing() const { return IsCustomMode(EParkourCustomMode::Traversal); }

	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsLedgeHanging() const { return IsCustomMode(EParkourCustomMode::LedgeHang); }

	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsSliding() const { return bSliding; }

	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsStaggered() const { return StaggerTimeLeft > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsAutoSprintEnabled() const { return bAutoSprint; }

	/** 走行速度で 0、最高速度で 1。FOV・ブラー・風切り音に使う */
	UFUNCTION(BlueprintPure, Category = "Parkour")
	float GetTopSpeedAlpha() const;

	FString GetParkourStateName() const;

	// ---- UCharacterMovementComponent ----
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	virtual void PhysicsRotation(float DeltaTime) override;

protected:
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
	struct FTraversalState
	{
		EParkourAction Action = EParkourAction::None;
		FVector Start = FVector::ZeroVector;
		FVector Control = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Duration = 0.f;
		float Elapsed = 0.f;
		FVector ExitVelocity = FVector::ZeroVector;
		FVector Facing = FVector::ForwardVector;
		bool bEndsInHang = false;
		float LedgeZ = 0.f;
		FVector WallNormal = FVector::ZeroVector;
		TWeakObjectPtr<const UPrimitiveComponent> WallComponent;
	};

	struct FWallRunState
	{
		FVector Normal = FVector::ZeroVector;
		float Time = 0.f;
		TWeakObjectPtr<const UPrimitiveComponent> Component;
	};

	struct FHangState
	{
		FVector Normal = FVector::ZeroVector;
		float LedgeZ = 0.f;
		TWeakObjectPtr<const UPrimitiveComponent> Component;
	};

	bool IsCustomMode(EParkourCustomMode Mode) const
	{
		return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(Mode);
	}

	float CapsuleRadius() const;
	float CapsuleHalfHeight() const;
	FVector GetIntentDirection2D() const;
	FCollisionQueryParams MakeQueryParams() const;
	bool HasRoomFor(const FVector& Center) const;

	// 判定
	void UpdateScan();
	FParkourScan ScanForObstacle() const;
	bool TryResolveUpAction();
	bool TryStartWallAction();
	bool TryJump();
	bool TryLedgeGrabAssist();

	// アクション開始
	bool StartVault();
	bool StartMantle();
	bool StartWallClimb();
	bool TryStartWallRun();
	bool TryClimbUpFromLedge();
	void DropFromLedge();
	void WallJump(const FVector& WallNormal, const TWeakObjectPtr<const UPrimitiveComponent>& WallComponent);
	void EndWallRun();
	void BeginTraversal(EParkourAction Action, const FVector& Start, const FVector& Control, const FVector& End,
		float Duration, const FVector& ExitVelocity, const FVector& Facing);
	void FinishTraversal();
	void EnterLedgeHang(float LedgeZ, const FVector& WallNormal, const TWeakObjectPtr<const UPrimitiveComponent>& WallComponent);
	bool LedgeContinuesAt(const FVector& CapsuleCenter) const;
	void StartSlide();
	void EndSlide();
	void UpdateSlide(float DeltaTime);
	void CloseActionBuffers();

	// 物理
	void PhysWallRun(float DeltaTime, int32 Iterations);
	void PhysTraversal(float DeltaTime, int32 Iterations);
	void PhysLedgeHang(float DeltaTime, int32 Iterations);

	void DrawScanDebug(const FParkourScan& InScan) const;

	// 入力
	float StickRaw = 0.f;
	bool bDownHeld = false;
	bool bAutoSprint = true;
	double LastDownPressTime = -1.0e9;

	// 速度
	float SprintAlpha = 0.f;

	// 時間窓
	ParkourRules::FTimedWindow CoyoteWindow;
	ParkourRules::FTimedWindow JumpBuffer;
	ParkourRules::FTimedWindow ParkourBuffer;
	bool bJumpingNow = false;

	// スキャン
	FParkourScan Scan;
	float ScanTimer = 0.f;

	// スライド
	bool bSliding = false;
	float SlideTimeLeft = 0.f;
	float SavedGroundFriction = 8.f;

	// 空中・着地
	float FallApexZ = 0.f;
	float StaggerTimeLeft = 0.f;
	float LedgeGrabCooldown = 0.f;

	// 壁（同じ壁での連続ウォールラン／ウォールジャンプ禁止。着地でリセット）
	TWeakObjectPtr<const UPrimitiveComponent> LastWallComponent;

	FTraversalState Trav;
	FWallRunState WallRunState;
	FHangState Hang;
};
