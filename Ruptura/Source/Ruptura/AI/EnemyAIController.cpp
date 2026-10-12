#include "AI/EnemyAIController.h"
#include "AI/EnemyCharacter.h"
#include "Player/KaelCharacter.h"
#include "Core/RupturaAssets.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

namespace
{
	constexpr float ShambleSpeed = 105.f;   // arrastando os pés
	constexpr float InvestigateSpeed = 175.f;
	constexpr float LoseTrackTime = 7.f;    // segundos sem ver o jogador até desistir da perseguição
}

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	UAIPerceptionComponent* Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));

	// Visão ruim, audição excelente.
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 2600.f;
	SightConfig->LoseSightRadius = 3000.f;
	SightConfig->PeripheralVisionAngleDegrees = 60.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->SetMaxAge(4.f);

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 4500.f;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->SetMaxAge(3.f);

	Perception->ConfigureSense(*SightConfig);
	Perception->ConfigureSense(*HearingConfig);
	Perception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*Perception);
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Enemy = Cast<AEnemyCharacter>(InPawn);
	if (Enemy)
	{
		HomeLocation = Enemy->GetActorLocation();
		Enemy->SetMoveSpeed(ShambleSpeed);
	}
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AEnemyAIController::OnPerceptionUpdated);
	}
	WaitUntil = Now() + FMath::FRandRange(0.f, 3.f);
}

float AEnemyAIController::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

APawn* AEnemyAIController::GetPlayer() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

bool AEnemyAIController::IsPlayerAlive() const
{
	const AKaelCharacter* Kael = Cast<AKaelCharacter>(GetPlayer());
	return Kael && !Kael->IsDead();
}

FVector AEnemyAIController::PlayerAimPoint() const
{
	const APawn* P = GetPlayer();
	if (!P)
	{
		return LastKnownLocation;
	}
	const ACharacter* C = Cast<ACharacter>(P);
	const float Up = (C && C->bIsCrouched) ? 15.f : 35.f;
	return P->GetActorLocation() + FVector(0.f, 0.f, Up);
}

EEnemyAlert AEnemyAIController::GetAlert() const
{
	switch (State)
	{
	case EState::Combate: return EEnemyAlert::Combate;
	case EState::Investigando: return EEnemyAlert::Suspeito;
	case EState::Buscando: return EEnemyAlert::Buscando;
	case EState::Morto: return EEnemyAlert::Morto;
	default: return Detection > 0.05f ? EEnemyAlert::Suspeito : EEnemyAlert::Calmo;
	}
}

// ---------------------------------------------------------------------------------------------
// Percepção
// ---------------------------------------------------------------------------------------------

void AEnemyAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Enemy || Enemy->IsDead() || State == EState::Morto)
	{
		return;
	}
	APawn* Player = GetPlayer();

	if (Stimulus.Type == SightConfig->GetSenseID())
	{
		if (Actor == Player)
		{
			bSensedBySight = Stimulus.WasSuccessfullySensed();
		}
		return;
	}

	if (Stimulus.Type != HearingConfig->GetSenseID() || !Stimulus.WasSuccessfullySensed())
	{
		return;
	}
	// Só reage a sons causados pelo jogador (os gritos dos outros são tratados pelo alerta em grupo).
	if (Actor != Player || !IsPlayerAlive())
	{
		return;
	}
	const FVector Where = Stimulus.StimulusLocation;
	const float Dist = FVector::Dist(Where, Enemy->GetActorLocation());
	const bool bLoud = Stimulus.Tag == FName(TEXT("Tiro")) || Stimulus.Tag == FName(TEXT("Explosao"));

	// Um tiro por perto acorda quem está fingindo de morto.
	if (Enemy->IsDormant())
	{
		if ((bLoud && Dist < 1300.f) || Dist < 450.f)
		{
			Enemy->Wake();
		}
		return;
	}
	if (!Enemy->IsActive())
	{
		return;
	}

	if (State == EState::Combate)
	{
		if (Now() - LastSeenTime > 1.5f)
		{
			LastKnownLocation = Where;
		}
	}
	else if (bLoud && Dist < 2800.f)
	{
		AlertTo(Where, true); // tiros atraem a horda direto para o atirador
	}
	else
	{
		AlertTo(Where, false);
	}
}

float AEnemyAIController::GetVisibilityFactor(const AKaelCharacter* Kael, float Dist) const
{
	if (!Kael)
	{
		return 1.f;
	}
	float Factor = 1.f;
	if (Kael->IsFlashlightOn())
	{
		Factor = 1.4f;
		// O feixe batendo no rosto do infectado denuncia o jogador na hora.
		const FVector ToEnemy = Enemy->GetActorLocation() + FVector(0.f, 0.f, 60.f) - Kael->GetFlashlightLocation();
		if (ToEnemy.Size() < 2800.f && FVector::DotProduct(Kael->GetFlashlightDirection(), ToEnemy.GetSafeNormal()) > 0.88f)
		{
			Factor = 3.5f;
		}
	}
	else
	{
		// No escuro, só a uma distância curta.
		Factor = Dist < 650.f ? 1.f : (Dist < 1200.f ? 0.3f : 0.06f);
	}
	if (Kael->bIsCrouched)
	{
		Factor *= 0.5f;
	}
	return Factor;
}

void AEnemyAIController::UpdateDetection(float DeltaSeconds)
{
	if (State == EState::Combate)
	{
		return;
	}
	APawn* Player = GetPlayer();
	if (bSensedBySight && Player && IsPlayerAlive())
	{
		const float Dist = FVector::Dist(Player->GetActorLocation(), Enemy->GetActorLocation());
		float Rate = Enemy->GetTuning().DetectionRate;
		Rate *= Dist < 500.f ? 5.f : (Dist < 1000.f ? 2.f : (Dist < 1800.f ? 0.9f : 0.4f));
		Rate *= GetVisibilityFactor(Cast<AKaelCharacter>(Player), Dist);
		if (State == EState::Investigando || State == EState::Buscando)
		{
			Rate *= 1.6f;
		}
		Detection = FMath::Min(1.f, Detection + DeltaSeconds * Rate);

		if (Detection >= 1.f)
		{
			LastKnownLocation = Player->GetActorLocation();
			LastSeenTime = Now();
			EnterState(EState::Combate);
		}
		else if (Detection > 0.35f && (State == EState::Patrulha || State == EState::Buscando))
		{
			InvestigateLocation = Player->GetActorLocation();
			EnterState(EState::Investigando);
		}
	}
	else
	{
		Detection = FMath::Max(0.f, Detection - DeltaSeconds * 0.15f);
	}
}

void AEnemyAIController::AlertTo(const FVector& Location, bool bCombat)
{
	if (!Enemy || Enemy->IsDead() || State == EState::Morto || !Enemy->IsActive())
	{
		return;
	}
	if (bCombat)
	{
		LastKnownLocation = Location;
		if (State != EState::Combate)
		{
			LastSeenTime = Now() - 1.f; // sabe a direção, mas ainda não vê o jogador
			Detection = 1.f;
			EnterState(EState::Combate);
		}
	}
	else if (State == EState::Patrulha || State == EState::Buscando || State == EState::Investigando)
	{
		InvestigateLocation = Location;
		Detection = FMath::Max(Detection, 0.4f);
		EnterState(EState::Investigando);
	}
}

void AEnemyAIController::NotifyDamaged(AActor* InstigatorActor)
{
	if (!Enemy || Enemy->IsDead())
	{
		return;
	}
	if (InstigatorActor && InstigatorActor == GetPlayer())
	{
		AlertTo(InstigatorActor->GetActorLocation(), true);
		LastSeenTime = Now();
	}
}

void AEnemyAIController::NotifyDeath()
{
	State = EState::Morto;
	StopMove();
	ClearFocus(EAIFocusPriority::Gameplay);
}

void AEnemyAIController::BroadcastAlert(float Radius)
{
	LastAlertBroadcast = Now();
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Other = *It;
		if (Other == Enemy || Other->IsDead() || Other->IsBoss())
		{
			continue;
		}
		if (FVector::Dist(Other->GetActorLocation(), Enemy->GetActorLocation()) > Radius)
		{
			continue;
		}
		if (AEnemyAIController* OtherAI = Cast<AEnemyAIController>(Other->GetController()))
		{
			if (!OtherAI->IsInCombat())
			{
				OtherAI->AlertTo(LastKnownLocation, true);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Estados
// ---------------------------------------------------------------------------------------------

void AEnemyAIController::EnterState(EState NewState)
{
	if (!Enemy || State == EState::Morto)
	{
		return;
	}
	const EState Old = State;
	State = NewState;
	StateTime = 0.f;
	bMoveIssued = false;
	bChasing = false;

	switch (NewState)
	{
	case EState::Patrulha:
		Enemy->SetMoveSpeed(ShambleSpeed);
		Enemy->SetCombatStance(false);
		ClearFocus(EAIFocusPriority::Gameplay);
		Detection = 0.f;
		StopMove();
		break;

	case EState::Investigando:
		Enemy->SetMoveSpeed(InvestigateSpeed);
		Enemy->SetCombatStance(false);
		ClearFocus(EAIFocusPriority::Gameplay);
		StopMove();
		if (Old == EState::Patrulha)
		{
			RupturaAssets::PlaySound3D(this, TEXT("infectado_gemido"), Enemy->GetActorLocation() + FVector(0.f, 0.f, 70.f), 0.9f, FMath::FRandRange(0.8f, 1.f));
		}
		break;

	case EState::Combate:
		Enemy->SetMoveSpeed(Enemy->GetTuning().ChaseSpeed);
		Enemy->SetCombatStance(true);
		NextDecision = 0.f;
		NextRepath = 0.f;
		StopMove();
		if (Old != EState::Combate)
		{
			// O grito avisa os outros: quanto maior o infectado, mais longe se ouve.
			RupturaAssets::PlaySound3D(this, TEXT("infectado_grito"), Enemy->GetActorLocation() + FVector(0.f, 0.f, 70.f), 1.f,
				FMath::FRandRange(0.9f, 1.1f) * (Enemy->IsElite() ? 0.75f : 1.f));
			BroadcastAlert(Enemy->IsElite() ? 3000.f : 1600.f);
		}
		break;

	case EState::Buscando:
		Enemy->SetMoveSpeed(InvestigateSpeed);
		Enemy->SetCombatStance(false);
		ClearFocus(EAIFocusPriority::Gameplay);
		SearchEnd = Now() + 18.f;
		StopMove();
		MoveToPoint(LastKnownLocation, 120.f);
		bMoveIssued = true;
		break;

	default:
		break;
	}
}

void AEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Enemy || Enemy->IsDead() || State == EState::Morto || !Enemy->IsActive())
	{
		return;
	}

	StateTime += DeltaSeconds;
	UpdateDirectMove(DeltaSeconds);
	UpdateStuck(DeltaSeconds);

	if (!IsPlayerAlive())
	{
		bSensedBySight = false;
		if (State != EState::Patrulha)
		{
			EnterState(EState::Patrulha);
		}
	}

	UpdateDetection(DeltaSeconds);

	switch (State)
	{
	case EState::Patrulha: TickPatrol(DeltaSeconds); break;
	case EState::Investigando: TickInvestigate(DeltaSeconds); break;
	case EState::Combate: TickCombat(DeltaSeconds); break;
	case EState::Buscando: TickSearch(DeltaSeconds); break;
	default: break;
	}
}

void AEnemyAIController::TickPatrol(float DeltaSeconds)
{
	const TArray<FVector>& Points = Enemy->GetPatrolPoints();
	if (Points.Num() == 0)
	{
		// Parado no lugar, balançando, de vez em quando virando a cabeça para um barulho imaginário.
		if (FVector::Dist2D(Enemy->GetActorLocation(), HomeLocation) > 150.f && !IsMoving())
		{
			ClearFocus(EAIFocusPriority::Gameplay);
			Enemy->SetCombatStance(false);
			MoveToPoint(HomeLocation, 60.f);
		}
		LookAround(DeltaSeconds);
		return;
	}

	if (bMoveIssued && !IsMoving())
	{
		bMoveIssued = false;
		WaitUntil = Now() + FMath::FRandRange(2.f, 6.f);
	}
	if (!bMoveIssued && Now() >= WaitUntil)
	{
		PatrolIndex = (PatrolIndex + 1) % Points.Num();
		ClearFocus(EAIFocusPriority::Gameplay);
		Enemy->SetCombatStance(false);
		MoveToPoint(Points[PatrolIndex], 70.f);
		bMoveIssued = true;
	}
	if (!bMoveIssued)
	{
		LookAround(DeltaSeconds);
	}
}

void AEnemyAIController::TickInvestigate(float DeltaSeconds)
{
	if (!bMoveIssued)
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		Enemy->SetCombatStance(false);
		MoveToPoint(InvestigateLocation, 150.f);
		bMoveIssued = true;
		return;
	}
	if (IsMoving())
	{
		return;
	}
	// Chegou: fica farejando em volta por alguns segundos.
	LookAround(DeltaSeconds);
	if (StateTime > 10.f)
	{
		EnterState(EState::Patrulha);
	}
}

void AEnemyAIController::TickSearch(float DeltaSeconds)
{
	if (Now() > SearchEnd)
	{
		EnterState(EState::Patrulha);
		return;
	}
	if (IsMoving())
	{
		return;
	}
	if (Now() < WaitUntil)
	{
		LookAround(DeltaSeconds);
		return;
	}

	// Vasculha pontos aleatórios perto da última posição conhecida.
	FVector Next = LastKnownLocation + FVector(FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-900.f, 900.f), 0.f);
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Loc;
		if (Nav->GetRandomReachablePointInRadius(LastKnownLocation, 1000.f, Loc))
		{
			Next = Loc.Location;
		}
	}
	ClearFocus(EAIFocusPriority::Gameplay);
	Enemy->SetCombatStance(false);
	MoveToPoint(Next, 100.f);
	WaitUntil = Now() + FMath::FRandRange(1.5f, 3.5f);
}

void AEnemyAIController::LookAround(float DeltaSeconds)
{
	if (Now() < NextLookAround)
	{
		return;
	}
	NextLookAround = Now() + FMath::FRandRange(2.5f, 6.f);
	const FVector Dir = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
	SetFocalPoint(Enemy->GetActorLocation() + Dir * 600.f, EAIFocusPriority::Gameplay);
	Enemy->SetCombatStance(true); // gira para o ponto de foco
}

void AEnemyAIController::TickCombat(float DeltaSeconds)
{
	APawn* Player = GetPlayer();
	if (!Player)
	{
		EnterState(EState::Buscando);
		return;
	}

	const float T = Now();
	const FVector Eye = Enemy->GetEyeLocationStanding();
	const FVector Aim = PlayerAimPoint();
	const float Dist = FVector::Dist(Enemy->GetActorLocation(), Player->GetActorLocation());
	const bool bLOS = Dist < 3500.f && HasLineOfSight(Eye, Aim);

	// Muito perto, ele sente o cheiro / ouve a respiração mesmo sem ver.
	if (bLOS || Dist < 500.f)
	{
		LastKnownLocation = Player->GetActorLocation();
		LastSeenTime = T;
		SetFocus(Player, EAIFocusPriority::Gameplay);
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		SetFocalPoint(LastKnownLocation + FVector(0.f, 0.f, 40.f), EAIFocusPriority::Gameplay);
	}

	if (T - LastAlertBroadcast > 5.f)
	{
		BroadcastAlert(1200.f);
	}

	const float SinceSeen = T - LastSeenTime;
	if (SinceSeen > LoseTrackTime)
	{
		EnterState(EState::Buscando);
		return;
	}

	// Golpeando ou atordoado: fica no lugar.
	if (Enemy->IsAttacking() || Enemy->IsStaggered())
	{
		if (IsMoving())
		{
			StopMove();
			bChasing = false;
		}
		return;
	}

	// Ao alcance: ataca.
	if (Dist < Enemy->GetAttackRange() && SinceSeen < 0.3f)
	{
		if (Enemy->CanAttack())
		{
			StopMove();
			bChasing = false;
			Enemy->StartMeleeAttack(Player);
		}
		return;
	}

	// Perseguição: segue o jogador direto enquanto o vê; senão, corre até a última posição conhecida.
	if (SinceSeen < 0.5f)
	{
		if (!bChasing || (!IsMoving() && T >= NextRepath))
		{
			ChaseActor(Player, 40.f);
			NextRepath = T + 0.4f;
		}
	}
	else if (bChasing || (!IsMoving() && T >= NextDecision))
	{
		bChasing = false;
		NextDecision = T + 0.8f;
		MoveToPoint(LastKnownLocation, 90.f);
	}
}

// ---------------------------------------------------------------------------------------------
// Movimento
// ---------------------------------------------------------------------------------------------

void AEnemyAIController::MoveToPoint(const FVector& Destination, float Acceptance)
{
	bDirectMove = false;
	DirectFollow.Reset();
	StuckTime = 0.f;
	MoveTarget = Destination;
	MoveAcceptance = Acceptance;

	const EPathFollowingRequestResult::Type Result = MoveToLocation(Destination, Acceptance, true, true, true, true, nullptr, true);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		// Sem navmesh ou destino fora dela: desloca em linha reta com desvio simples.
		bDirectMove = true;
	}
}

void AEnemyAIController::ChaseActor(AActor* Target, float Acceptance)
{
	if (!Target)
	{
		return;
	}
	bChasing = true;
	bDirectMove = false;
	DirectFollow.Reset();
	StuckTime = 0.f;
	MoveTarget = Target->GetActorLocation();
	MoveAcceptance = Acceptance;

	const EPathFollowingRequestResult::Type Result = MoveToActor(Target, Acceptance, true, true, true, nullptr, true);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		bDirectMove = true;
		DirectFollow = Target;
	}
}

bool AEnemyAIController::IsMoving() const
{
	return bDirectMove || GetMoveStatus() == EPathFollowingStatus::Moving;
}

void AEnemyAIController::StopMove()
{
	bDirectMove = false;
	DirectFollow.Reset();
	StopMovement();
}

void AEnemyAIController::UpdateDirectMove(float DeltaSeconds)
{
	if (!bDirectMove)
	{
		return;
	}
	if (const AActor* Follow = DirectFollow.Get())
	{
		MoveTarget = Follow->GetActorLocation();
	}
	FVector ToTarget = MoveTarget - Enemy->GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.Size() <= MoveAcceptance)
	{
		if (!DirectFollow.IsValid())
		{
			bDirectMove = false;
		}
		return;
	}

	FVector Dir = ToTarget.GetSafeNormal();
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemySteer), false, Enemy);
	if (const AActor* Follow = DirectFollow.Get())
	{
		Query.AddIgnoredActor(Follow);
	}
	const FVector Start = Enemy->GetActorLocation() - FVector(0.f, 0.f, 40.f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * 160.f, ECC_Visibility, Query))
	{
		FVector Side = FVector::CrossProduct(FVector::UpVector, Hit.ImpactNormal).GetSafeNormal2D();
		if (FVector::DotProduct(Side, Dir) < 0.f)
		{
			Side = -Side;
		}
		Dir = (Side + Hit.ImpactNormal.GetSafeNormal2D() * 0.3f).GetSafeNormal();
	}
	Enemy->AddMovementInput(Dir);
}

void AEnemyAIController::UpdateStuck(float DeltaSeconds)
{
	if (!IsMoving() || Enemy->IsAttacking())
	{
		StuckTime = 0.f;
		return;
	}
	if (Enemy->GetVelocity().Size2D() < 25.f)
	{
		StuckTime += DeltaSeconds;
		if (StuckTime > 2.f)
		{
			StopMove(); // desiste deste destino; a próxima decisão escolhe outro
			bChasing = false;
			StuckTime = 0.f;
		}
	}
	else
	{
		StuckTime = 0.f;
	}
}

bool AEnemyAIController::HasLineOfSight(const FVector& From, const FVector& To) const
{
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyLOS), false, Enemy);
	Query.AddIgnoredActor(GetPlayer());
	return !GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query);
}
