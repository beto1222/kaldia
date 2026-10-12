#include "Player/KaelCharacter.h"
#include "Player/KaelPlayerController.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Combat/Grenade.h"
#include "Mission/InteractableActor.h"
#include "AI/EnemyCharacter.h"
#include "Core/RupturaAssets.h"
#include "Core/RupturaGameInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"

AKaelCharacter::AKaelCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(38.f, 92.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->JumpZVelocity = 520.f;
	Move->AirControl = 0.3f;
	Move->MaxWalkSpeed = JogSpeed;
	Move->MaxWalkSpeedCrouched = 210.f;
	Move->BrakingDecelerationWalking = 2000.f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(60.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 290.f;
	CameraBoom->SocketOffset = FVector(0.f, 70.f, 55.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true; // colisão da câmera com o cenário
	CameraBoom->ProbeSize = 14.f;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 18.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(90.f);

	BodyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyRoot"));
	BodyRoot->SetupAttachment(RootComponent);
	BodyRoot->SetRelativeLocation(FVector(0.f, 0.f, -92.f));

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 100.f;

	Weapon = CreateDefaultSubobject<UWeaponComponent>(TEXT("Weapon"));

	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSource"));
}

void AKaelCharacter::BeginPlay()
{
	Super::BeginPlay();

	Weapon->InitPlayerLoadout();

	FSoldierColors Colors;
	Colors.Armor = FLinearColor(0.07f, 0.09f, 0.06f);
	Colors.Suit = FLinearColor(0.1f, 0.1f, 0.11f);
	Colors.Visor = FLinearColor(0.1f, 0.85f, 1.f);
	Colors.VisorGlow = 10.f;
	SoldierBody::Build(this, BodyRoot, Colors, 1.f, Body);
	SoldierBody::SetGunLength(Body, Weapon->GetDef().GunLength);

	Health->OnHealthChanged.AddDynamic(this, &AKaelCharacter::HandleHealthChanged);
	Health->OnDeath.AddDynamic(this, &AKaelCharacter::HandleDeath);

	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	StimuliSource->RegisterForSense(UAISense_Hearing::StaticClass());
	StimuliSource->RegisterWithPerceptionSystem();
}

// ---------------------------------------------------------------------------------------------
// Entrada (Enhanced Input criado em código, sem assets)
// ---------------------------------------------------------------------------------------------

void AKaelCharacter::BuildInput()
{
	if (Mapping)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
		Action->ValueType = Type;
		return Action;
	};

	IA_Move = MakeAction(TEXT("IA_Mover"), EInputActionValueType::Axis2D);
	IA_Look = MakeAction(TEXT("IA_Olhar"), EInputActionValueType::Axis2D);
	IA_LookPad = MakeAction(TEXT("IA_OlharControle"), EInputActionValueType::Axis2D);
	IA_Jump = MakeAction(TEXT("IA_Pular"), EInputActionValueType::Boolean);
	IA_Sprint = MakeAction(TEXT("IA_Correr"), EInputActionValueType::Boolean);
	IA_Crouch = MakeAction(TEXT("IA_Agachar"), EInputActionValueType::Boolean);
	IA_Dodge = MakeAction(TEXT("IA_Esquivar"), EInputActionValueType::Boolean);
	IA_Aim = MakeAction(TEXT("IA_Mirar"), EInputActionValueType::Boolean);
	IA_Fire = MakeAction(TEXT("IA_Atirar"), EInputActionValueType::Boolean);
	IA_Reload = MakeAction(TEXT("IA_Recarregar"), EInputActionValueType::Boolean);
	IA_Grenade = MakeAction(TEXT("IA_Granada"), EInputActionValueType::Boolean);
	IA_Weapon1 = MakeAction(TEXT("IA_Arma1"), EInputActionValueType::Boolean);
	IA_Weapon2 = MakeAction(TEXT("IA_Arma2"), EInputActionValueType::Boolean);
	IA_SwitchWeapon = MakeAction(TEXT("IA_TrocarArma"), EInputActionValueType::Boolean);
	IA_Cover = MakeAction(TEXT("IA_Cobertura"), EInputActionValueType::Boolean);
	IA_Interact = MakeAction(TEXT("IA_Interagir"), EInputActionValueType::Boolean);

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Kael"));

	auto MapAxisKey = [this](UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = Mapping->MapKey(Action, Key);
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Mapping);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			M.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			M.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
		}
	};
	auto MapStick = [this](UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& M = Mapping->MapKey(Action, Key);
		M.Modifiers.Add(NewObject<UInputModifierDeadZone>(Mapping));
	};

	// Teclado + mouse
	MapAxisKey(IA_Move, EKeys::W, true, false);
	MapAxisKey(IA_Move, EKeys::S, true, true);
	MapAxisKey(IA_Move, EKeys::D, false, false);
	MapAxisKey(IA_Move, EKeys::A, false, true);
	Mapping->MapKey(IA_Look, EKeys::Mouse2D);
	Mapping->MapKey(IA_Jump, EKeys::SpaceBar);
	Mapping->MapKey(IA_Sprint, EKeys::LeftShift);
	Mapping->MapKey(IA_Crouch, EKeys::C);
	Mapping->MapKey(IA_Crouch, EKeys::LeftControl);
	Mapping->MapKey(IA_Dodge, EKeys::LeftAlt);
	Mapping->MapKey(IA_Dodge, EKeys::V);
	Mapping->MapKey(IA_Aim, EKeys::RightMouseButton);
	Mapping->MapKey(IA_Fire, EKeys::LeftMouseButton);
	Mapping->MapKey(IA_Reload, EKeys::R);
	Mapping->MapKey(IA_Grenade, EKeys::G);
	Mapping->MapKey(IA_Weapon1, EKeys::One);
	Mapping->MapKey(IA_Weapon2, EKeys::Two);
	Mapping->MapKey(IA_SwitchWeapon, EKeys::MouseScrollUp);
	Mapping->MapKey(IA_SwitchWeapon, EKeys::MouseScrollDown);
	Mapping->MapKey(IA_Cover, EKeys::Q);
	Mapping->MapKey(IA_Interact, EKeys::E);

	// Controle
	MapStick(IA_Move, EKeys::Gamepad_Left2D);
	MapStick(IA_LookPad, EKeys::Gamepad_Right2D);
	Mapping->MapKey(IA_Jump, EKeys::Gamepad_FaceButton_Bottom);
	Mapping->MapKey(IA_Sprint, EKeys::Gamepad_LeftThumbstick);
	Mapping->MapKey(IA_Crouch, EKeys::Gamepad_FaceButton_Right);
	Mapping->MapKey(IA_Dodge, EKeys::Gamepad_DPad_Down);
	Mapping->MapKey(IA_Aim, EKeys::Gamepad_LeftTrigger);
	Mapping->MapKey(IA_Fire, EKeys::Gamepad_RightTrigger);
	Mapping->MapKey(IA_Reload, EKeys::Gamepad_RightShoulder);
	Mapping->MapKey(IA_Grenade, EKeys::Gamepad_LeftShoulder);
	Mapping->MapKey(IA_SwitchWeapon, EKeys::Gamepad_FaceButton_Top);
	Mapping->MapKey(IA_Cover, EKeys::Gamepad_RightThumbstick);
	Mapping->MapKey(IA_Interact, EKeys::Gamepad_FaceButton_Left);
}

void AKaelCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error, TEXT("Ruptura: Enhanced Input não está ativo. Verifique Config/DefaultInput.ini."));
		return;
	}

	EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AKaelCharacter::Input_Move);
	EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AKaelCharacter::Input_Look);
	EIC->BindAction(IA_LookPad, ETriggerEvent::Triggered, this, &AKaelCharacter::Input_LookPad);
	EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AKaelCharacter::Input_JumpStart);
	EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AKaelCharacter::Input_JumpStop);
	EIC->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AKaelCharacter::Input_SprintStart);
	EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AKaelCharacter::Input_SprintStop);
	EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AKaelCharacter::Input_Crouch);
	EIC->BindAction(IA_Dodge, ETriggerEvent::Started, this, &AKaelCharacter::Input_Dodge);
	EIC->BindAction(IA_Aim, ETriggerEvent::Started, this, &AKaelCharacter::Input_AimStart);
	EIC->BindAction(IA_Aim, ETriggerEvent::Completed, this, &AKaelCharacter::Input_AimStop);
	EIC->BindAction(IA_Fire, ETriggerEvent::Started, this, &AKaelCharacter::Input_FireStart);
	EIC->BindAction(IA_Fire, ETriggerEvent::Completed, this, &AKaelCharacter::Input_FireStop);
	EIC->BindAction(IA_Reload, ETriggerEvent::Started, this, &AKaelCharacter::Input_Reload);
	EIC->BindAction(IA_Grenade, ETriggerEvent::Started, this, &AKaelCharacter::Input_Grenade);
	EIC->BindAction(IA_Weapon1, ETriggerEvent::Started, this, &AKaelCharacter::Input_Weapon1);
	EIC->BindAction(IA_Weapon2, ETriggerEvent::Started, this, &AKaelCharacter::Input_Weapon2);
	EIC->BindAction(IA_SwitchWeapon, ETriggerEvent::Started, this, &AKaelCharacter::Input_SwitchWeapon);
	EIC->BindAction(IA_Cover, ETriggerEvent::Started, this, &AKaelCharacter::Input_Cover);
	EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &AKaelCharacter::Input_InteractStart);
	EIC->BindAction(IA_Interact, ETriggerEvent::Completed, this, &AKaelCharacter::Input_InteractStop);
}

void AKaelCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	BuildInput();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->RemoveMappingContext(Mapping);
				Subsystem->AddMappingContext(Mapping, 0);
			}
		}
	}
}

void AKaelCharacter::Input_Move(const FInputActionValue& Value)
{
	if (bInputLocked || bDead || !Controller)
	{
		return;
	}

	const FVector2D In = Value.Get<FVector2D>();
	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	const FVector Desired = Forward * In.Y + Right * In.X;

	if (bInCover)
	{
		// Afastar-se da parede sai da cobertura.
		if (FVector::DotProduct(Desired, CoverNormal) > 0.75f && In.Size() > 0.5f)
		{
			ExitCover();
			AddMovementInput(Desired);
			return;
		}
		// Desliza ao longo da parede, parando na borda.
		const FVector Tangent = FVector::CrossProduct(FVector::UpVector, CoverNormal).GetSafeNormal();
		const float Along = FVector::DotProduct(Desired, Tangent);
		if (FMath::Abs(Along) < 0.2f)
		{
			return;
		}
		const FVector Step = Tangent * FMath::Sign(Along);
		const FVector Probe = GetActorLocation() + Step * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 25.f);
		if (HasWallAt(Probe, -30.f))
		{
			AddMovementInput(Step, FMath::Abs(Along));
		}
		return;
	}

	AddMovementInput(Forward, In.Y);
	AddMovementInput(Right, In.X);
}

void AKaelCharacter::Input_Look(const FInputActionValue& Value)
{
	if (bInputLocked || bDead)
	{
		return;
	}
	const FVector2D L = Value.Get<FVector2D>();
	const float S = MouseSensitivity * (bAiming ? 0.55f : 1.f);
	AddControllerYawInput(L.X * S);
	AddControllerPitchInput(-L.Y * S);
}

void AKaelCharacter::Input_LookPad(const FInputActionValue& Value)
{
	if (bInputLocked || bDead)
	{
		return;
	}
	const FVector2D L = Value.Get<FVector2D>();
	const float S = PadSensitivity * GetWorld()->GetDeltaSeconds() * (bAiming ? 0.5f : 1.f) / 2.5f;
	AddControllerYawInput(L.X * S);
	AddControllerPitchInput(-L.Y * S);
}

bool AKaelCharacter::CanJumpInternal_Implementation() const
{
	return !bInputLocked && !bDead && !bInCover && Super::CanJumpInternal_Implementation();
}

void AKaelCharacter::Input_JumpStart()
{
	if (bInputLocked || bDead)
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
		return;
	}
	Jump();
}

void AKaelCharacter::Input_JumpStop()
{
	StopJumping();
}

void AKaelCharacter::Input_SprintStart()
{
	if (bInputLocked || bDead)
	{
		return;
	}
	bSprinting = true;
	if (bInCover)
	{
		ExitCover();
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
}

void AKaelCharacter::Input_SprintStop()
{
	bSprinting = false;
}

void AKaelCharacter::Input_Crouch()
{
	if (bInputLocked || bDead || bInCover)
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		bSprinting = false;
		Crouch();
	}
}

void AKaelCharacter::Input_Dodge()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (bInputLocked || bDead || Now < NextDodgeTime || GetCharacterMovement()->IsFalling())
	{
		return;
	}

	FVector Dir = GetLastMovementInputVector().GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = -GetActorForwardVector().GetSafeNormal2D(); // sem direção: recua
	}

	ExitCover();
	if (bIsCrouched)
	{
		UnCrouch();
	}

	LaunchCharacter(Dir * DodgeStrength + FVector(0.f, 0.f, 140.f), true, true);
	bDodging = true;
	DodgeSpin = 0.f;
	DodgeEndTime = Now + 0.45f;
	NextDodgeTime = Now + 0.9f;
	Health->bInvulnerable = true; // janela curta de invulnerabilidade
	SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	RupturaAssets::PlaySound3D(this, TEXT("esquiva"), GetActorLocation(), 0.7f);
}

void AKaelCharacter::Input_AimStart()
{
	if (bInputLocked || bDead)
	{
		return;
	}
	bAiming = true;
	bSprinting = false;
}

void AKaelCharacter::Input_AimStop()
{
	bAiming = false;
}

void AKaelCharacter::Input_FireStart()
{
	if (bInputLocked || bDead)
	{
		return;
	}
	bFireHeld = true;
	bSemiShotQueued = true;
}

void AKaelCharacter::Input_FireStop()
{
	bFireHeld = false;
}

void AKaelCharacter::Input_Reload()
{
	if (!bInputLocked && !bDead)
	{
		Weapon->StartReload();
	}
}

void AKaelCharacter::Input_Grenade()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (bInputLocked || bDead || Grenades <= 0 || Now < NextGrenadeTime || !Controller)
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	Controller->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector SpawnLoc = GetActorLocation() + GetActorForwardVector() * 35.f + GetActorRightVector() * 20.f + FVector(0.f, 0.f, 55.f);
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AGrenade* G = GetWorld()->SpawnActor<AGrenade>(AGrenade::StaticClass(), SpawnLoc, CamRot, Params))
	{
		const FVector Velocity = CamRot.Vector() * 1350.f + FVector(0.f, 0.f, 320.f) + GetVelocity() * 0.4f;
		G->Launch(Velocity, false);
		--Grenades;
		NextGrenadeTime = Now + 0.8f;
		RupturaAssets::PlaySound3D(this, TEXT("lancar"), SpawnLoc, 0.8f);
	}
}

void AKaelCharacter::Input_Weapon1()
{
	EquipWeapon(0);
}

void AKaelCharacter::Input_Weapon2()
{
	EquipWeapon(1);
}

void AKaelCharacter::Input_SwitchWeapon()
{
	if (Weapon->GetNumWeapons() > 0)
	{
		EquipWeapon((Weapon->GetCurrentIndex() + 1) % Weapon->GetNumWeapons());
	}
}

void AKaelCharacter::EquipWeapon(int32 Index)
{
	if (bInputLocked || bDead)
	{
		return;
	}
	Weapon->Equip(Index);
	SoldierBody::SetGunLength(Body, Weapon->GetDef().GunLength);
}

void AKaelCharacter::Input_Cover()
{
	if (bInputLocked || bDead)
	{
		return;
	}
	if (bInCover)
	{
		ExitCover();
	}
	else
	{
		TryEnterCover();
	}
}

void AKaelCharacter::Input_InteractStart()
{
	bInteractHeld = true;
	bInteractConsumed = false;
}

void AKaelCharacter::Input_InteractStop()
{
	bInteractHeld = false;
	InteractProgress = 0.f;
}

// ---------------------------------------------------------------------------------------------
// Cobertura
// ---------------------------------------------------------------------------------------------

bool AKaelCharacter::HasWallAt(const FVector& Point, float Height) const
{
	const FVector Start = Point + FVector(0.f, 0.f, Height);
	const FVector End = Start - CoverNormal * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 60.f);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KaelCoverProbe), false, this);
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
}

bool AKaelCharacter::TryEnterCover()
{
	if (!Controller)
	{
		return false;
	}

	const FVector Look = FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f).Vector();
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, -30.f);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KaelCoverEnter), false, this);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Look * 180.f, ECC_Visibility, Query))
	{
		return false;
	}
	if (FMath::Abs(Hit.ImpactNormal.Z) > 0.3f)
	{
		return false; // não é uma parede
	}

	CoverNormal = Hit.ImpactNormal.GetSafeNormal2D();
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	FVector Target = Hit.ImpactPoint + CoverNormal * (Radius + 6.f);
	Target.Z = GetActorLocation().Z;
	SetActorLocation(Target, true);

	// Cobertura baixa: nada bloqueando na altura do peito.
	FHitResult High;
	const FVector HighStart = Target + FVector(0.f, 0.f, 60.f);
	bCoverLow = !GetWorld()->LineTraceSingleByChannel(High, HighStart, HighStart - CoverNormal * (Radius + 70.f), ECC_Visibility, Query);

	bInCover = true;
	bSprinting = false;
	if (bCoverLow)
	{
		Crouch();
	}
	RupturaAssets::PlaySound3D(this, TEXT("cobertura"), GetActorLocation(), 0.6f);
	return true;
}

void AKaelCharacter::ExitCover()
{
	if (!bInCover)
	{
		return;
	}
	bInCover = false;
	if (bCoverLow && bIsCrouched)
	{
		UnCrouch();
	}
}

void AKaelCharacter::UpdateCover(float DeltaSeconds)
{
	if (!bInCover)
	{
		return;
	}
	if (!HasWallAt(GetActorLocation(), -30.f))
	{
		ExitCover();
		return;
	}
	if (bCoverLow)
	{
		// Levanta para mirar/atirar por cima da cobertura baixa.
		const bool bWantUp = bAiming || bFireHeld;
		if (bWantUp && bIsCrouched)
		{
			UnCrouch();
		}
		else if (!bWantUp && !bIsCrouched)
		{
			Crouch();
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void AKaelCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateMovementMode(DeltaSeconds);
	UpdateCover(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
	UpdateFiring();
	UpdateInteraction(DeltaSeconds);
	UpdateAimTarget();
	UpdateFootsteps(DeltaSeconds);
	UpdateBodyPose(DeltaSeconds);

	// Regeneração lenta até 35 de vida, após 6 s sem dano (evita ficar travado sem kits médicos).
	if (!bDead && Health->Health < 35.f && GetWorld()->GetTimeSeconds() - LastDamageTime > 6.f)
	{
		Health->Heal(FMath::Min(4.f * DeltaSeconds, 35.f - Health->Health));
	}
}

void AKaelCharacter::UpdateMovementMode(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Now = GetWorld()->GetTimeSeconds();

	if (bDodging && Now >= DodgeEndTime)
	{
		bDodging = false;
		Health->bInvulnerable = false;
	}

	float Speed = JogSpeed;
	if (bAiming)
	{
		Speed = AimSpeed;
	}
	else if (bSprinting && !bFireHeld)
	{
		Speed = SprintSpeed;
	}
	if (bInCover)
	{
		Speed = 230.f;
	}
	if (Now < StaggerUntil)
	{
		Speed *= 0.6f; // reação a tiros
	}
	Move->MaxWalkSpeed = Speed;

	if (bDead)
	{
		return;
	}

	const bool bCombatFacing = bAiming || bFireHeld || (Now - LastShotTime) < 0.8f;
	if (bDodging)
	{
		Move->bOrientRotationToMovement = false;
	}
	else if (bCombatFacing && Controller)
	{
		Move->bOrientRotationToMovement = false;
		const FRotator Target(0.f, Controller->GetControlRotation().Yaw, 0.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, 18.f));
	}
	else if (bInCover)
	{
		Move->bOrientRotationToMovement = false;
		const FRotator Target(0.f, (-CoverNormal).Rotation().Yaw, 0.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, 10.f));
	}
	else
	{
		Move->bOrientRotationToMovement = true;
	}
}

void AKaelCharacter::UpdateCamera(float DeltaSeconds)
{
	const bool bMoving = GetVelocity().Size2D() > 50.f;
	const float TargetLength = bAiming ? 150.f : (bInCover ? 230.f : ((bSprinting && bMoving) ? 330.f : 290.f));
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetLength, DeltaSeconds, 10.f);

	const FVector TargetOffset = bAiming ? FVector(0.f, 58.f, 48.f) : FVector(0.f, 70.f, 55.f);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetOffset, DeltaSeconds, 10.f);

	const float TargetFov = bAiming ? 62.f : ((bSprinting && bMoving) ? 96.f : 90.f);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, TargetFov, DeltaSeconds, 10.f));

	// Tremor de câmera baseado em "trauma" (tiros, explosões, dano).
	Trauma = FMath::Max(0.f, Trauma - DeltaSeconds * 0.9f);
	const float Shake = Trauma * Trauma;
	const float T = GetWorld()->GetTimeSeconds() * 25.f;
	FollowCamera->SetRelativeRotation(FRotator(
		FMath::PerlinNoise1D(T) * 4.f * Shake,
		FMath::PerlinNoise1D(T + 31.7f) * 4.f * Shake,
		FMath::PerlinNoise1D(T + 77.1f) * 6.f * Shake));
	FollowCamera->SetRelativeLocation(FVector(0.f, FMath::PerlinNoise1D(T + 11.3f) * 8.f * Shake, FMath::PerlinNoise1D(T + 51.9f) * 8.f * Shake));

	DamageFlash = FMath::FInterpTo(DamageFlash, 0.f, DeltaSeconds, 2.5f);
}

float AKaelCharacter::GetCurrentSpreadDeg() const
{
	const bool bMoving = GetVelocity().Size2D() > 60.f;
	return Weapon->GetSpread(bAiming, bMoving) * (bIsCrouched ? 0.8f : 1.f);
}

FVector AKaelCharacter::GetMuzzleLocation() const
{
	return Body.Muzzle ? Body.Muzzle->GetComponentLocation() : GetActorLocation() + GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 40.f);
}

void AKaelCharacter::UpdateFiring()
{
	if (!bFireHeld || bInputLocked || bDead || bDodging || !Controller)
	{
		return;
	}

	const FWeaponDef& Def = Weapon->GetDef();
	if (!Def.bAutomatic && !bSemiShotQueued)
	{
		return;
	}
	if (bInCover && bCoverLow && bIsCrouched)
	{
		return; // ainda levantando da cobertura
	}
	if (Weapon->GetAmmo().Mag > 0 && !Weapon->CanFire())
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	Controller->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Dir = CamRot.Vector();
	// Começa o traço da mira na altura do personagem para não acertar nada atrás dele.
	const float Along = FVector::DotProduct(GetActorLocation() - CamLoc, Dir);
	const FVector Start = CamLoc + Dir * FMath::Max(0.f, Along);

	FShotResult Result;
	const bool bFired = Weapon->FireShot(Start, Dir, GetMuzzleLocation(), GetCurrentSpreadDeg(), Result);
	bSemiShotQueued = false;
	if (!bFired)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	LastShotTime = Now;
	Body.Kick = 1.f;

	// Recuo aplicado direto na rotação de controle (graus).
	FRotator ControlRot = Controller->GetControlRotation();
	const float K = bAiming ? 0.55f : 1.f;
	ControlRot.Pitch += Def.RecoilPitch * K;
	ControlRot.Yaw += FMath::FRandRange(-Def.RecoilYaw, Def.RecoilYaw) * K;
	Controller->SetControlRotation(ControlRot);
	AddTrauma(0.04f);

	if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		++GI->ShotsFired;
		if (Result.bHitPawn)
		{
			++GI->ShotsHit;
		}
	}
	if (Result.bHitPawn)
	{
		HitMarkerTime = Now;
		bLastHitKill = Result.bKilled;
		RupturaAssets::PlaySound2D(this, Result.bKilled ? FName(TEXT("confirmacao_abate")) : FName(TEXT("marcador_acerto")), 0.5f);
	}
}

void AKaelCharacter::UpdateInteraction(float DeltaSeconds)
{
	AInteractableActor* Best = nullptr;

	if (!bDead && !bInputLocked && Controller)
	{
		FVector CamLoc;
		FRotator CamRot;
		Controller->GetPlayerViewPoint(CamLoc, CamRot);
		const FVector Forward = CamRot.Vector();
		float BestScore = -1.f;

		for (TActorIterator<AInteractableActor> It(GetWorld()); It; ++It)
		{
			AInteractableActor* Candidate = *It;
			if (Candidate->GetInteractPrompt(this).IsEmpty())
			{
				continue; // nada a fazer aqui (ex.: porta já aberta)
			}
			const FVector Loc = Candidate->GetInteractLocation();
			const float Dist = FVector::Dist(Loc, GetActorLocation());
			if (Dist > Candidate->InteractRadius)
			{
				continue;
			}
			const float Dot = FVector::DotProduct(Forward, (Loc - CamLoc).GetSafeNormal());
			if (Dot < 0.45f)
			{
				continue;
			}
			FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(KaelInteract), false, this);
			Query.AddIgnoredActor(Candidate);
			if (GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation() + FVector(0.f, 0.f, 50.f), Loc, ECC_Visibility, Query))
			{
				continue;
			}
			const float Score = Dot - Dist / 1000.f;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Candidate;
			}
		}
	}

	if (Best != FocusedInteractable.Get())
	{
		InteractProgress = 0.f;
	}
	FocusedInteractable = Best;

	if (bInteractHeld && !bInteractConsumed && Best && Best->CanInteract(this))
	{
		InteractProgress += DeltaSeconds;
		if (InteractProgress >= Best->GetInteractHoldTime())
		{
			Best->Interact(this);
			InteractProgress = 0.f;
			bInteractConsumed = true;
		}
	}
	else if (!bInteractHeld)
	{
		InteractProgress = 0.f;
	}
}

bool AKaelCharacter::GetInteractionInfo(FText& OutPrompt, float& OutProgress, bool& bOutCanInteract) const
{
	const AInteractableActor* Interactable = FocusedInteractable.Get();
	if (!Interactable)
	{
		return false;
	}
	OutPrompt = Interactable->GetInteractPrompt(this);
	bOutCanInteract = Interactable->CanInteract(this);
	const float Hold = Interactable->GetInteractHoldTime();
	OutProgress = Hold > 0.f ? FMath::Clamp(InteractProgress / Hold, 0.f, 1.f) : 0.f;
	return true;
}

void AKaelCharacter::UpdateAimTarget()
{
	bAimingAtEnemy = false;
	if (!Controller || bDead)
	{
		return;
	}
	FVector CamLoc;
	FRotator CamRot;
	Controller->GetPlayerViewPoint(CamLoc, CamRot);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KaelAimTarget), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + CamRot.Vector() * 7000.f, ECC_Pawn, Query))
	{
		if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Hit.GetActor()))
		{
			bAimingAtEnemy = !Enemy->IsDead();
		}
	}
}

void AKaelCharacter::UpdateFootsteps(float DeltaSeconds)
{
	const float Speed = GetVelocity().Size2D();
	if (bDead || GetCharacterMovement()->IsFalling() || Speed < 150.f)
	{
		return;
	}
	FootstepTimer -= DeltaSeconds * (Speed / 400.f);
	if (FootstepTimer > 0.f)
	{
		return;
	}
	FootstepTimer = 0.5f;

	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	RupturaAssets::PlaySound3D(this, TEXT("passo"), Feet, bIsCrouched ? 0.12f : (Speed > 500.f ? 0.45f : 0.28f), FMath::FRandRange(0.9f, 1.1f));

	// Passos são ouvidos pela IA (correndo = mais longe; agachado = silencioso).
	if (!bIsCrouched)
	{
		const bool bRunning = Speed > 500.f;
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), Feet, bRunning ? 0.6f : 0.3f, this, bRunning ? 1100.f : 450.f, TEXT("Passos"));
	}
}

void AKaelCharacter::UpdateBodyPose(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	FVector RootLoc(0.f, 0.f, -HalfHeight);
	FRotator RootRot = FRotator::ZeroRotator;
	float ScaleZ = HalfHeight / 92.f;

	if (bDodging)
	{
		DodgeSpin = FMath::Min(1.f, DodgeSpin + DeltaSeconds / 0.45f);
		ScaleZ *= FMath::Lerp(0.6f, 1.f, FMath::Abs(DodgeSpin * 2.f - 1.f));
		RootRot.Pitch = -25.f * FMath::Sin(DodgeSpin * PI);
	}
	if (bDead)
	{
		const float A = FMath::Clamp((Now - DeathTime) / 0.7f, 0.f, 1.f);
		RootRot.Roll = 85.f * A;
	}

	BodyRoot->SetRelativeLocation(RootLoc);
	BodyRoot->SetRelativeRotation(RootRot);
	BodyRoot->SetRelativeScale3D(FVector(1.f, 1.f, ScaleZ));

	const bool bAimPose = bAiming || bFireHeld || (Now - LastShotTime) < 0.8f;
	SoldierBody::Animate(Body, bDead ? 0.f : GetVelocity().Size2D(), DeltaSeconds, bAimPose);
}

// ---------------------------------------------------------------------------------------------
// Dano e estado
// ---------------------------------------------------------------------------------------------

void AKaelCharacter::AddTrauma(float Amount)
{
	Trauma = FMath::Clamp(Trauma + Amount, 0.f, 1.f);
}

void AKaelCharacter::SetInputLocked(bool bLocked)
{
	bInputLocked = bLocked;
	if (bLocked)
	{
		bFireHeld = false;
		bAiming = false;
		bSprinting = false;
		bInteractHeld = false;
	}
}

int32 AKaelCharacter::AddGrenades(int32 Amount)
{
	const int32 Before = Grenades;
	Grenades = FMath::Clamp(Grenades + Amount, 0, MaxGrenades);
	return Grenades - Before;
}

void AKaelCharacter::HandleHealthChanged(UHealthComponent* Component, float NewHealth, float Delta, AActor* InstigatorActor)
{
	if (Delta >= 0.f)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	AddTrauma(FMath::Clamp(-Delta / 30.f, 0.15f, 0.6f));
	DamageFlash = FMath::Min(1.f, DamageFlash + 0.45f - Delta / 50.f);
	LastDamageTime = Now;
	if (InstigatorActor)
	{
		LastDamageSource = InstigatorActor->GetActorLocation();
	}
	StaggerUntil = Now + 0.25f;
	Body.Kick += 0.6f;
	RupturaAssets::PlaySound2D(this, TEXT("dano"), 0.8f, FMath::FRandRange(0.9f, 1.1f));
}

void AKaelCharacter::HandleDeath(UHealthComponent* Component, AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	DeathTime = GetWorld()->GetTimeSeconds();
	ExitCover();
	SetInputLocked(true);
	GetCharacterMovement()->DisableMovement();

	if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		++GI->Deaths;
	}
	if (AKaelPlayerController* PC = Cast<AKaelPlayerController>(GetController()))
	{
		PC->OnKaelDied();
	}
}
