// パルクールの調整値。仕様「数値をコードに直書きしない」に従い、全てここに置く。
// Data Asset を作らなくても、このクラスの既定値（＝仕様書の初期値）が使われる。
// 「仮値」と書いたものは仕様書に数値がなく、実装のために置いた値。実機で調整すること。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "ParkourRules.h"
#include "ParkourMovementSettings.generated.h"

UCLASS(BlueprintType)
class PARQUELE_API UParkourMovementSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	// ---- 基本移動 ----
	/** 歩行速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (Units = "cm/s"))
	float WalkSpeed = 350.f;

	/** 走行速度（スティック倒し量が RunStickThreshold 以上） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (Units = "cm/s"))
	float RunSpeed = 650.f;

	/** 最高速度（自動加速の上限） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (Units = "cm/s"))
	float TopSpeed = 900.f;

	/** 0→走行速度までの時間。加速度 = RunSpeed / この値 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (Units = "s"))
	float TimeToRunSpeed = 0.25f;

	/** 仮値：走り続けて走行速度→最高速度に達するまでの時間 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (Units = "s"))
	float TopSpeedRampTime = 2.0f;

	/** 仮値：しゃがみ歩きの速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (Units = "cm/s"))
	float CrouchSpeed = 200.f;

	/** 仮値：向きの追従速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (Units = "deg/s"))
	float RotationRateYaw = 900.f;

	/** 入力デッドゾーン */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground")
	float StickDeadZone = 0.15f;

	/** 走りに移行する倒し量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground")
	float RunStickThreshold = 0.6f;

	// ---- 空中 ----
	/** ジャンプの最高到達点 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (Units = "cm"))
	float JumpApexHeight = 120.f;

	/** 上昇中の重力倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air")
	float GravityScaleRising = 1.6f;

	/** 下降中の重力倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air")
	float GravityScaleFalling = 2.2f;

	/** 空中制御 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air")
	float AirControl = 0.35f;

	// ---- ヴォルト ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vault", meta = (Units = "cm"))
	float VaultMinHeight = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vault", meta = (Units = "cm"))
	float VaultMaxHeight = 120.f;

	/** この奥行き未満なら飛び越え、以上なら上に乗る（マントル） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vault", meta = (Units = "cm"))
	float VaultMaxDepth = 100.f;

	/** 走行中のヴォルトで得る速度ボーナス（0.05 = +5%） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vault")
	float VaultSpeedBonus = 0.05f;

	/** 仮値：ヴォルトの最短時間（低速時に一瞬で終わらないように） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vault", meta = (Units = "s"))
	float VaultMinDuration = 0.2f;

	// ---- マントル ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle", meta = (Units = "cm"))
	float MantleMaxHeight = 220.f;

	/** 仮値：通常マントルの時間 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle", meta = (Units = "s"))
	float MantleDuration = 0.45f;

	/** 仮値：高速時（片手の速いモーション）のマントル時間 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle", meta = (Units = "s"))
	float MantleFastDuration = 0.28f;

	/** 仮値：この速度以上なら高速マントル */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle", meta = (Units = "cm/s"))
	float MantleFastSpeed = 600.f;

	// ---- ウォールクライム ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallClimb", meta = (Units = "cm"))
	float WallClimbMaxHeight = 320.f;

	/** 仮値：壁を駆け上がって縁をつかむまでの時間 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallClimb", meta = (Units = "s"))
	float WallClimbDuration = 0.55f;

	// ---- ウォールラン ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallRun", meta = (Units = "deg"))
	float WallRunMinAngle = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallRun", meta = (Units = "deg"))
	float WallRunMaxAngle = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallRun", meta = (Units = "cm/s"))
	float WallRunMinSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallRun", meta = (Units = "s"))
	float WallRunMaxTime = 1.2f;

	/** 壁走り中の重力（通常比） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallRun")
	float WallRunGravityScale = 0.25f;

	/** 仮値：壁走り開始時の最低上昇速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallRun", meta = (Units = "cm/s"))
	float WallRunEntryUpSpeed = 100.f;

	/** 仮値：壁までこの距離以内に近づいたら壁走りに入る */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallRun", meta = (Units = "cm"))
	float WallRunAttachDistance = 30.f;

	/** 仮値：壁走り終了時に壁から離す速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallRun", meta = (Units = "cm/s"))
	float WallRunExitPush = 50.f;

	// ---- ウォールジャンプ ----
	/** 壁の法線から上向きに傾ける角度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallJump", meta = (Units = "deg"))
	float WallJumpAngle = 45.f;

	/** 仮値：壁を蹴る速さ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WallJump", meta = (Units = "cm/s"))
	float WallJumpSpeed = 750.f;

	// ---- スライド ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (Units = "s"))
	float SlideDuration = 0.8f;

	/** 開始時の加速 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (Units = "cm/s"))
	float SlideBoost = 150.f;

	/** 仮値：スライドに入れる最低速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (Units = "cm/s"))
	float SlideMinStartSpeed = 550.f;

	/** 仮値：平地での減速 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (Units = "cm/s"))
	float SlideFriction = 300.f;

	/** 仮値：下り坂で重力をどれだけ速度に変えるか */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide")
	float SlideSlopeGravityScale = 1.0f;

	/** 仮値：スライドの速度上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (Units = "cm/s"))
	float SlideMaxSpeed = 1100.f;

	/** 仮値：スライド・しゃがみ時のカプセル半高（高さ110cmのバーをくぐれる） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (Units = "cm"))
	float SlideHalfHeight = 50.f;

	// ---- 着地 ----
	/** この高さ以上から落ちたら受け身が必要 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (Units = "cm"))
	float RollMinFallHeight = 400.f;

	/** 着地のこの秒数前〜着地時に下アクションで受け身 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (Units = "s"))
	float RollInputWindow = 0.25f;

	/** 受け身失敗時の硬直 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (Units = "s"))
	float StaggerTime = 0.4f;

	// ---- レッジ ----
	/** 仮値：ぶら下がり中の左右移動速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ledge", meta = (Units = "cm/s"))
	float LedgeShimmySpeed = 150.f;

	/** 仮値：ぶら下がり時、縁からカプセル中心までの下方向距離 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ledge", meta = (Units = "cm"))
	float HangCenterBelowLedge = 90.f;

	/** 縁吸着アシスト：壁からこの水平距離以内なら縁をつかむ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ledge", meta = (Units = "cm"))
	float LedgeMagnetDistance = 40.f;

	/** 仮値：縁を離した直後に再びつかまない時間 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ledge", meta = (Units = "s"))
	float LedgeRegrabCooldown = 0.3f;

	// ---- 入力の手触り ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (Units = "s"))
	float CoyoteTime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (Units = "s"))
	float JumpBufferTime = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (Units = "s"))
	float ParkourBufferTime = 0.20f;

	/** 進行方向補正の上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (Units = "deg"))
	float HeadingAssistMaxDeg = 10.f;

	// ---- 判定 ----
	/** 移動中の事前スキャン間隔 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan", meta = (Units = "s"))
	float ScanInterval = 0.05f;

	/** 仮値：カプセル表面から前方を見る基本距離。実際は 速度×ParkourBufferTime を足す */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan", meta = (Units = "cm"))
	float ScanBaseDistance = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	// ---- カメラ ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (Units = "deg"))
	float RunFOV = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (Units = "deg"))
	float TopSpeedFOV = 105.f;

	ParkourRules::FObstacleThresholds GetObstacleThresholds() const
	{
		return { VaultMinHeight, VaultMaxHeight, VaultMaxDepth, MantleMaxHeight, WallClimbMaxHeight };
	}

	ParkourRules::FWallRunThresholds GetWallRunThresholds() const
	{
		return { WallRunMinAngle, WallRunMaxAngle, WallRunMinSpeed, HeadingAssistMaxDeg };
	}

	ParkourRules::FLandingThresholds GetLandingThresholds() const
	{
		return { RollMinFallHeight, RollInputWindow };
	}
};
