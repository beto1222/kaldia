#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Core/RupturaTypes.h"
#include "EnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class AEnemyCharacter;

/**
 * IA dos soldados: máquina de estados em C++ alimentada pelo AI Perception (visão + audição).
 * Patrulha -> Suspeita/Investigação -> Combate (cobertura, rajadas, flanco) -> Busca -> Patrulha.
 * Usa a navmesh quando existir; sem navmesh, cai para um deslocamento direto com desvio de obstáculos.
 */
UCLASS()
class RUPTURA_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	virtual void Tick(float DeltaSeconds) override;

	EEnemyAlert GetAlert() const;
	float GetDetection() const { return Detection; }

	/** Alerta vindo de aliado, barulho ou do MissionDirector. */
	void AlertTo(const FVector& Location, bool bCombat);
	void NotifyDamaged(AActor* InstigatorActor);
	void NotifyDeath();
	bool IsInCombat() const { return State == EState::Combate; }

protected:
	enum class EState : uint8 { Patrulha, Investigando, Combate, Buscando, Morto };

	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void EnterState(EState NewState);
	void UpdateDetection(float DeltaSeconds);
	void TickPatrol(float DeltaSeconds);
	void TickInvestigate(float DeltaSeconds);
	void TickSearch(float DeltaSeconds);
	virtual void TickCombat(float DeltaSeconds);

	// Movimento
	void MoveToPoint(const FVector& Destination, float Acceptance);
	bool IsMoving() const;
	void StopMove();
	void UpdateDirectMove(float DeltaSeconds);
	void UpdateStuck(float DeltaSeconds);

	// Táticas
	bool FindCoverPoint(FVector& OutPoint) const;
	bool FindFlankPoint(FVector& OutPoint) const;
	bool IsCoverValid(const FVector& FloorPoint) const;
	bool IsSpotFree(const FVector& FloorPoint) const;
	bool ProjectToFloor(const FVector& Point, FVector& OutFloor) const;
	bool HasLineOfSight(const FVector& From, const FVector& To) const;
	bool AvoidGrenades();
	void BroadcastAlert();
	void LookAround(float DeltaSeconds);

	APawn* GetPlayer() const;
	bool IsPlayerAlive() const;
	FVector PlayerAimPoint() const;
	float Now() const;

	UPROPERTY(VisibleAnywhere, Category = "IA") TObjectPtr<UAISenseConfig_Sight> SightConfig;
	UPROPERTY(VisibleAnywhere, Category = "IA") TObjectPtr<UAISenseConfig_Hearing> HearingConfig;
	UPROPERTY(Transient) TObjectPtr<AEnemyCharacter> Enemy;

	EState State = EState::Patrulha;
	float StateTime = 0.f;
	float Detection = 0.f;
	bool bSensedBySight = false;

	FVector LastKnownLocation = FVector::ZeroVector;
	float LastSeenTime = -100.f;
	float LastAlertBroadcast = -100.f;
	FVector InvestigateLocation = FVector::ZeroVector;

	// Patrulha
	int32 PatrolIndex = 0;
	float WaitUntil = 0.f;
	bool bMoveIssued = false;
	FVector HomeLocation = FVector::ZeroVector;

	// Movimento direto (sem navmesh)
	bool bDirectMove = false;
	FVector MoveTarget = FVector::ZeroVector;
	float MoveAcceptance = 80.f;
	float StuckTime = 0.f;

	// Combate
	FVector CoverPoint = FVector::ZeroVector;
	bool bHasCover = false;
	bool bFlanking = false;
	float NextDecision = 0.f;
	bool bBurst = false;
	float BurstEnd = 0.f;
	float PauseEnd = 0.f;
	float CanShootAfter = 0.f;
	float NextLookAround = 0.f;
	float SearchEnd = 0.f;
};
