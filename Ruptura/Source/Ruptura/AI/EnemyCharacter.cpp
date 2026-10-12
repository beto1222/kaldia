#include "AI/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "Combat/HealthComponent.h"
#include "Mission/MissionDirector.h"
#include "Mission/Pickup.h"
#include "Core/RupturaAssets.h"
#include "Core/RupturaGameInstance.h"
#include "Core/FxActor.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(38.f, 92.f);

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->bUseControllerDesiredRotation = false;
	Move->RotationRate = FRotator(0.f, 420.f, 0.f);
	Move->MaxWalkSpeed = 110.f;
	Move->BrakingDecelerationWalking = 900.f;
	Move->bUseRVOAvoidance = true;
	Move->AvoidanceConsiderationRadius = 250.f;

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	BodyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyRoot"));
	BodyRoot->SetupAttachment(RootComponent);
	BodyRoot->SetRelativeLocation(FVector(0.f, 0.f, -92.f));

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
}

float AEnemyCharacter::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AEnemyCharacter::Setup(FName InGroup, const TArray<FVector>& InPatrolPoints, bool bInElite, bool bInDormant)
{
	Group = InGroup;
	PatrolPoints = InPatrolPoints;
	bDormant = bInDormant;
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
	NextVoice = Now() + FMath::FRandRange(1.f, 8.f);
}

void AEnemyCharacter::BuildBody()
{
	FRandomStream Rng(FMath::Rand());
	const FInfectedLook Look = InfectedBody::RandomLook(Rng, bElite);
	InfectedBody::Build(this, BodyRoot, Look, bElite ? 1.12f : FMath::FRandRange(0.94f, 1.04f), Flesh);
}

void AEnemyCharacter::ApplyDifficulty(ERupturaDifficulty Difficulty)
{
	Tuning = FRupturaTuning::ForDifficulty(Difficulty, bElite);

	if (HasActorBegunPlay() && !Health->IsDead())
	{
		const float Fraction = Health->GetFraction();
		Health->SetMaxHealth(Tuning.MaxHealth, false);
		Health->Health = FMath::Max(1.f, Health->MaxHealth * Fraction);
	}
}

float AEnemyCharacter::GetDamageMultiplierForHit(const FHitResult& Hit, const FVector& ShotDir) const
{
	if (bDormant || IsRising())
	{
		return 1.5f; // caído no chão: o corpo todo fica exposto
	}
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Top = GetActorLocation().Z + Half;
	const float Z = Hit.ImpactPoint.Z;
	if (Z > Top - 34.f)
	{
		return bElite ? 2.2f : 3.f; // tiro na cabeça
	}
	if (Z < Top - Half * 2.f * 0.55f)
	{
		return 0.75f; // pernas
	}
	return 1.f;
}

bool AEnemyCharacter::IsDead() const
{
	return bDeadFlag || (Health && Health->IsDead());
}

bool AEnemyCharacter::IsRising() const
{
	return Now() - WakeTime < RiseDuration;
}

bool AEnemyCharacter::IsActive() const
{
	return !bDormant && !IsRising() && !IsDead();
}

bool AEnemyCharacter::IsAttacking() const
{
	return Now() - AttackStart < AttackDuration;
}

bool AEnemyCharacter::IsStaggered() const
{
	return Now() < StaggerUntil;
}

bool AEnemyCharacter::CanAttack() const
{
	return IsActive() && Now() >= NextAttackTime && !IsAttacking() && !IsStaggered();
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

// ---------------------------------------------------------------------------------------------
// Golpe corpo a corpo
// ---------------------------------------------------------------------------------------------

bool AEnemyCharacter::StartMeleeAttack(AActor* Target)
{
	if (!Target || !CanAttack())
	{
		return false;
	}
	AttackTarget = Target;
	AttackStart = Now();
	AttackDuration = Tuning.AttackWindup + 0.6f;
	NextAttackTime = AttackStart + Tuning.AttackInterval;
	bAttackResolved = false;

	const FVector To = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	}
	RupturaAssets::PlaySound3D(this, TEXT("infectado_ataque"), GetActorLocation() + FVector(0.f, 0.f, 70.f), 1.f, FMath::FRandRange(0.9f, 1.1f) * (bElite ? 0.85f : 1.f));
	return true;
}

void AEnemyCharacter::UpdateMelee(float DeltaSeconds)
{
	if (bAttackResolved || Now() - AttackStart < Tuning.AttackWindup)
	{
		return;
	}
	bAttackResolved = true;

	AActor* Target = AttackTarget.Get();
	if (!Target || IsDead())
	{
		return;
	}

	// Bote para frente no momento do golpe.
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	LaunchCharacter(Forward * 380.f, true, false);

	const FVector Delta = Target->GetActorLocation() - GetActorLocation();
	const float Dot = FVector::DotProduct(Forward, Delta.GetSafeNormal2D());
	if (Delta.Size2D() <= AttackRange + 60.f && Dot > 0.35f && FMath::Abs(Delta.Z) < 120.f)
	{
		UGameplayStatics::ApplyDamage(Target, Tuning.MeleeDamage, GetController(), this, UDamageType::StaticClass());
		RupturaFx::Blood(GetWorld(), Target->GetActorLocation() + FVector(0.f, 0.f, 40.f), -Delta.GetSafeNormal(), 5);
		RupturaAssets::PlaySound3D(this, TEXT("infectado_mordida"), Target->GetActorLocation(), 1.f, FMath::FRandRange(0.9f, 1.1f));
	}
}

void AEnemyCharacter::Shove(const FVector& Direction, float Strength)
{
	if (IsDead())
	{
		return;
	}
	LaunchCharacter(Direction.GetSafeNormal2D() * Strength + FVector(0.f, 0.f, 60.f), true, false);
	StaggerUntil = FMath::Max(StaggerUntil, Now() + 0.8f);
	if (!bAttackResolved)
	{
		bAttackResolved = true;
		AttackStart = -10.f; // golpe interrompido
	}
}

// ---------------------------------------------------------------------------------------------
// Dormente: parece um cadáver até o jogador chegar perto
// ---------------------------------------------------------------------------------------------

void AEnemyCharacter::Wake()
{
	if (!bDormant || IsDead())
	{
		return;
	}
	bDormant = false;
	WakeTime = Now();
	bWakeAlertSent = false;
	RupturaAssets::PlaySound3D(this, TEXT("infectado_levantar"), GetActorLocation(), 1.f, FMath::FRandRange(0.9f, 1.1f));
}

void AEnemyCharacter::UpdateDormant(float DeltaSeconds)
{
	if (bDormant)
	{
		const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < WakeRadius)
		{
			Wake();
		}
		return;
	}
	if (!bWakeAlertSent && !IsRising())
	{
		bWakeAlertSent = true;
		if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
		{
			if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				AI->AlertTo(Player->GetActorLocation(), true);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Voz: gemidos ao vagar, rosnados ofegantes ao perseguir
// ---------------------------------------------------------------------------------------------

void AEnemyCharacter::UpdateVoice(float DeltaSeconds)
{
	if (bDormant || Now() < NextVoice)
	{
		return;
	}
	const bool bChasing = GetAlert() == EEnemyAlert::Combate;
	NextVoice = Now() + (bChasing ? FMath::FRandRange(1.8f, 3.8f) : FMath::FRandRange(5.f, 12.f));
	const FVector Mouth = GetActorLocation() + FVector(0.f, 0.f, 70.f);
	const float Pitch = FMath::FRandRange(0.85f, 1.15f) * (bElite ? 0.75f : 1.f);
	RupturaAssets::PlaySound3D(this, bChasing ? FName(TEXT("infectado_rosnado")) : FName(TEXT("infectado_gemido")), Mouth, bChasing ? 0.9f : 0.6f, Pitch);
}

// ---------------------------------------------------------------------------------------------
// Dano e morte
// ---------------------------------------------------------------------------------------------

void AEnemyCharacter::HandleHealthChanged(UHealthComponent* Component, float NewHealth, float Delta, AActor* InstigatorActor)
{
	if (Delta < 0.f)
	{
		OnDamaged(-Delta, InstigatorActor);
	}
}

void AEnemyCharacter::OnDamaged(float Amount, AActor* InstigatorActor)
{
	const float T = Now();
	FlinchTime = T;

	// Atordoamento: dano grande concentrado (tiro de pistola, rajada de chumbos) interrompe o golpe.
	if (T - LastDamageAt > 0.15f)
	{
		RecentDamage = 0.f;
	}
	RecentDamage += Amount;
	LastDamageAt = T;
	if (RecentDamage >= 22.f * Tuning.StaggerResist)
	{
		RecentDamage = 0.f;
		StaggerUntil = FMath::Max(StaggerUntil, T + 0.55f);
		if (!bAttackResolved)
		{
			bAttackResolved = true;
			AttackStart = -10.f;
		}
	}

	if (bDormant)
	{
		Wake();
	}
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
	bDiedLying = bDormant || IsRising();
	bDeadFlag = true;
	DeathTime = Now();
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

	RupturaAssets::PlaySound3D(this, TEXT("infectado_morte"), GetActorLocation() + FVector(0.f, 0.f, 60.f), 0.9f, FMath::FRandRange(0.9f, 1.1f));
	RupturaFx::BloodPool(GetWorld(), GetActorLocation(), bElite ? 170.f : 120.f);

	if (AMissionDirector* Director = AMissionDirector::Get(this))
	{
		Director->OnEnemyKilled(this, Killer);
	}

	// Às vezes o infectado ainda carrega algo útil de quando era gente.
	const float Roll = FMath::FRand();
	if (Roll < 0.45f)
	{
		const bool bBattery = Roll < 0.13f;
		const FVector DropLoc = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APickup* Drop = GetWorld()->SpawnActor<APickup>(APickup::StaticClass(), DropLoc, FRotator::ZeroRotator, Params))
		{
			Drop->Configure(bBattery ? EPickupType::Bateria : EPickupType::Municao, bBattery ? 25 : 6, 0.f);
			Drop->SetLifeSpan(90.f);
		}
	}

	SetLifeSpan(60.f);
}

// ---------------------------------------------------------------------------------------------
// Animação
// ---------------------------------------------------------------------------------------------

void AEnemyCharacter::AnimateBody(float DeltaSeconds)
{
	const float T = Now();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	FRotator RootRot = FRotator::ZeroRotator;
	FVector RootLoc(0.f, 0.f, -HalfHeight);

	// Deitado de bruços (dormente), levantando aos trancos, ou caindo morto.
	float Lying = 0.f;
	if (bDeadFlag)
	{
		if (bDiedLying)
		{
			Lying = 1.f;
		}
		else
		{
			const float A = FMath::Clamp((T - DeathTime) / 0.6f, 0.f, 1.f);
			const float Ease = A * A;
			RootRot.Pitch = -85.f * Ease * DeathFallDir.X;
			RootRot.Roll = 85.f * Ease * DeathFallDir.Y;
		}
	}
	else if (bDormant)
	{
		Lying = 1.f;
	}
	else if (IsRising())
	{
		const float A = FMath::Clamp((T - WakeTime) / RiseDuration, 0.f, 1.f);
		Lying = FMath::Clamp(1.f - FMath::SmoothStep(0.f, 1.f, A) + 0.07f * FMath::Sin(A * 40.f) * (1.f - A), 0.f, 1.f);
	}
	if (Lying > 0.f)
	{
		// O corpo deitado fica centrado na cápsula para que tiros nele acertem.
		RootRot.Pitch = -86.f * Lying;
		RootLoc.X = -90.f * Lying;
	}

	BodyRoot->SetRelativeLocation(RootLoc);
	BodyRoot->SetRelativeRotation(RootRot);

	// Morto há mais de um segundo: o corpo fica imóvel.
	if (bDeadFlag && T - DeathTime > 1.f)
	{
		return;
	}

	FInfectedPose Pose;
	Pose.Speed = bDeadFlag ? 0.f : GetVelocity().Size2D();
	Pose.bChasing = !bDeadFlag && (GetAlert() == EEnemyAlert::Combate || IsRising());
	Pose.Attack = IsAttacking() ? FMath::Clamp((T - AttackStart) / AttackDuration, 0.f, 1.f) : -1.f;
	const float FlinchAge = T - FlinchTime;
	Pose.Flinch = (FlinchAge < 0.3f) ? 1.f - FlinchAge / 0.3f : 0.f;
	if (IsStaggered())
	{
		Pose.Flinch = FMath::Max(Pose.Flinch, 0.6f);
	}
	InfectedBody::Animate(Flesh, Pose, DeltaSeconds);
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBodyBuilt)
	{
		return;
	}
	if (!bDeadFlag)
	{
		UpdateDormant(DeltaSeconds);
		UpdateMelee(DeltaSeconds);
		UpdateVoice(DeltaSeconds);
	}
	AnimateBody(DeltaSeconds);
}
