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
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, 35.f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(60.f);
	Glow->SetAttenuationRadius(260.f);
	Glow->SetCastShadows(false);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
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
		UMaterialInterface* White = RupturaAssets::MakeMaterial(this, FLinearColor(0.85f, 0.85f, 0.85f), 0.5f, 0.f, 0.f);
		UMaterialInterface* Red = RupturaAssets::MakeMaterial(this, FLinearColor(1.f, 0.05f, 0.05f), 0.4f, 0.f, 4.f);
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector::ZeroVector, FVector(40.f, 28.f, 22.f), White));
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector(0.f, 0.f, 12.f), FVector(24.f, 7.f, 3.f), Red));
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector(0.f, 0.f, 12.f), FVector(7.f, 24.f, 3.f), Red));
		Glow->SetLightColor(FLinearColor(1.f, 0.2f, 0.2f));
		break;
	}
	case EPickupType::Granada:
	{
		UMaterialInterface* Olive = RupturaAssets::MakeMaterial(this, FLinearColor(0.08f, 0.1f, 0.06f), 0.5f, 0.3f, 0.f);
		UMaterialInterface* Band = RupturaAssets::MakeMaterial(this, FLinearColor(1.f, 0.6f, 0.05f), 0.4f, 0.f, 3.f);
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Sphere, FVector(-9.f, 0.f, 0.f), FVector(16.f), Olive));
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Sphere, FVector(9.f, 0.f, 0.f), FVector(16.f), Olive));
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector(0.f, 0.f, -10.f), FVector(40.f, 24.f, 4.f), Band));
		Glow->SetLightColor(FLinearColor(1.f, 0.6f, 0.1f));
		break;
	}
	default:
	{
		UMaterialInterface* Green = RupturaAssets::MakeMaterial(this, FLinearColor(0.08f, 0.12f, 0.06f), 0.6f, 0.2f, 0.f);
		UMaterialInterface* Stripe = RupturaAssets::MakeMaterial(this, FLinearColor(1.f, 0.8f, 0.1f), 0.4f, 0.f, 3.f);
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector::ZeroVector, FVector(46.f, 26.f, 24.f), Green));
		Parts.Add(RupturaAssets::AddPart(this, Pivot, EShape::Cube, FVector(0.f, 0.f, 4.f), FVector(47.f, 27.f, 4.f), Stripe));
		Glow->SetLightColor(FLinearColor(1.f, 0.85f, 0.3f));
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
		const int32 Added = W->AddReserve(1, Amount) + W->AddReserve(0, FMath::Max(6, Amount / 2));
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

	Pivot->SetRelativeRotation(FRotator(0.f, Age * 60.f, 0.f));
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, 35.f + FMath::Sin(Age * 2.f) * 5.f));

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
			static const TCHAR* Names[] = { TEXT("+ Kit médico"), TEXT("+ Munição"), TEXT("+ Granadas") };
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
