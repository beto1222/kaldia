#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/RupturaTypes.h"
#include "Core/InfectedBody.h"
#include "EnemyCharacter.generated.h"

class UHealthComponent;

/**
 * Infectado: ataca corpo a corpo, se arrasta quando vaga e dispara quando percebe o jogador.
 * Pode começar "dormente" (caído no chão, como um cadáver) e levantar quando o jogador chega perto.
 * O "cérebro" fica em AEnemyAIController.
 */
UCLASS()
class RUPTURA_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	virtual void Tick(float DeltaSeconds) override;

	/** Chamado por quem cria o inimigo (MissionDirector), antes de terminar o spawn. */
	void Setup(FName InGroup, const TArray<FVector>& InPatrolPoints, bool bInElite, bool bInDormant = false);
	virtual void ApplyDifficulty(ERupturaDifficulty Difficulty);

	/** Multiplicador por região atingida (cabeça, coração do chefe...). */
	virtual float GetDamageMultiplierForHit(const FHitResult& Hit, const FVector& ShotDir) const;
	virtual bool IsBoss() const { return false; }
	/** Falso enquanto dormente ou levantando: a IA não age. */
	virtual bool IsActive() const;

	// Combate corpo a corpo
	virtual bool CanAttack() const;
	/** Começa um golpe contra Target; o dano sai no fim da preparação, se ainda estiver ao alcance. */
	virtual bool StartMeleeAttack(AActor* Target);
	bool IsAttacking() const;
	bool IsStaggered() const;
	float GetAttackRange() const { return AttackRange; }

	/** Desperta um infectado dormente (levanta do chão e ataca). */
	void Wake();
	bool IsDormant() const { return bDormant; }
	bool IsRising() const;

	/** Empurrão sofrido (faca, chute do Hospedeiro...). */
	void Shove(const FVector& Direction, float Strength);

	FVector GetEyeLocationStanding() const;
	void SetCombatStance(bool bCombat);
	void SetMoveSpeed(float Speed);

	bool IsDead() const;
	UHealthComponent* GetHealth() const { return Health; }
	EEnemyAlert GetAlert() const;
	float GetDetection() const;
	const FEnemyTuning& GetTuning() const { return Tuning; }
	FName GetGroup() const { return Group; }
	const TArray<FVector>& GetPatrolPoints() const { return PatrolPoints; }
	bool IsElite() const { return bElite; }

protected:
	virtual void BeginPlay() override;
	virtual void BuildBody();
	virtual void AnimateBody(float DeltaSeconds);
	virtual void OnDamaged(float Amount, AActor* InstigatorActor);
	virtual void OnDied(AActor* Killer);
	virtual void UpdateMelee(float DeltaSeconds);
	void UpdateVoice(float DeltaSeconds);
	void UpdateDormant(float DeltaSeconds);
	float Now() const;

	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* Component, float NewHealth, float Delta, AActor* InstigatorActor);

	UFUNCTION()
	void HandleDeath(UHealthComponent* Component, AActor* Killer);

	UPROPERTY(VisibleAnywhere, Category = "Inimigo") TObjectPtr<UHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, Category = "Inimigo") TObjectPtr<USceneComponent> BodyRoot;
	UPROPERTY() FInfectedBody Flesh;

	UPROPERTY(EditAnywhere, Category = "Inimigo") FName Group;
	UPROPERTY(EditAnywhere, Category = "Inimigo") TArray<FVector> PatrolPoints;
	UPROPERTY(EditAnywhere, Category = "Inimigo") bool bElite = false;
	UPROPERTY(EditAnywhere, Category = "Inimigo") bool bDormant = false;
	/** Distância (cm) em que um infectado dormente percebe o jogador e levanta. */
	UPROPERTY(EditAnywhere, Category = "Inimigo") float WakeRadius = 430.f;
	UPROPERTY(EditAnywhere, Category = "Inimigo") float AttackRange = 170.f;

	FEnemyTuning Tuning;
	float StandingHalfHeight = 92.f;
	float DeathTime = 0.f;
	float FlinchTime = -10.f;
	bool bDeadFlag = false;
	bool bDiedLying = false;
	bool bBodyBuilt = false;
	FVector2D DeathFallDir = FVector2D(1.f, 0.f);

	// Golpe
	TWeakObjectPtr<AActor> AttackTarget;
	float AttackStart = -10.f;
	float AttackDuration = 1.f;
	float NextAttackTime = 0.f;
	bool bAttackResolved = true;
	float StaggerUntil = 0.f;
	/** Dano somado numa janela curta (os chumbos da espingarda contam juntos para atordoar). */
	float RecentDamage = 0.f;
	float LastDamageAt = -10.f;

	// Dormente / levantando
	float WakeTime = -100.f;
	bool bWakeAlertSent = true;
	float RiseDuration = 1.5f;

	// Voz
	float NextVoice = 0.f;
};
