#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/RupturaAssets.h"
#include "FxActor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Efeito visual efêmero feito de geometria 3D + luz dinâmica (traçantes, clarões, faíscas, explosões).
 * É o substituto funcional dos sistemas Niagara enquanto não há assets de partículas no projeto.
 */
UCLASS()
class RUPTURA_API AFxActor : public AActor
{
	GENERATED_BODY()

public:
	AFxActor();

	virtual void Tick(float DeltaSeconds) override;

	void Setup(RupturaAssets::EShape Shape, const FLinearColor& Color, float Emissive, const FVector& StartSize, const FVector& EndSize, float Lifetime,
		float LightIntensity = 0.f, const FLinearColor& LightColor = FLinearColor::White, float LightRadius = 0.f);

	FVector Velocity = FVector::ZeroVector;
	float Gravity = 0.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Ruptura")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Ruptura")
	TObjectPtr<UPointLightComponent> Light;

	float Age = 0.f;
	float Life = 0.2f;
	FVector StartScale = FVector::OneVector;
	FVector EndScale = FVector::OneVector;
	float LightPeak = 0.f;
};

namespace RupturaFx
{
	RUPTURA_API void Tracer(UWorld* World, const FVector& Start, const FVector& End, const FLinearColor& Color);
	RUPTURA_API void Impact(UWorld* World, const FVector& Location, const FVector& Normal, bool bFlesh);
	RUPTURA_API void MuzzleFlash(UWorld* World, const FVector& Location, const FRotator& Rotation, const FLinearColor& Color, float Scale = 1.f);
	RUPTURA_API void Sparks(UWorld* World, const FVector& Location, const FLinearColor& Color, int32 Count, float Speed = 500.f);
	RUPTURA_API void Explosion(UWorld* World, const FVector& Location, float Radius);
	RUPTURA_API void Smoke(UWorld* World, const FVector& Location, float Size, float Lifetime);
}
