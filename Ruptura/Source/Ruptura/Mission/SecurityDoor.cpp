#include "Mission/SecurityDoor.h"
#include "Player/KaelCharacter.h"
#include "Core/RupturaAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/CollisionProfile.h"

namespace
{
	UStaticMeshComponent* MakeDoorPart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, bool bCollide)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		if (bCollide)
		{
			C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			C->SetCanEverAffectNavigation(true);
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCanEverAffectNavigation(false);
		}
		return C;
	}
}

ASecurityDoor::ASecurityDoor()
{
	PanelL = MakeDoorPart(this, RootComponent, TEXT("PanelL"), true);
	PanelR = MakeDoorPart(this, RootComponent, TEXT("PanelR"), true);
	Header = MakeDoorPart(this, RootComponent, TEXT("Header"), false);
	Lamp = MakeDoorPart(this, RootComponent, TEXT("Lamp"), false);

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(RootComponent);
	StatusLight->SetMobility(EComponentMobility::Movable);
	StatusLight->SetIntensityUnits(ELightUnits::Candelas);
	StatusLight->SetIntensity(250.f);
	StatusLight->SetAttenuationRadius(700.f);
	StatusLight->SetCastShadows(false);

	InteractRadius = 320.f;
}

void ASecurityDoor::Configure(float InWidth, float InHeight, bool bNeedsKeycard, bool bLockedBySecurity, const FText& InName)
{
	Width = InWidth;
	Height = InHeight;
	bRequiresKeycard = bNeedsKeycard;
	bLocked = bLockedBySecurity;
	DoorName = InName;

	UStaticMesh* Cube = RupturaAssets::Mesh(RupturaAssets::EShape::Cube);
	// Aço encardido com faixas de alerta gastas.
	UMaterialInterface* PanelMat = RupturaAssets::MakeMaterial(this, FLinearColor(0.07f, 0.075f, 0.075f), 0.55f, 0.85f, 0.f);
	UMaterialInterface* StripeMat = RupturaAssets::MakeMaterial(this, FLinearColor(0.4f, 0.27f, 0.04f), 0.75f, 0.1f, 0.f);

	PanelL->SetStaticMesh(Cube);
	PanelR->SetStaticMesh(Cube);
	PanelL->SetMaterial(0, PanelMat);
	PanelR->SetMaterial(0, PanelMat);

	Header->SetStaticMesh(Cube);
	Header->SetMaterial(0, StripeMat);
	Header->SetRelativeLocation(FVector(-30.f, 0.f, Height + 20.f));
	Header->SetRelativeScale3D(FVector(0.4f, (Width + 80.f) / 100.f, 0.4f));

	LampMaterial = RupturaAssets::MakeMaterial(this, FLinearColor::Red, 0.2f, 0.f, 6.f);
	Lamp->SetStaticMesh(Cube);
	Lamp->SetMaterial(0, LampMaterial);
	Lamp->SetRelativeLocation(FVector(-52.f, 0.f, Height + 20.f));
	Lamp->SetRelativeScale3D(FVector(0.08f, 0.6f, 0.15f));
	StatusLight->SetRelativeLocation(FVector(-90.f, 0.f, Height + 10.f));

	LayoutPanels();
	UpdateStatus();
}

void ASecurityDoor::LayoutPanels()
{
	const float HalfW = Width * 0.5f;
	const float Slide = (HalfW - 20.f) * OpenAlpha;
	const FVector PanelScale(0.3f, HalfW / 100.f, Height / 100.f);
	PanelL->SetRelativeScale3D(PanelScale);
	PanelR->SetRelativeScale3D(PanelScale);
	PanelL->SetRelativeLocation(FVector(0.f, HalfW * 0.5f + Slide, Height * 0.5f));
	PanelR->SetRelativeLocation(FVector(0.f, -HalfW * 0.5f - Slide, Height * 0.5f));
}

void ASecurityDoor::UpdateStatus()
{
	FLinearColor C = FLinearColor(0.1f, 1.f, 0.3f);
	if (bLocked)
	{
		C = FLinearColor(1.f, 0.05f, 0.03f);
	}
	else if (!bOpen && bRequiresKeycard)
	{
		C = FLinearColor(1.f, 0.6f, 0.05f);
	}
	if (LampMaterial)
	{
		LampMaterial->SetVectorParameterValue(TEXT("Cor"), C);
		LampMaterial->SetVectorParameterValue(TEXT("Color"), C);
	}
	StatusLight->SetLightColor(C);
}

void ASecurityDoor::Unlock()
{
	bLocked = false;
	UpdateStatus();
}

void ASecurityDoor::Open(bool bInstant)
{
	if (bOpen)
	{
		return;
	}
	bOpen = true;
	bLocked = false;
	if (bInstant)
	{
		OpenAlpha = 1.f;
		LayoutPanels();
	}
	else
	{
		RupturaAssets::PlaySound3D(this, TEXT("porta"), GetActorLocation() + FVector(0.f, 0.f, 150.f), 1.f);
	}
	UpdateStatus();
}

void ASecurityDoor::Close(bool bInstant, bool bLock)
{
	bLocked = bLock;
	if (bOpen)
	{
		bOpen = false;
		if (bInstant)
		{
			OpenAlpha = 0.f;
			LayoutPanels();
		}
		else
		{
			RupturaAssets::PlaySound3D(this, TEXT("porta_batida"), GetActorLocation() + FVector(0.f, 0.f, 150.f), 1.f);
		}
	}
	UpdateStatus();
}

void ASecurityDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Target = bOpen ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(OpenAlpha, Target))
	{
		OpenAlpha = FMath::FInterpConstantTo(OpenAlpha, Target, DeltaSeconds, bOpen ? 0.5f : 4.f);
		LayoutPanels();
	}
}

FText ASecurityDoor::GetInteractPrompt(const AKaelCharacter* Kael) const
{
	if (bOpen)
	{
		return FText::GetEmpty();
	}
	if (bLocked)
	{
		return FText::FromString(FString::Printf(TEXT("%s — travada. Sem energia."), *DoorName.ToString()));
	}
	if (bRequiresKeycard && !(Kael && Kael->HasKeycard()))
	{
		return FText::FromString(FString::Printf(TEXT("%s — precisa de um cartão de acesso"), *DoorName.ToString()));
	}
	return FText::FromString(bRequiresKeycard ? TEXT("Usar cartão de acesso") : TEXT("Abrir porta"));
}

bool ASecurityDoor::CanInteract(const AKaelCharacter* Kael) const
{
	return !bOpen && !bLocked && (!bRequiresKeycard || (Kael && Kael->HasKeycard()));
}

void ASecurityDoor::Interact(AKaelCharacter* Kael)
{
	if (CanInteract(Kael))
	{
		if (bRequiresKeycard)
		{
			RupturaAssets::PlaySound3D(this, TEXT("cartao"), GetInteractLocation(), 1.f);
		}
		Open();
	}
}

FVector ASecurityDoor::GetInteractLocation() const
{
	return GetActorLocation() + GetActorForwardVector() * -40.f + FVector(0.f, 0.f, 120.f);
}
