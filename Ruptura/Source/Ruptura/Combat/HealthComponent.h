#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class UHealthComponent;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnHealthChanged, UHealthComponent*, Component, float, Health, float, Delta, AActor*, InstigatorActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthDeath, UHealthComponent*, Component, AActor*, Killer);

/** Vida genérica: escuta OnTakeAnyDamage do dono (dano pontual e radial do motor). */
UCLASS(ClassGroup = (Ruptura), meta = (BlueprintSpawnableComponent))
class RUPTURA_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vida")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vida")
	float Health = 100.f;

	/** Multiplicador geral de dano recebido (armadura). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vida")
	float DamageMultiplier = 1.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vida")
	bool bInvulnerable = false;

	UPROPERTY(BlueprintAssignable, Category = "Vida")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Vida")
	FOnHealthDeath OnDeath;

	UFUNCTION(BlueprintPure, Category = "Vida")
	bool IsDead() const { return Health <= 0.f; }

	UFUNCTION(BlueprintPure, Category = "Vida")
	float GetFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	/** Aplica dano diretamente; retorna o dano efetivo. */
	float ApplyDamageAmount(float Amount, AActor* InstigatorActor);
	void Heal(float Amount);
	void SetMaxHealth(float NewMax, bool bRefill);

	float GetLastDamageTime() const { return LastDamageTime; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	float LastDamageTime = -100.f;
};
