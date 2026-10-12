#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/RupturaTypes.h"
#include "Core/SoldierBody.h"
#include "EnemyCharacter.generated.h"

class UHealthComponent;
class UWeaponComponent;

/** Soldado inimigo. O "cérebro" fica em AEnemyAIController. */
UCLASS()
class RUPTURA_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	virtual void Tick(float DeltaSeconds) override;

	/** Chamado por quem cria o inimigo (MissionDirector). */
	void Setup(FName InGroup, const TArray<FVector>& InPatrolPoints, bool bInElite);
	virtual void ApplyDifficulty(ERupturaDifficulty Difficulty);

	/** Multiplicador por região atingida (cabeça, ponto fraco do chefe...). */
	virtual float GetDamageMultiplierForHit(const FHitResult& Hit, const FVector& ShotDir) const;
	virtual bool IsBoss() const { return false; }
	virtual bool IsActive() const { return true; }

	/** Dispara na direção do alvo usando a precisão da dificuldade. */
	bool FireAt(const FVector& Target, float ExtraSpreadDeg);
	FVector GetEyeLocationStanding() const;
	FVector GetMuzzleLocation() const;
	void SetCombatStance(bool bCombat);
	void SetMoveSpeed(float Speed);

	bool IsDead() const;
	UHealthComponent* GetHealth() const { return Health; }
	UWeaponComponent* GetWeapon() const { return Weapon; }
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

	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* Component, float NewHealth, float Delta, AActor* InstigatorActor);

	UFUNCTION()
	void HandleDeath(UHealthComponent* Component, AActor* Killer);

	UPROPERTY(VisibleAnywhere, Category = "Inimigo") TObjectPtr<UHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, Category = "Inimigo") TObjectPtr<UWeaponComponent> Weapon;
	UPROPERTY(VisibleAnywhere, Category = "Inimigo") TObjectPtr<USceneComponent> BodyRoot;
	UPROPERTY() FSoldierBody Body;

	UPROPERTY(EditAnywhere, Category = "Inimigo") FName Group;
	UPROPERTY(EditAnywhere, Category = "Inimigo") TArray<FVector> PatrolPoints;
	UPROPERTY(EditAnywhere, Category = "Inimigo") bool bElite = false;

	FEnemyTuning Tuning;
	float StandingHalfHeight = 92.f;
	float DeathTime = 0.f;
	float FlinchTime = -10.f;
	bool bDeadFlag = false;
	bool bBodyBuilt = false;
	FVector2D DeathFallDir = FVector2D(1.f, 0.f);
};
