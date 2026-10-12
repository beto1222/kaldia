#include "Core/RupturaAssets.h"
#include "Ruptura.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"

namespace
{
	TWeakObjectPtr<UMaterialInterface> GBaseMaterial;
	bool GTriedCustomMaterial = false;
	bool GHasCustomMaterial = false;

	TMap<FName, TWeakObjectPtr<USoundBase>> GSounds;
	TSet<FName> GMissingSounds;
	TWeakObjectPtr<USoundAttenuation> GAttenuation;

	const TCHAR* CustomMaterialPackage = TEXT("/Game/Materials/M_Ruptura_Base");
	const TCHAR* CustomMaterialPath = TEXT("/Game/Materials/M_Ruptura_Base.M_Ruptura_Base");
	const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	UMaterialInterface* GetBaseMaterial()
	{
		if (GBaseMaterial.IsValid())
		{
			return GBaseMaterial.Get();
		}

		UMaterialInterface* Material = nullptr;
		if (!GTriedCustomMaterial)
		{
			GTriedCustomMaterial = true;
			if (FPackageName::DoesPackageExist(CustomMaterialPackage))
			{
				Material = LoadObject<UMaterialInterface>(nullptr, CustomMaterialPath);
			}
			GHasCustomMaterial = Material != nullptr;
			if (!GHasCustomMaterial)
			{
				UE_LOG(LogRuptura, Warning, TEXT("M_Ruptura_Base não encontrado. Rode Scripts/setup_projeto.py no editor para materiais PBR. Usando material básico."));
			}
		}
		else if (GHasCustomMaterial)
		{
			Material = LoadObject<UMaterialInterface>(nullptr, CustomMaterialPath);
		}

		if (!Material)
		{
			Material = LoadObject<UMaterialInterface>(nullptr, FallbackMaterialPath);
		}
		GBaseMaterial = Material;
		return Material;
	}
}

UStaticMesh* RupturaAssets::Mesh(EShape Shape)
{
	switch (Shape)
	{
	case EShape::Sphere: return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	case EShape::Cylinder: return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	case EShape::Cone: return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	case EShape::Cube:
	default: return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
}

bool RupturaAssets::HasCustomMaterial()
{
	GetBaseMaterial();
	return GHasCustomMaterial;
}

UMaterialInstanceDynamic* RupturaAssets::MakeMaterial(UObject* Outer, const FLinearColor& Color, float Roughness, float Metallic, float Emissive)
{
	UMaterialInterface* Base = GetBaseMaterial();
	if (!Base)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
	if (!MID)
	{
		return nullptr;
	}

	// Parâmetros do material PBR do projeto.
	MID->SetVectorParameterValue(TEXT("Cor"), Color);
	MID->SetScalarParameterValue(TEXT("Rugosidade"), Roughness);
	MID->SetScalarParameterValue(TEXT("Metalico"), Metallic);
	MID->SetScalarParameterValue(TEXT("Emissivo"), Emissive);

	// Parâmetro do material básico do motor (fallback). Emissivos ficam mais claros para continuar legíveis.
	const FLinearColor FallbackColor = Emissive > 0.f
		? FLinearColor(FMath::Min(1.f, Color.R * 1.6f + 0.15f), FMath::Min(1.f, Color.G * 1.6f + 0.15f), FMath::Min(1.f, Color.B * 1.6f + 0.15f), 1.f)
		: Color;
	MID->SetVectorParameterValue(TEXT("Color"), FallbackColor);
	return MID;
}

UStaticMeshComponent* RupturaAssets::AddPart(AActor* Owner, USceneComponent* Parent, EShape Shape, const FVector& Location, const FVector& Size, UMaterialInterface* Material, const FRotator& Rotation)
{
	if (!Owner || !Parent)
	{
		return nullptr;
	}

	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetStaticMesh(Mesh(Shape));
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->RegisterComponent();
	Part->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeRotation(Rotation);
	Part->SetRelativeScale3D(Size / 100.f);
	if (Material)
	{
		Part->SetMaterial(0, Material);
	}
	Owner->AddInstanceComponent(Part);
	return Part;
}

USceneComponent* RupturaAssets::AddPivot(AActor* Owner, USceneComponent* Parent, const FVector& Location)
{
	if (!Owner || !Parent)
	{
		return nullptr;
	}

	USceneComponent* Pivot = NewObject<USceneComponent>(Owner);
	Pivot->SetMobility(EComponentMobility::Movable);
	Pivot->RegisterComponent();
	Pivot->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
	Pivot->SetRelativeLocation(Location);
	Owner->AddInstanceComponent(Pivot);
	return Pivot;
}

USoundBase* RupturaAssets::Sound(FName Name)
{
	if (Name.IsNone() || GMissingSounds.Contains(Name))
	{
		return nullptr;
	}

	if (TWeakObjectPtr<USoundBase>* Found = GSounds.Find(Name))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	const FString Package = FString::Printf(TEXT("/Game/Audio/%s"), *Name.ToString());
	if (!FPackageName::DoesPackageExist(Package))
	{
		GMissingSounds.Add(Name);
		UE_LOG(LogRuptura, Warning, TEXT("Som '%s' não importado (esperado em %s). Rode Scripts/setup_projeto.py."), *Name.ToString(), *Package);
		return nullptr;
	}

	USoundBase* Loaded = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, *Name.ToString()));
	if (!Loaded)
	{
		GMissingSounds.Add(Name);
		return nullptr;
	}
	GSounds.Add(Name, Loaded);
	return Loaded;
}

USoundAttenuation* RupturaAssets::Attenuation()
{
	if (GAttenuation.IsValid())
	{
		return GAttenuation.Get();
	}

	USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("RupturaAttenuation"));
	Att->AddToRoot(); // vive durante todo o processo; é um único objeto pequeno
	Att->Attenuation.bAttenuate = true;
	Att->Attenuation.bSpatialize = true;
	Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Att->Attenuation.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
	Att->Attenuation.FalloffDistance = 6000.f;
	GAttenuation = Att;
	return Att;
}

void RupturaAssets::PlaySound3D(const UObject* WorldContext, FName Name, const FVector& Location, float Volume, float Pitch)
{
	if (USoundBase* S = Sound(Name))
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContext, S, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f, Attenuation());
	}
}

void RupturaAssets::PlaySound2D(const UObject* WorldContext, FName Name, float Volume, float Pitch)
{
	if (USoundBase* S = Sound(Name))
	{
		UGameplayStatics::PlaySound2D(WorldContext, S, Volume, Pitch);
	}
}

UAudioComponent* RupturaAssets::CreateLoop2D(UObject* WorldContext, FName Name, float Volume)
{
	USoundBase* S = Sound(Name);
	if (!S)
	{
		return nullptr;
	}
	if (USoundWave* Wave = Cast<USoundWave>(S))
	{
		Wave->bLooping = true;
	}
	UAudioComponent* Comp = UGameplayStatics::CreateSound2D(WorldContext, S, Volume, 1.f, 0.f, nullptr, false, false);
	if (Comp)
	{
		Comp->Play();
	}
	return Comp;
}
