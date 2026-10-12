#include "Mission/MissionActors.h"
#include "Mission/MissionDirector.h"
#include "Player/KaelCharacter.h"
#include "Combat/HealthComponent.h"
#include "Core/RupturaAssets.h"
#include "Core/FxActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"

using RupturaAssets::EShape;

namespace
{
	UPointLightComponent* MakeLight(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const FLinearColor& Color, float Candelas, float Radius)
	{
		UPointLightComponent* L = Owner->CreateDefaultSubobject<UPointLightComponent>(Name);
		L->SetupAttachment(Parent);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(Candelas);
		L->SetLightColor(Color);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		return L;
	}

	UStaticMeshComponent* MakeVisual(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		return C;
	}
}

// ---------------------------------------------------------------------------------------------
// Cartão de acesso
// ---------------------------------------------------------------------------------------------

AKeycardPickup::AKeycardPickup()
{
	Card = MakeVisual(this, RootComponent, TEXT("Card"));
	Card->SetRelativeScale3D(FVector(0.09f, 0.055f, 0.008f));
	Card->SetRelativeRotation(FRotator(0.f, 23.f, 0.f));
	Glow = MakeLight(this, RootComponent, TEXT("Glow"), FLinearColor(0.85f, 0.9f, 1.f), 14.f, 180.f);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 25.f));
	InteractRadius = 220.f;
}

void AKeycardPickup::BeginPlay()
{
	Super::BeginPlay();
	Card->SetStaticMesh(RupturaAssets::Mesh(EShape::Cube));
	// Crachá branco manchado de sangue.
	Card->SetMaterial(0, RupturaAssets::MakeMaterial(this, FLinearColor(0.5f, 0.48f, 0.45f), 0.3f, 0.1f, 0.f));
	RupturaAssets::AddPart(this, Card, EShape::Cube, FVector(20.f, 15.f, 60.f), FVector(55.f, 50.f, 20.f), RupturaAssets::MakeMaterial(this, FLinearColor(0.09f, 0.004f, 0.003f), 0.2f, 0.f, 0.f));
	BaseLocation = GetActorLocation();
}

void AKeycardPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	Glow->SetIntensity(12.f + 6.f * FMath::Max(0.f, FMath::PerlinNoise1D(Age * 1.5f)));
}

FText AKeycardPickup::GetInteractPrompt(const AKaelCharacter* Kael) const
{
	return FText::FromString(TEXT("Pegar o crachá de acesso"));
}

void AKeycardPickup::Interact(AKaelCharacter* Kael)
{
	if (!Kael)
	{
		return;
	}
	Kael->GiveKeycard();
	RupturaAssets::PlaySound2D(this, TEXT("item"), 1.f);
	if (AMissionDirector* Director = AMissionDirector::Get(this))
	{
		Director->OnKeycardTaken();
	}
	Destroy();
}

// ---------------------------------------------------------------------------------------------
// Terminal de segurança
// ---------------------------------------------------------------------------------------------

ASecurityTerminal::ASecurityTerminal()
{
	Console = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Console"));
	Console->SetupAttachment(RootComponent);
	Console->SetMobility(EComponentMobility::Movable);
	Console->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Console->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
	Console->SetRelativeScale3D(FVector(0.6f, 1.3f, 1.1f));

	Screen = MakeVisual(this, RootComponent, TEXT("Screen"));
	Screen->SetRelativeLocation(FVector(28.f, 0.f, 135.f));
	Screen->SetRelativeRotation(FRotator(-20.f, 0.f, 0.f));
	Screen->SetRelativeScale3D(FVector(0.05f, 1.1f, 0.6f));

	ScreenLight = MakeLight(this, RootComponent, TEXT("ScreenLight"), FLinearColor(1.f, 0.1f, 0.05f), 250.f, 600.f);
	ScreenLight->SetRelativeLocation(FVector(80.f, 0.f, 140.f));
	InteractRadius = 230.f;
}

void ASecurityTerminal::BeginPlay()
{
	Super::BeginPlay();
	Console->SetStaticMesh(RupturaAssets::Mesh(EShape::Cube));
	Console->SetMaterial(0, RupturaAssets::MakeMaterial(this, FLinearColor(0.06f, 0.065f, 0.07f), 0.4f, 0.8f, 0.f));
	ScreenMaterial = RupturaAssets::MakeMaterial(this, FLinearColor(1.f, 0.1f, 0.05f), 0.1f, 0.f, 4.f);
	Screen->SetStaticMesh(RupturaAssets::Mesh(EShape::Cube));
	Screen->SetMaterial(0, ScreenMaterial);
	if (bHacked)
	{
		SetHacked(true);
	}
}

void ASecurityTerminal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	BlinkTime += DeltaSeconds;
	if (!bHacked)
	{
		ScreenLight->SetIntensity(FMath::Frac(BlinkTime * 1.5f) < 0.5f ? 180.f : 60.f);
	}
}

FText ASecurityTerminal::GetInteractPrompt(const AKaelCharacter* Kael) const
{
	return bHacked ? FText::GetEmpty() : FText::FromString(TEXT("Segure para religar a energia de emergência"));
}

FVector ASecurityTerminal::GetInteractLocation() const
{
	return GetActorLocation() + GetActorForwardVector() * 40.f + FVector(0.f, 0.f, 120.f);
}

void ASecurityTerminal::SetHacked(bool bSilent)
{
	bHacked = true;
	const FLinearColor Green(0.1f, 1.f, 0.35f);
	if (ScreenMaterial)
	{
		ScreenMaterial->SetVectorParameterValue(TEXT("Cor"), Green);
		ScreenMaterial->SetVectorParameterValue(TEXT("Color"), Green);
	}
	ScreenLight->SetLightColor(Green);
	ScreenLight->SetIntensity(250.f);
	if (!bSilent)
	{
		RupturaAssets::PlaySound3D(this, TEXT("terminal"), GetActorLocation() + FVector(0.f, 0.f, 120.f), 1.f);
	}
}

void ASecurityTerminal::Interact(AKaelCharacter* Kael)
{
	if (bHacked)
	{
		return;
	}
	SetHacked(false);
	if (AMissionDirector* Director = AMissionDirector::Get(this))
	{
		Director->OnTerminalHacked();
	}
}

// ---------------------------------------------------------------------------------------------
// Núcleo de energia
// ---------------------------------------------------------------------------------------------

APowerCore::APowerCore()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Core->SetupAttachment(Root);
	Core->SetMobility(EComponentMobility::Movable);
	// WorldDynamic para que o dano radial das granadas também o atinja.
	Core->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Core->SetRelativeLocation(FVector(0.f, 0.f, 230.f));
	Core->SetRelativeScale3D(FVector(2.6f, 2.6f, 4.2f));

	Light = MakeLight(this, Root, TEXT("Light"), FLinearColor(1.f, 0.12f, 0.06f), 3500.f, 3500.f);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	Light->SetCastShadows(true);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 700.f;
}

void APowerCore::BeginPlay()
{
	Super::BeginPlay();

	Core->SetStaticMesh(RupturaAssets::Mesh(EShape::Cylinder));
	// Cilindro de contenção com algo vermelho e vivo pulsando dentro; os pilares viraram tentáculos.
	Core->SetMaterial(0, RupturaAssets::MakeMaterial(this, FLinearColor(0.6f, 0.04f, 0.03f), 0.15f, 0.f, 5.f));

	UMaterialInterface* RingMat = RupturaAssets::MakeMaterial(this, FLinearColor(0.06f, 0.06f, 0.065f), 0.45f, 0.95f, 0.f);
	UMaterialInterface* PylonMat = RupturaAssets::MakeMaterial(this, FLinearColor(0.3f, 0.06f, 0.05f), 0.25f, 0.f, 0.f);

	for (int32 i = 0; i < 3; ++i)
	{
		Rings.Add(RupturaAssets::AddPart(this, RootComponent, EShape::Cylinder, FVector(0.f, 0.f, 90.f + i * 140.f), FVector(380.f, 380.f, 10.f), RingMat));
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Dir = FRotator(0.f, 45.f + i * 90.f, 0.f).Vector();
		Pylons.Add(RupturaAssets::AddPart(this, RootComponent, EShape::Cylinder, Dir * 360.f + FVector(0.f, 0.f, 200.f), FVector(30.f, 30.f, 400.f), PylonMat));
		UStaticMeshComponent* Arc = RupturaAssets::AddPart(this, RootComponent, EShape::Cube, Dir * 250.f + FVector(0.f, 0.f, 330.f), FVector(220.f, 5.f, 5.f), PylonMat, Dir.Rotation());
		Arcs.Add(Arc);
	}

	Health->OnDeath.AddDynamic(this, &APowerCore::HandleDeath);
	SetShield(bShielded);
}

void APowerCore::SetShield(bool bOn)
{
	bShielded = bOn;
	Health->bInvulnerable = bOn;
	for (UStaticMeshComponent* Arc : Arcs)
	{
		if (Arc)
		{
			Arc->SetVisibility(bOn);
		}
	}
	UMaterialInterface* PylonMat = bOn
		? RupturaAssets::MakeMaterial(this, FLinearColor(0.3f, 0.06f, 0.05f), 0.25f, 0.f, 0.f)
		: RupturaAssets::MakeMaterial(this, FLinearColor(0.06f, 0.02f, 0.02f), 0.8f, 0.f, 0.f);
	for (UStaticMeshComponent* P : Pylons)
	{
		if (P)
		{
			P->SetMaterial(0, PylonMat);
		}
	}
	if (!bOn && HasActorBegunPlay())
	{
		RupturaAssets::PlaySound3D(this, TEXT("membrana_rompe"), GetActorLocation() + FVector(0.f, 0.f, 200.f), 1.f);
		RupturaFx::Blood(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 250.f), FVector::UpVector, 25);
	}
}

float APowerCore::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bCoreDestroyed)
	{
		return 0.f;
	}
	if (bShielded)
	{
		FVector Where = GetActorLocation() + FVector(0.f, 0.f, 230.f);
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			Where = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.ImpactPoint;
		}
		RupturaFx::Blood(GetWorld(), Where, FVector::UpVector, 3);
		if (GetWorld()->GetTimeSeconds() - LastShieldHint > 4.f)
		{
			LastShieldHint = GetWorld()->GetTimeSeconds();
			if (AMissionDirector* Director = AMissionDirector::Get(this))
			{
				Director->ShowHint(FText::FromString(TEXT("A membrana se regenera. Enquanto o Hospedeiro viver, o núcleo está protegido.")));
			}
		}
		return 0.f;
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void APowerCore::HandleDeath(UHealthComponent* Component, AActor* Killer)
{
	if (bCoreDestroyed)
	{
		return;
	}
	bCoreDestroyed = true;
	Blasts = 5;
	NextBlast = GetWorld()->GetTimeSeconds();
	if (AMissionDirector* Director = AMissionDirector::Get(this))
	{
		Director->OnCoreDestroyed();
	}
}

void APowerCore::SetDestroyedState()
{
	bCoreDestroyed = true;
	bShielded = false;
	Health->bInvulnerable = true;
	Core->SetVisibility(false);
	Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (UStaticMeshComponent* Arc : Arcs)
	{
		if (Arc)
		{
			Arc->SetVisibility(false);
		}
	}
	Light->SetLightColor(FLinearColor(1.f, 0.4f, 0.1f));
	Light->SetIntensity(2500.f);
}

void APowerCore::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (!bCoreDestroyed)
	{
		for (int32 i = 0; i < Rings.Num(); ++i)
		{
			if (Rings[i])
			{
				const float Wobble = 12.f * FMath::Sin(Age * (0.8f + i * 0.3f) + i);
				Rings[i]->SetRelativeRotation(FRotator(Wobble, Age * (40.f + i * 25.f), -Wobble));
			}
		}
		// Pulsa como um coração.
		Light->SetIntensity(3500.f * (0.55f + 0.45f * FMath::Pow(FMath::Max(0.f, FMath::Sin(Age * 2.6f)), 4.f)));
		return;
	}

	// Sequência de destruição
	if (Blasts > 0 && GetWorld()->GetTimeSeconds() >= NextBlast)
	{
		--Blasts;
		NextBlast = GetWorld()->GetTimeSeconds() + 0.3f;
		const FVector P = GetActorLocation() + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(100.f, 400.f));
		RupturaFx::Explosion(GetWorld(), P, Blasts == 0 ? 600.f : 250.f);
		RupturaAssets::PlaySound3D(this, TEXT("explosao"), P, 1.f, FMath::FRandRange(0.7f, 0.95f));
		if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Kael->AddTrauma(Blasts == 0 ? 1.f : 0.35f);
		}
		if (Blasts == 0)
		{
			SetDestroyedState();
		}
	}
	else if (Blasts == 0)
	{
		// Fogo residual tremulando
		Light->SetIntensity(2500.f * (0.6f + 0.4f * FMath::PerlinNoise1D(Age * 8.f)));
	}
}

// ---------------------------------------------------------------------------------------------
// Zona de objetivo
// ---------------------------------------------------------------------------------------------

AObjectiveZone::AObjectiveZone()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);
	Box->SetCanEverAffectNavigation(false);
	RootComponent = Box;
}

void AObjectiveZone::Configure(FName InZoneId, const FVector& Extent)
{
	ZoneId = InZoneId;
	Box->SetBoxExtent(Extent, true);
	Box->OnComponentBeginOverlap.AddUniqueDynamic(this, &AObjectiveZone::OnOverlap);

	// Se o jogador já estiver dentro (ex.: carregou um checkpoint aqui), dispara imediatamente.
	TArray<AActor*> Inside;
	Box->GetOverlappingActors(Inside, AKaelCharacter::StaticClass());
	if (Inside.Num() > 0)
	{
		if (AMissionDirector* Director = AMissionDirector::Get(this))
		{
			Director->OnZoneEntered(ZoneId);
		}
	}
}

void AObjectiveZone::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA<AKaelCharacter>())
	{
		if (AMissionDirector* Director = AMissionDirector::Get(this))
		{
			Director->OnZoneEntered(ZoneId);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Aparição
// ---------------------------------------------------------------------------------------------

AApparition::AApparition()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	BodyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyRoot"));
	BodyRoot->SetupAttachment(Root);
}

void AApparition::BeginPlay()
{
	Super::BeginPlay();
	FRandomStream Rng(FMath::Rand());
	FInfectedLook Look = InfectedBody::RandomLook(Rng, false);
	Look.Skin = FLinearColor(0.2f, 0.19f, 0.18f);
	Look.Shirt = FLinearColor(0.03f, 0.03f, 0.03f);
	Look.EyeShine = 3.f;
	InfectedBody::Build(this, BodyRoot, Look, 1.08f, Body);
}

void AApparition::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bVanished)
	{
		return;
	}
	Age += DeltaSeconds;

	AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Kael)
	{
		return;
	}

	// Encara o jogador, imóvel, com a cabeça tombada.
	const FVector ToPlayer = Kael->GetActorLocation() - GetActorLocation();
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, ToPlayer.Rotation().Yaw, 0.f), DeltaSeconds, 1.5f));
	InfectedBody::Animate(Body, FInfectedPose(), DeltaSeconds);

	// Iluminado pela lanterna: dá tempo de ver... e some.
	const FVector ToMe = GetActorLocation() + FVector(0.f, 0.f, 120.f) - Kael->GetFlashlightLocation();
	const bool bLit = Kael->IsFlashlightOn() && ToMe.Size() < 3500.f && FVector::DotProduct(Kael->GetFlashlightDirection(), ToMe.GetSafeNormal()) > 0.94f;
	LitTime = bLit ? LitTime + DeltaSeconds : 0.f;

	if (ToPlayer.Size() < VanishDistance || LitTime > 0.35f || Age > MaxLife)
	{
		Vanish();
	}
}

void AApparition::Vanish()
{
	bVanished = true;
	const FVector Where = GetActorLocation() + FVector(0.f, 0.f, 100.f);
	RupturaAssets::PlaySound2D(this, TEXT("susto"), 0.8f);
	RupturaAssets::PlaySound3D(this, TEXT("sussurro"), Where, 1.f);
	RupturaFx::Smoke(GetWorld(), Where, 140.f, 1.5f);
	if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Kael->AddFear(0.5f);
		Kael->AddTrauma(0.15f);
		Kael->SetFlashlightInterference(1.f);
	}
	if (AMissionDirector* Director = AMissionDirector::Get(this))
	{
		Director->OnApparitionVanished(GetActorLocation());
	}
	Destroy();
}
