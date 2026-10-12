#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "Abomination.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Chefe da missão: o HOSPEDEIRO, a massa de carne que alimenta o núcleo do Protocolo Zero.
 * Fica dormente num casulo até o jogador entrar no salão. Ataques: investida (se bater numa
 * parede ou pilar fica atordoado), pancada no chão com onda de choque e vômito de bile ácida.
 * Ponto fraco: o coração exposto nas costas.
 */
UCLASS()
class RUPTURA_API AAbomination : public AEnemyCharacter
{
	GENERATED_BODY()

public:
	AAbomination();

	virtual bool IsBoss() const override { return true; }
	virtual bool IsActive() const override { return bActivated && !IsDead(); }
	virtual float GetDamageMultiplierForHit(const FHitResult& Hit, const FVector& ShotDir) const override;
	virtual void ApplyDifficulty(ERupturaDifficulty Difficulty) override;
	virtual void Tick(float DeltaSeconds) override;

	void Activate();
	bool IsEnraged() const { return bEnraged; }

	// Ações usadas pelo controlador
	enum class EAction : uint8 { None, Slam, ChargeWindup, Charging, Stunned, Acid, Recover };
	bool IsBusy() const { return Action != EAction::None; }
	bool IsCharging() const { return Action == EAction::Charging || Action == EAction::ChargeWindup; }
	bool IsStunned() const { return Action == EAction::Stunned; }
	void StartSlam();
	void StartCharge(const AActor* Target);
	void StartAcid(const AActor* Target);

protected:
	virtual void BuildBody() override;
	virtual void AnimateBody(float DeltaSeconds) override;
	virtual void OnDamaged(float Amount, AActor* InstigatorActor) override;
	virtual void OnDied(AActor* Killer) override;
	virtual void UpdateMelee(float DeltaSeconds) override;

	void SetAction(EAction NewAction, float Duration);
	void UpdateAction(float DeltaSeconds);
	void DoSlamImpact();
	void FireAcidSalvo();
	void Roar(float Pitch = 1.f);
	float ActionAge() const;

	UPROPERTY() TObjectPtr<USceneComponent> HipL;
	UPROPERTY() TObjectPtr<USceneComponent> HipR;
	UPROPERTY() TObjectPtr<USceneComponent> Torso;
	UPROPERTY() TObjectPtr<USceneComponent> Head;
	UPROPERTY() TObjectPtr<USceneComponent> ShoulderL;
	UPROPERTY() TObjectPtr<USceneComponent> ShoulderR;
	UPROPERTY() TObjectPtr<USceneComponent> ElbowL;
	UPROPERTY() TObjectPtr<USceneComponent> ElbowR;
	UPROPERTY() TObjectPtr<USceneComponent> Mouth;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Heart;
	UPROPERTY() TObjectPtr<UPointLightComponent> HeartLight;
	UPROPERTY() TObjectPtr<UPointLightComponent> EyeLight;

	bool bActivated = false;
	bool bEnraged = false;
	EAction Action = EAction::None;
	float ActionStart = 0.f;
	float ActionEnd = 0.f;
	bool bActionFired = false;
	FVector ChargeDir = FVector::ForwardVector;
	TWeakObjectPtr<const AActor> ActionTarget;

	float Phase = 0.f;
	float StepSide = 1.f;
	int32 DeathBursts = 0;
	float NextDeathBurst = 0.f;
};

/** Cérebro do Hospedeiro: persegue sem parar e escolhe o ataque pela distância e pelos tempos de recarga. */
UCLASS()
class RUPTURA_API AAbominationAIController : public AEnemyAIController
{
	GENERATED_BODY()

public:
	void ActivateBoss();

protected:
	virtual void TickCombat(float DeltaSeconds) override;

	float NextSlam = 0.f;
	float NextCharge = 0.f;
	float NextAcid = 0.f;
};
