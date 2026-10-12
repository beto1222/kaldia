#include "AI/CombatRobot.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
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

ACombatRobot::ACombatRobot()
{
	GetCapsuleComponent()->InitCapsuleSize(85.f, 175.f);
	BodyRoot->SetRelativeLocation(FVector(0.f, 0.f, -175.f));

	AIControllerClass = ACombatRobotAIController::StaticClass();

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = 300.f;
	Move->RotationRate = FRotator(0.f, 160.f, 0.f);
	Move->GetNavAgentPropertiesRef().bCanCrouch = false;
	Move->bUseRVOAvoidance = false;
	Move->Mass = 2000.f;
}

void ACombatRobot::ApplyDifficulty(ERupturaDifficulty Difficulty)
{
	Tuning = FRupturaTuning::ForDifficulty(Difficulty, false);
	float Damage = 5.f;
	switch (Difficulty)
	{
	case ERupturaDifficulty::Facil: Tuning.MaxHealth = 1300.f; Tuning.SpreadDeg = 6.f; Damage = 4.f; break;
	case ERupturaDifficulty::Dificil: Tuning.MaxHealth = 2200.f; Tuning.SpreadDeg = 3.2f; Damage = 7.f; break;
	default: Tuning.MaxHealth = 1700.f; Tuning.SpreadDeg = 4.5f; Damage = 5.f; break;
	}
	Tuning.ReactionTime = 0.f;
	Weapon->InitBossLoadout(Damage, 0.07f);

	if (HasActorBegunPlay() && !Health->IsDead())
	{
		const float Fraction = Health->GetFraction();
		Health->SetMaxHealth(Tuning.MaxHealth, false);
		Health->Health = FMath::Max(1.f, Health->MaxHealth * Fraction);
	}
}

void ACombatRobot::BuildBody()
{
	UMaterialInterface* Metal = RupturaAssets::MakeMaterial(this, FLinearColor(0.05f, 0.055f, 0.06f), 0.35f, 0.9f, 0.f);
	UMaterialInterface* Armor = RupturaAssets::MakeMaterial(this, FLinearColor(0.11f, 0.12f, 0.1f), 0.5f, 0.6f, 0.f);
	UMaterialInterface* Hazard = RupturaAssets::MakeMaterial(this, FLinearColor(0.8f, 0.5f, 0.04f), 0.5f, 0.2f, 0.f);
	UMaterialInterface* Eye = RupturaAssets::MakeMaterial(this, FLinearColor(1.f, 0.05f, 0.03f), 0.2f, 0.f, 20.f);
	UMaterialInterface* Hot = RupturaAssets::MakeMaterial(this, FLinearColor(1.f, 0.45f, 0.05f), 0.2f, 0.f, 18.f);

	Body.Root = BodyRoot;
	Body.Scale = 1.f;

	// Pernas
	Body.HipL = RupturaAssets::AddPivot(this, BodyRoot, FVector(0.f, 48.f, 170.f));
	RupturaAssets::AddPart(this, Body.HipL, EShape::Cube, FVector(0.f, 0.f, -45.f), FVector(46.f, 46.f, 90.f), Metal);
	RupturaAssets::AddPart(this, Body.HipL, EShape::Cube, FVector(10.f, 0.f, -120.f), FVector(52.f, 52.f, 80.f), Armor);
	RupturaAssets::AddPart(this, Body.HipL, EShape::Cube, FVector(18.f, 0.f, -160.f), FVector(95.f, 64.f, 22.f), Metal);
	Body.HipR = RupturaAssets::AddPivot(this, BodyRoot, FVector(0.f, -48.f, 170.f));
	RupturaAssets::AddPart(this, Body.HipR, EShape::Cube, FVector(0.f, 0.f, -45.f), FVector(46.f, 46.f, 90.f), Metal);
	RupturaAssets::AddPart(this, Body.HipR, EShape::Cube, FVector(10.f, 0.f, -120.f), FVector(52.f, 52.f, 80.f), Armor);
	RupturaAssets::AddPart(this, Body.HipR, EShape::Cube, FVector(18.f, 0.f, -160.f), FVector(95.f, 64.f, 22.f), Metal);

	// Tronco
	RupturaAssets::AddPart(this, BodyRoot, EShape::Cube, FVector(0.f, 0.f, 182.f), FVector(70.f, 120.f, 40.f), Metal);
	RupturaAssets::AddPart(this, BodyRoot, EShape::Cube, FVector(0.f, 0.f, 255.f), FVector(120.f, 160.f, 115.f), Armor);
	RupturaAssets::AddPart(this, BodyRoot, EShape::Cube, FVector(64.f, 0.f, 258.f), FVector(16.f, 150.f, 90.f), Metal);
	RupturaAssets::AddPart(this, BodyRoot, EShape::Cube, FVector(66.f, 0.f, 206.f), FVector(16.f, 150.f, 12.f), Hazard);

	// Cabeça / sensor
	RupturaAssets::AddPart(this, BodyRoot, EShape::Cube, FVector(25.f, 0.f, 335.f), FVector(55.f, 70.f, 45.f), Metal);
	Body.Visor = RupturaAssets::AddPart(this, BodyRoot, EShape::Cube, FVector(54.f, 0.f, 338.f), FVector(6.f, 50.f, 10.f), Eye);
	RupturaAssets::AddPart(this, BodyRoot, EShape::Cylinder, FVector(-30.f, 40.f, 345.f), FVector(5.f, 5.f, 70.f), Metal);

	// Braço direito: metralhadora rotativa
	Body.ShoulderR = RupturaAssets::AddPivot(this, BodyRoot, FVector(0.f, -100.f, 280.f));
	RupturaAssets::AddPart(this, Body.ShoulderR, EShape::Cube, FVector(0.f, 0.f, 0.f), FVector(62.f, 52.f, 62.f), Armor);
	Body.GunRestLocation = FVector(30.f, -105.f, 235.f);
	Body.GunPivot = RupturaAssets::AddPivot(this, BodyRoot, Body.GunRestLocation);
	RupturaAssets::AddPart(this, Body.GunPivot, EShape::Cylinder, FVector(40.f, 0.f, 0.f), FVector(46.f, 46.f, 90.f), Metal, FRotator(-90.f, 0.f, 0.f));
	BarrelSpin = RupturaAssets::AddPivot(this, Body.GunPivot, FVector(90.f, 0.f, 0.f));
	for (int32 i = 0; i < 4; ++i)
	{
		const float A = i * 90.f;
		const FVector Offset(0.f, FMath::Cos(FMath::DegreesToRadians(A)) * 11.f, FMath::Sin(FMath::DegreesToRadians(A)) * 11.f);
		RupturaAssets::AddPart(this, BarrelSpin, EShape::Cylinder, Offset + FVector(30.f, 0.f, 0.f), FVector(8.f, 8.f, 70.f), Metal, FRotator(-90.f, 0.f, 0.f));
	}
	Body.Muzzle = RupturaAssets::AddPivot(this, Body.GunPivot, FVector(160.f, 0.f, 0.f));

	// Ombro esquerdo: lançador de foguetes
	Body.ShoulderL = RupturaAssets::AddPivot(this, BodyRoot, FVector(0.f, 100.f, 290.f));
	RupturaAssets::AddPart(this, Body.ShoulderL, EShape::Cube, FVector(5.f, 0.f, 25.f), FVector(75.f, 62.f, 62.f), Armor);
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Offset(43.f, (i % 2 == 0) ? -14.f : 14.f, 25.f + ((i < 2) ? -14.f : 14.f));
		RupturaAssets::AddPart(this, Body.ShoulderL, EShape::Cube, Offset, FVector(4.f, 16.f, 16.f), Hot);
	}
	RocketPod = RupturaAssets::AddPivot(this, Body.ShoulderL, FVector(55.f, 0.f, 25.f));

	// Ponto fraco: núcleo nas costas
	BackCore = RupturaAssets::AddPart(this, BodyRoot, EShape::Sphere, FVector(-68.f, 0.f, 258.f), FVector(55.f, 55.f, 55.f), Hot);

	EyeLight = NewObject<UPointLightComponent>(this);
	EyeLight->SetMobility(EComponentMobility::Movable);
	EyeLight->SetIntensityUnits(ELightUnits::Candelas);
	EyeLight->SetIntensity(150.f);
	EyeLight->SetLightColor(FLinearColor(1.f, 0.1f, 0.05f));
	EyeLight->SetAttenuationRadius(900.f);
	EyeLight->SetCastShadows(false);
	EyeLight->RegisterComponent();
	EyeLight->AttachToComponent(BodyRoot, FAttachmentTransformRules::KeepRelativeTransform);
	EyeLight->SetRelativeLocation(FVector(80.f, 0.f, 338.f));
	AddInstanceComponent(EyeLight);

	CoreLight = NewObject<UPointLightComponent>(this);
	CoreLight->SetMobility(EComponentMobility::Movable);
	CoreLight->SetIntensityUnits(ELightUnits::Candelas);
	CoreLight->SetIntensity(400.f);
	CoreLight->SetLightColor(FLinearColor(1.f, 0.45f, 0.1f));
	CoreLight->SetAttenuationRadius(700.f);
	CoreLight->SetCastShadows(false);
	CoreLight->RegisterComponent();
	CoreLight->AttachToComponent(BodyRoot, FAttachmentTransformRules::KeepRelativeTransform);
	CoreLight->SetRelativeLocation(FVector(-110.f, 0.f, 258.f));
	AddInstanceComponent(CoreLight);
}

void ACombatRobot::Activate()
{
	if (bActivated || IsDead())
	{
		return;
	}
	bActivated = true;
	if (EyeLight)
	{
		EyeLight->SetIntensity(1200.f);
	}
	RupturaAssets::PlaySound3D(this, TEXT("robo_ativar"), GetActorLocation(), 1.f);
	if (ACombatRobotAIController* AI = Cast<ACombatRobotAIController>(GetController()))
	{
		AI->ActivateBoss();
	}
}

float ACombatRobot::GetDamageMultiplierForHit(const FHitResult& Hit, const FVector& ShotDir) const
{
	// Tiro viajando na mesma direção para onde o robô olha = acertou pelas costas (núcleo exposto).
	if (FVector::DotProduct(ShotDir.GetSafeNormal2D(), GetActorForwardVector().GetSafeNormal2D()) > 0.35f)
	{
		return 2.2f;
	}
	const float Top = GetActorLocation().Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (Hit.ImpactPoint.Z > Top - 40.f)
	{
		return 1.4f; // sensor da cabeça
	}
	return bEnraged ? 0.85f : 0.7f; // blindagem frontal
}

void ACombatRobot::OnDamaged(float Amount, AActor* InstigatorActor)
{
	Super::OnDamaged(Amount, InstigatorActor);

	if (!bActivated)
	{
		Activate();
	}
	if (!bEnraged && Health->GetFraction() < 0.5f && !Health->IsDead())
	{
		bEnraged = true;
		GetCharacterMovement()->MaxWalkSpeed = 380.f;
		if (EyeLight)
		{
			EyeLight->SetIntensity(3000.f);
		}
		RupturaFx::Sparks(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 80.f), FLinearColor(1.f, 0.5f, 0.1f), 16, 900.f);
		RupturaAssets::PlaySound3D(this, TEXT("robo_ativar"), GetActorLocation(), 1.f, 0.8f);
		if (AMissionDirector* Director = AMissionDirector::Get(this))
		{
			Director->OnBossEnraged();
		}
	}
}

void ACombatRobot::SpinUp()
{
	BarrelSpeed = 1.f;
	RupturaAssets::PlaySound3D(this, TEXT("robo_giro"), GetActorLocation(), 0.8f);
}

void ACombatRobot::FireRocketSalvo(const APawn* Target)
{
	if (!Target || !RocketPod || IsDead())
	{
		return;
	}
	UWorld* World = GetWorld();
	const int32 Count = bEnraged ? 4 : 3;
	const FVector TargetFeet = Target->GetActorLocation() - FVector(0.f, 0.f, 80.f);

	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Start = RocketPod->GetComponentLocation() + FMath::VRand() * 15.f + FVector(0.f, 0.f, 30.f);
		FVector Aim = TargetFeet + Target->GetVelocity() * 0.5f;
		if (i > 0)
		{
			Aim += FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 0.f);
		}

		FVector Velocity;
		if (!UGameplayStatics::SuggestProjectileVelocity_CustomArc(this, Velocity, Start, Aim, 0.f, 0.35f))
		{
			Velocity = (Aim - Start).GetSafeNormal() * 2000.f + FVector(0.f, 0.f, 400.f);
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AGrenade* Rocket = World->SpawnActor<AGrenade>(AGrenade::StaticClass(), Start, Velocity.Rotation(), Params))
		{
			Rocket->BaseDamage = 40.f;
			Rocket->MinDamage = 5.f;
			Rocket->InnerRadius = 120.f;
			Rocket->OuterRadius = 380.f;
			Rocket->Launch(Velocity, true);
		}
	}
	RupturaFx::MuzzleFlash(World, RocketPod->GetComponentLocation(), GetActorRotation(), FLinearColor(1.f, 0.5f, 0.15f), 2.f);
	RupturaAssets::PlaySound3D(this, TEXT("foguete"), RocketPod->GetComponentLocation(), 1.f);
}

void ACombatRobot::Stomp()
{
	if (IsDead())
	{
		return;
	}
	UWorld* World = GetWorld();
	const FVector Center = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 30.f);

	TArray<AActor*> Ignore;
	Ignore.Add(this);
	UGameplayStatics::ApplyRadialDamageWithFalloff(World, bEnraged ? 40.f : 32.f, 5.f, Center, 250.f, 520.f, 1.f,
		UDamageType::StaticClass(), Ignore, this, GetController(), ECC_Visibility);

	if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		const FVector Delta = Kael->GetActorLocation() - Center;
		Kael->AddTrauma(FMath::Clamp(1.f - Delta.Size() / 2000.f, 0.f, 0.8f));
		if (Delta.Size() < 600.f)
		{
			Kael->LaunchCharacter(Delta.GetSafeNormal2D() * 900.f + FVector(0.f, 0.f, 350.f), true, true);
		}
	}

	for (int32 i = 0; i < 6; ++i)
	{
		const FVector Ring = FRotator(0.f, i * 60.f, 0.f).Vector() * 180.f;
		RupturaFx::Smoke(World, Center + Ring, 160.f, 1.4f);
	}
	RupturaFx::Sparks(World, Center, FLinearColor(1.f, 0.6f, 0.2f), 10, 700.f);
	RupturaAssets::PlaySound3D(this, TEXT("pisada_robo"), Center, 1.f);
}

void ACombatRobot::AnimateBody(float DeltaSeconds)
{
	const float Speed = IsDead() ? 0.f : GetVelocity().Size2D();
	const float Stride = FMath::Clamp(Speed / 300.f, 0.f, 1.2f);
	const float PrevPhase = Body.Phase;
	Body.Phase += DeltaSeconds * (1.6f + Speed * 0.006f);
	const float Swing = FMath::Sin(Body.Phase) * 22.f * Stride;
	if (Body.HipL && Body.HipR)
	{
		Body.HipL->SetRelativeRotation(FRotator(Swing, 0.f, 0.f));
		Body.HipR->SetRelativeRotation(FRotator(-Swing, 0.f, 0.f));
	}

	// Passos pesados: som + tremor no jogador próximo.
	if (Stride > 0.2f && FMath::Sign(FMath::Sin(PrevPhase)) != FMath::Sign(FMath::Sin(Body.Phase)))
	{
		RupturaAssets::PlaySound3D(this, TEXT("robo_passo"), GetActorLocation(), 0.9f, FMath::FRandRange(0.9f, 1.05f));
		if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			const float Dist = FVector::Dist(Kael->GetActorLocation(), GetActorLocation());
			Kael->AddTrauma(FMath::Clamp(0.25f - Dist / 6000.f, 0.f, 0.25f));
		}
	}

	// Coice e giro dos canos
	Body.Kick = FMath::FInterpTo(Body.Kick, 0.f, DeltaSeconds, 12.f);
	if (Body.GunPivot)
	{
		Body.GunPivot->SetRelativeLocation(Body.GunRestLocation + FVector(-Body.Kick * 5.f, 0.f, FMath::Abs(FMath::Sin(Body.Phase)) * 4.f * Stride));
	}
	BarrelSpeed = FMath::FInterpTo(BarrelSpeed, 0.f, DeltaSeconds, 0.8f);
	if (BarrelSpin)
	{
		BarrelSpin->AddLocalRotation(FRotator(0.f, 0.f, BarrelSpeed * 1500.f * DeltaSeconds));
	}

	// Pulsação do núcleo das costas
	if (CoreLight && !IsDead())
	{
		const float Pulse = 0.7f + 0.3f * FMath::Sin(GetWorld()->GetTimeSeconds() * (bEnraged ? 9.f : 4.f));
		CoreLight->SetIntensity(400.f * Pulse * (bEnraged ? 2.f : 1.f));
	}

	// Morte: tomba para frente
	FRotator RootRot = FRotator::ZeroRotator;
	if (IsDead())
	{
		const float A = FMath::Clamp((GetWorld()->GetTimeSeconds() - DeathTime) / 1.6f, 0.f, 1.f);
		RootRot.Pitch = -70.f * A * A;
	}
	BodyRoot->SetRelativeRotation(RootRot);
}

void ACombatRobot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Sequência de explosões ao morrer
	if (IsDead() && DeathBlasts > 0 && GetWorld()->GetTimeSeconds() >= NextDeathBlast)
	{
		--DeathBlasts;
		NextDeathBlast = GetWorld()->GetTimeSeconds() + 0.35f;
		const FVector P = GetActorLocation() + FVector(FMath::FRandRange(-80.f, 80.f), FMath::FRandRange(-80.f, 80.f), FMath::FRandRange(0.f, 150.f));
		RupturaFx::Explosion(GetWorld(), P, DeathBlasts == 0 ? 420.f : 180.f);
		RupturaAssets::PlaySound3D(this, TEXT("explosao"), P, 1.f, FMath::FRandRange(0.8f, 1.f));
		if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Kael->AddTrauma(DeathBlasts == 0 ? 0.8f : 0.3f);
		}
	}
}

void ACombatRobot::OnDied(AActor* Killer)
{
	Super::OnDied(Killer);
	SetLifeSpan(0.f); // os destroços ficam no salão
	DeathBlasts = 5;
	NextDeathBlast = GetWorld()->GetTimeSeconds() + 0.2f;
	if (EyeLight)
	{
		EyeLight->SetIntensity(0.f);
	}
	if (CoreLight)
	{
		CoreLight->SetIntensity(0.f);
	}
	if (BackCore)
	{
		BackCore->SetMaterial(0, RupturaAssets::MakeMaterial(this, FLinearColor(0.05f, 0.03f, 0.02f), 0.8f, 0.f, 0.f));
	}
}

// ---------------------------------------------------------------------------------------------
// Controlador do chefe
// ---------------------------------------------------------------------------------------------

void ACombatRobotAIController::ActivateBoss()
{
	if (APawn* Player = GetPlayer())
	{
		AlertTo(Player->GetActorLocation(), true);
		LastSeenTime = Now();
	}
	NextRocket = Now() + 5.f;
	NextStomp = Now() + 2.f;
}

void ACombatRobotAIController::TickCombat(float DeltaSeconds)
{
	ACombatRobot* Boss = Cast<ACombatRobot>(Enemy);
	APawn* Player = GetPlayer();
	if (!Boss || !Player || !IsPlayerAlive())
	{
		StopMove();
		return;
	}

	const float T = Now();
	const bool bEnraged = Boss->IsEnraged();
	Boss->SetCombatStance(true);
	Boss->SetMoveSpeed(bEnraged ? 380.f : 300.f);
	SetFocus(Player, EAIFocusPriority::Gameplay);

	// O chefe sempre sabe onde o jogador está.
	LastKnownLocation = Player->GetActorLocation();
	LastSeenTime = T;

	const FVector BossLoc = Boss->GetActorLocation();
	const float Dist = FVector::Dist2D(BossLoc, LastKnownLocation);

	// Movimento: mantém distância média, recua se o jogador chega perto, orbita para os lados.
	if (T >= NextDecision)
	{
		NextDecision = T + (bEnraged ? 1.5f : 2.2f);
		const FVector ToPlayer = (LastKnownLocation - BossLoc).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(ToPlayer, FVector::UpVector) * (FMath::RandBool() ? 1.f : -1.f);
		FVector Dest;
		if (Dist > 1700.f)
		{
			Dest = BossLoc + ToPlayer * 600.f;
		}
		else if (Dist < 700.f)
		{
			Dest = BossLoc - ToPlayer * 500.f + Side * 200.f;
		}
		else
		{
			Dest = BossLoc + Side * 600.f;
		}
		const FBox2D Arena = RupturaLayout::ReactorArena();
		Dest.X = FMath::Clamp(Dest.X, Arena.Min.X, Arena.Max.X);
		Dest.Y = FMath::Clamp(Dest.Y, Arena.Min.Y, Arena.Max.Y);
		MoveToPoint(Dest, 100.f);
	}

	// Pisão de curto alcance
	if (Dist < 450.f && T >= NextStomp)
	{
		NextStomp = T + 4.f;
		Boss->Stomp();
		return;
	}

	const FVector Eye = Boss->GetEyeLocationStanding();
	const FVector Aim = PlayerAimPoint();
	const bool bLOS = HasLineOfSight(Eye, Aim);

	// Salva de foguetes (em arco, funciona mesmo sem linha de visão direta)
	if (T >= NextRocket)
	{
		NextRocket = T + (bEnraged ? 6.f : 9.f);
		Boss->FireRocketSalvo(Player);
	}

	// Metralhadora em rajadas longas
	if (bLOS)
	{
		if (bBurst)
		{
			if (T > BurstEnd)
			{
				bBurst = false;
				PauseEnd = T + (bEnraged ? 1.0f : 1.6f);
			}
			else
			{
				Boss->FireAt(Aim, IsMoving() ? 1.f : 0.f);
			}
		}
		else if (T >= PauseEnd)
		{
			bBurst = true;
			BurstEnd = T + (bEnraged ? 3.2f : 2.6f);
			Boss->SpinUp();
		}
	}
	else
	{
		bBurst = false;
	}
}
