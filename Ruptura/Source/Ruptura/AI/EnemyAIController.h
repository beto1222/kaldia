#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Core/RupturaTypes.h"
#include "EnemyAIController.generated.h"

class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class AEnemyCharacter;
class AKaelCharacter;

/**
 * IA dos infectados: máquina de estados em C++ alimentada pelo AI Perception (visão + audição).
 * Vagando -> Investigando (barulho, vulto) -> Perseguição (corre e ataca corpo a corpo) -> Busca -> Vagando.
 *
 * Os infectados enxergam mal no escuro: a lanterna acesa (principalmente apontada para eles)
 * e os tiros denunciam o jogador. Agachado e com a lanterna apagada, dá para passar despercebido.
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

	/** Alerta vindo de outro infectado, barulho ou do MissionDirector. */
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
	/** Quão visível o jogador está para este infectado (lanterna, escuridão, distância). */
	float GetVisibilityFactor(const AKaelCharacter* Kael, float Dist) const;
	void TickPatrol(float DeltaSeconds);
	void TickInvestigate(float DeltaSeconds);
	void TickSearch(float DeltaSeconds);
	virtual void TickCombat(float DeltaSeconds);

	// Movimento
	void MoveToPoint(const FVector& Destination, float Acceptance);
	/** Persegue um ator em movimento (a navegação acompanha o alvo sozinha). */
	void ChaseActor(AActor* Target, float Acceptance);
	bool IsMoving() const;
	void StopMove();
	void UpdateDirectMove(float DeltaSeconds);
	void UpdateStuck(float DeltaSeconds);

	bool HasLineOfSight(const FVector& From, const FVector& To) const;
	void BroadcastAlert(float Radius);
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

	// Vagar
	int32 PatrolIndex = 0;
	float WaitUntil = 0.f;
	bool bMoveIssued = false;
	FVector HomeLocation = FVector::ZeroVector;

	// Movimento direto (sem navmesh)
	bool bDirectMove = false;
	FVector MoveTarget = FVector::ZeroVector;
	TWeakObjectPtr<AActor> DirectFollow;
	float MoveAcceptance = 80.f;
	float StuckTime = 0.f;

	// Perseguição
	bool bChasing = false;
	float NextRepath = 0.f;
	float NextDecision = 0.f;
	float NextLookAround = 0.f;
	float SearchEnd = 0.f;
};
