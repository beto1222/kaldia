#pragma once

#include "CoreMinimal.h"
#include "Mission/InteractableActor.h"
#include "Core/InfectedBody.h"
#include "MissionActors.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UBoxComponent;
class UHealthComponent;
class UMaterialInstanceDynamic;

/** Crachá de acesso largado sobre a mesa do posto da guarda. */
UCLASS()
class RUPTURA_API AKeycardPickup : public AInteractableActor
{
	GENERATED_BODY()

public:
	AKeycardPickup();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FText GetInteractPrompt(const AKaelCharacter* Kael) const override;
	virtual void Interact(AKaelCharacter* Kael) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Cartao") TObjectPtr<UStaticMeshComponent> Card;
	UPROPERTY(VisibleAnywhere, Category = "Cartao") TObjectPtr<UPointLightComponent> Glow;
	FVector BaseLocation = FVector::ZeroVector;
	float Age = 0.f;
};

/** Terminal da sala de controle: restaura a energia de emergência e destrava a porta do reator (segurar E). */
UCLASS()
class RUPTURA_API ASecurityTerminal : public AInteractableActor
{
	GENERATED_BODY()

public:
	ASecurityTerminal();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FText GetInteractPrompt(const AKaelCharacter* Kael) const override;
	virtual bool CanInteract(const AKaelCharacter* Kael) const override { return !bHacked; }
	virtual float GetInteractHoldTime() const override { return 3.5f; }
	virtual void Interact(AKaelCharacter* Kael) override;
	virtual FVector GetInteractLocation() const override;

	void SetHacked(bool bSilent);
	bool IsHacked() const { return bHacked; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Terminal") TObjectPtr<UStaticMeshComponent> Console;
	UPROPERTY(VisibleAnywhere, Category = "Terminal") TObjectPtr<UStaticMeshComponent> Screen;
	UPROPERTY(VisibleAnywhere, Category = "Terminal") TObjectPtr<UPointLightComponent> ScreenLight;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ScreenMaterial;
	bool bHacked = false;
	float BlinkTime = 0.f;
};

/** Núcleo do Protocolo Zero. Envolvido por uma membrana de carne até o Hospedeiro morrer. */
UCLASS()
class RUPTURA_API APowerCore : public AActor
{
	GENERATED_BODY()

public:
	APowerCore();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	void SetShield(bool bOn);
	bool IsShielded() const { return bShielded; }
	bool IsDestroyedCore() const { return bCoreDestroyed; }
	/** Usado ao carregar o checkpoint de fuga: já começa destruído, sem efeitos. */
	void SetDestroyedState();
	UHealthComponent* GetHealth() const { return Health; }

protected:
	UFUNCTION()
	void HandleDeath(UHealthComponent* Component, AActor* Killer);

	UPROPERTY(VisibleAnywhere, Category = "Nucleo") TObjectPtr<UStaticMeshComponent> Core;
	UPROPERTY(VisibleAnywhere, Category = "Nucleo") TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(VisibleAnywhere, Category = "Nucleo") TObjectPtr<UHealthComponent> Health;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Rings;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Pylons;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Arcs;

	bool bShielded = true;
	bool bCoreDestroyed = false;
	float LastShieldHint = -100.f;
	int32 Blasts = 0;
	float NextBlast = 0.f;
	float Age = 0.f;
};

/** Volume de gatilho de objetivo (chegar a um lugar). */
UCLASS()
class RUPTURA_API AObjectiveZone : public AActor
{
	GENERATED_BODY()

public:
	AObjectiveZone();
	void Configure(FName InZoneId, const FVector& Extent);
	FName GetZoneId() const { return ZoneId; }

protected:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "Zona") TObjectPtr<UBoxComponent> Box;
	FName ZoneId;
};

/**
 * Aparição: um vulto parado, encarando o jogador de longe. Some quando ele se aproxima,
 * quando a lanterna o ilumina ou depois de alguns segundos — deixando a dúvida se estava mesmo lá.
 */
UCLASS()
class RUPTURA_API AApparition : public AActor
{
	GENERATED_BODY()

public:
	AApparition();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Aparicao") float VanishDistance = 1500.f;
	UPROPERTY(EditAnywhere, Category = "Aparicao") float MaxLife = 14.f;

protected:
	void Vanish();

	UPROPERTY(VisibleAnywhere, Category = "Aparicao") TObjectPtr<USceneComponent> BodyRoot;
	UPROPERTY() FInfectedBody Body;

	float Age = 0.f;
	float LitTime = 0.f;
	bool bVanished = false;
};
