#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Pickup.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EPickupType : uint8
{
	Vida,
	Municao,
	Granada
};

/** Item coletado automaticamente ao passar por cima (kit médico, munição, granadas). */
UCLASS()
class RUPTURA_API APickup : public AActor
{
	GENERATED_BODY()

public:
	APickup();
	virtual void Tick(float DeltaSeconds) override;

	/** RespawnTime > 0 faz o item reaparecer (usado na arena do chefe). */
	void Configure(EPickupType InType, int32 InAmount, float InRespawnTime);

protected:
	bool TryApply(class AKaelCharacter* Kael);
	void SetAvailable(bool bInAvailable);

	UPROPERTY(VisibleAnywhere, Category = "Item") TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere, Category = "Item") TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	EPickupType Type = EPickupType::Municao;
	int32 Amount = 30;
	float RespawnTime = 0.f;
	bool bAvailable = true;
	float RespawnAt = 0.f;
	float Age = 0.f;
	bool bConfigured = false;
};
