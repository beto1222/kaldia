#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/RupturaAssets.h"
#include "CityBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;

USTRUCT()
struct FCityLight
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	/** Malha emissiva da lâmpada (apagada junto com a luz). */
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> LampISM;
	int32 LampInstance = INDEX_NONE;
	float Base = 0.f;
	uint8 Mode = 0; // 0 fixa, 1 tremulante, 2 defeituosa (pisca), 3 farol pulsante, 4 emergência/alarme
	float Phase = 0.f;
	/** Momento agendado para a lâmpada estourar (< 0 = nunca). */
	float DieAt = -1.f;
	bool bDieEffects = false;
	bool bDead = false;
};

/**
 * Constrói a fase inteira em tempo de execução com geometria instanciada (avenida morta,
 * instalação militar, túnel, ala subterrânea e salão do reator tomado por carne), iluminação
 * dinâmica (Lumen), neblina volumétrica densa, pós-processamento de terror, chuva e relâmpagos.
 * Também controla os apagões: lâmpadas que estouram em cascata, piscadas gerais e luz de emergência.
 */
UCLASS()
class RUPTURA_API ACityBuilder : public AActor
{
	GENERATED_BODY()

public:
	ACityBuilder();

	virtual void Tick(float DeltaSeconds) override;

	void Build();
	/** Liga/desliga as luzes vermelhas de emergência. */
	void SetAlarm(bool bOn);
	/**
	 * Apaga as luzes dentro de Area, uma a uma, começando pela mais próxima de From.
	 * StartDelay = 0 e Interval = 0 apagam tudo na hora (usado ao carregar checkpoints).
	 * As luzes de emergência sobrevivem.
	 */
	void BlackoutArea(const FBox& Area, const FVector& From, float StartDelay, float Interval, bool bEffects);
	/** Todas as luzes falham ao mesmo tempo por alguns segundos. */
	void Flicker(float Duration);
	/** Estoura a lâmpada mais próxima de Location (dentro de Radius). */
	void PopLightNear(const FVector& Location, float Radius);

protected:
	enum class EMat : uint8
	{
		Asphalt, Concrete, WetConcrete, MilConcrete, Metal, DarkMetal, Glass, WindowLit,
		CarRed, CarBlue, CarWhite, Burned, Rubber, Sandbag, Hazard, ContainerRust, ContainerTeal,
		EmRed, EmCyan, EmOrange, EmWhite, Water, Rubble, Blood, Flesh, FleshDark, Sheet, Bone, Count
	};

	UInstancedStaticMeshComponent* GetISM(EMat Mat, RupturaAssets::EShape Shape);
	int32 Box(EMat Mat, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator);
	int32 Cyl(EMat Mat, const FVector& Center, float Diameter, float Height, const FRotator& Rotation = FRotator::ZeroRotator);
	int32 Blob(EMat Mat, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator);
	UPointLightComponent* AddLight(const FVector& Location, const FLinearColor& Color, float Candelas, float Radius, uint8 Mode, bool bShadows = false);
	/** Associa a última luz criada à malha emissiva da lâmpada, para apagá-las juntas. */
	void LinkLampMesh(EMat Mat, int32 Instance);
	void KillLight(FCityLight& L, bool bEffects);

	void BuildAtmosphere();
	void BuildStreet();
	void BuildBuilding(float X0, float X1, float Height, float Side, bool bDestroyed);
	void BuildCar(const FVector& Center, float Yaw, EMat Paint, bool bBurned);
	void BuildCorpse(const FVector& Location, float Yaw);
	void BuildBloodTrail(const FVector& Start, const FVector& End);
	void BuildCompound();
	void BuildTunnel();
	void BuildSecurityWing();
	void BuildReactor();
	void BuildRain();

	void UpdateLights(float DeltaSeconds);
	void UpdateLightning(float DeltaSeconds);
	void UpdateRain(float DeltaSeconds);

	UPROPERTY(Transient) TMap<int32, TObjectPtr<UInstancedStaticMeshComponent>> Instanced;
	UPROPERTY(Transient) TArray<FCityLight> Lights;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> RainMesh;
	UPROPERTY(Transient) TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY(Transient) TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(Transient) TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(Transient) TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(Transient) TObjectPtr<UPostProcessComponent> PostProcess;

	FRandomStream Rng;
	TArray<FVector> RainDrops;
	TArray<FTransform> RainTransforms;
	bool bBuilt = false;
	bool bAlarm = false;
	float Time = 0.f;
	float MoonBase = 0.12f;
	float SkyBase = 0.15f;
	float NextLightning = 8.f;
	float LightningAge = 100.f;
	float ThunderAt = -1.f;
	float FlickerUntil = -1.f;
};
