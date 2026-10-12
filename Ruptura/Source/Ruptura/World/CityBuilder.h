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
	float Base = 0.f;
	uint8 Mode = 0; // 0 fixa, 1 tremulante, 2 defeituosa (pisca), 3 farol pulsante, 4 alarme
	float Phase = 0.f;
};

/**
 * Constrói a fase inteira em tempo de execução com geometria instanciada (cidade destruída,
 * instalação militar, túnel, ala subterrânea e salão do reator), iluminação dinâmica (Lumen),
 * neblina volumétrica, pós-processamento, chuva e relâmpagos.
 */
UCLASS()
class RUPTURA_API ACityBuilder : public AActor
{
	GENERATED_BODY()

public:
	ACityBuilder();

	virtual void Tick(float DeltaSeconds) override;

	void Build();
	void SetAlarm(bool bOn);

protected:
	enum class EMat : uint8
	{
		Asphalt, Concrete, WetConcrete, MilConcrete, Metal, DarkMetal, Glass, WindowLit,
		CarRed, CarBlue, CarWhite, Burned, Rubber, Sandbag, Hazard, ContainerRust, ContainerTeal,
		EmRed, EmCyan, EmOrange, EmWhite, Water, Rubble, Count
	};

	UInstancedStaticMeshComponent* GetISM(EMat Mat, RupturaAssets::EShape Shape);
	void Box(EMat Mat, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator);
	void Cyl(EMat Mat, const FVector& Center, float Diameter, float Height, const FRotator& Rotation = FRotator::ZeroRotator);
	UPointLightComponent* AddLight(const FVector& Location, const FLinearColor& Color, float Candelas, float Radius, uint8 Mode, bool bShadows = false);

	void BuildAtmosphere();
	void BuildStreet();
	void BuildBuilding(float X0, float X1, float Height, float Side, bool bDestroyed);
	void BuildCar(const FVector& Center, float Yaw, EMat Paint, bool bBurned);
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
	float MoonBase = 1.5f;
	float NextLightning = 6.f;
	float LightningAge = 100.f;
	float ThunderAt = -1.f;
};
