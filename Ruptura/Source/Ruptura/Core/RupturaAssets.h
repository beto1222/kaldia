#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;
class USoundAttenuation;
class UAudioComponent;

/**
 * Acesso centralizado a malhas, materiais e sons.
 *
 * Malhas: formas básicas do próprio motor (/Engine/BasicShapes) — placeholders 3D funcionais.
 * Material: usa /Game/Materials/M_Ruptura_Base (criado pelo script Scripts/setup_projeto.py, com
 *           parâmetros Cor/Rugosidade/Metalico/Emissivo). Se não existir, cai para o material básico
 *           do motor (só cor), sem quebrar o jogo.
 * Sons: /Game/Audio/<nome> (importados de AudioFonte/ pelo mesmo script). Som ausente = silêncio + aviso no log.
 */
namespace RupturaAssets
{
	enum class EShape : uint8 { Cube, Sphere, Cylinder, Cone };

	RUPTURA_API UStaticMesh* Mesh(EShape Shape);
	RUPTURA_API bool HasCustomMaterial();
	RUPTURA_API UMaterialInstanceDynamic* MakeMaterial(UObject* Outer, const FLinearColor& Color, float Roughness = 0.6f, float Metallic = 0.f, float Emissive = 0.f);

	/** Cria uma peça de malha sem colisão, anexada a Parent. Size em centímetros. */
	RUPTURA_API UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, EShape Shape, const FVector& Location, const FVector& Size, UMaterialInterface* Material, const FRotator& Rotation = FRotator::ZeroRotator);
	/** Cria um ponto de articulação (scene component) usado na animação procedural. */
	RUPTURA_API USceneComponent* AddPivot(AActor* Owner, USceneComponent* Parent, const FVector& Location);

	RUPTURA_API USoundBase* Sound(FName Name);
	RUPTURA_API USoundAttenuation* Attenuation();
	RUPTURA_API void PlaySound3D(const UObject* WorldContext, FName Name, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);
	RUPTURA_API void PlaySound2D(const UObject* WorldContext, FName Name, float Volume = 1.f, float Pitch = 1.f);
	RUPTURA_API UAudioComponent* CreateLoop2D(UObject* WorldContext, FName Name, float Volume);
}
