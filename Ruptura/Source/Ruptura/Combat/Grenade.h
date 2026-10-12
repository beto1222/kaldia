#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grenade.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPointLightComponent;

/** Granada de mão (quica, explode no tempo) e também foguete do robô (explode no impacto). */
UCLASS()
class RUPTURA_API AGrenade : public AActor
{
	GENERATED_BODY()

public:
	AGrenade();

	virtual void Tick(float DeltaSeconds) override;

	/** Configura e lança. bRocket = explode ao tocar em algo, inclusive personagens. */
	void Launch(const FVector& Velocity, bool bInRocket);

	float GetTimeLeft() const;
	bool IsRocket() const { return bRocket; }

	/** Granadas vivas no mundo (a IA usa para fugir delas). */
	static void GetActiveGrenades(TArray<AGrenade*>& Out);

	UPROPERTY(EditAnywhere, Category = "Granada") float FuseTime = 2.4f;
	UPROPERTY(EditAnywhere, Category = "Granada") float BaseDamage = 150.f;
	UPROPERTY(EditAnywhere, Category = "Granada") float MinDamage = 10.f;
	UPROPERTY(EditAnywhere, Category = "Granada") float InnerRadius = 180.f;
	UPROPERTY(EditAnywhere, Category = "Granada") float OuterRadius = 650.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void OnStop(const FHitResult& ImpactResult);

	void Explode();

	UPROPERTY(VisibleAnywhere, Category = "Granada") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category = "Granada") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Granada") TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(VisibleAnywhere, Category = "Granada") TObjectPtr<UPointLightComponent> Blink;

	bool bRocket = false;
	bool bExploded = false;
	float Age = 0.f;
	float TrailTimer = 0.f;
	float LastBounceSound = -10.f;
};
