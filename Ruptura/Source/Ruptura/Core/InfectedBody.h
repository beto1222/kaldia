#pragma once

#include "CoreMinimal.h"
#include "InfectedBody.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/**
 * Corpo humanoide de um infectado montado com formas básicas e animado proceduralmente:
 * andar arrastado e mancando, tronco curvado, braços estendidos na perseguição, espasmos
 * bruscos da cabeça e golpe com o braço. Substituível por Skeletal Mesh + Animation Blueprint.
 */
USTRUCT()
struct FInfectedBody
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Pelvis;
	UPROPERTY() TObjectPtr<USceneComponent> Chest;
	UPROPERTY() TObjectPtr<USceneComponent> Head;
	UPROPERTY() TObjectPtr<USceneComponent> HipL;
	UPROPERTY() TObjectPtr<USceneComponent> HipR;
	UPROPERTY() TObjectPtr<USceneComponent> KneeL;
	UPROPERTY() TObjectPtr<USceneComponent> KneeR;
	UPROPERTY() TObjectPtr<USceneComponent> ShoulderL;
	UPROPERTY() TObjectPtr<USceneComponent> ShoulderR;
	UPROPERTY() TObjectPtr<USceneComponent> ElbowL;
	UPROPERTY() TObjectPtr<USceneComponent> ElbowR;

	float Scale = 1.f;
	float Phase = 0.f;
	/** Quanto a perna esquerda arrasta (0 = normal, 1 = manca muito). Sorteado por indivíduo. */
	float Limp = 0.f;
	/** Inclinação permanente da cabeça (graus de roll), sorteada por indivíduo. */
	float HeadTilt = 0.f;
	float Seed = 0.f;
	FRotator TwitchTarget = FRotator::ZeroRotator;
	FRotator TwitchCurrent = FRotator::ZeroRotator;
	float NextTwitch = 0.f;
	float Clock = 0.f;
};

struct FInfectedLook
{
	FLinearColor Skin = FLinearColor(0.3f, 0.27f, 0.24f);
	FLinearColor Shirt = FLinearColor(0.06f, 0.065f, 0.07f);
	FLinearColor Pants = FLinearColor(0.035f, 0.035f, 0.04f);
	/** Brilho dos olhos refletindo a lanterna. */
	float EyeShine = 2.f;
	bool bMutated = false;
};

/** Estado instantâneo usado pela animação. */
struct FInfectedPose
{
	float Speed = 0.f;
	bool bChasing = false;
	/** Progresso do golpe (0..1) ou negativo se não está atacando. */
	float Attack = -1.f;
	/** 1 logo após levar dano, decai até 0. */
	float Flinch = 0.f;
};

namespace InfectedBody
{
	/** Sorteia uma aparência (pele, roupas) para variar a multidão. */
	RUPTURA_API FInfectedLook RandomLook(FRandomStream& Rng, bool bMutated);
	/** Monta o corpo com os pés na origem de Root (eixo X = frente). */
	RUPTURA_API void Build(AActor* Owner, USceneComponent* Root, const FInfectedLook& Look, float Scale, FInfectedBody& Out);
	RUPTURA_API void Animate(FInfectedBody& Body, const FInfectedPose& Pose, float DeltaSeconds);
}
