#include "Player/KaelCharacter.h"
#include "Player/KaelPlayerController.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Combat/Grenade.h"
#include "Mission/InteractableActor.h"
#include "AI/EnemyCharacter.h"
#include "Core/FxActor.h"
#include "Core/RupturaAssets.h"
#include "Core/RupturaGameInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
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
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->JumpZVelocity = 420.f;
	Move->AirControl = 0.2f;
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = 170.f;
	Move->BrakingDecelerationWalking = 1600.f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(60.f);

	// Câmera sobre o ombro bem fechada: o cenário ocupa a tela e o que está atrás de você fica fora de vista.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 165.f;
	CameraBoom->SocketOffset = FVector(0.f, 58.f, 42.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 12.f;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 14.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(74.f);
	FollowCamera->PostProcessBlendWeight = 1.f;

	// Lanterna: o feixe aparece na neblina volumétrica e projeta sombras que se mexem com você.
	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(RootComponent);
	Flashlight->SetAbsolute(true, true, false);
	Flashlight->SetMobility(EComponentMobility::Movable);
	Flashlight->SetIntensityUnits(ELightUnits::Candelas);
	Flashlight->SetIntensity(FlashlightCandelas);
	Flashlight->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
	Flashlight->InnerConeAngle = 11.f;
	Flashlight->OuterConeAngle = 27.f;
	Flashlight->SetAttenuationRadius(3200.f);
	Flashlight->SetCastShadows(true);
	Flashlight->VolumetricScatteringIntensity = 1.3f;

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

	// Roupas civis/táticas foscas, óculos escuros sem brilho.
	FSoldierColors Colors;
	Colors.Armor = FLinearColor(0.05f, 0.055f, 0.05f);
	Colors.Suit = FLinearColor(0.075f, 0.07f, 0.068f);
	Colors.Visor = FLinearColor(0.02f, 0.02f, 0.024f);
	Colors.VisorGlow = 0.f;
	SoldierBody::Build(this, BodyRoot, Colors, 1.f, Body);
	SoldierBody::SetGunLength(Body, Weapon->GetDef().GunLength);

	Health->OnHealthChanged.AddDynamic(this, &AKaelCharacter::HandleHealthChanged);
	Health->OnDeath.AddDynamic(this, &AKaelCharacter::HandleDeath);

	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	StimuliSource->RegisterForSense(UAISense_Hearing::StaticClass());
	StimuliSource->RegisterWithPerceptionSystem();

	BreathLoop = RupturaAssets::CreateLoop2D(this, TEXT("respiracao_loop"), 0.01f);
	SetFirstPerson(bStartInFirstPerson);
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
	IA_Melee = MakeAction(TEXT("IA_Faca"), EInputActionValueType::Boolean);
	IA_Flashlight = MakeAction(TEXT("IA_Lanterna"), EInputActionValueType::Boolean);
	IA_View = MakeAction(TEXT("IA_Camera"), EInputActionValueType::Boolean);
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
	Mapping->MapKey(IA_Aim, EKeys::RightMouseButton);
	Mapping->MapKey(IA_Fire, EKeys::LeftMouseButton);
	Mapping->MapKey(IA_Reload, EKeys::R);
	Mapping->MapKey(IA_Grenade, EKeys::G);
	Mapping->MapKey(IA_Weapon1, EKeys::One);
	Mapping->MapKey(IA_Weapon2, EKeys::Two);
	Mapping->MapKey(IA_SwitchWeapon, EKeys::MouseScrollUp);
	Mapping->MapKey(IA_SwitchWeapon, EKeys::MouseScrollDown);
	Mapping->MapKey(IA_Melee, EKeys::Q);
	Mapping->MapKey(IA_Melee, EKeys::ThumbMouseButton);
	Mapping->MapKey(IA_Flashlight, EKeys::F);
	Mapping->MapKey(IA_View, EKeys::V);
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
	Mapping->MapKey(IA_Melee, EKeys::Gamepad_RightThumbstick);
	Mapping->MapKey(IA_Flashlight, EKeys::Gamepad_DPad_Up);
	Mapping->MapKey(IA_View, EKeys::Gamepad_DPad_Right);
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
	EIC->BindAction(IA_Melee, ETriggerEvent::Started, this, &AKaelCharacter::Input_Melee);
	EIC->BindAction(IA_Flashlight, ETriggerEvent::Started, this, &AKaelCharacter::Input_Flashlight);
	EIC->BindAction(IA_View, ETriggerEvent::Started, this, &AKaelCharacter::Input_ToggleView);
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
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), In.Y);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), In.X);
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
	return !bInputLocked && !bDead && !bExhausted && Super::CanJumpInternal_Implementation();
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
	if (bInputLocked || bDead)
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
	if (bInputLocked || bDead || Now < NextDodgeTime || GetCharacterMovement()->IsFalling() || Stamina < 0.2f)
	{
		return;
	}

	FVector Dir = GetLastMovementInputVector().GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = -GetActorForwardVector().GetSafeNormal2D(); // sem direção: recua
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}

	// Um passo desesperado para o lado, não uma cambalhota de herói.
	LaunchCharacter(Dir * DodgeStrength + FVector(0.f, 0.f, 90.f), true, true);
	bDodging = true;
	DodgeSpin = 0.f;
	DodgeEndTime = Now + 0.35f;
	NextDodgeTime = Now + 1.1f;
	Stamina = FMath::Max(0.f, Stamina - 0.2f);
	LastSprintTime = Now;
	Health->bInvulnerable = true;
	if (!bFirstPerson)
	{
		SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	}
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
		const FVector Velocity = CamRot.Vector() * 1250.f + FVector(0.f, 0.f, 300.f) + GetVelocity() * 0.4f;
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

void AKaelCharacter::Input_Melee()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (bInputLocked || bDead || Now < NextMeleeTime || !Controller)
	{
		return;
	}
	NextMeleeTime = Now + 0.7f;
	MeleeTime = Now;
	Stamina = FMath::Max(0.f, Stamina - 0.08f);
	LastSprintTime = Now;

	const FVector Forward = FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f).Vector();
	SetActorRotation(Forward.Rotation());

	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 30.f);
	const FVector End = Start + Forward * 130.f;
	TArray<FHitResult> Hits;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KaelKnife), false, this);
	GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(45.f), Query);

	TSet<AActor*> Done;
	bool bHitAny = false;
	for (const FHitResult& H : Hits)
	{
		AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(H.GetActor());
		if (!Enemy || Enemy->IsDead() || Done.Contains(Enemy))
		{
			continue;
		}
		Done.Add(Enemy);
		// Golpe de misericórdia em quem ainda está caído.
		const float Damage = KnifeDamage * (Enemy->IsDormant() ? 2.f : 1.f);
		UGameplayStatics::ApplyPointDamage(Enemy, Damage, Forward, H, GetController(), this, UDamageType::StaticClass());
		if (!Enemy->IsBoss())
		{
			Enemy->Shove(Forward, 520.f);
		}
		RupturaFx::Blood(GetWorld(), Enemy->GetActorLocation() + FVector(0.f, 0.f, 40.f), Forward, 6);
		bHitAny = true;
	}

	RupturaAssets::PlaySound3D(this, bHitAny ? FName(TEXT("faca_acerto")) : FName(TEXT("faca")), GetActorLocation() + Forward * 60.f, 0.9f, FMath::FRandRange(0.92f, 1.08f));
	AddTrauma(bHitAny ? 0.15f : 0.04f);
	// A faca é quase silenciosa.
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 0.2f, this, 350.f, TEXT("Faca"));
}

void AKaelCharacter::Input_Flashlight()
{
	if (bDead)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (!bFlashlightOn && Battery <= 0.f)
	{
		RupturaAssets::PlaySound2D(this, TEXT("lanterna_click"), 0.5f, 0.8f);
		if (AKaelPlayerController* PC = Cast<AKaelPlayerController>(GetController()))
		{
			PC->ShowToast(FText::FromString(TEXT("Sem bateria. Procure pilhas.")), 2.f);
		}
		return;
	}
	bFlashlightOn = !bFlashlightOn;
	LastFlashlightToggle = Now;
	RupturaAssets::PlaySound2D(this, TEXT("lanterna_click"), 0.6f, bFlashlightOn ? 1.f : 0.9f);
}

void AKaelCharacter::Input_ToggleView()
{
	if (!bDead)
	{
		SetFirstPerson(!bFirstPerson);
	}
}

void AKaelCharacter::SetFirstPerson(bool bEnable)
{
	bFirstPerson = bEnable;
	// Em primeira pessoa o corpo some da câmera, mas continua projetando sombra.
	TArray<USceneComponent*> Children;
	BodyRoot->GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Child))
		{
			Prim->SetOwnerNoSee(bEnable);
			Prim->SetCastHiddenShadow(true);
		}
	}
	CameraBoom->bDoCollisionTest = !bEnable;
	CameraBoom->bEnableCameraLag = !bEnable;
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
// Tick
// ---------------------------------------------------------------------------------------------

void AKaelCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateMovementMode(DeltaSeconds);
	UpdateStamina(DeltaSeconds);
	UpdateAimPoint();
	UpdateFlashlight(DeltaSeconds);
	UpdateFear(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
	UpdateAimSway(DeltaSeconds);
	UpdatePostProcess(DeltaSeconds);
	UpdateFiring();
	UpdateInteraction(DeltaSeconds);
	UpdateFootsteps(DeltaSeconds);
	UpdateBodyPose(DeltaSeconds);
	UpdateHeartAndBreath(DeltaSeconds);
	// Sem regeneração: só kits médicos curam.
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

	const bool bCanSprint = bSprinting && !bExhausted && !bFireHeld && !bAiming;
	float Speed = bAiming ? AimSpeed : (bCanSprint ? SprintSpeed : WalkSpeed);
	if (Health->GetFraction() < 0.3f)
	{
		Speed *= 0.85f; // ferido, mancando
	}
	if (Now < StaggerUntil)
	{
		Speed *= 0.5f;
	}
	Move->MaxWalkSpeed = Speed;

	if (bDead)
	{
		return;
	}

	const bool bCombatFacing = bAiming || bFireHeld || (Now - LastShotTime) < 0.8f || (Now - MeleeTime) < 0.4f;
	if (bFirstPerson && Controller)
	{
		Move->bOrientRotationToMovement = false;
		SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
	}
	else if (bDodging)
	{
		Move->bOrientRotationToMovement = false;
	}
	else if (bCombatFacing && Controller)
	{
		Move->bOrientRotationToMovement = false;
		const FRotator Target(0.f, Controller->GetControlRotation().Yaw, 0.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, 18.f));
	}
	else
	{
		Move->bOrientRotationToMovement = true;
	}
}

void AKaelCharacter::UpdateStamina(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float Speed = GetVelocity().Size2D();
	if (bSprinting && !bExhausted && !bAiming && Speed > 250.f)
	{
		Stamina -= DeltaSeconds / FMath::Max(1.f, SprintDuration);
		LastSprintTime = Now;
		if (Stamina <= 0.f)
		{
			Stamina = 0.f;
			bExhausted = true;
			RupturaAssets::PlaySound2D(this, TEXT("ofegar"), 0.8f);
		}
	}
	else if (Now - LastSprintTime > 0.8f)
	{
		Stamina = FMath::Min(1.f, Stamina + DeltaSeconds * (Speed < 50.f ? 0.22f : 0.13f));
	}
	if (bExhausted && Stamina > 0.35f)
	{
		bExhausted = false;
	}
}

void AKaelCharacter::UpdateAimPoint()
{
	if (!Controller)
	{
		AimPoint = GetActorLocation() + GetActorForwardVector() * 1000.f;
		return;
	}
	FVector CamLoc;
	FRotator CamRot;
	Controller->GetPlayerViewPoint(CamLoc, CamRot);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KaelAimPoint), false, this);
	const FVector End = CamLoc + CamRot.Vector() * 6000.f;
	AimPoint = GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, Query) ? Hit.ImpactPoint : End;
}

FVector AKaelCharacter::GetFlashlightLocation() const
{
	return Flashlight ? Flashlight->GetComponentLocation() : GetActorLocation();
}

FVector AKaelCharacter::GetFlashlightDirection() const
{
	return Flashlight ? Flashlight->GetForwardVector() : GetActorForwardVector();
}

float AKaelCharacter::AddBattery(float Amount)
{
	const float Before = Battery;
	Battery = FMath::Clamp(Battery + Amount, 0.f, 1.f);
	return Battery - Before;
}

void AKaelCharacter::UpdateFlashlight(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();

	// Posição: em 1ª pessoa, na mão junto à câmera; em 3ª, presa no ombro/capacete, à frente e acima
	// da arma e dos braços (assim o próprio corpo não bloqueia o feixe), apontando para a mira.
	FVector Loc;
	FVector Dir;
	if (Controller && bFirstPerson)
	{
		FVector CamLoc;
		FRotator CamRot;
		Controller->GetPlayerViewPoint(CamLoc, CamRot);
		Loc = CamLoc + CamRot.RotateVector(FVector(15.f, 16.f, -14.f));
	}
	else
	{
		Loc = GetActorLocation() + GetActorRotation().RotateVector(FVector(40.f, -18.f, 72.f));
	}
	Dir = (AimPoint - Loc).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = GetActorForwardVector();
	}
	// O feixe acompanha a mira com um leve atraso (mão/corpo).
	const FRotator NewRot = FMath::RInterpTo(Flashlight->GetComponentRotation(), Dir.Rotation(), DeltaSeconds, bFirstPerson ? 14.f : 10.f);
	Flashlight->SetWorldLocationAndRotation(Loc, NewRot);

	const bool bOn = IsFlashlightOn();
	if (bOn && !bDead)
	{
		Battery = FMath::Max(0.f, Battery - DeltaSeconds / FMath::Max(1.f, BatteryLife));
		if (Battery <= 0.f)
		{
			bFlashlightOn = false;
			RupturaAssets::PlaySound2D(this, TEXT("lanterna_click"), 0.6f, 0.7f);
			if (AKaelPlayerController* PC = Cast<AKaelPlayerController>(GetController()))
			{
				PC->ShowToast(FText::FromString(TEXT("A lanterna apagou. Sem bateria.")), 3.f);
			}
		}
	}

	float K = IsFlashlightOn() ? 1.f : 0.f;
	if (K > 0.f)
	{
		// Bateria fraca: luz amarelada e mais fraca; falhas com bateria no fim ou interferência.
		K *= FMath::Lerp(0.3f, 1.f, FMath::Clamp(Battery / 0.25f, 0.f, 1.f));
		const float FlickerRate = (Battery < 0.12f ? 0.6f : 0.f) + Interference * 2.5f;
		if (Now >= FlickerUntil && FMath::FRand() < FlickerRate * DeltaSeconds)
		{
			FlickerUntil = Now + FMath::FRandRange(0.08f, 0.5f);
		}
		if (Now < FlickerUntil)
		{
			K *= FMath::FRand() < 0.5f ? 0.03f : 0.5f;
		}
	}
	Flashlight->SetIntensity(FlashlightCandelas * K);
	Flashlight->SetVisibility(K > 0.001f);
	Interference = FMath::Max(0.f, Interference - DeltaSeconds * 0.4f);
}

void AKaelCharacter::AddFear(float Amount)
{
	Fear = FMath::Clamp(Fear + Amount, 0.f, 1.f);
}

void AKaelCharacter::UpdateFear(float DeltaSeconds)
{
	FearScanTimer -= DeltaSeconds;
	if (FearScanTimer <= 0.f)
	{
		FearScanTimer = 0.25f;
		float Closest = TNumericLimits<float>::Max();
		float BossDist = TNumericLimits<float>::Max();
		bool bHunted = false;
		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsDead())
			{
				continue;
			}
			const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
			if (It->IsBoss())
			{
				if (It->IsActive())
				{
					BossDist = FMath::Min(BossDist, D);
				}
				continue;
			}
			if (!It->IsActive())
			{
				continue; // os "cadáveres" no chão ainda não assustam... ainda.
			}
			Closest = FMath::Min(Closest, D);
			if (It->GetAlert() == EEnemyAlert::Combate && D < 2500.f)
			{
				bHunted = true;
			}
		}

		float Target = 0.f;
		if (Closest < 1600.f)
		{
			Target = FMath::Max(Target, (1.f - Closest / 1600.f) * 0.85f);
		}
		if (bHunted)
		{
			Target = FMath::Max(Target, 0.55f);
		}
		if (BossDist < 4000.f)
		{
			Target = FMath::Max(Target, 0.7f + 0.3f * (1.f - BossDist / 4000.f));
			// O Hospedeiro interfere em tudo que é elétrico.
			SetFlashlightInterference(0.6f * FMath::Clamp(1.f - BossDist / 3000.f, 0.f, 1.f));
		}
		if (!IsFlashlightOn())
		{
			Target += 0.15f;
		}
		Target += (1.f - Health->GetFraction()) * 0.35f;
		FearTarget = FMath::Clamp(Target, 0.f, 1.f);
	}
	Fear = Fear < FearTarget
		? FMath::FInterpConstantTo(Fear, FearTarget, DeltaSeconds, 1.2f)
		: FMath::FInterpConstantTo(Fear, FearTarget, DeltaSeconds, 0.12f);
}

void AKaelCharacter::UpdateCamera(float DeltaSeconds)
{
	const float Speed = GetVelocity().Size2D();
	const bool bMoving = Speed > 50.f;
	const bool bRunning = bSprinting && !bExhausted && bMoving && !bAiming;

	float TargetLength, TargetFov;
	FVector TargetSocket, TargetOffset;
	if (bFirstPerson)
	{
		TargetLength = 0.f;
		TargetSocket = FVector::ZeroVector;
		TargetOffset = FVector(0.f, 0.f, 68.f);
		TargetFov = bAiming ? 62.f : (bRunning ? 86.f : 80.f);
	}
	else
	{
		TargetLength = bAiming ? 105.f : (bRunning ? 200.f : 165.f);
		TargetSocket = bAiming ? FVector(0.f, 48.f, 38.f) : FVector(0.f, 58.f, 42.f);
		TargetOffset = FVector::ZeroVector;
		TargetFov = bAiming ? 58.f : (bRunning ? 80.f : 74.f);
	}
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetLength, DeltaSeconds, 10.f);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetSocket, DeltaSeconds, 10.f);
	CameraBoom->TargetOffset = FMath::VInterpTo(CameraBoom->TargetOffset, TargetOffset, DeltaSeconds, 12.f);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, TargetFov, DeltaSeconds, 8.f));

	// Balanço da caminhada: mais forte em 1ª pessoa e correndo.
	if (bMoving && !GetCharacterMovement()->IsFalling())
	{
		BobPhase += DeltaSeconds * (4.f + Speed * 0.018f);
	}
	float BobAmp = (bFirstPerson ? 1.6f : 0.7f) * FMath::Clamp(Speed / 330.f, 0.f, 1.6f) * (bIsCrouched ? 0.5f : 1.f);
	if (bDead)
	{
		BobAmp = 0.f;
	}
	const FVector Bob(0.f, FMath::Cos(BobPhase) * BobAmp * 0.8f, -FMath::Abs(FMath::Sin(BobPhase)) * BobAmp * 1.4f);

	// Câmera "na mão": respiração, medo e cansaço deixam a imagem instável.
	const float Now = GetWorld()->GetTimeSeconds();
	const float Hand = 0.25f + Fear * 0.9f + (1.f - Stamina) * 0.8f;
	const FRotator HandRot(
		FMath::PerlinNoise1D(Now * 0.6f) * Hand,
		FMath::PerlinNoise1D(Now * 0.5f + 20.f) * Hand,
		FMath::PerlinNoise1D(Now * 0.4f + 40.f) * Hand * 0.6f);

	// Tremor por "trauma" (golpes, explosões, rugidos).
	Trauma = FMath::Max(0.f, Trauma - DeltaSeconds * 0.9f);
	const float Shake = Trauma * Trauma;
	const float T = Now * 25.f;
	const FRotator ShakeRot(
		FMath::PerlinNoise1D(T) * 4.f * Shake,
		FMath::PerlinNoise1D(T + 31.7f) * 4.f * Shake,
		FMath::PerlinNoise1D(T + 77.1f) * 6.f * Shake);
	const FVector ShakeLoc(0.f, FMath::PerlinNoise1D(T + 11.3f) * 8.f * Shake, FMath::PerlinNoise1D(T + 51.9f) * 8.f * Shake);

	FollowCamera->SetRelativeRotation(HandRot + ShakeRot);
	FollowCamera->SetRelativeLocation(Bob + ShakeLoc);

	DamageFlash = FMath::FInterpTo(DamageFlash, 0.f, DeltaSeconds, 2.5f);
}

void AKaelCharacter::UpdateAimSway(float DeltaSeconds)
{
	// Mãos trêmulas: a mira oscila de verdade (não só a câmera) com medo, cansaço e ferimentos.
	const float LowHealth = FMath::Clamp((0.45f - Health->GetFraction()) / 0.45f, 0.f, 1.f);
	const float TargetAmp = bAiming ? (0.15f + Fear * 0.9f + (1.f - Stamina) * 0.6f + LowHealth * 0.5f) : 0.f;
	SwayAmp = FMath::FInterpTo(SwayAmp, TargetAmp, DeltaSeconds, 4.f);

	const float Now = GetWorld()->GetTimeSeconds();
	const FRotator Target(FMath::PerlinNoise1D(Now * 0.9f + 5.f) * SwayAmp, FMath::PerlinNoise1D(Now * 0.8f + 50.f) * SwayAmp, 0.f);
	const FRotator Delta = Target - AppliedSway;
	AppliedSway = Target;
	if (Controller && !bInputLocked && !bDead)
	{
		FRotator Control = Controller->GetControlRotation();
		Control.Pitch += Delta.Pitch;
		Control.Yaw += Delta.Yaw;
		Controller->SetControlRotation(Control);
	}
}

void AKaelCharacter::UpdatePostProcess(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float LowHealth = bDead ? 1.f : FMath::Clamp((0.45f - Health->GetFraction()) / 0.45f, 0.f, 1.f);
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 6.f);

	FPostProcessSettings& P = FollowCamera->PostProcessSettings;
	P.bOverride_VignetteIntensity = true;
	P.VignetteIntensity = 0.5f + Fear * 0.45f + LowHealth * 0.5f * Pulse;
	P.bOverride_SceneFringeIntensity = true;
	P.SceneFringeIntensity = 0.3f + Fear * 2.2f + DamageFlash * 4.f;
	const float Sat = FMath::Clamp(0.82f - Fear * 0.3f - LowHealth * 0.45f, 0.15f, 1.f);
	P.bOverride_ColorSaturation = true;
	P.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	P.bOverride_FilmGrainIntensity = true;
	P.FilmGrainIntensity = 0.32f + Fear * 0.25f;
	const float Red = FMath::Clamp(LowHealth * 0.35f + DamageFlash * 0.35f, 0.f, 0.7f);
	P.bOverride_SceneColorTint = true;
	P.SceneColorTint = FLinearColor(1.f, 1.f - Red, 1.f - Red, 1.f);
}

float AKaelCharacter::GetCurrentSpreadDeg() const
{
	const bool bMoving = GetVelocity().Size2D() > 60.f;
	return Weapon->GetSpread(bAiming, bMoving) * (bIsCrouched ? 0.8f : 1.f) + Fear * 0.8f;
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

	LastShotTime = GetWorld()->GetTimeSeconds();
	Body.Kick = 1.f;

	// Recuo aplicado direto na rotação de controle (graus).
	FRotator ControlRot = Controller->GetControlRotation();
	const float K = bAiming ? 0.6f : 1.f;
	ControlRot.Pitch += Def.RecoilPitch * K;
	ControlRot.Yaw += FMath::FRandRange(-Def.RecoilYaw, Def.RecoilYaw) * K;
	Controller->SetControlRotation(ControlRot);
	AddTrauma(Def.Pellets > 1 ? 0.22f : 0.06f);

	if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		++GI->ShotsFired;
		if (Result.bHitPawn)
		{
			++GI->ShotsHit;
		}
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

void AKaelCharacter::UpdateFootsteps(float DeltaSeconds)
{
	const float Speed = GetVelocity().Size2D();
	if (bDead || GetCharacterMovement()->IsFalling() || Speed < 120.f)
	{
		return;
	}
	FootstepTimer -= DeltaSeconds * (Speed / 330.f);
	if (FootstepTimer > 0.f)
	{
		return;
	}
	FootstepTimer = 0.52f;

	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	RupturaAssets::PlaySound3D(this, TEXT("passo"), Feet, bIsCrouched ? 0.12f : (Speed > 450.f ? 0.5f : 0.3f), FMath::FRandRange(0.88f, 1.1f));

	// Passos são ouvidos pelos infectados (correndo = bem mais longe; agachado = silencioso).
	if (!bIsCrouched)
	{
		const bool bRunning = Speed > 450.f;
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), Feet, bRunning ? 0.7f : 0.3f, this, bRunning ? 1400.f : 420.f, TEXT("Passos"));
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
		DodgeSpin = FMath::Min(1.f, DodgeSpin + DeltaSeconds / 0.35f);
		ScaleZ *= FMath::Lerp(0.8f, 1.f, FMath::Abs(DodgeSpin * 2.f - 1.f));
		RootRot.Roll = -12.f * FMath::Sin(DodgeSpin * PI);
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

	// Golpe de faca: o braço direito desce em arco.
	const float MeleeAge = Now - MeleeTime;
	if (MeleeAge < 0.35f && Body.ShoulderR)
	{
		const float A = MeleeAge / 0.35f;
		Body.ShoulderR->SetRelativeRotation(FRotator(FMath::Lerp(150.f, 25.f, A), 0.f, -20.f));
	}
}

void AKaelCharacter::UpdateHeartAndBreath(float DeltaSeconds)
{
	const float LowHealth = FMath::Clamp((0.45f - Health->GetFraction()) / 0.45f, 0.f, 1.f);
	const float Intensity = FMath::Max(Fear, LowHealth);
	if (!bDead && Intensity > 0.3f)
	{
		HeartTimer -= DeltaSeconds;
		if (HeartTimer <= 0.f)
		{
			HeartTimer = FMath::Lerp(1.05f, 0.38f, Intensity);
			RupturaAssets::PlaySound2D(this, TEXT("batimento"), 0.25f + 0.6f * Intensity);
		}
	}
	else
	{
		HeartTimer = 0.2f;
	}

	if (BreathLoop)
	{
		const float Breath = bDead ? 0.f : FMath::Clamp(FMath::Max((1.f - Stamina) * 1.1f - 0.1f, Fear * 0.55f), 0.f, 1.f);
		BreathLoop->SetVolumeMultiplier(FMath::Max(0.01f, Breath * 0.8f));
		BreathLoop->SetPitchMultiplier(1.f + 0.15f * Fear);
	}
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
	AddTrauma(FMath::Clamp(-Delta / 30.f, 0.2f, 0.7f));
	AddFear(0.25f);
	DamageFlash = FMath::Min(1.f, DamageFlash + 0.5f - Delta / 40.f);
	LastDamageTime = Now;
	StaggerUntil = Now + 0.35f;
	Body.Kick += 0.6f;
	Weapon->CancelReload(); // o golpe atrapalha a recarga
	RupturaAssets::PlaySound2D(this, TEXT("dano"), 0.9f, FMath::FRandRange(0.9f, 1.1f));
}

void AKaelCharacter::HandleDeath(UHealthComponent* Component, AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	DeathTime = GetWorld()->GetTimeSeconds();
	SetInputLocked(true);
	GetCharacterMovement()->DisableMovement();
	if (bFirstPerson)
	{
		SetFirstPerson(false); // mostra o corpo caindo
	}
	if (BreathLoop)
	{
		BreathLoop->FadeOut(1.f, 0.f);
	}
	RupturaAssets::PlaySound2D(this, TEXT("morte_kael"), 1.f);

	if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		++GI->Deaths;
	}
	if (AKaelPlayerController* PC = Cast<AKaelPlayerController>(GetController()))
	{
		PC->OnKaelDied();
	}
}
