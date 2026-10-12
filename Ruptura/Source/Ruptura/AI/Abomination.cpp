#include "AI/Abomination.h"
#include "Combat/HealthComponent.h"
#include "Combat/Grenade.h"
#include "Player/KaelCharacter.h"
#include "Mission/MissionDirector.h"
#include "World/LevelLayout.h"
#include "Core/FxActor.h"
#include "Core/RupturaAssets.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

using RupturaAssets::EShape;

namespace
{
	constexpr float WalkSpeed = 250.f;
	constexpr float EnragedWalkSpeed = 330.f;

	FRotator Approach(const FRotator& Current, const FRotator& Target, float DeltaSeconds, float Speed)
	{
		return FMath::RInterpTo(Current, Target, DeltaSeconds, Speed);
	}
}

AAbomination::AAbomination()
{
	GetCapsuleComponent()->InitCapsuleSize(85.f, 175.f);
	BodyRoot->SetRelativeLocation(FVector(0.f, 0.f, -175.f));

	AIControllerClass = AAbominationAIController::StaticClass();

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxAcceleration = 1500.f;
	Move->RotationRate = FRotator(0.f, 150.f, 0.f);
	Move->bUseRVOAvoidance = false;
	Move->Mass = 3000.f;

	AttackRange = 380.f;
}

void AAbomination::ApplyDifficulty(ERupturaDifficulty Difficulty)
{
	Tuning = FRupturaTuning::ForDifficulty(Difficulty, false);
	switch (Difficulty)
	{
	case ERupturaDifficulty::Facil: Tuning.MaxHealth = 1400.f; Tuning.MeleeDamage = 26.f; break;
	case ERupturaDifficulty::Dificil: Tuning.MaxHealth = 2600.f; Tuning.MeleeDamage = 45.f; break;
	default: Tuning.MaxHealth = 2000.f; Tuning.MeleeDamage = 34.f; break;
	}
	Tuning.StaggerResist = 1000.f; // tiros não interrompem o Hospedeiro

	if (HasActorBegunPlay() && !Health->IsDead())
	{
		const float Fraction = Health->GetFraction();
		Health->SetMaxHealth(Tuning.MaxHealth, false);
		Health->Health = FMath::Max(1.f, Health->MaxHealth * Fraction);
	}
}

// ---------------------------------------------------------------------------------------------
// Corpo
// ---------------------------------------------------------------------------------------------

void AAbomination::BuildBody()
{
	UMaterialInterface* FleshM = RupturaAssets::MakeMaterial(this, FLinearColor(0.3f, 0.07f, 0.06f), 0.28f, 0.f, 0.f);
	UMaterialInterface* DarkM = RupturaAssets::MakeMaterial(this, FLinearColor(0.13f, 0.03f, 0.03f), 0.35f, 0.f, 0.f);
	UMaterialInterface* PaleM = RupturaAssets::MakeMaterial(this, FLinearColor(0.32f, 0.29f, 0.26f), 0.55f, 0.f, 0.f);
	UMaterialInterface* BoneM = RupturaAssets::MakeMaterial(this, FLinearColor(0.55f, 0.5f, 0.42f), 0.6f, 0.f, 0.f);
	UMaterialInterface* BloodM = RupturaAssets::MakeMaterial(this, FLinearColor(0.09f, 0.004f, 0.003f), 0.12f, 0.f, 0.f);
	UMaterialInterface* HeartM = RupturaAssets::MakeMaterial(this, FLinearColor(0.6f, 0.03f, 0.02f), 0.2f, 0.f, 4.f);
	UMaterialInterface* EyeM = RupturaAssets::MakeMaterial(this, FLinearColor(0.85f, 0.8f, 0.55f), 0.05f, 0.f, 3.f);

	// Pernas grossas
	for (const float Side : { 1.f, -1.f })
	{
		USceneComponent* Hip = RupturaAssets::AddPivot(this, BodyRoot, FVector(0.f, 55.f * Side, 140.f));
		RupturaAssets::AddPart(this, Hip, EShape::Cube, FVector(0.f, 0.f, -35.f), FVector(60.f, 58.f, 82.f), FleshM);
		RupturaAssets::AddPart(this, Hip, EShape::Cube, FVector(12.f, 0.f, -100.f), FVector(52.f, 50.f, 72.f), DarkM);
		RupturaAssets::AddPart(this, Hip, EShape::Cube, FVector(25.f, 0.f, -133.f), FVector(95.f, 64.f, 16.f), DarkM);
		(Side > 0.f ? HipL : HipR) = Hip;
	}

	// Tronco: massa curvada de carne com costelas expostas e tumores
	Torso = RupturaAssets::AddPivot(this, BodyRoot, FVector(0.f, 0.f, 150.f));
	RupturaAssets::AddPart(this, Torso, EShape::Sphere, FVector(5.f, 0.f, 30.f), FVector(150.f, 170.f, 120.f), FleshM);
	RupturaAssets::AddPart(this, Torso, EShape::Sphere, FVector(35.f, 0.f, 95.f), FVector(160.f, 190.f, 130.f), FleshM);
	RupturaAssets::AddPart(this, Torso, EShape::Sphere, FVector(-35.f, 0.f, 120.f), FVector(120.f, 150.f, 110.f), DarkM);
	RupturaAssets::AddPart(this, Torso, EShape::Cube, FVector(108.f, -32.f, 95.f), FVector(10.f, 50.f, 40.f), PaleM);
	for (int32 i = 0; i < 4; ++i)
	{
		RupturaAssets::AddPart(this, Torso, EShape::Cube, FVector(110.f - i * 3.f, 0.f, 72.f + i * 13.f), FVector(5.f, 92.f - i * 10.f, 4.f), BoneM);
	}
	static const FVector Tumors[] = {
		FVector(20.f, 80.f, 130.f), FVector(-10.f, -85.f, 90.f), FVector(60.f, 60.f, 40.f),
		FVector(-50.f, 50.f, 60.f), FVector(70.f, -70.f, 140.f), FVector(-20.f, -40.f, 170.f),
	};
	for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Tumors); ++i)
	{
		const float Size = 30.f + 5.f * i;
		RupturaAssets::AddPart(this, Torso, EShape::Sphere, Tumors[i], FVector(Size, Size * 1.1f, Size * 0.9f), (i % 2 == 0) ? DarkM : PaleM);
	}
	RupturaAssets::AddPart(this, Torso, EShape::Cube, FVector(112.f, 20.f, 40.f), FVector(4.f, 40.f, 50.f), BloodM);

	// Ponto fraco: coração exposto nas costas, com veias
	Heart = RupturaAssets::AddPart(this, Torso, EShape::Sphere, FVector(-82.f, 0.f, 110.f), FVector(60.f, 58.f, 70.f), HeartM);
	for (int32 i = 0; i < 3; ++i)
	{
		const float A = -40.f + i * 40.f;
		RupturaAssets::AddPart(this, Torso, EShape::Cylinder, FVector(-75.f, FMath::Sin(FMath::DegreesToRadians(A)) * 45.f, 110.f + FMath::Cos(FMath::DegreesToRadians(A)) * 30.f),
			FVector(7.f, 7.f, 90.f), DarkM, FRotator(0.f, 0.f, A));
	}

	// Cabeça afundada à frente do tronco
	Head = RupturaAssets::AddPivot(this, Torso, FVector(110.f, 0.f, 105.f));
	RupturaAssets::AddPart(this, Head, EShape::Sphere, FVector(15.f, 0.f, 0.f), FVector(60.f, 55.f, 62.f), PaleM);
	Mouth = RupturaAssets::AddPivot(this, Head, FVector(30.f, 0.f, -20.f));
	RupturaAssets::AddPart(this, Mouth, EShape::Cube, FVector(10.f, 0.f, -8.f), FVector(40.f, 45.f, 16.f), BloodM);
	for (int32 i = 0; i < 5; ++i)
	{
		RupturaAssets::AddPart(this, Mouth, EShape::Cube, FVector(28.f, -16.f + i * 8.f, 0.f), FVector(4.f, 4.f, 9.f), BoneM);
	}
	for (const float Side : { 1.f, -1.f })
	{
		RupturaAssets::AddPart(this, Head, EShape::Sphere, FVector(40.f, 10.f * Side, 12.f), FVector(6.f, 7.f, 6.f), EyeM);
		RupturaAssets::AddPart(this, Head, EShape::Sphere, FVector(36.f, 21.f * Side, 3.f), FVector(4.f, 5.f, 4.f), EyeM);
	}

	// Braços longos com garras de osso (anda apoiado nos punhos)
	for (const float Side : { 1.f, -1.f })
	{
		USceneComponent* Shoulder = RupturaAssets::AddPivot(this, Torso, FVector(40.f, 105.f * Side, 120.f));
		RupturaAssets::AddPart(this, Shoulder, EShape::Cube, FVector(0.f, 0.f, -60.f), FVector(48.f, 48.f, 120.f), FleshM);
		USceneComponent* Elbow = RupturaAssets::AddPivot(this, Shoulder, FVector(0.f, 0.f, -118.f));
		RupturaAssets::AddPart(this, Elbow, EShape::Cube, FVector(0.f, 0.f, -60.f), FVector(40.f, 40.f, 120.f), DarkM);
		for (int32 k = -1; k <= 1; ++k)
		{
			RupturaAssets::AddPart(this, Elbow, EShape::Cone, FVector(6.f, k * 12.f, -140.f), FVector(12.f, 12.f, 55.f), BoneM, FRotator(180.f, 0.f, 0.f));
		}
		if (Side > 0.f)
		{
			ShoulderL = Shoulder;
			ElbowL = Elbow;
		}
		else
		{
			ShoulderR = Shoulder;
			ElbowR = Elbow;
		}
	}

	HeartLight = NewObject<UPointLightComponent>(this);
	HeartLight->SetMobility(EComponentMobility::Movable);
	HeartLight->SetIntensityUnits(ELightUnits::Candelas);
	HeartLight->SetIntensity(150.f);
	HeartLight->SetLightColor(FLinearColor(1.f, 0.08f, 0.04f));
	HeartLight->SetAttenuationRadius(800.f);
	HeartLight->SetCastShadows(false);
	HeartLight->RegisterComponent();
	HeartLight->AttachToComponent(Torso, FAttachmentTransformRules::KeepRelativeTransform);
	HeartLight->SetRelativeLocation(FVector(-140.f, 0.f, 110.f));
	AddInstanceComponent(HeartLight);

	EyeLight = NewObject<UPointLightComponent>(this);
	EyeLight->SetMobility(EComponentMobility::Movable);
	EyeLight->SetIntensityUnits(ELightUnits::Candelas);
	EyeLight->SetIntensity(0.f);
	EyeLight->SetLightColor(FLinearColor(0.9f, 0.85f, 0.6f));
	EyeLight->SetAttenuationRadius(450.f);
	EyeLight->SetCastShadows(false);
	EyeLight->RegisterComponent();
	EyeLight->AttachToComponent(Head, FAttachmentTransformRules::KeepRelativeTransform);
	EyeLight->SetRelativeLocation(FVector(75.f, 0.f, 5.f));
	AddInstanceComponent(EyeLight);
}

// ---------------------------------------------------------------------------------------------
// Ativação, dano e morte
// ---------------------------------------------------------------------------------------------

void AAbomination::Roar(float Pitch)
{
	RupturaAssets::PlaySound3D(this, TEXT("hospedeiro_rugido"), GetActorLocation() + FVector(0.f, 0.f, 150.f), 1.f, Pitch);
	if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Kael->AddTrauma(FMath::Clamp(0.45f - FVector::Dist(Kael->GetActorLocation(), GetActorLocation()) / 6000.f, 0.f, 0.45f));
		Kael->AddFear(0.4f);
	}
}

void AAbomination::Activate()
{
	if (bActivated || IsDead())
	{
		return;
	}
	bActivated = true;
	if (EyeLight)
	{
		EyeLight->SetIntensity(90.f);
	}
	Roar(0.85f);
	if (AAbominationAIController* AI = Cast<AAbominationAIController>(GetController()))
	{
		AI->ActivateBoss();
	}
}

float AAbomination::GetDamageMultiplierForHit(const FHitResult& Hit, const FVector& ShotDir) const
{
	// Tiro viajando na mesma direção para onde ele olha = acertou pelas costas (coração exposto).
	float Mult = FVector::DotProduct(ShotDir.GetSafeNormal2D(), GetActorForwardVector().GetSafeNormal2D()) > 0.35f ? 2.5f : 0.55f;
	if (IsStunned())
	{
		Mult *= 1.6f; // atordoado depois de bater numa parede: a hora de atacar
	}
	return Mult;
}

void AAbomination::OnDamaged(float Amount, AActor* InstigatorActor)
{
	Super::OnDamaged(Amount, InstigatorActor);

	if (!bActivated)
	{
		Activate();
	}
	if (!bEnraged && Health->GetFraction() < 0.5f && !Health->IsDead())
	{
		bEnraged = true;
		Roar(0.7f);
		RupturaFx::Blood(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 120.f), FVector::UpVector, 20);
		if (AMissionDirector* Director = AMissionDirector::Get(this))
		{
			Director->OnBossEnraged();
		}
	}
}

void AAbomination::OnDied(AActor* Killer)
{
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		AI->NotifyDeath();
	}
	Action = EAction::None;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetachFromControllerPendingDestroy();

	RupturaAssets::PlaySound3D(this, TEXT("hospedeiro_morte"), GetActorLocation() + FVector(0.f, 0.f, 150.f), 1.f);
	if (AMissionDirector* Director = AMissionDirector::Get(this))
	{
		Director->OnEnemyKilled(this, Killer);
	}

	DeathBursts = 6;
	NextDeathBurst = Now() + 0.3f;
	if (EyeLight)
	{
		EyeLight->SetIntensity(0.f);
	}
	SetLifeSpan(0.f); // o cadáver fica no salão
}

void AAbomination::UpdateMelee(float DeltaSeconds)
{
	// Os ataques do Hospedeiro são tratados em UpdateAction.
}

// ---------------------------------------------------------------------------------------------
// Ataques
// ---------------------------------------------------------------------------------------------

float AAbomination::ActionAge() const
{
	return Now() - ActionStart;
}

void AAbomination::SetAction(EAction NewAction, float Duration)
{
	Action = NewAction;
	ActionStart = Now();
	ActionEnd = ActionStart + Duration;
	bActionFired = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (NewAction == EAction::Charging)
	{
		SetCombatStance(false);
		Move->MaxWalkSpeed = bEnraged ? 1250.f : 1050.f;
		Move->MaxAcceleration = 4000.f;
	}
	else
	{
		SetCombatStance(true);
		Move->MaxWalkSpeed = bEnraged ? EnragedWalkSpeed : WalkSpeed;
		Move->MaxAcceleration = 1500.f;
	}
}

void AAbomination::StartSlam()
{
	if (IsDead() || IsBusy())
	{
		return;
	}
	SetAction(EAction::Slam, 1.4f);
	Roar(1.3f);
}

void AAbomination::StartCharge(const AActor* Target)
{
	if (IsDead() || IsBusy() || !Target)
	{
		return;
	}
	ActionTarget = Target;
	ChargeDir = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	SetAction(EAction::ChargeWindup, bEnraged ? 0.7f : 1.f);
	Roar(1.f);
}

void AAbomination::StartAcid(const AActor* Target)
{
	if (IsDead() || IsBusy() || !Target)
	{
		return;
	}
	ActionTarget = Target;
	SetAction(EAction::Acid, 1.4f);
	RupturaAssets::PlaySound3D(this, TEXT("hospedeiro_rugido"), GetActorLocation() + FVector(0.f, 0.f, 150.f), 0.8f, 1.5f);
}

void AAbomination::DoSlamImpact()
{
	UWorld* World = GetWorld();
	const FVector Center = GetActorLocation() + GetActorForwardVector() * 150.f - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 30.f);

	TArray<AActor*> Ignore;
	Ignore.Add(this);
	UGameplayStatics::ApplyRadialDamageWithFalloff(World, Tuning.MeleeDamage, 5.f, Center, 220.f, 520.f, 1.f,
		UDamageType::StaticClass(), Ignore, this, GetController(), ECC_Visibility);

	if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		const FVector Delta = Kael->GetActorLocation() - Center;
		Kael->AddTrauma(FMath::Clamp(1.f - Delta.Size() / 2000.f, 0.f, 0.85f));
		if (Delta.Size() < 600.f)
		{
			Kael->LaunchCharacter(Delta.GetSafeNormal2D() * 900.f + FVector(0.f, 0.f, 350.f), true, true);
		}
	}

	for (int32 i = 0; i < 6; ++i)
	{
		const FVector Ring = FRotator(0.f, i * 60.f, 0.f).Vector() * 180.f;
		RupturaFx::Smoke(World, Center + Ring, 160.f, 1.6f);
	}
	RupturaFx::CeilingDust(World, Center + FVector(0.f, 0.f, 900.f));
	RupturaAssets::PlaySound3D(this, TEXT("hospedeiro_golpe"), Center, 1.f);
}

void AAbomination::FireAcidSalvo()
{
	const AActor* Target = ActionTarget.Get();
	if (!Target || !Mouth || IsDead())
	{
		return;
	}
	UWorld* World = GetWorld();
	const int32 Count = bEnraged ? 5 : 3;
	const FVector TargetFeet = Target->GetActorLocation() - FVector(0.f, 0.f, 80.f);

	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Start = Mouth->GetComponentLocation() + GetActorForwardVector() * 40.f + FMath::VRand() * 10.f;
		FVector Aim = TargetFeet + Target->GetVelocity() * 0.5f;
		if (i > 0)
		{
			Aim += FVector(FMath::FRandRange(-320.f, 320.f), FMath::FRandRange(-320.f, 320.f), 0.f);
		}

		FVector Velocity;
		if (!UGameplayStatics::SuggestProjectileVelocity_CustomArc(this, Velocity, Start, Aim, 0.f, 0.45f))
		{
			Velocity = (Aim - Start).GetSafeNormal() * 1600.f + FVector(0.f, 0.f, 450.f);
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AGrenade* Glob = World->SpawnActor<AGrenade>(AGrenade::StaticClass(), Start, Velocity.Rotation(), Params))
		{
			Glob->BaseDamage = 22.f;
			Glob->MinDamage = 4.f;
			Glob->InnerRadius = 100.f;
			Glob->OuterRadius = 300.f;
			Glob->Launch(Velocity, true);
			Glob->MakeAcid();
		}
	}
	RupturaFx::Blood(World, Mouth->GetComponentLocation(), GetActorForwardVector(), 10);
	RupturaAssets::PlaySound3D(this, TEXT("acido"), Mouth->GetComponentLocation(), 0.8f, 0.7f);
}

void AAbomination::UpdateAction(float DeltaSeconds)
{
	if (Action == EAction::None || IsDead())
	{
		return;
	}
	const float T = Now();
	const float Age = ActionAge();

	switch (Action)
	{
	case EAction::Slam:
		if (!bActionFired && Age >= 0.75f)
		{
			bActionFired = true;
			DoSlamImpact();
		}
		if (T >= ActionEnd)
		{
			SetAction(EAction::Recover, 0.5f);
		}
		break;

	case EAction::ChargeWindup:
		// Acompanha o jogador durante a preparação e trava a direção no fim.
		if (const AActor* Target = ActionTarget.Get())
		{
			const FVector To = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (!To.IsNearlyZero())
			{
				ChargeDir = To;
				SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f), DeltaSeconds, 6.f));
			}
		}
		if (T >= ActionEnd)
		{
			SetActorRotation(FRotator(0.f, ChargeDir.Rotation().Yaw, 0.f));
			SetAction(EAction::Charging, 1.9f);
			RupturaAssets::PlaySound3D(this, TEXT("hospedeiro_passo"), GetActorLocation(), 1.f, 0.8f);
		}
		break;

	case EAction::Charging:
	{
		AddMovementInput(ChargeDir);

		if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			const FVector Delta = Kael->GetActorLocation() - GetActorLocation();
			if (!bActionFired && !Kael->IsDead() && Delta.Size2D() < 230.f && FMath::Abs(Delta.Z) < 220.f)
			{
				bActionFired = true;
				UGameplayStatics::ApplyDamage(Kael, Tuning.MeleeDamage * 1.2f, GetController(), this, UDamageType::StaticClass());
				Kael->LaunchCharacter(ChargeDir * 1100.f + FVector(0.f, 0.f, 380.f), true, true);
				Kael->AddTrauma(0.8f);
				RupturaAssets::PlaySound3D(this, TEXT("hospedeiro_golpe"), Kael->GetActorLocation(), 1.f, 1.1f);
				SetAction(EAction::Recover, 1.f);
				break;
			}
		}

		// Bateu em algo sólido: fica atordoado com o coração à mostra.
		if (Age > 0.35f && GetVelocity().Size2D() < 250.f)
		{
			SetAction(EAction::Stunned, bEnraged ? 2.4f : 3.2f);
			RupturaAssets::PlaySound3D(this, TEXT("hospedeiro_impacto"), GetActorLocation(), 1.f);
			RupturaFx::CeilingDust(GetWorld(), GetActorLocation() + GetActorForwardVector() * 150.f + FVector(0.f, 0.f, 800.f));
			if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
			{
				Kael->AddTrauma(0.5f);
			}
			break;
		}
		if (T >= ActionEnd)
		{
			SetAction(EAction::Recover, 0.8f);
		}
		break;
	}

	case EAction::Stunned:
		if (T >= ActionEnd)
		{
			Roar(0.9f);
			SetAction(EAction::Recover, 0.6f);
		}
		break;

	case EAction::Acid:
		if (!bActionFired && Age >= 0.7f)
		{
			bActionFired = true;
			FireAcidSalvo();
		}
		if (T >= ActionEnd)
		{
			SetAction(EAction::Recover, 0.5f);
		}
		break;

	case EAction::Recover:
		if (T >= ActionEnd)
		{
			Action = EAction::None;
		}
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// Animação e tick
// ---------------------------------------------------------------------------------------------

void AAbomination::AnimateBody(float DeltaSeconds)
{
	if (!Torso || !HipL || !HipR || !ShoulderL || !ShoulderR || !ElbowL || !ElbowR || !Head || !Mouth)
	{
		return;
	}
	const float T = Now();
	const bool bDead = IsDead();
	const float Speed = bDead ? 0.f : GetVelocity().Size2D();
	const float Stride = FMath::Clamp(Speed / 260.f, 0.f, 1.4f);
	const float PrevPhase = Phase;
	Phase += DeltaSeconds * (1.4f + Speed * 0.005f);
	const float Swing = FMath::Sin(Phase) * 20.f * FMath::Min(Stride, 1.f);

	HipL->SetRelativeRotation(FRotator(Swing, 0.f, 0.f));
	HipR->SetRelativeRotation(FRotator(-Swing, 0.f, 0.f));

	// Passos pesados: som + tremor na câmera do jogador.
	if (Stride > 0.2f && FMath::Sign(FMath::Sin(PrevPhase)) != FMath::Sign(FMath::Sin(Phase)))
	{
		RupturaAssets::PlaySound3D(this, TEXT("hospedeiro_passo"), GetActorLocation(), 0.9f, FMath::FRandRange(0.85f, 1.f));
		if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			const float Dist = FVector::Dist(Kael->GetActorLocation(), GetActorLocation());
			Kael->AddTrauma(FMath::Clamp(0.3f - Dist / 6000.f, 0.f, 0.3f));
		}
	}

	// Poses-alvo por estado; as articulações perseguem o alvo suavemente.
	const float Breath = FMath::Sin(T * (bEnraged ? 3.f : 1.6f)) * 3.f;
	FRotator TorsoT(-15.f + Breath, 0.f, 0.f);
	FRotator HeadT(10.f, 0.f, 0.f);
	FRotator MouthT(0.f, 0.f, 0.f);
	FRotator ArmLT(10.f + FMath::Sin(Phase) * 18.f * Stride, 0.f, 6.f);
	FRotator ArmRT(10.f - FMath::Sin(Phase) * 18.f * Stride, 0.f, -6.f);
	FRotator ElbowT(15.f, 0.f, 0.f);
	float Speed01 = 8.f;

	if (!bActivated && !bDead)
	{
		// Casulo: encolhido, abraçando o próprio corpo.
		TorsoT = FRotator(-60.f + Breath, 0.f, 0.f);
		HeadT = FRotator(-30.f, 0.f, 0.f);
		ArmLT = FRotator(45.f, 0.f, -35.f);
		ArmRT = FRotator(45.f, 0.f, 35.f);
		ElbowT = FRotator(70.f, 0.f, 0.f);
	}
	else if (!bDead)
	{
		const float Age = ActionAge();
		switch (Action)
		{
		case EAction::Slam:
			if (Age < 0.75f)
			{
				ArmLT = ArmRT = FRotator(170.f, 0.f, 0.f);
				ElbowT = FRotator(30.f, 0.f, 0.f);
				TorsoT = FRotator(5.f, 0.f, 0.f);
				MouthT = FRotator(-25.f, 0.f, 0.f);
			}
			else
			{
				ArmLT = ArmRT = FRotator(60.f, 0.f, 0.f);
				ElbowT = FRotator(0.f, 0.f, 0.f);
				TorsoT = FRotator(-38.f, 0.f, 0.f);
				Speed01 = 25.f;
			}
			break;
		case EAction::ChargeWindup:
			TorsoT = FRotator(12.f, 0.f, 0.f);
			HeadT = FRotator(30.f, 0.f, 0.f);
			MouthT = FRotator(-30.f, 0.f, 0.f);
			ArmLT = FRotator(20.f, 0.f, 45.f);
			ArmRT = FRotator(20.f, 0.f, -45.f);
			break;
		case EAction::Charging:
			TorsoT = FRotator(-40.f, 0.f, 0.f);
			HeadT = FRotator(25.f, 0.f, 0.f);
			ArmLT = FRotator(-25.f + FMath::Sin(Phase * 2.f) * 20.f, 0.f, 10.f);
			ArmRT = FRotator(-25.f - FMath::Sin(Phase * 2.f) * 20.f, 0.f, -10.f);
			Speed01 = 14.f;
			break;
		case EAction::Stunned:
			TorsoT = FRotator(-50.f, 0.f, FMath::Sin(T * 2.f) * 8.f);
			HeadT = FRotator(-35.f, 0.f, 15.f);
			ArmLT = ArmRT = FRotator(0.f, 0.f, 0.f);
			ElbowT = FRotator(5.f, 0.f, 0.f);
			Speed01 = 5.f;
			break;
		case EAction::Acid:
			TorsoT = Age < 0.7f ? FRotator(8.f, 0.f, 0.f) : FRotator(-28.f, 0.f, 0.f);
			HeadT = Age < 0.7f ? FRotator(30.f, 0.f, 0.f) : FRotator(-5.f, 0.f, 0.f);
			MouthT = FRotator(-35.f, 0.f, 0.f);
			Speed01 = Age < 0.7f ? 6.f : 20.f;
			break;
		default:
			break;
		}
		if (bEnraged)
		{
			// Tremores de fúria
			TorsoT.Roll += FMath::PerlinNoise1D(T * 6.f) * 6.f;
			HeadT.Yaw += FMath::PerlinNoise1D(T * 9.f + 3.f) * 14.f;
		}
	}

	Torso->SetRelativeRotation(Approach(Torso->GetRelativeRotation(), TorsoT, DeltaSeconds, Speed01));
	Head->SetRelativeRotation(Approach(Head->GetRelativeRotation(), HeadT, DeltaSeconds, Speed01));
	Mouth->SetRelativeRotation(Approach(Mouth->GetRelativeRotation(), MouthT, DeltaSeconds, 12.f));
	ShoulderL->SetRelativeRotation(Approach(ShoulderL->GetRelativeRotation(), ArmLT, DeltaSeconds, Speed01));
	ShoulderR->SetRelativeRotation(Approach(ShoulderR->GetRelativeRotation(), ArmRT, DeltaSeconds, Speed01));
	ElbowL->SetRelativeRotation(Approach(ElbowL->GetRelativeRotation(), ElbowT, DeltaSeconds, Speed01));
	ElbowR->SetRelativeRotation(Approach(ElbowR->GetRelativeRotation(), ElbowT, DeltaSeconds, Speed01));

	// Coração pulsando (mais rápido com fúria, disparado quando atordoado).
	if (Heart && HeartLight)
	{
		float Pulse = 0.f;
		if (!bDead)
		{
			const float Rate = IsStunned() ? 3.f : (bEnraged ? 2.4f : (bActivated ? 1.3f : 0.6f));
			Pulse = FMath::Pow(FMath::Max(0.f, FMath::Sin(T * Rate * 2.f * PI)), 4.f);
		}
		Heart->SetRelativeScale3D(FVector(0.6f, 0.58f, 0.7f) * (1.f + 0.14f * Pulse));
		HeartLight->SetIntensity(bDead ? 0.f : (150.f + 650.f * Pulse) * (IsStunned() ? 2.f : 1.f));
	}

	// Morte: tomba para frente.
	FRotator RootRot = FRotator::ZeroRotator;
	if (bDead)
	{
		const float A = FMath::Clamp((T - DeathTime) / 1.8f, 0.f, 1.f);
		RootRot.Pitch = -70.f * A * A;
	}
	BodyRoot->SetRelativeRotation(RootRot);
}

void AAbomination::Tick(float DeltaSeconds)
{
	// Pula o Tick do infectado comum (voz, golpe, dormência) e usa o do Character.
	ACharacter::Tick(DeltaSeconds);
	if (!bBodyBuilt)
	{
		return;
	}

	UpdateAction(DeltaSeconds);
	AnimateBody(DeltaSeconds);

	if (!IsDead() && bActivated && Now() >= NextVoice)
	{
		NextVoice = Now() + FMath::FRandRange(2.5f, 5.f);
		RupturaAssets::PlaySound3D(this, TEXT("hospedeiro_respiracao"), GetActorLocation() + FVector(0.f, 0.f, 150.f), 0.8f, FMath::FRandRange(0.85f, 1.05f));
	}

	// Ao morrer, o corpo se rompe em jatos de sangue.
	if (IsDead() && DeathBursts > 0 && Now() >= NextDeathBurst)
	{
		--DeathBursts;
		NextDeathBurst = Now() + 0.35f;
		const FVector P = GetActorLocation() + FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-40.f, 120.f));
		RupturaFx::Blood(GetWorld(), P, FMath::VRand() + FVector(0.f, 0.f, 0.8f), 18);
		RupturaAssets::PlaySound3D(this, TEXT("impacto_corpo"), P, 1.f, FMath::FRandRange(0.5f, 0.7f));
		if (DeathBursts == 0)
		{
			RupturaFx::BloodPool(GetWorld(), GetActorLocation(), 420.f);
		}
		if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Kael->AddTrauma(0.25f);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Controlador do chefe
// ---------------------------------------------------------------------------------------------

void AAbominationAIController::ActivateBoss()
{
	if (APawn* Player = GetPlayer())
	{
		AlertTo(Player->GetActorLocation(), true);
		LastSeenTime = Now();
	}
	NextSlam = Now() + 1.5f;
	NextCharge = Now() + 4.f;
	NextAcid = Now() + 7.f;
}

void AAbominationAIController::TickCombat(float DeltaSeconds)
{
	AAbomination* Boss = Cast<AAbomination>(Enemy);
	APawn* Player = GetPlayer();
	if (!Boss || !Player || !IsPlayerAlive())
	{
		StopMove();
		return;
	}

	const float T = Now();
	// O Hospedeiro sempre sabe onde o jogador está.
	LastKnownLocation = Player->GetActorLocation();
	LastSeenTime = T;

	if (Boss->IsBusy())
	{
		if (IsMoving())
		{
			StopMove();
		}
		if (Boss->IsCharging())
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}
		else
		{
			SetFocus(Player, EAIFocusPriority::Gameplay);
		}
		return;
	}

	const bool bEnraged = Boss->IsEnraged();
	Boss->SetCombatStance(true);
	Boss->SetMoveSpeed(bEnraged ? EnragedWalkSpeed : WalkSpeed);
	SetFocus(Player, EAIFocusPriority::Gameplay);

	const float Dist = FVector::Dist2D(Boss->GetActorLocation(), LastKnownLocation);
	const bool bLOS = HasLineOfSight(Boss->GetEyeLocationStanding(), PlayerAimPoint());

	if (Dist < 420.f && T >= NextSlam)
	{
		NextSlam = T + (bEnraged ? 3.f : 4.5f);
		StopMove();
		Boss->StartSlam();
		return;
	}
	if (Dist > 700.f && Dist < 2600.f && bLOS && T >= NextCharge)
	{
		NextCharge = T + (bEnraged ? 6.5f : 9.f);
		StopMove();
		Boss->StartCharge(Player);
		return;
	}
	if (Dist > 600.f && T >= NextAcid)
	{
		NextAcid = T + (bEnraged ? 5.f : 8.f);
		StopMove();
		Boss->StartAcid(Player);
		return;
	}

	// Persegue sem pressa: ele sabe que você não tem para onde ir.
	if (T >= NextDecision || !IsMoving())
	{
		NextDecision = T + 0.8f;
		FVector Dest = LastKnownLocation;
		const FBox2D Arena = RupturaLayout::ReactorArena();
		Dest.X = FMath::Clamp(Dest.X, Arena.Min.X, Arena.Max.X);
		Dest.Y = FMath::Clamp(Dest.Y, Arena.Min.Y, Arena.Max.Y);
		MoveToPoint(Dest, 200.f);
	}
}
