#include "AI/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Mission/MissionDirector.h"
#include "Mission/Pickup.h"
#include "Core/RupturaAssets.h"
#include "Core/RupturaGameInstance.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(38.f, 92.f);

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->bUseControllerDesiredRotation = false;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->MaxWalkSpeed = 200.f;
	Move->MaxWalkSpeedCrouched = 180.f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(58.f);
	Move->bUseRVOAvoidance = true;
	Move->AvoidanceConsiderationRadius = 300.f;

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	BodyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyRoot"));
	BodyRoot->SetupAttachment(RootComponent);
	BodyRoot->SetRelativeLocation(FVector(0.f, 0.f, -92.f));

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Weapon = CreateDefaultSubobject<UWeaponComponent>(TEXT("Weapon"));
}

void AEnemyCharacter::Setup(FName InGroup, const TArray<FVector>& InPatrolPoints, bool bInElite)
{
	Group = InGroup;
	PatrolPoints = InPatrolPoints;
	if (bElite != bInElite)
	{
		bElite = bInElite;
		if (const URupturaGameInstance* GI = URupturaGameInstance::Get(this))
		{
			ApplyDifficulty(GI->Difficulty);
		}
	}
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	StandingHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	ERupturaDifficulty Difficulty = ERupturaDifficulty::Normal;
	if (const URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		Difficulty = GI->Difficulty;
	}
	ApplyDifficulty(Difficulty);
	Health->SetMaxHealth(Tuning.MaxHealth, true);

	Health->OnHealthChanged.AddDynamic(this, &AEnemyCharacter::HandleHealthChanged);
	Health->OnDeath.AddDynamic(this, &AEnemyCharacter::HandleDeath);

	BuildBody();
	bBodyBuilt = true;
}

void AEnemyCharacter::BuildBody()
{
	FSoldierColors Colors;
	Colors.Armor = bElite ? FLinearColor(0.18f, 0.03f, 0.02f) : FLinearColor(0.035f, 0.035f, 0.04f);
	Colors.Suit = FLinearColor(0.12f, 0.06f, 0.05f);
	Colors.Visor = bElite ? FLinearColor(1.f, 0.45f, 0.05f) : FLinearColor(1.f, 0.08f, 0.05f);
	Colors.VisorGlow = 12.f;
	SoldierBody::Build(this, BodyRoot, Colors, bElite ? 1.05f : 1.f, Body);
	SoldierBody::SetGunLength(Body, 60.f);
}

void AEnemyCharacter::ApplyDifficulty(ERupturaDifficulty Difficulty)
{
	Tuning = FRupturaTuning::ForDifficulty(Difficulty, bElite);
	Weapon->InitEnemyLoadout(Tuning.ShotDamage, Tuning.FireInterval);

	if (HasActorBegunPlay() && !Health->IsDead())
	{
		const float Fraction = Health->GetFraction();
		Health->SetMaxHealth(Tuning.MaxHealth, false);
		Health->Health = FMath::Max(1.f, Health->MaxHealth * Fraction);
	}
}

float AEnemyCharacter::GetDamageMultiplierForHit(const FHitResult& Hit, const FVector& ShotDir) const
{
	const float Top = GetActorLocation().Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const bool bHead = Hit.ImpactPoint.Z > Top - 28.f;
	if (bHead)
	{
		return bElite ? 2.f : 2.5f;
	}
	return 1.f;
}

bool AEnemyCharacter::IsDead() const
{
	return bDeadFlag || (Health && Health->IsDead());
}

EEnemyAlert AEnemyCharacter::GetAlert() const
{
	if (IsDead())
	{
		return EEnemyAlert::Morto;
	}
	if (const AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		return AI->GetAlert();
	}
	return EEnemyAlert::Calmo;
}

float AEnemyCharacter::GetDetection() const
{
	if (const AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		return AI->GetDetection();
	}
	return 0.f;
}

FVector AEnemyCharacter::GetEyeLocationStanding() const
{
	const float Feet = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return FVector(GetActorLocation().X, GetActorLocation().Y, Feet + StandingHalfHeight * 2.f * 0.87f);
}

FVector AEnemyCharacter::GetMuzzleLocation() const
{
	return Body.Muzzle ? Body.Muzzle->GetComponentLocation() : GetActorLocation() + GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 40.f);
}

bool AEnemyCharacter::FireAt(const FVector& Target, float ExtraSpreadDeg)
{
	if (IsDead() || bIsCrouched)
	{
		return false;
	}
	const FVector Eye = GetEyeLocationStanding();
	const FVector Dir = (Target - Eye).GetSafeNormal();

	// Só atira se estiver mais ou menos virado para o alvo.
	if (FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), Dir.GetSafeNormal2D()) < 0.7f)
	{
		return false;
	}

	FShotResult Result;
	const bool bFired = Weapon->FireShot(Eye, Dir, GetMuzzleLocation(), Tuning.SpreadDeg + ExtraSpreadDeg, Result);
	if (bFired)
	{
		Body.Kick = 1.f;
	}
	return bFired;
}

void AEnemyCharacter::SetCombatStance(bool bCombat)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = !bCombat;
	Move->bUseControllerDesiredRotation = bCombat;
}

void AEnemyCharacter::SetMoveSpeed(float Speed)
{
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void AEnemyCharacter::HandleHealthChanged(UHealthComponent* Component, float NewHealth, float Delta, AActor* InstigatorActor)
{
	if (Delta < 0.f)
	{
		OnDamaged(-Delta, InstigatorActor);
	}
}

void AEnemyCharacter::OnDamaged(float Amount, AActor* InstigatorActor)
{
	FlinchTime = GetWorld()->GetTimeSeconds();
	Body.Kick += 0.8f;
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		AI->NotifyDamaged(InstigatorActor);
	}
}

void AEnemyCharacter::HandleDeath(UHealthComponent* Component, AActor* Killer)
{
	if (bDeadFlag)
	{
		return;
	}
	bDeadFlag = true;
	DeathTime = GetWorld()->GetTimeSeconds();
	OnDied(Killer);
}

void AEnemyCharacter::OnDied(AActor* Killer)
{
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		AI->NotifyDeath();
	}

	// Direção da queda: para longe de quem matou.
	if (Killer)
	{
		const FVector Away = (GetActorLocation() - Killer->GetActorLocation()).GetSafeNormal2D();
		const FVector LocalAway = GetActorTransform().InverseTransformVectorNoScale(Away);
		DeathFallDir = FVector2D(LocalAway.X, LocalAway.Y).GetSafeNormal();
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetachFromControllerPendingDestroy();

	RupturaAssets::PlaySound3D(this, TEXT("morte_inimigo"), GetActorLocation(), 0.9f, FMath::FRandRange(0.9f, 1.1f));

	if (AMissionDirector* Director = AMissionDirector::Get(this))
	{
		Director->OnEnemyKilled(this, Killer);
	}

	// Chance de deixar munição para trás.
	if (FMath::FRand() < 0.6f)
	{
		const FVector DropLoc = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 20.f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APickup* Drop = GetWorld()->SpawnActor<APickup>(APickup::StaticClass(), DropLoc, FRotator::ZeroRotator, Params))
		{
			Drop->Configure(EPickupType::Municao, 30, 0.f);
			Drop->SetLifeSpan(60.f);
		}
	}

	SetLifeSpan(30.f);
}

void AEnemyCharacter::AnimateBody(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	FRotator RootRot = FRotator::ZeroRotator;
	float ScaleZ = HalfHeight / StandingHalfHeight;

	const float FlinchAge = Now - FlinchTime;
	if (FlinchAge < 0.25f)
	{
		RootRot.Pitch = 8.f * (1.f - FlinchAge / 0.25f);
	}
	if (bDeadFlag)
	{
		const float A = FMath::Clamp((Now - DeathTime) / 0.6f, 0.f, 1.f);
		const float Ease = A * A;
		RootRot.Pitch = -85.f * Ease * DeathFallDir.X;
		RootRot.Roll = 85.f * Ease * DeathFallDir.Y;
		ScaleZ = 1.f;
	}

	BodyRoot->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
	BodyRoot->SetRelativeRotation(RootRot);
	BodyRoot->SetRelativeScale3D(FVector(1.f, 1.f, ScaleZ));

	const bool bCombat = GetAlert() == EEnemyAlert::Combate;
	SoldierBody::Animate(Body, bDeadFlag ? 0.f : GetVelocity().Size2D(), DeltaSeconds, bCombat);
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bBodyBuilt)
	{
		AnimateBody(DeltaSeconds);
	}
}
