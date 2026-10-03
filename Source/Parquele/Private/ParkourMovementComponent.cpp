#include "ParkourMovementComponent.h"

#include "ParkourMovementSettings.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

namespace
{
	/** これ以上上を向いた面は床として扱う */
	constexpr float WalkableNormalZ = 0.7f;
	/** これ以下の傾きの面だけを壁として扱う */
	constexpr float WallMaxNormalZ = 0.35f;
	/** 奥行き計測の刻み */
	constexpr float DepthStep = 20.f;

	FVector QuadBezier(const FVector& A, const FVector& Control, const FVector& B, float T)
	{
		const float U = 1.f - T;
		return U * U * A + 2.f * U * T * Control + T * T * B;
	}

	/** 水平方向 From を To へ最大 MaxDeg だけ回す（進行方向補正） */
	FVector RotateTowards2D(const FVector& From, const FVector& To, float MaxDeg)
	{
		const FVector F = From.GetSafeNormal2D();
		const FVector T = To.GetSafeNormal2D();
		if (F.IsNearlyZero()) return T;
		if (T.IsNearlyZero()) return F;
		const float Delta = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(F.X * T.Y - F.Y * T.X, FVector::DotProduct(F, T))));
		return F.RotateAngleAxis(FMath::Clamp(Delta, -MaxDeg, MaxDeg), FVector::UpVector);
	}
}

UParkourMovementComponent::UParkourMovementComponent()
{
	NavAgentProps.bCanCrouch = true;
	bOrientRotationToMovement = true;
	bCanWalkOffLedgesWhenCrouching = true;
	ApplySettings();
}

const UParkourMovementSettings* UParkourMovementComponent::GetSettings() const
{
	return Settings ? Settings.Get() : GetDefault<UParkourMovementSettings>();
}

void UParkourMovementComponent::ApplySettings()
{
	const UParkourMovementSettings* S = GetSettings();
	MaxWalkSpeed = S->RunSpeed;
	MaxWalkSpeedCrouched = S->CrouchSpeed;
	MaxAcceleration = S->RunSpeed / FMath::Max(S->TimeToRunSpeed, KINDA_SMALL_NUMBER);
	BrakingDecelerationWalking = MaxAcceleration;
	AirControl = S->AirControl;
	RotationRate = FRotator(0.f, S->RotationRateYaw, 0.f);
	SetCrouchedHalfHeight(S->SlideHalfHeight);

	const UWorld* World = GetWorld();
	const float GravityAbs = FMath::Abs(World ? World->GetGravityZ() : -980.f);
	JumpZVelocity = ParkourRules::JumpVelocityForApex(S->JumpApexHeight, GravityAbs, S->GravityScaleRising);
}

void UParkourMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplySettings();
	SavedGroundFriction = GroundFriction;
}

float UParkourMovementComponent::CapsuleRadius() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float UParkourMovementComponent::CapsuleHalfHeight() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

FVector UParkourMovementComponent::GetIntentDirection2D() const
{
	const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	if (Vel2D.SizeSquared() > FMath::Square(50.f)) return Vel2D.GetSafeNormal();
	if (!Acceleration.IsNearlyZero()) return Acceleration.GetSafeNormal2D();
	return UpdatedComponent->GetForwardVector().GetSafeNormal2D();
}

FCollisionQueryParams UParkourMovementComponent::MakeQueryParams() const
{
	return FCollisionQueryParams(FName(TEXT("ParkourScan")), false, CharacterOwner);
}

bool UParkourMovementComponent::HasRoomFor(const FVector& Center) const
{
	FCollisionQueryParams Params = MakeQueryParams();
	FCollisionResponseParams ResponseParams;
	InitCollisionParams(Params, ResponseParams);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(CapsuleRadius() - 1.f, CapsuleHalfHeight() - 1.f);
	return !GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(),
		Shape, Params, ResponseParams);
}

float UParkourMovementComponent::GetTopSpeedAlpha() const
{
	const UParkourMovementSettings* S = GetSettings();
	const float Speed = static_cast<float>(Velocity.Size2D());
	return FMath::Clamp((Speed - S->RunSpeed) / FMath::Max(S->TopSpeed - S->RunSpeed, 1.f), 0.f, 1.f);
}

FString UParkourMovementComponent::GetParkourStateName() const
{
	if (IsWallRunning()) return TEXT("WallRun");
	if (IsLedgeHanging()) return TEXT("LedgeHang");
	if (IsTraversing()) return UEnum::GetValueAsString(Trav.Action);
	if (bSliding) return TEXT("Slide");
	if (IsStaggered()) return TEXT("Stagger");
	if (IsFalling()) return TEXT("Air");
	if (IsCrouching()) return TEXT("Crouch");
	return IsMovingOnGround() ? TEXT("Ground") : TEXT("Other");
}

// ============================================================================
// Tick
// ============================================================================

void UParkourMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (!HasValidData() || !CharacterOwner)
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
		return;
	}

	const UParkourMovementSettings* S = GetSettings();

	CoyoteWindow.Tick(DeltaTime);
	JumpBuffer.Tick(DeltaTime);
	ParkourBuffer.Tick(DeltaTime);
	StaggerTimeLeft = FMath::Max(0.f, StaggerTimeLeft - DeltaTime);
	LedgeGrabCooldown = FMath::Max(0.f, LedgeGrabCooldown - DeltaTime);

	// 自動加速：走り続けると最高速度へ。空中・パルクール中は維持する
	if (IsMovingOnGround())
	{
		const bool bRunInput = ParkourRules::EvaluateStick(StickRaw, S->StickDeadZone, S->RunStickThreshold).bRun;
		const bool bSustain = bAutoSprint && bRunInput && !bSliding && Velocity.Size2D() >= S->RunSpeed * 0.95f;
		SprintAlpha = ParkourRules::StepSprintAlpha(SprintAlpha, bSustain, DeltaTime, S->TopSpeedRampTime);
	}

	if (IsFalling())
	{
		FallApexZ = FMath::Max(FallApexZ, static_cast<float>(UpdatedComponent->GetComponentLocation().Z));
	}

	// 事前スキャン（入力時に即決定するため、毎フレームではなく一定間隔で）
	ScanTimer -= DeltaTime;
	if (ScanTimer <= 0.f)
	{
		ScanTimer = S->ScanInterval;
		UpdateScan();
	}

	const bool bFreeToAct = !IsStaggered() && !IsTraversing() && !IsLedgeHanging() && !IsWallRunning();

	// 先行入力：障害物の手前で押した上アクション
	if (bFreeToAct && ParkourBuffer.IsOpen() && TryStartWallAction())
	{
		ParkourBuffer.Close();
	}
	// 先行入力：着地直前に押したジャンプ
	if (bFreeToAct && JumpBuffer.IsOpen() && IsMovingOnGround())
	{
		TryJump();
	}
	// 縁吸着アシスト
	if (bFreeToAct && IsFalling())
	{
		TryLedgeGrabAssist();
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

float UParkourMovementComponent::GetMaxSpeed() const
{
	const UParkourMovementSettings* S = GetSettings();
	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
	{
		// スライド中は入力で加速させず、向きだけ変えられるようにする
		if (bSliding) return FMath::Max(static_cast<float>(Velocity.Size2D()), 1.f);
		if (IsCrouching()) return S->CrouchSpeed;
		const bool bRun = ParkourRules::EvaluateStick(StickRaw, S->StickDeadZone, S->RunStickThreshold).bRun;
		return ParkourRules::MaxGroundSpeed(bRun, bAutoSprint ? SprintAlpha : 0.f, 0.f, S->WalkSpeed, S->RunSpeed, S->TopSpeed);
	}
	case MOVE_Falling:
		return ParkourRules::MaxGroundSpeed(true, bAutoSprint ? SprintAlpha : 0.f, 0.f, S->WalkSpeed, S->RunSpeed, S->TopSpeed);
	default:
		return Super::GetMaxSpeed();
	}
}

float UParkourMovementComponent::GetMaxBrakingDeceleration() const
{
	// スライドの減速は UpdateSlide で自前で行う
	if (bSliding && MovementMode == MOVE_Walking) return 0.f;
	return Super::GetMaxBrakingDeceleration();
}

void UParkourMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// キビキビした放物線：上昇と下降で重力倍率を変える
	if (MovementMode == MOVE_Falling)
	{
		const UParkourMovementSettings* S = GetSettings();
		GravityScale = Velocity.Z > 0.f ? S->GravityScaleRising : S->GravityScaleFalling;
	}
	else
	{
		GravityScale = 1.f;
	}

	if (bSliding)
	{
		UpdateSlide(DeltaSeconds);
	}
}

void UParkourMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (!UpdatedComponent) return;

	if (MovementMode == MOVE_Falling)
	{
		FallApexZ = static_cast<float>(UpdatedComponent->GetComponentLocation().Z);
		// 縁から歩いて落ちたときだけコヨーテタイムを開く
		if (PreviousMovementMode == MOVE_Walking && !bJumpingNow)
		{
			CoyoteWindow.Open(GetSettings()->CoyoteTime);
		}
	}

	if (MovementMode != MOVE_Walking && bSliding)
	{
		EndSlide();
	}

	if (MovementMode == MOVE_Walking)
	{
		LastWallComponent.Reset();
		LedgeGrabCooldown = 0.f;
	}
}

void UParkourMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	const UParkourMovementSettings* S = GetSettings();
	const float FallHeight = FallApexZ - static_cast<float>(UpdatedComponent->GetComponentLocation().Z);
	const float SinceDown = static_cast<float>(GetWorld()->GetTimeSeconds() - LastDownPressTime);
	const ParkourRules::ELandingResult Result = ParkourRules::EvaluateLanding(FallHeight, SinceDown, S->GetLandingThresholds());

	Super::ProcessLanded(Hit, remainingTime, Iterations);

	switch (Result)
	{
	case ParkourRules::ELandingResult::Roll:
		// 速度を維持して転がる（速度は CMC が保持する。演出は Phase 2）
		OnParkourAction.Broadcast(EParkourAction::Roll);
		break;
	case ParkourRules::ELandingResult::Stagger:
		StaggerTimeLeft = S->StaggerTime;
		OnParkourAction.Broadcast(EParkourAction::Stagger);
		break;
	default:
		break;
	}
}

void UParkourMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (MovementMode != MOVE_Custom)
	{
		Super::PhysicsRotation(DeltaTime);
		return;
	}

	FVector Facing = FVector::ZeroVector;
	switch (static_cast<EParkourCustomMode>(CustomMovementMode))
	{
	case EParkourCustomMode::WallRun: Facing = Velocity.GetSafeNormal2D(); break;
	case EParkourCustomMode::Traversal: Facing = Trav.Facing; break;
	case EParkourCustomMode::LedgeHang: Facing = -Hang.Normal; break;
	default: break;
	}
	if (Facing.IsNearlyZero()) return;

	const FRotator Current = UpdatedComponent->GetComponentRotation();
	const FRotator Target(0.f, Facing.Rotation().Yaw, 0.f);
	const FRotator NewRotation = FMath::RInterpConstantTo(Current, Target, DeltaTime, 1080.f);
	MoveUpdatedComponent(FVector::ZeroVector, NewRotation.Quaternion(), false);
}

// ============================================================================
// 入力
// ============================================================================

void UParkourMovementComponent::UpActionPressed()
{
	if (!HasValidData() || !CharacterOwner) return;
	const UParkourMovementSettings* S = GetSettings();
	if (IsStaggered() || !TryResolveUpAction())
	{
		JumpBuffer.Open(S->JumpBufferTime);
		ParkourBuffer.Open(S->ParkourBufferTime);
	}
}

void UParkourMovementComponent::DownActionPressed()
{
	if (!HasValidData() || !CharacterOwner) return;
	bDownHeld = true;
	LastDownPressTime = GetWorld()->GetTimeSeconds();
	if (IsStaggered()) return;

	if (IsLedgeHanging())
	{
		DropFromLedge();
		return;
	}
	if (IsMovingOnGround())
	{
		if (Velocity.Size2D() >= GetSettings()->SlideMinStartSpeed)
		{
			StartSlide();
		}
		else
		{
			CharacterOwner->Crouch();
		}
	}
	// 空中では押した時刻だけ記録し、着地時に受け身判定に使う
}

void UParkourMovementComponent::DownActionReleased()
{
	bDownHeld = false;
	if (!bSliding && CharacterOwner)
	{
		CharacterOwner->UnCrouch();
	}
}

void UParkourMovementComponent::ResetParkourState()
{
	EndSlide();
	if (CharacterOwner) CharacterOwner->UnCrouch();
	CloseActionBuffers();
	SprintAlpha = 0.f;
	StaggerTimeLeft = 0.f;
	LedgeGrabCooldown = 0.f;
	LastWallComponent.Reset();
	Trav = FTraversalState();
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Falling);
}

void UParkourMovementComponent::CloseActionBuffers()
{
	CoyoteWindow.Close();
	JumpBuffer.Close();
	ParkourBuffer.Close();
}

// ============================================================================
// 判定
// ============================================================================

void UParkourMovementComponent::UpdateScan()
{
	Scan = ScanForObstacle();
	if (bDrawDebug) DrawScanDebug(Scan);
}

FParkourScan UParkourMovementComponent::ScanForObstacle() const
{
	FParkourScan Out;
	UWorld* World = GetWorld();
	Out.Timestamp = World->GetTimeSeconds();

	const UParkourMovementSettings* S = GetSettings();
	const ECollisionChannel Channel = S->TraceChannel;
	const FCollisionQueryParams Params = MakeQueryParams();
	const float Radius = CapsuleRadius();
	const float HalfHeight = CapsuleHalfHeight();
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	const float FeetZ = Loc.Z - HalfHeight;
	const FVector Dir = GetIntentDirection2D();
	if (Dir.IsNearlyZero()) return Out;

	// 1) 前方カプセルトレース。足元の段差（MaxStepHeight）より上だけを見る
	const float SweepRadius = Radius * 0.7f;
	const float SweepHalfHeight = FMath::Max(HalfHeight - MaxStepHeight * 0.5f, SweepRadius);
	const FVector SweepStart = Loc + FVector(0.f, 0.f, MaxStepHeight * 0.5f);
	const float Distance = Radius + S->ScanBaseDistance + static_cast<float>(Velocity.Size2D()) * S->ParkourBufferTime;
	FHitResult WallHit;
	if (!World->SweepSingleByChannel(WallHit, SweepStart, SweepStart + Dir * Distance, FQuat::Identity, Channel,
		FCollisionShape::MakeCapsule(SweepRadius, SweepHalfHeight), Params) || WallHit.bStartPenetrating)
	{
		return Out;
	}
	if (FMath::Abs(WallHit.ImpactNormal.Z) > WallMaxNormalZ) return Out;

	const FVector N = WallHit.ImpactNormal.GetSafeNormal2D();
	const FVector WallPoint = WallHit.ImpactPoint;
	Out.bHitWall = true;
	Out.WallPoint = WallPoint;
	Out.WallNormal = N;
	Out.WallComponent = WallHit.GetComponent();
	Out.AngleToWallDeg = ParkourRules::AngleToWallDeg(FVector::DotProduct(Dir, N));
	Out.WallDistance = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(Loc - WallPoint, N)) - Radius);
	Out.Height = TNumericLimits<float>::Max();

	FHitResult Tmp;
	const float ProbeTopZ = FeetZ + S->WallClimbMaxHeight + 30.f;

	// 2) 登攀上限より上にも壁が続くなら「高すぎる」（ウォールランは可能）
	const FVector HighFrom = FVector(WallPoint.X, WallPoint.Y, ProbeTopZ) + N * (Radius + 10.f);
	if (World->LineTraceSingleByChannel(Tmp, HighFrom, HighFrom - N * (Radius + 20.f), Channel, Params))
	{
		return Out;
	}

	// 3) 上から下向きトレースで上面の高さ
	const FVector Inner = WallPoint - N * 5.f;
	FHitResult TopHit;
	if (!World->LineTraceSingleByChannel(TopHit, FVector(Inner.X, Inner.Y, ProbeTopZ), FVector(Inner.X, Inner.Y, FeetZ - 5.f), Channel, Params)
		|| TopHit.bStartPenetrating || TopHit.ImpactNormal.Z < WalkableNormalZ)
	{
		return Out;
	}
	const float TopZ = static_cast<float>(TopHit.ImpactPoint.Z);
	Out.Height = TopZ - FeetZ;
	Out.TopPoint = FVector(WallPoint.X, WallPoint.Y, TopZ);

	// 4) 宙に浮いた障害物（スライドでくぐるバーなど）は対象外：上面の下にも壁が続いているか
	if (Out.Height > 0.f)
	{
		const float BaseZ = FeetZ + FMath::Min(Out.Height - 10.f, MaxStepHeight + 10.f);
		const FVector BaseFrom = FVector(WallPoint.X, WallPoint.Y, BaseZ) + N * (Radius + 10.f);
		if (!World->LineTraceSingleByChannel(Tmp, BaseFrom, BaseFrom - N * (Radius + 20.f), Channel, Params))
		{
			FParkourScan Empty;
			Empty.Timestamp = Out.Timestamp;
			return Empty;
		}
	}

	// 5) 奥行き：上面が途切れる位置を探す
	const float MaxProbe = S->VaultMaxDepth + DepthStep * 2.f;
	Out.Depth = MaxProbe;
	for (float D = DepthStep; D <= MaxProbe; D += DepthStep)
	{
		const FVector P = WallPoint - N * D;
		FHitResult DepthHit;
		const bool bTopContinues = World->LineTraceSingleByChannel(DepthHit, FVector(P.X, P.Y, TopZ + 20.f), FVector(P.X, P.Y, TopZ - 30.f), Channel, Params);
		if (!bTopContinues)
		{
			Out.Depth = D - DepthStep * 0.5f;
			const FVector Edge = WallPoint - N * Out.Depth;
			Out.bHasFarEdge = true;
			Out.FarEdge = FVector(Edge.X, Edge.Y, TopZ);
			break;
		}
	}

	// 6) ヴォルト後の着地点
	if (Out.bHasFarEdge)
	{
		const FVector L = Out.FarEdge - N * (Radius + 20.f);
		FHitResult LandHit;
		if (World->LineTraceSingleByChannel(LandHit, FVector(L.X, L.Y, TopZ + 20.f), FVector(L.X, L.Y, FeetZ - 600.f), Channel, Params))
		{
			Out.bHasLanding = true;
			Out.LandingPoint = LandHit.ImpactPoint;
		}
	}
	return Out;
}

bool UParkourMovementComponent::TryResolveUpAction()
{
	if (IsLedgeHanging()) return TryClimbUpFromLedge();
	if (IsWallRunning())
	{
		WallJump(WallRunState.Normal, WallRunState.Component);
		return true;
	}
	if (IsTraversing())
	{
		if (Trav.Action == EParkourAction::WallClimb)
		{
			WallJump(Trav.WallNormal, Trav.WallComponent);
			return true;
		}
		return false;
	}

	if (GetWorld()->GetTimeSeconds() - Scan.Timestamp > GetSettings()->ScanInterval)
	{
		UpdateScan();
	}
	if (TryStartWallAction()) return true;
	return TryJump();
}

bool UParkourMovementComponent::TryStartWallAction()
{
	if (!Scan.bHitWall) return false;
	const UParkourMovementSettings* S = GetSettings();
	using ParkourRules::EWallAction;
	switch (ParkourRules::ChooseWallAction(Scan.Height, Scan.Depth, Scan.AngleToWallDeg, Velocity.Size2D(),
		S->GetObstacleThresholds(), S->GetWallRunThresholds()))
	{
	case EWallAction::Vault: return StartVault();
	case EWallAction::Mantle: return StartMantle();
	case EWallAction::WallClimb: return StartWallClimb();
	case EWallAction::WallRun: return TryStartWallRun();
	default: return false;
	}
}

bool UParkourMovementComponent::TryJump()
{
	if (!(IsMovingOnGround() || CoyoteWindow.IsOpen())) return false;
	if (bSliding) EndSlide();
	if (CharacterOwner->bIsCrouched) CharacterOwner->UnCrouch();

	bJumpingNow = true;
	Velocity.Z = JumpZVelocity;
	SetMovementMode(MOVE_Falling);
	bJumpingNow = false;

	CloseActionBuffers();
	// 跳んだ先で障害物に届いたら、そのまま上アクションとして発動させる
	ParkourBuffer.Open(GetSettings()->ParkourBufferTime);
	OnParkourAction.Broadcast(EParkourAction::Jump);
	return true;
}

bool UParkourMovementComponent::TryLedgeGrabAssist()
{
	const UParkourMovementSettings* S = GetSettings();
	if (LedgeGrabCooldown > 0.f || !Scan.bHitWall || Scan.Height <= 0.f || Scan.Height == TNumericLimits<float>::Max()) return false;
	if (Scan.AngleToWallDeg < 45.f || Velocity.Z > 150.f || Scan.WallDistance > S->LedgeMagnetDistance) return false;
	// 壁へ向かう入力があるときだけ
	if (FVector::DotProduct(Acceleration.GetSafeNormal2D(), -Scan.WallNormal) < 0.3f) return false;

	const FVector Loc = UpdatedComponent->GetComponentLocation();
	const float TopZ = static_cast<float>(Scan.TopPoint.Z);
	// 手が届く範囲：カプセル中心〜頭上40cm
	if (TopZ < Loc.Z || TopZ > Loc.Z + CapsuleHalfHeight() + 40.f) return false;

	FVector HangPos = Scan.TopPoint + Scan.WallNormal * (CapsuleRadius() + 2.f);
	HangPos.Z = TopZ - S->HangCenterBelowLedge;
	if (!HasRoomFor(HangPos)) return false;

	BeginTraversal(EParkourAction::LedgeGrab, Loc, (Loc + HangPos) * 0.5f, HangPos, 0.1f, FVector::ZeroVector, -Scan.WallNormal);
	Trav.bEndsInHang = true;
	Trav.LedgeZ = TopZ;
	Trav.WallNormal = Scan.WallNormal;
	Trav.WallComponent = Scan.WallComponent;
	return true;
}

// ============================================================================
// アクション開始
// ============================================================================

void UParkourMovementComponent::BeginTraversal(EParkourAction Action, const FVector& Start, const FVector& Control, const FVector& End,
	float Duration, const FVector& ExitVelocity, const FVector& Facing)
{
	if (bSliding) EndSlide();
	Trav = FTraversalState();
	Trav.Action = Action;
	Trav.Start = Start;
	Trav.Control = Control;
	Trav.End = End;
	Trav.Duration = FMath::Max(Duration, 0.05f);
	Trav.ExitVelocity = ExitVelocity;
	Trav.Facing = Facing.GetSafeNormal2D();
	CloseActionBuffers();
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EParkourCustomMode::Traversal));
	OnParkourAction.Broadcast(Action);
}

bool UParkourMovementComponent::StartVault()
{
	const UParkourMovementSettings* S = GetSettings();
	const float Radius = CapsuleRadius();
	const float HalfHeight = CapsuleHalfHeight();
	const FVector N = Scan.WallNormal;
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const float TopZ = static_cast<float>(Scan.TopPoint.Z);

	// 奥の縁の先へ抜ける。着地点の方が高ければその上に出る
	const FVector Far = Scan.bHasFarEdge ? Scan.FarEdge : Scan.TopPoint - N * Scan.Depth;
	FVector End = Far - N * (Radius + 20.f);
	End.Z = Start.Z;
	if (Scan.bHasLanding) End.Z = FMath::Max(End.Z, Scan.LandingPoint.Z + HalfHeight + 2.f);
	if (!HasRoomFor(End)) return false;

	// 経路の中間点で上面をカプセルが越えるように制御点を置く
	FVector Mid = Scan.TopPoint - N * (Scan.Depth * 0.5f);
	Mid.Z = TopZ + HalfHeight + 10.f;
	const FVector Control = 2.f * Mid - 0.5f * (Start + End);

	// 速度を落とさず飛び越える
	const float InSpeed = static_cast<float>(Velocity.Size2D());
	const float PathSpeed = FMath::Max(InSpeed, S->WalkSpeed);
	const float Duration = FMath::Max(static_cast<float>(FVector::Dist2D(Start, End)) / PathSpeed, S->VaultMinDuration);
	const FVector ExitDir = RotateTowards2D(GetIntentDirection2D(), -N, S->HeadingAssistMaxDeg);
	const float Bonus = InSpeed >= S->RunSpeed * 0.9f ? 1.f + S->VaultSpeedBonus : 1.f;

	BeginTraversal(EParkourAction::Vault, Start, Control, End, Duration, ExitDir * InSpeed * Bonus, -N);
	return true;
}

bool UParkourMovementComponent::StartMantle()
{
	const UParkourMovementSettings* S = GetSettings();
	const float Radius = CapsuleRadius();
	const FVector N = Scan.WallNormal;
	const FVector Start = UpdatedComponent->GetComponentLocation();

	FVector End = Scan.TopPoint - N * (Radius + 15.f);
	End.Z = Scan.TopPoint.Z + CapsuleHalfHeight() + 2.f;
	if (!HasRoomFor(End)) return false;

	// 壁の手前で縁の高さまで上がり、そこから前へ
	FVector Control = Scan.TopPoint + N * Radius;
	Control.Z = End.Z + 20.f;

	const float Speed = static_cast<float>(Velocity.Size2D());
	const float Approach = Scan.WallDistance / FMath::Max(Speed, S->RunSpeed);
	const float Duration = (Speed >= S->MantleFastSpeed ? S->MantleFastDuration : S->MantleDuration) + Approach;
	const FVector ExitDir = RotateTowards2D(GetIntentDirection2D(), -N, S->HeadingAssistMaxDeg);

	BeginTraversal(EParkourAction::Mantle, Start, Control, End, Duration, ExitDir * Speed, -N);
	return true;
}

bool UParkourMovementComponent::StartWallClimb()
{
	const UParkourMovementSettings* S = GetSettings();
	const FVector N = Scan.WallNormal;
	const FVector Start = UpdatedComponent->GetComponentLocation();

	FVector HangPos = Scan.TopPoint + N * (CapsuleRadius() + 2.f);
	HangPos.Z = Scan.TopPoint.Z - S->HangCenterBelowLedge;
	if (!HasRoomFor(HangPos)) return false;

	// 壁際まで走り込んでから駆け上がる
	FVector Control = HangPos;
	Control.Z = Start.Z;
	const float Approach = Scan.WallDistance / FMath::Max(static_cast<float>(Velocity.Size2D()), S->RunSpeed);

	BeginTraversal(EParkourAction::WallClimb, Start, Control, HangPos, S->WallClimbDuration + Approach, FVector::ZeroVector, -N);
	Trav.bEndsInHang = true;
	Trav.LedgeZ = static_cast<float>(Scan.TopPoint.Z);
	Trav.WallNormal = N;
	Trav.WallComponent = Scan.WallComponent;
	return true;
}

bool UParkourMovementComponent::TryStartWallRun()
{
	const UParkourMovementSettings* S = GetSettings();
	if (LastWallComponent.IsValid() && Scan.WallComponent == LastWallComponent) return false;
	// 遠いうちは発動しない（先行入力で近づいた時点に発動する）
	if (Scan.WallDistance > S->WallRunAttachDistance) return false;

	const FVector N = Scan.WallNormal;
	const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
	const float Speed = static_cast<float>(Horizontal.Size());
	const FVector Along = (Horizontal - N * FVector::DotProduct(Horizontal, N)).GetSafeNormal();
	if (Along.IsNearlyZero()) return false;

	if (bSliding) EndSlide();
	// 進入速度を落とさず、向きだけ壁沿いに変える
	Velocity = Along * Speed;
	Velocity.Z = FMath::Max(static_cast<float>(Velocity.Z), S->WallRunEntryUpSpeed);
	WallRunState = FWallRunState();
	WallRunState.Normal = N;
	WallRunState.Component = Scan.WallComponent;
	CloseActionBuffers();
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EParkourCustomMode::WallRun));
	OnParkourAction.Broadcast(EParkourAction::WallRun);
	return true;
}

void UParkourMovementComponent::EndWallRun()
{
	LastWallComponent = WallRunState.Component;
	Velocity += WallRunState.Normal * GetSettings()->WallRunExitPush;
	SetMovementMode(MOVE_Falling);
}

void UParkourMovementComponent::WallJump(const FVector& WallNormal, const TWeakObjectPtr<const UPrimitiveComponent>& WallComponent)
{
	const UParkourMovementSettings* S = GetSettings();
	FVector Along(Velocity.X, Velocity.Y, 0.f);
	Along -= WallNormal * FVector::DotProduct(Along, WallNormal);
	const float Rad = FMath::DegreesToRadians(S->WallJumpAngle);
	// 壁沿いの勢いは残し、法線方向から上へ45°に蹴り出す
	Velocity = Along + (WallNormal * FMath::Cos(Rad) + FVector::UpVector * FMath::Sin(Rad)) * S->WallJumpSpeed;
	LastWallComponent = WallComponent;
	CloseActionBuffers();
	SetMovementMode(MOVE_Falling);
	OnParkourAction.Broadcast(EParkourAction::WallJump);
}

void UParkourMovementComponent::FinishTraversal()
{
	if (Trav.bEndsInHang)
	{
		EnterLedgeHang(Trav.LedgeZ, Trav.WallNormal, Trav.WallComponent);
		return;
	}
	Velocity = Trav.ExitVelocity;
	SetMovementMode(MOVE_Falling);
}

void UParkourMovementComponent::EnterLedgeHang(float LedgeZ, const FVector& WallNormal, const TWeakObjectPtr<const UPrimitiveComponent>& WallComponent)
{
	Hang = FHangState();
	Hang.Normal = WallNormal;
	Hang.LedgeZ = LedgeZ;
	Hang.Component = WallComponent;
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EParkourCustomMode::LedgeHang));
	OnParkourAction.Broadcast(EParkourAction::LedgeGrab);
}

bool UParkourMovementComponent::TryClimbUpFromLedge()
{
	const UParkourMovementSettings* S = GetSettings();
	const float Radius = CapsuleRadius();
	const FVector N = Hang.Normal;
	const FVector Start = UpdatedComponent->GetComponentLocation();

	FVector End = Start - N * (2.f * Radius + 17.f);
	End.Z = Hang.LedgeZ + CapsuleHalfHeight() + 2.f;
	if (!HasRoomFor(End)) return false;

	const FVector Control(Start.X, Start.Y, End.Z + 10.f);
	BeginTraversal(EParkourAction::LedgeClimbUp, Start, Control, End, S->MantleDuration, FVector::ZeroVector, -N);
	return true;
}

void UParkourMovementComponent::DropFromLedge()
{
	Velocity = Hang.Normal * 100.f;
	LedgeGrabCooldown = GetSettings()->LedgeRegrabCooldown;
	SetMovementMode(MOVE_Falling);
	OnParkourAction.Broadcast(EParkourAction::LedgeDrop);
}

bool UParkourMovementComponent::LedgeContinuesAt(const FVector& CapsuleCenter) const
{
	const UParkourMovementSettings* S = GetSettings();
	const FCollisionQueryParams Params = MakeQueryParams();
	const float Radius = CapsuleRadius();
	const FVector N = Hang.Normal;

	// 縁の上面がまだあるか
	const FVector Probe = CapsuleCenter - N * (Radius + 5.f);
	FHitResult TopHit;
	if (!GetWorld()->LineTraceSingleByChannel(TopHit, FVector(Probe.X, Probe.Y, Hang.LedgeZ + 30.f), FVector(Probe.X, Probe.Y, Hang.LedgeZ - 30.f), S->TraceChannel, Params)
		|| FMath::Abs(TopHit.ImpactPoint.Z - Hang.LedgeZ) > 15.f)
	{
		return false;
	}
	// 壁がまだあるか
	FHitResult WallHit;
	const FVector From = CapsuleCenter + FVector(0.f, 0.f, CapsuleHalfHeight() * 0.5f);
	return GetWorld()->LineTraceSingleByChannel(WallHit, From, From - N * (Radius + 20.f), S->TraceChannel, Params);
}

// ============================================================================
// スライド
// ============================================================================

void UParkourMovementComponent::StartSlide()
{
	const UParkourMovementSettings* S = GetSettings();
	bSliding = true;
	SlideTimeLeft = S->SlideDuration;
	SavedGroundFriction = GroundFriction;
	GroundFriction = 0.f;
	Velocity += Velocity.GetSafeNormal2D() * S->SlideBoost;
	Velocity = Velocity.GetClampedToMaxSize(S->SlideMaxSpeed);
	CharacterOwner->Crouch();
	OnParkourAction.Broadcast(EParkourAction::Slide);
}

void UParkourMovementComponent::EndSlide()
{
	if (!bSliding) return;
	bSliding = false;
	GroundFriction = SavedGroundFriction;
	// 頭上が塞がっていれば CMC がしゃがみを維持し、空いた時点で立つ
	if (!bDownHeld && CharacterOwner) CharacterOwner->UnCrouch();
}

void UParkourMovementComponent::UpdateSlide(float DeltaTime)
{
	if (!IsMovingOnGround())
	{
		EndSlide();
		return;
	}
	const UParkourMovementSettings* S = GetSettings();

	// 斜面に沿った重力成分（長さ = sin(傾斜角)）
	const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal;
	const FVector Down(0.f, 0.f, -1.f);
	const FVector Downhill = Down - FloorNormal * FVector::DotProduct(Down, FloorNormal);
	const float GravityAbs = FMath::Abs(GetWorld()->GetGravityZ());
	const bool bDownhill = Downhill.Size() > 0.087f && FVector::DotProduct(Velocity, Downhill) > 0.f; // 約5°以上の下り

	Velocity += Downhill * GravityAbs * S->SlideSlopeGravityScale * DeltaTime;
	if (!bDownhill)
	{
		// 下り坂では時間切れにせず加速を続ける
		SlideTimeLeft -= DeltaTime;
		const float Speed = static_cast<float>(Velocity.Size());
		Velocity = Velocity.GetSafeNormal() * FMath::Max(Speed - S->SlideFriction * DeltaTime, 0.f);
	}
	Velocity = Velocity.GetClampedToMaxSize(S->SlideMaxSpeed);

	if (SlideTimeLeft <= 0.f || Velocity.Size2D() < S->WalkSpeed)
	{
		EndSlide();
	}
}

// ============================================================================
// 物理
// ============================================================================

void UParkourMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	Super::PhysCustom(DeltaTime, Iterations);
	if (DeltaTime < MIN_TICK_TIME || !CharacterOwner) return;

	switch (static_cast<EParkourCustomMode>(CustomMovementMode))
	{
	case EParkourCustomMode::WallRun: PhysWallRun(DeltaTime, Iterations); break;
	case EParkourCustomMode::Traversal: PhysTraversal(DeltaTime, Iterations); break;
	case EParkourCustomMode::LedgeHang: PhysLedgeHang(DeltaTime, Iterations); break;
	default: SetMovementMode(MOVE_Falling); break;
	}
}

void UParkourMovementComponent::PhysWallRun(float DeltaTime, int32 Iterations)
{
	const UParkourMovementSettings* S = GetSettings();
	const FCollisionQueryParams Params = MakeQueryParams();
	const float Radius = CapsuleRadius();
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	WallRunState.Time += DeltaTime;

	FHitResult WallHit;
	const bool bOnWall = GetWorld()->LineTraceSingleByChannel(WallHit, Loc, Loc - WallRunState.Normal * (Radius + 40.f), S->TraceChannel, Params)
		&& FMath::Abs(WallHit.ImpactNormal.Z) <= WallMaxNormalZ;
	if (!bOnWall || WallRunState.Time >= S->WallRunMaxTime || Velocity.Size2D() < S->WallRunMinSpeed * 0.5f)
	{
		EndWallRun();
		return;
	}

	// 曲面の壁にも沿うよう法線を更新し、水平速度を壁沿いに保つ
	const FVector N = WallHit.ImpactNormal.GetSafeNormal2D();
	WallRunState.Normal = N;
	FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
	const float Speed = static_cast<float>(Horizontal.Size());
	Horizontal = (Horizontal - N * FVector::DotProduct(Horizontal, N)).GetSafeNormal() * Speed;
	Velocity = FVector(Horizontal.X, Horizontal.Y, Velocity.Z + GetWorld()->GetGravityZ() * S->WallRunGravityScale * DeltaTime);

	// 壁に吸い付ける
	const float Gap = WallHit.Distance - Radius;
	const FVector Delta = Velocity * DeltaTime - N * FMath::Max(Gap - 1.f, 0.f);
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		if (Hit.ImpactNormal.Z >= WalkableNormalZ)
		{
			// 床に触れた：落下モードに戻して通常の着地処理に任せる
			EndWallRun();
			return;
		}
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}
}

void UParkourMovementComponent::PhysTraversal(float DeltaTime, int32 Iterations)
{
	Trav.Elapsed += DeltaTime;
	const float Alpha = FMath::Clamp(Trav.Elapsed / Trav.Duration, 0.f, 1.f);
	const FVector Target = QuadBezier(Trav.Start, Trav.Control, Trav.End, Alpha);
	const FVector Delta = Target - UpdatedComponent->GetComponentLocation();
	Velocity = Delta / DeltaTime;

	// 経路は開始前に終点の空きを確認済み。角に引っかからないよう掃引しない
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), false, Hit);

	if (Alpha >= 1.f)
	{
		FinishTraversal();
	}
}

void UParkourMovementComponent::PhysLedgeHang(float DeltaTime, int32 Iterations)
{
	const UParkourMovementSettings* S = GetSettings();
	const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Hang.Normal).GetSafeNormal();
	float Input = 0.f;
	if (!Acceleration.IsNearlyZero())
	{
		Input = FVector::DotProduct(Acceleration.GetSafeNormal2D(), Tangent);
		if (FMath::Abs(Input) < 0.3f) Input = 0.f;
	}
	Velocity = Tangent * Input * S->LedgeShimmySpeed;
	if (Velocity.IsNearlyZero()) return;

	const FVector Delta = Velocity * DeltaTime;
	if (!LedgeContinuesAt(UpdatedComponent->GetComponentLocation() + Delta))
	{
		Velocity = FVector::ZeroVector;
		return;
	}
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		Velocity = FVector::ZeroVector;
	}
}

void UParkourMovementComponent::DrawScanDebug(const FParkourScan& InScan) const
{
#if ENABLE_DRAW_DEBUG
	if (!InScan.bHitWall) return;
	const UWorld* World = GetWorld();
	const float Life = GetSettings()->ScanInterval;
	DrawDebugSphere(World, InScan.WallPoint, 6.f, 8, FColor::Yellow, false, Life);
	DrawDebugLine(World, InScan.WallPoint, InScan.WallPoint + InScan.WallNormal * 50.f, FColor::Yellow, false, Life);
	if (InScan.Height != TNumericLimits<float>::Max())
	{
		DrawDebugSphere(World, InScan.TopPoint, 6.f, 8, FColor::Green, false, Life);
		DrawDebugString(World, InScan.TopPoint + FVector(0.f, 0.f, 20.f),
			FString::Printf(TEXT("H%.0f D%.0f A%.0f"), InScan.Height, InScan.Depth, InScan.AngleToWallDeg), nullptr, FColor::White, Life);
	}
	if (InScan.bHasLanding)
	{
		DrawDebugSphere(World, InScan.LandingPoint, 6.f, 8, FColor::Cyan, false, Life);
	}
#endif
}
