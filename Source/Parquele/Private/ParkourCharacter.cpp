#include "ParkourCharacter.h"

#include "ParkourMovementComponent.h"
#include "ParkourMovementSettings.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "UObject/ConstructorHelpers.h"

AParkourCharacter::AParkourCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UParkourMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 50.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 15.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(90.f);

	// 仮の見た目：カプセルと同寸の円柱＋向きが分かる鼻
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetCapsuleComponent());
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CylinderMesh.Succeeded()) PlaceholderBody->SetStaticMesh(CylinderMesh.Object);

	PlaceholderNose = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderNose"));
	PlaceholderNose->SetupAttachment(GetCapsuleComponent());
	PlaceholderNose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderNose->SetRelativeScale3D(FVector(0.3f, 0.2f, 0.15f));
	if (CubeMesh.Succeeded()) PlaceholderNose->SetStaticMesh(CubeMesh.Object);

	UpdatePlaceholderForHalfHeight(88.f);
}

UParkourMovementComponent* AParkourCharacter::GetParkourMovement() const
{
	return CastChecked<UParkourMovementComponent>(GetCharacterMovement());
}

void AParkourCharacter::BeginPlay()
{
	Super::BeginPlay();
	CourseStartTransform = GetActorTransform();
	RespawnTransform = CourseStartTransform;
	RunStartTime = GetWorld()->GetTimeSeconds();

	if (GetMesh() && GetMesh()->GetSkeletalMeshAsset())
	{
		PlaceholderBody->SetVisibility(false);
		PlaceholderNose->SetVisibility(false);
	}
}

void AParkourCharacter::UpdatePlaceholderForHalfHeight(float HalfHeight)
{
	const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	// BasicShapes の Cylinder は直径100・高さ100、中心が原点
	PlaceholderBody->SetRelativeScale3D(FVector(Radius * 2.f / 100.f, Radius * 2.f / 100.f, HalfHeight * 2.f / 100.f));
	PlaceholderNose->SetRelativeLocation(FVector(Radius + 5.f, 0.f, HalfHeight * 0.6f));
}

void AParkourCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	UpdatePlaceholderForHalfHeight(GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
}

void AParkourCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	UpdatePlaceholderForHalfHeight(GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
}

void AParkourCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UParkourMovementComponent* PM = GetParkourMovement();
	const UParkourMovementSettings* S = PM->GetSettings();

	if (GetActorLocation().Z < RespawnKillZ)
	{
		Respawn(false);
	}

	// 速度に連動した FOV（走行 90° → 最高速 105°）
	const float TargetFOV = FMath::Lerp(S->RunFOV, S->TopSpeedFOV, PM->GetTopSpeedAlpha());
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaSeconds, 6.f));

	if (bShowDebugHud && GEngine)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		FString Text = FString::Printf(TEXT("Speed %4.0f cm/s  State %s  AutoSprint %s  Time %.2f s"),
			GetVelocity().Size2D(), *PM->GetParkourStateName(), PM->IsAutoSprintEnabled() ? TEXT("ON") : TEXT("OFF"), Now - RunStartTime);
		if (LastGoalTime >= 0.0)
		{
			Text += FString::Printf(TEXT("  GOAL %.2f s"), LastGoalTime);
		}
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::White, Text);
	}
}

void AParkourCharacter::ReachCheckpoint(const FTransform& RespawnAt, bool bIsGoal)
{
	RespawnTransform = RespawnAt;
	if (bIsGoal && LastGoalTime < 0.0)
	{
		LastGoalTime = GetWorld()->GetTimeSeconds() - RunStartTime;
	}
}

void AParkourCharacter::Respawn(bool bToCourseStart)
{
	if (bToCourseStart)
	{
		RespawnTransform = CourseStartTransform;
		RunStartTime = GetWorld()->GetTimeSeconds();
		LastGoalTime = -1.0;
	}
	SetActorLocationAndRotation(RespawnTransform.GetLocation(), RespawnTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	GetParkourMovement()->ResetParkourState();
	if (Controller)
	{
		Controller->SetControlRotation(RespawnTransform.Rotator());
	}
}

// ============================================================================
// 入力
// ============================================================================

void AParkourCharacter::BuildDefaultInput()
{
	if (DefaultMappingContext) return;

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookStickAction = MakeAction(TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
	LookMouseAction = MakeAction(TEXT("IA_LookMouse"), EInputActionValueType::Axis2D);
	UpAction = MakeAction(TEXT("IA_Up"), EInputActionValueType::Boolean);
	DownAction = MakeAction(TEXT("IA_Down"), EInputActionValueType::Boolean);
	SprintToggleAction = MakeAction(TEXT("IA_SprintToggle"), EInputActionValueType::Boolean);
	RestartAction = MakeAction(TEXT("IA_Restart"), EInputActionValueType::Boolean);

	UInputMappingContext* Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Parkour"));

	auto Swizzle = [Context]()
	{
		UInputModifierSwizzleAxis* Modifier = NewObject<UInputModifierSwizzleAxis>(Context);
		Modifier->Order = EInputAxisSwizzle::YXZ;
		return Modifier;
	};
	auto Negate = [Context](bool bX, bool bY)
	{
		UInputModifierNegate* Modifier = NewObject<UInputModifierNegate>(Context);
		Modifier->bX = bX;
		Modifier->bY = bY;
		Modifier->bZ = false;
		return Modifier;
	};
	auto Hold = [Context](float Seconds)
	{
		UInputTriggerHold* Trigger = NewObject<UInputTriggerHold>(Context);
		Trigger->HoldTimeThreshold = Seconds;
		Trigger->bIsOneShot = true;
		return Trigger;
	};

	// 移動：左スティック（デッドゾーンと歩き／走りの判定は ParkourRules で行うので修飾しない）＋ WASD
	Context->MapKey(MoveAction, EKeys::Gamepad_Left2D);
	Context->MapKey(MoveAction, EKeys::W).Modifiers.Add(Swizzle());
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(MoveAction, EKeys::S);
		Mapping.Modifiers.Add(Swizzle());
		Mapping.Modifiers.Add(Negate(true, true));
	}
	Context->MapKey(MoveAction, EKeys::A).Modifiers.Add(Negate(true, false));
	Context->MapKey(MoveAction, EKeys::D);

	// カメラ
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(LookStickAction, EKeys::Gamepad_Right2D);
		UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(Context);
		DeadZone->LowerThreshold = GetParkourMovement()->GetSettings()->StickDeadZone;
		Mapping.Modifiers.Add(DeadZone);
		Mapping.Modifiers.Add(Negate(false, true));
	}
	Context->MapKey(LookMouseAction, EKeys::Mouse2D).Modifiers.Add(Negate(false, true));

	// 上アクション：A（または LB）
	Context->MapKey(UpAction, EKeys::Gamepad_FaceButton_Bottom);
	Context->MapKey(UpAction, EKeys::Gamepad_LeftShoulder);
	Context->MapKey(UpAction, EKeys::SpaceBar);

	// 下アクション：B（または RB）
	Context->MapKey(DownAction, EKeys::Gamepad_FaceButton_Right);
	Context->MapKey(DownAction, EKeys::Gamepad_RightShoulder);
	Context->MapKey(DownAction, EKeys::LeftControl);
	Context->MapKey(DownAction, EKeys::C);

	// ダッシュ（自動加速）切り替え：左スティック押し込み
	Context->MapKey(SprintToggleAction, EKeys::Gamepad_LeftThumbstick);
	Context->MapKey(SprintToggleAction, EKeys::LeftShift);

	// リスタート：Back 長押し
	Context->MapKey(RestartAction, EKeys::Gamepad_Special_Left).Triggers.Add(Hold(1.f));
	Context->MapKey(RestartAction, EKeys::R).Triggers.Add(Hold(1.f));

	DefaultMappingContext = Context;
}

void AParkourCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	BuildDefaultInput();
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AParkourCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildDefaultInput();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("Enhanced Input が無効です。Config/DefaultInput.ini を確認してください。"));
		return;
	}
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AParkourCharacter::Move);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &AParkourCharacter::MoveCompleted);
	Input->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &AParkourCharacter::LookStick);
	Input->BindAction(LookMouseAction, ETriggerEvent::Triggered, this, &AParkourCharacter::LookMouse);
	Input->BindAction(UpAction, ETriggerEvent::Started, this, &AParkourCharacter::UpPressed);
	Input->BindAction(DownAction, ETriggerEvent::Started, this, &AParkourCharacter::DownPressed);
	Input->BindAction(DownAction, ETriggerEvent::Completed, this, &AParkourCharacter::DownReleased);
	Input->BindAction(SprintToggleAction, ETriggerEvent::Started, this, &AParkourCharacter::ToggleSprint);
	Input->BindAction(RestartAction, ETriggerEvent::Triggered, this, &AParkourCharacter::RestartPressed);
}

void AParkourCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	UParkourMovementComponent* PM = GetParkourMovement();
	const float Raw = FMath::Min(static_cast<float>(Axis.Size()), 1.f);
	PM->SetStickInput(Raw);
	if (!Controller || PM->IsStaggered()) return;

	const UParkourMovementSettings* S = PM->GetSettings();
	const ParkourRules::FStickResponse Response = ParkourRules::EvaluateStick(Raw, S->StickDeadZone, S->RunStickThreshold);
	if (Response.InputScale <= 0.f) return;

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix Basis(YawRotation);
	const FVector Direction = (Basis.GetUnitAxis(EAxis::X) * Axis.Y + Basis.GetUnitAxis(EAxis::Y) * Axis.X).GetSafeNormal();
	AddMovementInput(Direction, Response.InputScale);
}

void AParkourCharacter::MoveCompleted(const FInputActionValue& Value)
{
	GetParkourMovement()->SetStickInput(0.f);
}

void AParkourCharacter::LookStick(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const float Step = LookStickRate * GetWorld()->GetDeltaSeconds();
	AddControllerYawInput(Axis.X * Step);
	AddControllerPitchInput(Axis.Y * Step);
}

void AParkourCharacter::LookMouse(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * MouseSensitivity);
	AddControllerPitchInput(Axis.Y * MouseSensitivity);
}

void AParkourCharacter::UpPressed()
{
	GetParkourMovement()->UpActionPressed();
}

void AParkourCharacter::DownPressed()
{
	GetParkourMovement()->DownActionPressed();
}

void AParkourCharacter::DownReleased()
{
	GetParkourMovement()->DownActionReleased();
}

void AParkourCharacter::ToggleSprint()
{
	GetParkourMovement()->ToggleAutoSprint();
}

void AParkourCharacter::RestartPressed()
{
	Respawn(true);
}
