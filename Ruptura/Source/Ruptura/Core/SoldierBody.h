#pragma once

#include "CoreMinimal.h"
#include "SoldierBody.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/**
 * Corpo humanoide placeholder montado com formas básicas e animado proceduralmente
 * (passada das pernas, braços de mira, coice da arma). Substituível por um Skeletal Mesh
 * + Animation Blueprint (ex.: Manny do pacote Third Person) — ver Docs/RELATORIO.md.
 */
USTRUCT()
struct FSoldierBody
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> HipL;
	UPROPERTY() TObjectPtr<USceneComponent> HipR;
	UPROPERTY() TObjectPtr<USceneComponent> ShoulderL;
	UPROPERTY() TObjectPtr<USceneComponent> ShoulderR;
	UPROPERTY() TObjectPtr<USceneComponent> GunPivot;
	UPROPERTY() TObjectPtr<USceneComponent> Muzzle;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Gun;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Visor;

	float Phase = 0.f;
	float Kick = 0.f;
	float Scale = 1.f;
	FVector GunRestLocation = FVector::ZeroVector;
};

struct FSoldierColors
{
	FLinearColor Armor = FLinearColor(0.08f, 0.1f, 0.07f);
	FLinearColor Suit = FLinearColor(0.12f, 0.12f, 0.13f);
	FLinearColor Visor = FLinearColor(0.1f, 0.8f, 1.f);
	float VisorGlow = 8.f;
};

namespace SoldierBody
{
	/** Monta o corpo com os pés na origem de Root (eixo X = frente). */
	RUPTURA_API void Build(AActor* Owner, USceneComponent* Root, const FSoldierColors& Colors, float Scale, FSoldierBody& Out);
	RUPTURA_API void Animate(FSoldierBody& Body, float Speed, float DeltaSeconds, bool bAiming);
	RUPTURA_API void SetGunLength(FSoldierBody& Body, float LengthCm);
}
