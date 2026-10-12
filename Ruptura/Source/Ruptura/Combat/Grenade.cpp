#include "Combat/Grenade.h"
#include "Core/FxActor.h"
#include "Core/RupturaAssets.h"
#include "Player/KaelCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "EngineUtils.h"

namespace
{
	TArray<TWeakObjectPtr<AGrenade>> GActiveGrenades;
}

AGrenade::AGrenade()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(8.f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.16f));
	Mesh->SetCanEverAffectNavigation(false);

	Blink = CreateDefaultSubobject<UPointLightComponent>(TEXT("Blink"));
	Blink->SetupAttachment(Collision);
	Blink->SetIntensityUnits(ELightUnits::Candelas);
	Blink->SetIntensity(0.f);
	Blink->SetAttenuationRadius(250.f);
	Blink->SetLightColor(FLinearColor(1.f, 0.1f, 0.05f));
	Blink->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 4000.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.35f;
	Movement->Friction = 0.35f;
	Movement->ProjectileGravityScale = 1.f;
}

void AGrenade::BeginPlay()
{
	Super::BeginPlay();
	GActiveGrenades.Add(this);

	Mesh->SetStaticMesh(RupturaAssets::Mesh(RupturaAssets::EShape::Sphere));
	Mesh->SetMaterial(0, RupturaAssets::MakeMaterial(this, FLinearColor(0.08f, 0.1f, 0.06f), 0.5f, 0.4f, 0.f));

	Movement->OnProjectileBounce.AddDynamic(this, &AGrenade::OnBounce);
	Movement->OnProjectileStop.AddDynamic(this, &AGrenade::OnStop);

	if (APawn* InstigatorPawn = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(InstigatorPawn, true);
	}
}

void AGrenade::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GActiveGrenades.RemoveAll([this](const TWeakObjectPtr<AGrenade>& G) { return !G.IsValid() || G.Get() == this; });
	Super::EndPlay(EndPlayReason);
}

void AGrenade::GetActiveGrenades(TArray<AGrenade*>& Out)
{
	Out.Reset();
	GActiveGrenades.RemoveAll([](const TWeakObjectPtr<AGrenade>& G) { return !G.IsValid(); });
	for (const TWeakObjectPtr<AGrenade>& G : GActiveGrenades)
	{
		Out.Add(G.Get());
	}
}

void AGrenade::Launch(const FVector& Velocity, bool bInRocket)
{
	bRocket = bInRocket;
	if (bRocket)
	{
		Movement->bShouldBounce = false;
		Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Mesh->SetRelativeScale3D(FVector(0.35f, 0.18f, 0.18f));
		Mesh->SetMaterial(0, RupturaAssets::MakeMaterial(this, FLinearColor(1.f, 0.35f, 0.1f), 0.3f, 0.f, 10.f));
		Blink->SetLightColor(FLinearColor(1.f, 0.45f, 0.15f));
		Blink->SetIntensity(1500.f);
		Blink->SetAttenuationRadius(600.f);
		FuseTime = 6.f; // segurança: explode no ar se não acertar nada
	}
	Movement->Velocity = Velocity;
	Movement->UpdateComponentVelocity();
}

float AGrenade::GetTimeLeft() const
{
	return FMath::Max(0.f, FuseTime - Age);
}

void AGrenade::OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (bRocket)
	{
		Explode();
		return;
	}
	if (Age - LastBounceSound > 0.15f && ImpactVelocity.Size() > 150.f)
	{
		LastBounceSound = Age;
		RupturaAssets::PlaySound3D(this, TEXT("quique"), GetActorLocation(), FMath::Clamp(ImpactVelocity.Size() / 900.f, 0.2f, 1.f));
	}
}

void AGrenade::OnStop(const FHitResult& ImpactResult)
{
	if (bRocket)
	{
		Explode();
	}
}

void AGrenade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (bRocket)
	{
		TrailTimer -= DeltaSeconds;
		if (TrailTimer <= 0.f)
		{
			TrailTimer = 0.04f;
			RupturaFx::Smoke(GetWorld(), GetActorLocation(), 45.f, 0.8f);
		}
	}
	else
	{
		// Pisca cada vez mais rápido perto de explodir.
		const float Rate = FMath::Lerp(3.f, 14.f, FMath::Clamp(Age / FuseTime, 0.f, 1.f));
		const bool bOn = FMath::Frac(Age * Rate) < 0.35f;
		Blink->SetIntensity(bOn ? 800.f : 0.f);
	}

	if (Age >= FuseTime)
	{
		Explode();
	}
}

void AGrenade::Explode()
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;

	UWorld* World = GetWorld();
	const FVector Origin = GetActorLocation() + FVector(0.f, 0.f, 15.f);
	APawn* InstigatorPawn = GetInstigator();
	AController* InstigatorController = InstigatorPawn ? InstigatorPawn->GetController() : nullptr;

	TArray<AActor*> Ignore;
	Ignore.Add(this);
	UGameplayStatics::ApplyRadialDamageWithFalloff(World, BaseDamage, MinDamage, Origin, InnerRadius, OuterRadius, 1.f,
		UDamageType::StaticClass(), Ignore, this, InstigatorController, ECC_Visibility);

	// Empurrão físico nos personagens e tremor na câmera do jogador.
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Char = *It;
		const FVector Delta = Char->GetActorLocation() - Origin;
		const float Dist = Delta.Size();
		if (AKaelCharacter* Kael = Cast<AKaelCharacter>(Char))
		{
			Kael->AddTrauma(FMath::Clamp(1.2f - Dist / 2500.f, 0.f, 1.f));
		}
		if (Dist < OuterRadius * 0.7f)
		{
			const float Strength = 1.f - Dist / (OuterRadius * 0.7f);
			FVector Push = Delta.GetSafeNormal2D() * 700.f * Strength;
			Push.Z = 250.f * Strength;
			Char->LaunchCharacter(Push, false, false);
		}
	}

	RupturaFx::Explosion(World, Origin, OuterRadius * 0.45f);
	RupturaAssets::PlaySound3D(this, TEXT("explosao"), Origin, 1.f, bRocket ? 1.1f : 1.f);
	UAISense_Hearing::ReportNoiseEvent(World, Origin, 1.f, InstigatorPawn, 0.f, TEXT("Explosao"));

	Destroy();
}
