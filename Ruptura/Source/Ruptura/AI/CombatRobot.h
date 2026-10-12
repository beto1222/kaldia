#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "CombatRobot.generated.h"

class UPointLightComponent;

/** Chefe da missão: unidade de combate T-9 "COLOSSO". */
UCLASS()
class RUPTURA_API ACombatRobot : public AEnemyCharacter
{
	GENERATED_BODY()

public:
	ACombatRobot();

	virtual bool IsBoss() const override { return true; }
	virtual bool IsActive() const override { return bActivated; }
	virtual float GetDamageMultiplierForHit(const FHitResult& Hit, const FVector& ShotDir) const override;
	virtual void ApplyDifficulty(ERupturaDifficulty Difficulty) override;
	virtual void Tick(float DeltaSeconds) override;

	void Activate();
	bool IsEnraged() const { return bEnraged; }

	/** Ataques especiais usados pelo controlador. */
	void FireRocketSalvo(const APawn* Target);
	void Stomp();
	void SpinUp();

protected:
	virtual void BuildBody() override;
	virtual void AnimateBody(float DeltaSeconds) override;
	virtual void OnDamaged(float Amount, AActor* InstigatorActor) override;
	virtual void OnDied(AActor* Killer) override;

	UPROPERTY() TObjectPtr<USceneComponent> RocketPod;
	UPROPERTY() TObjectPtr<USceneComponent> BarrelSpin;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BackCore;
	UPROPERTY() TObjectPtr<UPointLightComponent> EyeLight;
	UPROPERTY() TObjectPtr<UPointLightComponent> CoreLight;

	bool bActivated = false;
	bool bEnraged = false;
	float BarrelSpeed = 0.f;
	float StepTimer = 0.f;
	float LastStepSide = 1.f;
	int32 DeathBlasts = 0;
	float NextDeathBlast = 0.f;
};

/** Cérebro do chefe: mantém distância, metralha em rajadas longas, salvas de foguetes e pisão de curto alcance. */
UCLASS()
class RUPTURA_API ACombatRobotAIController : public AEnemyAIController
{
	GENERATED_BODY()

public:
	void ActivateBoss();

protected:
	virtual void TickCombat(float DeltaSeconds) override;

	float NextRocket = 0.f;
	float NextStomp = 0.f;
};
