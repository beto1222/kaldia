#include "Core/FxActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

AFxActor::AFxActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->CastShadow = false;

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetCastShadows(false);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.f);
	Light->SetVisibility(false);
	Light->SetAbsolute(false, false, true); // a escala do mesh não afeta a luz
}

void AFxActor::Setup(RupturaAssets::EShape Shape, const FLinearColor& Color, float Emissive, const FVector& StartSize, const FVector& EndSize, float Lifetime,
	float LightIntensity, const FLinearColor& LightColor, float LightRadius)
{
	Mesh->SetStaticMesh(RupturaAssets::Mesh(Shape));
	Mesh->SetMaterial(0, RupturaAssets::MakeMaterial(this, Color, 0.5f, 0.f, Emissive));
	StartScale = StartSize / 100.f;
	EndScale = EndSize / 100.f;
	Mesh->SetWorldScale3D(StartScale);
	Life = FMath::Max(0.01f, Lifetime);
	Age = 0.f;

	LightPeak = LightIntensity;
	if (LightIntensity > 0.f)
	{
		Light->SetVisibility(true);
		Light->SetLightColor(LightColor);
		Light->SetAttenuationRadius(LightRadius > 0.f ? LightRadius : 800.f);
		Light->SetIntensity(LightIntensity);
	}
}

void AFxActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Life, 0.f, 1.f);

	if (!Velocity.IsNearlyZero() || Gravity != 0.f)
	{
		Velocity.Z -= Gravity * DeltaSeconds;
		AddActorWorldOffset(Velocity * DeltaSeconds);
	}

	// Ease-out na escala para dar "estouro" inicial.
	const float Ease = 1.f - FMath::Square(1.f - T);
	Mesh->SetWorldScale3D(FMath::Lerp(StartScale, EndScale, Ease));

	if (LightPeak > 0.f)
	{
		Light->SetIntensity(LightPeak * FMath::Square(1.f - T));
	}

	if (Age >= Life)
	{
		Destroy();
	}
}

namespace
{
	AFxActor* SpawnFx(UWorld* World, const FVector& Location, const FRotator& Rotation)
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<AFxActor>(AFxActor::StaticClass(), Location, Rotation, Params);
	}
}

void RupturaFx::Tracer(UWorld* World, const FVector& Start, const FVector& End, const FLinearColor& Color)
{
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	if (Length < 10.f)
	{
		return;
	}
	if (AFxActor* Fx = SpawnFx(World, Start + Delta * 0.5f, Delta.Rotation()))
	{
		Fx->Setup(RupturaAssets::EShape::Cube, Color, 12.f, FVector(Length, 2.5f, 2.5f), FVector(Length, 0.4f, 0.4f), 0.06f);
	}
}

void RupturaFx::Impact(UWorld* World, const FVector& Location, const FVector& Normal, bool bFlesh)
{
	const FLinearColor Color = bFlesh ? FLinearColor(0.35f, 0.02f, 0.02f) : FLinearColor(1.f, 0.6f, 0.25f);
	if (AFxActor* Fx = SpawnFx(World, Location + Normal * 2.f, Normal.Rotation()))
	{
		Fx->Setup(RupturaAssets::EShape::Sphere, Color, bFlesh ? 0.f : 6.f, FVector(6.f), FVector(bFlesh ? 26.f : 14.f), bFlesh ? 0.18f : 0.1f,
			bFlesh ? 0.f : 600.f, FLinearColor(1.f, 0.7f, 0.4f), 250.f);
	}
	if (!bFlesh)
	{
		Sparks(World, Location + Normal * 3.f, FLinearColor(1.f, 0.75f, 0.35f), 4, 450.f);
		// Poeira do impacto
		if (AFxActor* Dust = SpawnFx(World, Location + Normal * 6.f, FRotator::ZeroRotator))
		{
			Dust->Setup(RupturaAssets::EShape::Sphere, FLinearColor(0.12f, 0.12f, 0.12f), 0.f, FVector(8.f), FVector(40.f), 0.45f);
			Dust->Velocity = Normal * 60.f;
		}
	}
}

void RupturaFx::MuzzleFlash(UWorld* World, const FVector& Location, const FRotator& Rotation, const FLinearColor& Color, float Scale)
{
	if (AFxActor* Fx = SpawnFx(World, Location, Rotation))
	{
		// Altura do cone fica no eixo Z local da malha.
		Fx->Setup(RupturaAssets::EShape::Cone, Color, 20.f, FVector(14.f, 14.f, 22.f) * Scale, FVector(4.f, 4.f, 34.f) * Scale, 0.05f,
			2500.f * Scale, Color, 700.f);
		// O cone do motor aponta para +Z; gira para apontar para frente.
		Fx->AddActorLocalRotation(FRotator(-90.f, 0.f, 0.f));
	}
}

void RupturaFx::Sparks(UWorld* World, const FVector& Location, const FLinearColor& Color, int32 Count, float Speed)
{
	for (int32 i = 0; i < Count; ++i)
	{
		if (AFxActor* Fx = SpawnFx(World, Location, FRotator::ZeroRotator))
		{
			Fx->Setup(RupturaAssets::EShape::Cube, Color, 15.f, FVector(2.5f), FVector(0.5f), FMath::FRandRange(0.2f, 0.45f));
			Fx->Velocity = FMath::VRand() * Speed * FMath::FRandRange(0.4f, 1.f) + FVector(0.f, 0.f, Speed * 0.4f);
			Fx->Gravity = 980.f;
		}
	}
}

void RupturaFx::Smoke(UWorld* World, const FVector& Location, float Size, float Lifetime)
{
	if (AFxActor* Fx = SpawnFx(World, Location, FRotator::ZeroRotator))
	{
		Fx->Setup(RupturaAssets::EShape::Sphere, FLinearColor(0.05f, 0.05f, 0.055f), 0.f, FVector(Size * 0.3f), FVector(Size), Lifetime);
		Fx->Velocity = FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(60.f, 140.f));
	}
}

void RupturaFx::Explosion(UWorld* World, const FVector& Location, float Radius)
{
	// Bola de fogo + clarão de luz forte
	if (AFxActor* Core = SpawnFx(World, Location, FRotator::ZeroRotator))
	{
		Core->Setup(RupturaAssets::EShape::Sphere, FLinearColor(1.f, 0.45f, 0.08f), 25.f, FVector(Radius * 0.2f), FVector(Radius * 1.1f), 0.45f,
			60000.f, FLinearColor(1.f, 0.55f, 0.2f), Radius * 6.f);
	}
	if (AFxActor* Hot = SpawnFx(World, Location, FRotator::ZeroRotator))
	{
		Hot->Setup(RupturaAssets::EShape::Sphere, FLinearColor(1.f, 0.9f, 0.6f), 40.f, FVector(Radius * 0.1f), FVector(Radius * 0.6f), 0.18f);
	}
	// Fumaça
	for (int32 i = 0; i < 5; ++i)
	{
		Smoke(World, Location + FMath::VRand() * Radius * 0.3f, Radius * FMath::FRandRange(0.8f, 1.3f), FMath::FRandRange(2.f, 3.5f));
	}
	// Destroços
	for (int32 i = 0; i < 10; ++i)
	{
		if (AFxActor* Debris = SpawnFx(World, Location, FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f)))
		{
			const float S = FMath::FRandRange(6.f, 16.f);
			Debris->Setup(RupturaAssets::EShape::Cube, FLinearColor(0.06f, 0.05f, 0.04f), 0.f, FVector(S), FVector(S * 0.8f), FMath::FRandRange(1.f, 1.8f));
			FVector Dir = FMath::VRand();
			Dir.Z = FMath::Abs(Dir.Z) + 0.4f;
			Debris->Velocity = Dir.GetSafeNormal() * FMath::FRandRange(400.f, 1100.f);
			Debris->Gravity = 980.f;
		}
	}
	Sparks(World, Location, FLinearColor(1.f, 0.6f, 0.2f), 10, 900.f);
}
