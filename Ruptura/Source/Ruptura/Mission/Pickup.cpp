#include "Mission/Pickup.h"
#include "Player/KaelCharacter.h"
#include "Player/KaelPlayerController.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Core/RupturaAssets.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

using RupturaAssets::EShape;

APickup::APickup()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, 8.f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(18.f);
	Glow->SetAttenuationRadius(180.f);
	Glow->SetCastShadows(false);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
}

void APickup::Configure(EPickupType InType, int32 InAmount, float InRespawnTime)
{
	Type = InType;
	Amount = InAmount;
	RespawnTime = InRespawnTime;

	for (UStaticMeshComponent* P : Parts)
	{
		if (P)
		{
			P->DestroyComponent();
		}
	}
	Parts.Reset();

	switch (Type)
	{
	case EPickupType::Vida:
	{
		// Kit de primeiros socorros encardido.
		UMaterialInterface* White = RupturaAssets::MakeMaterial(this, FLinearColor(0.55f, 0.53f, 0.5f), 0.6f, 0.f, 0.f);
		UMaterialInterface* Red = RupturaAssets::MakeMaterial(this, FLinearColor(0.45f, 0.03f, 0.03f), 0.5f, 0.f, 0.f);
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector::ZeroVector, FVector(36.f, 24.f, 14.f), White));
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector(0.f, 0.f, 7.5f), FVector(20.f, 6.f, 1.5f), Red));
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector(0.f, 0.f, 7.5f), FVector(6.f, 20.f, 1.5f), Red));
		Glow->SetLightColor(FLinearColor(1.f, 0.9f, 0.85f));
		break;
	}
	case EPickupType::Granada:
	{
		UMaterialInterface* Olive = RupturaAssets::MakeMaterial(this, FLinearColor(0.07f, 0.08f, 0.05f), 0.55f, 0.3f, 0.f);
		UMaterialInterface* Metal = RupturaAssets::MakeMaterial(this, FLinearColor(0.3f, 0.3f, 0.3f), 0.35f, 1.f, 0.f);
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Sphere, FVector::ZeroVector, FVector(12.f, 12.f, 15.f), Olive));
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cylinder, FVector(0.f, 0.f, 9.f), FVector(4.f, 4.f, 5.f), Metal));
		Glow->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
		break;
	}
	case EPickupType::Bateria:
	{
		// Par de pilhas grandes.
		UMaterialInterface* Body = RupturaAssets::MakeMaterial(this, FLinearColor(0.04f, 0.04f, 0.045f), 0.4f, 0.2f, 0.f);
		UMaterialInterface* Copper = RupturaAssets::MakeMaterial(this, FLinearColor(0.75f, 0.45f, 0.2f), 0.3f, 1.f, 0.f);
		for (const float Side : { 1.f, -1.f })
		{
			Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cylinder, FVector(0.f, 3.5f * Side, 0.f), FVector(6.f, 6.f, 14.f), Body, FRotator(90.f, 0.f, 0.f)));
			Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cylinder, FVector(7.f, 3.5f * Side, 0.f), FVector(6.2f, 6.2f, 3.f), Copper, FRotator(90.f, 0.f, 0.f)));
		}
		Glow->SetLightColor(FLinearColor(0.9f, 0.95f, 1.f));
		break;
	}
	default:
	{
		// Caixinha de munição de papelão.
		UMaterialInterface* Box = RupturaAssets::MakeMaterial(this, FLinearColor(0.18f, 0.12f, 0.06f), 0.85f, 0.f, 0.f);
		UMaterialInterface* Label = RupturaAssets::MakeMaterial(this, FLinearColor(0.5f, 0.45f, 0.3f), 0.8f, 0.f, 0.f);
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector::ZeroVector, FVector(18.f, 12.f, 9.f), Box));
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector(0.f, 6.2f, 0.f), FVector(10.f, 0.5f, 5.f), Label));
		Glow->SetLightColor(FLinearColor(1.f, 0.9f, 0.7f));
		break;
	}
	}
	bConfigured = true;
	SetAvailable(true);
}

void APickup::SetAvailable(bool bInAvailable)
{
	bAvailable = bInAvailable;
	Pivot->SetVisibility(bInAvailable, true);
	Glow->SetVisibility(bInAvailable);
}

bool APickup::TryApply(AKaelCharacter* Kael)
{
	switch (Type)
	{
	case EPickupType::Vida:
	{
		UHealthComponent* H = Kael->GetHealth();
		if (!H || H->Health >= H->MaxHealth)
		{
			return false;
		}
		H->Heal(static_cast<float>(Amount));
		RupturaAssets::PlaySound2D(this, TEXT("cura"), 0.9f);
		return true;
	}
	case EPickupType::Bateria:
	{
		if (Kael->IsBatteryFull())
		{
			return false;
		}
		Kael->AddBattery(static_cast<float>(Amount) / 100.f);
		RupturaAssets::PlaySound2D(this, TEXT("bateria"), 0.9f);
		return true;
	}
	case EPickupType::Granada:
	{
		if (Kael->IsGrenadesFull())
		{
			return false;
		}
		Kael->AddGrenades(Amount);
		RupturaAssets::PlaySound2D(this, TEXT("item"), 0.8f);
		return true;
	}
	default:
	{
		UWeaponComponent* W = Kael->GetWeapon();
		if (!W)
		{
			return false;
		}
		// Amount = balas de pistola; cartuchos de espingarda vêm em proporção menor.
		const int32 Added = W->AddReserve(0, Amount) + W->AddReserve(1, FMath::Max(1, Amount / 4));
		if (Added <= 0)
		{
			return false;
		}
		RupturaAssets::PlaySound2D(this, TEXT("municao"), 0.9f);
		return true;
	}
	}
}

void APickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (!bConfigured)
	{
		Configure(Type, Amount, RespawnTime);
	}

	if (!bAvailable)
	{
		if (RespawnTime > 0.f && GetWorld()->GetTimeSeconds() >= RespawnAt)
		{
			SetAvailable(true);
		}
		return;
	}

	// Reflexo fraco e irregular, como um brilho de lanterna em plástico/metal.
	Glow->SetIntensity(14.f + 8.f * FMath::Max(0.f, FMath::PerlinNoise1D(Age * 1.3f)));

	AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Kael || Kael->IsDead())
	{
		return;
	}
	const FVector Delta = Kael->GetActorLocation() - GetActorLocation();
	if (Delta.Size2D() > 110.f || FMath::Abs(Delta.Z) > 200.f)
	{
		return;
	}
	if (TryApply(Kael))
	{
		if (AKaelPlayerController* PC = Cast<AKaelPlayerController>(Kael->GetController()))
		{
			static const TCHAR* Names[] = { TEXT("+ Kit de primeiros socorros"), TEXT("+ Munição"), TEXT("+ Granada"), TEXT("+ Pilhas para a lanterna") };
			PC->ShowToast(FText::FromString(Names[static_cast<int32>(Type)]), 1.5f);
		}
		if (RespawnTime > 0.f)
		{
			SetAvailable(false);
			RespawnAt = GetWorld()->GetTimeSeconds() + RespawnTime;
		}
		else
		{
			Destroy();
		}
	}
}
