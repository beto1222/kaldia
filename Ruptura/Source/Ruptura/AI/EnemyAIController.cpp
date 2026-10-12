#include "AI/EnemyAIController.h"
#include "AI/EnemyCharacter.h"
#include "Player/KaelCharacter.h"
#include "Combat/Grenade.h"
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
	constexpr float WalkSpeed = 190.f;
	constexpr float AlertSpeed = 300.f;
	constexpr float RunSpeed = 470.f;
}

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	UAIPerceptionComponent* Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 3200.f;
	SightConfig->LoseSightRadius = 3800.f;
	SightConfig->PeripheralVisionAngleDegrees = 65.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->SetMaxAge(4.f);

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 4000.f;
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
		Enemy->SetMoveSpeed(WalkSpeed);
	}
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AEnemyAIController::OnPerceptionUpdated);
	}
	WaitUntil = Now() + FMath::FRandRange(0.f, 2.f);
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
	// Mira no tronco (mais baixo se estiver agachado atrás de cobertura).
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
	if (!Enemy || Enemy->IsDead() || !Enemy->IsActive() || State == EState::Morto)
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

	if (Stimulus.Type == HearingConfig->GetSenseID() && Stimulus.WasSuccessfullySensed())
	{
		// Só reage a sons causados pelo jogador (tiros de aliados são tratados pelo alerta em grupo).
		if (Actor != Player || !IsPlayerAlive())
		{
			return;
		}
		const FVector Where = Stimulus.StimulusLocation;
		const float Dist = FVector::Dist(Where, Enemy->GetActorLocation());
		const bool bLoud = Stimulus.Tag == FName(TEXT("Tiro")) || Stimulus.Tag == FName(TEXT("Explosao"));

		if (State == EState::Combate)
		{
			if (Now() - LastSeenTime > 2.f)
			{
				LastKnownLocation = Where;
			}
		}
		else if (bLoud && Dist < 2000.f)
		{
			AlertTo(Where, true);
		}
		else
		{
			AlertTo(Where, false);
		}
	}
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
		Rate *= Dist < 700.f ? 4.f : (Dist < 1500.f ? 1.6f : (Dist < 2500.f ? 0.9f : 0.5f));
		if (const ACharacter* C = Cast<ACharacter>(Player))
		{
			if (C->bIsCrouched)
			{
				Rate *= 0.5f;
			}
		}
		if (State == EState::Investigando || State == EState::Buscando)
		{
			Rate *= 1.8f;
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
		Detection = FMath::Max(0.f, Detection - DeltaSeconds * 0.2f);
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
			LastSeenTime = Now() - 1.5f; // sabe a direção, mas ainda não vê o jogador
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
		const bool bWasInCombat = State == EState::Combate;
		AlertTo(InstigatorActor->GetActorLocation(), true);
		LastSeenTime = Now() - (bWasInCombat ? 0.f : 1.f);
		bHasCover = false; // fui atingido: reavaliar posição
		NextDecision = 0.f;
	}
}

void AEnemyAIController::NotifyDeath()
{
	State = EState::Morto;
	StopMove();
	ClearFocus(EAIFocusPriority::Gameplay);
}

void AEnemyAIController::BroadcastAlert()
{
	LastAlertBroadcast = Now();
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Other = *It;
		if (Other == Enemy || Other->IsDead() || Other->IsBoss())
		{
			continue;
		}
		if (FVector::Dist(Other->GetActorLocation(), Enemy->GetActorLocation()) > 3000.f)
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

	switch (NewState)
	{
	case EState::Patrulha:
		Enemy->SetMoveSpeed(WalkSpeed);
		Enemy->SetCombatStance(false);
		Enemy->UnCrouch();
		ClearFocus(EAIFocusPriority::Gameplay);
		Detection = 0.f;
		StopMove();
		break;

	case EState::Investigando:
		Enemy->SetMoveSpeed(AlertSpeed);
		Enemy->SetCombatStance(false);
		ClearFocus(EAIFocusPriority::Gameplay);
		StopMove();
		if (Old == EState::Patrulha)
		{
			RupturaAssets::PlaySound3D(this, TEXT("inimigo_suspeita"), Enemy->GetActorLocation(), 0.8f);
		}
		break;

	case EState::Combate:
		Enemy->SetMoveSpeed(RunSpeed);
		Enemy->SetCombatStance(true);
		bHasCover = false;
		bFlanking = false;
		bBurst = false;
		NextDecision = 0.f;
		CanShootAfter = Now() + Enemy->GetTuning().ReactionTime;
		StopMove();
		if (Old != EState::Combate)
		{
			RupturaAssets::PlaySound3D(this, TEXT("inimigo_alerta"), Enemy->GetActorLocation(), 1.f);
			BroadcastAlert();
		}
		break;

	case EState::Buscando:
		Enemy->SetMoveSpeed(AlertSpeed);
		Enemy->SetCombatStance(false);
		Enemy->UnCrouch();
		ClearFocus(EAIFocusPriority::Gameplay);
		SearchEnd = Now() + 16.f;
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
		// Sentinela: fica no posto olhando ao redor.
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
		WaitUntil = Now() + FMath::FRandRange(1.5f, 3.5f);
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
	// Chegou: olha em volta por alguns segundos.
	LookAround(DeltaSeconds);
	if (StateTime > 9.f)
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
	FVector Next = LastKnownLocation + FVector(FMath::FRandRange(-1000.f, 1000.f), FMath::FRandRange(-1000.f, 1000.f), 0.f);
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Loc;
		if (Nav->GetRandomReachablePointInRadius(LastKnownLocation, 1100.f, Loc))
		{
			Next = Loc.Location;
		}
	}
	ClearFocus(EAIFocusPriority::Gameplay);
	Enemy->SetCombatStance(false);
	MoveToPoint(Next, 100.f);
	WaitUntil = Now() + FMath::FRandRange(1.5f, 3.f);
}

void AEnemyAIController::LookAround(float DeltaSeconds)
{
	if (Now() < NextLookAround)
	{
		return;
	}
	NextLookAround = Now() + FMath::FRandRange(1.2f, 2.5f);
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
	const bool bLOS = FVector::Dist(Eye, Aim) < 5000.f && HasLineOfSight(Eye, Aim);

	if (bLOS)
	{
		if (T - LastSeenTime > 1.5f)
		{
			CanShootAfter = FMath::Max(CanShootAfter, T + Enemy->GetTuning().ReactionTime * 0.6f);
		}
		LastKnownLocation = Player->GetActorLocation();
		LastSeenTime = T;
		SetFocus(Player, EAIFocusPriority::Gameplay);
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		SetFocalPoint(LastKnownLocation + FVector(0.f, 0.f, 40.f), EAIFocusPriority::Gameplay);
	}

	if (T - LastAlertBroadcast > 3.f)
	{
		BroadcastAlert();
	}

	if (AvoidGrenades())
	{
		return;
	}

	const float SinceSeen = T - LastSeenTime;
	if (SinceSeen > 9.f)
	{
		EnterState(EState::Buscando);
		return;
	}

	// --- Decisões de posicionamento ---
	if (T >= NextDecision)
	{
		NextDecision = T + FMath::FRandRange(0.6f, 1.1f);
		const float Dist = FVector::Dist2D(Enemy->GetActorLocation(), LastKnownLocation);

		if (!IsMoving())
		{
			if (bHasCover && !IsCoverValid(CoverPoint))
			{
				bHasCover = false;
			}

			FVector Point;
			if (SinceSeen > 2.5f && !bFlanking && FMath::FRand() < Enemy->GetTuning().FlankChance && FindFlankPoint(Point))
			{
				bFlanking = true;
				bHasCover = false;
				MoveToPoint(Point, 80.f);
			}
			else if (!bHasCover || Dist > 2600.f)
			{
				if (FindCoverPoint(Point))
				{
					CoverPoint = Point;
					bHasCover = true;
					bFlanking = false;
					MoveToPoint(Point, 50.f);
				}
				else if (Dist > 1600.f || !bLOS)
				{
					MoveToPoint(LastKnownLocation, 900.f); // avança
				}
			}
			else if (SinceSeen > 4.5f)
			{
				bHasCover = false;
				MoveToPoint(LastKnownLocation, 500.f); // pressiona a última posição conhecida
			}
		}
	}

	// --- Disparo em rajadas ---
	const bool bAtCover = bHasCover && !IsMoving() && FVector::Dist2D(Enemy->GetActorLocation(), CoverPoint) < 130.f;
	bool bStand = true;

	if (bLOS && T >= CanShootAfter)
	{
		if (bBurst)
		{
			if (T > BurstEnd)
			{
				bBurst = false;
				PauseEnd = T + FMath::FRandRange(0.7f, 1.6f);
			}
			else
			{
				const float PlayerSpeed = Player->GetVelocity().Size2D();
				const float Extra = (IsMoving() ? 2.f : 0.f) + (PlayerSpeed > 500.f ? 1.5f : 0.f);
				Enemy->FireAt(Aim, Extra);
			}
		}
		else if (T >= PauseEnd)
		{
			bBurst = true;
			BurstEnd = T + Enemy->GetTuning().BurstTime * FMath::FRandRange(0.7f, 1.3f);
		}
	}
	else
	{
		bBurst = false;
	}

	if (bAtCover && !bBurst)
	{
		bStand = false; // abaixa atrás da cobertura entre as rajadas
	}
	if (bStand && Enemy->bIsCrouched)
	{
		Enemy->UnCrouch();
	}
	else if (!bStand && !Enemy->bIsCrouched)
	{
		Enemy->Crouch();
	}
}

bool AEnemyAIController::AvoidGrenades()
{
	TArray<AGrenade*> Grenades;
	AGrenade::GetActiveGrenades(Grenades);
	for (AGrenade* G : Grenades)
	{
		if (!G || G->IsRocket() || G->GetInstigator() == Enemy)
		{
			continue;
		}
		const FVector Delta = Enemy->GetActorLocation() - G->GetActorLocation();
		if (Delta.Size() < 480.f && G->GetTimeLeft() > 0.3f)
		{
			const FVector Away = Enemy->GetActorLocation() + Delta.GetSafeNormal2D() * 700.f;
			if (Enemy->bIsCrouched)
			{
				Enemy->UnCrouch();
			}
			MoveToPoint(Away, 80.f);
			bHasCover = false;
			NextDecision = Now() + 1.2f;
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------------------------
// Movimento
// ---------------------------------------------------------------------------------------------

void AEnemyAIController::MoveToPoint(const FVector& Destination, float Acceptance)
{
	bDirectMove = false;
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

bool AEnemyAIController::IsMoving() const
{
	return bDirectMove || GetMoveStatus() == EPathFollowingStatus::Moving;
}

void AEnemyAIController::StopMove()
{
	bDirectMove = false;
	StopMovement();
}

void AEnemyAIController::UpdateDirectMove(float DeltaSeconds)
{
	if (!bDirectMove)
	{
		return;
	}
	FVector ToTarget = MoveTarget - Enemy->GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.Size() <= MoveAcceptance)
	{
		bDirectMove = false;
		return;
	}

	FVector Dir = ToTarget.GetSafeNormal();
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemySteer), false, Enemy);
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
	if (!IsMoving())
	{
		StuckTime = 0.f;
		return;
	}
	if (Enemy->GetVelocity().Size2D() < 25.f)
	{
		StuckTime += DeltaSeconds;
		if (StuckTime > 1.8f)
		{
			StopMove(); // desiste deste destino; a próxima decisão escolhe outro
			StuckTime = 0.f;
			bHasCover = false;
		}
	}
	else
	{
		StuckTime = 0.f;
	}
}

// ---------------------------------------------------------------------------------------------
// Táticas
// ---------------------------------------------------------------------------------------------

bool AEnemyAIController::HasLineOfSight(const FVector& From, const FVector& To) const
{
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyLOS), false, Enemy);
	Query.AddIgnoredActor(GetPlayer());
	return !GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query);
}

bool AEnemyAIController::ProjectToFloor(const FVector& Point, FVector& OutFloor) const
{
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Loc;
		if (Nav->ProjectPointToNavigation(Point, Loc, FVector(120.f, 120.f, 400.f)))
		{
			OutFloor = Loc.Location;
			return true;
		}
	}
	// Sem navmesh: traço para baixo para achar o chão.
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyFloor), false, Enemy);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Point + FVector(0.f, 0.f, 150.f), Point - FVector(0.f, 0.f, 400.f), ECC_Visibility, Query))
	{
		OutFloor = Hit.ImpactPoint;
		return true;
	}
	return false;
}

bool AEnemyAIController::IsSpotFree(const FVector& FloorPoint) const
{
	const float Radius = Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float Half = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemySpot), false, Enemy);
	return !GetWorld()->OverlapBlockingTestByChannel(FloorPoint + FVector(0.f, 0.f, Half + 5.f), FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Radius, Half), Query);
}

bool AEnemyAIController::IsCoverValid(const FVector& FloorPoint) const
{
	const FVector ThreatEye = LastKnownLocation + FVector(0.f, 0.f, 60.f);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyCover), false, Enemy);
	Query.AddIgnoredActor(GetPlayer());

	// Agachado (60 cm) precisa estar protegido por algo próximo...
	const FVector Low = FloorPoint + FVector(0.f, 0.f, 60.f);
	FHitResult LowHit;
	if (!GetWorld()->LineTraceSingleByChannel(LowHit, Low, ThreatEye, ECC_Visibility, Query) || LowHit.Distance > 220.f)
	{
		return false;
	}
	// ...e em pé (165 cm) deve conseguir atirar por cima.
	const FVector High = FloorPoint + FVector(0.f, 0.f, 165.f);
	FHitResult HighHit;
	return !GetWorld()->LineTraceSingleByChannel(HighHit, High, ThreatEye, ECC_Visibility, Query);
}

bool AEnemyAIController::FindCoverPoint(FVector& OutPoint) const
{
	const FVector Self = Enemy->GetActorLocation();
	float BestScore = TNumericLimits<float>::Max();
	bool bFound = false;

	for (int32 i = 0; i < 24; ++i)
	{
		const float Angle = i * 15.f + FMath::FRandRange(-6.f, 6.f);
		const float Radius = FMath::FRandRange(250.f, 1100.f);
		const FVector Candidate = Self + FRotator(0.f, Angle, 0.f).Vector() * Radius;

		FVector Floor;
		if (!ProjectToFloor(Candidate, Floor))
		{
			continue;
		}
		const float ToThreat = FVector::Dist2D(Floor, LastKnownLocation);
		if (ToThreat < 500.f || ToThreat > 2300.f)
		{
			continue;
		}
		if (!IsCoverValid(Floor) || !IsSpotFree(Floor))
		{
			continue;
		}
		// Evita amontoar inimigos no mesmo ponto.
		bool bTaken = false;
		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != Enemy && !It->IsDead() && FVector::Dist2D(It->GetActorLocation(), Floor) < 150.f)
			{
				bTaken = true;
				break;
			}
		}
		if (bTaken)
		{
			continue;
		}
		const float Score = FVector::Dist(Self, Floor) + FMath::Abs(ToThreat - 1200.f) * 0.5f;
		if (Score < BestScore)
		{
			BestScore = Score;
			OutPoint = Floor;
			bFound = true;
		}
	}
	return bFound;
}

bool AEnemyAIController::FindFlankPoint(FVector& OutPoint) const
{
	const FVector Self = Enemy->GetActorLocation();
	const FVector FromThreat = (Self - LastKnownLocation).GetSafeNormal2D();
	const FVector ThreatEye = LastKnownLocation + FVector(0.f, 0.f, 60.f);
	float BestDist = TNumericLimits<float>::Max();
	bool bFound = false;

	for (int32 i = 0; i < 12; ++i)
	{
		const float Side = (i % 2 == 0) ? 1.f : -1.f;
		const float Angle = Side * FMath::FRandRange(65.f, 110.f);
		const FVector Dir = FromThreat.RotateAngleAxis(Angle, FVector::UpVector);
		const FVector Candidate = LastKnownLocation + Dir * FMath::FRandRange(800.f, 1300.f);

		FVector Floor;
		if (!ProjectToFloor(Candidate, Floor) || !IsSpotFree(Floor))
		{
			continue;
		}
		if (!HasLineOfSight(Floor + FVector(0.f, 0.f, 160.f), ThreatEye))
		{
			continue;
		}
		const float D = FVector::Dist(Self, Floor);
		if (D < BestDist)
		{
			BestDist = D;
			OutPoint = Floor;
			bFound = true;
		}
	}
	return bFound;
}
