#include "Combat/WeaponComponent.h"
#include "Combat/HealthComponent.h"
#include "AI/EnemyCharacter.h"
#include "Core/FxActor.h"
#include "Core/RupturaAssets.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"

namespace
{
	const FWeaponDef GEmptyDef;
	const FWeaponAmmo GEmptyAmmo;

	FWeaponAmmo MakeAmmo(int32 Mag, int32 Reserve)
	{
		FWeaponAmmo A;
		A.Mag = Mag;
		A.Reserve = Reserve;
		return A;
	}
}

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

float UWeaponComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

void UWeaponComponent::InitPlayerLoadout()
{
	Weapons.Reset();
	Ammo.Reset();

	FWeaponDef Pistol;
	Pistol.DisplayName = FText::FromString(TEXT("PISTOLA M9-R"));
	Pistol.Damage = 24.f; Pistol.FireInterval = 0.2f; Pistol.MagSize = 12; Pistol.MaxReserve = 84;
	Pistol.HipSpreadDeg = 2.0f; Pistol.AimSpreadDeg = 0.45f; Pistol.BloomPerShot = 0.7f; Pistol.MaxBloom = 2.5f;
	Pistol.RecoilPitch = 1.6f; Pistol.RecoilYaw = 0.5f; Pistol.Range = 6000.f; Pistol.bAutomatic = false;
	Pistol.ReloadTime = 1.3f; Pistol.GunLength = 32.f; Pistol.FireSound = TEXT("tiro_pistola"); Pistol.Loudness = 0.8f;
	Pistol.TracerColor = FLinearColor(1.f, 0.8f, 0.45f);
	Weapons.Add(Pistol);
	Ammo.Add(MakeAmmo(12, 48));

	FWeaponDef Rifle;
	Rifle.DisplayName = FText::FromString(TEXT("RIFLE DE ASSALTO AR-7"));
	Rifle.Damage = 17.f; Rifle.FireInterval = 0.095f; Rifle.MagSize = 30; Rifle.MaxReserve = 210;
	Rifle.HipSpreadDeg = 3.0f; Rifle.AimSpreadDeg = 0.8f; Rifle.BloomPerShot = 0.35f; Rifle.MaxBloom = 3.5f;
	Rifle.RecoilPitch = 0.7f; Rifle.RecoilYaw = 0.35f; Rifle.Range = 9000.f; Rifle.bAutomatic = true;
	Rifle.ReloadTime = 2.0f; Rifle.GunLength = 66.f; Rifle.FireSound = TEXT("tiro_rifle"); Rifle.Loudness = 1.f;
	Rifle.TracerColor = FLinearColor(1.f, 0.7f, 0.3f);
	Weapons.Add(Rifle);
	Ammo.Add(MakeAmmo(30, 90));

	Current = 1;
	bInfiniteReserve = false;
}

void UWeaponComponent::InitEnemyLoadout(float Damage, float FireInterval)
{
	Weapons.Reset();
	Ammo.Reset();

	FWeaponDef Rifle;
	Rifle.DisplayName = FText::FromString(TEXT("RIFLE HOSTIL"));
	Rifle.Damage = Damage; Rifle.FireInterval = FireInterval; Rifle.MagSize = 24; Rifle.MaxReserve = 999;
	Rifle.HipSpreadDeg = 0.f; Rifle.AimSpreadDeg = 0.f; Rifle.BloomPerShot = 0.f; Rifle.MaxBloom = 0.f;
	Rifle.Range = 6000.f; Rifle.bAutomatic = true; Rifle.ReloadTime = 2.2f; Rifle.GunLength = 60.f;
	Rifle.FireSound = TEXT("tiro_inimigo"); Rifle.Loudness = 1.f; Rifle.TracerColor = FLinearColor(1.f, 0.25f, 0.15f);
	Weapons.Add(Rifle);
	Ammo.Add(MakeAmmo(24, 999));
	Current = 0;
	bInfiniteReserve = true;
}

void UWeaponComponent::InitBossLoadout(float Damage, float FireInterval)
{
	Weapons.Reset();
	Ammo.Reset();

	FWeaponDef Minigun;
	Minigun.DisplayName = FText::FromString(TEXT("METRALHADORA ROTATIVA"));
	Minigun.Damage = Damage; Minigun.FireInterval = FireInterval; Minigun.MagSize = 90; Minigun.MaxReserve = 999;
	Minigun.Range = 7000.f; Minigun.bAutomatic = true; Minigun.ReloadTime = 2.6f; Minigun.GunLength = 90.f;
	Minigun.FireSound = TEXT("tiro_robo"); Minigun.Loudness = 1.f; Minigun.TracerColor = FLinearColor(1.f, 0.15f, 0.1f);
	Weapons.Add(Minigun);
	Ammo.Add(MakeAmmo(90, 999));
	Current = 0;
	bInfiniteReserve = true;
}

const FWeaponDef& UWeaponComponent::GetDef() const
{
	return GetDefAt(Current);
}

const FWeaponAmmo& UWeaponComponent::GetAmmo() const
{
	return GetAmmoAt(Current);
}

const FWeaponDef& UWeaponComponent::GetDefAt(int32 Index) const
{
	return Weapons.IsValidIndex(Index) ? Weapons[Index] : GEmptyDef;
}

const FWeaponAmmo& UWeaponComponent::GetAmmoAt(int32 Index) const
{
	return Ammo.IsValidIndex(Index) ? Ammo[Index] : GEmptyAmmo;
}

bool UWeaponComponent::CanFire() const
{
	if (!Weapons.IsValidIndex(Current) || bReloading)
	{
		return false;
	}
	return Ammo[Current].Mag > 0 && (Now() - LastFireTime) >= Weapons[Current].FireInterval;
}

float UWeaponComponent::GetSpread(bool bAiming, bool bMoving) const
{
	const FWeaponDef& Def = GetDef();
	return (bAiming ? Def.AimSpreadDeg : Def.HipSpreadDeg) + Bloom + (bMoving ? (bAiming ? 0.4f : 1.2f) : 0.f);
}

float UWeaponComponent::GetReloadProgress() const
{
	if (!bReloading || ReloadEnd <= ReloadStart)
	{
		return 0.f;
	}
	return FMath::Clamp((Now() - ReloadStart) / (ReloadEnd - ReloadStart), 0.f, 1.f);
}

bool UWeaponComponent::AreAllies(const AActor* A, const AActor* B)
{
	return A && B && A->IsA<AEnemyCharacter>() && B->IsA<AEnemyCharacter>();
}

bool UWeaponComponent::FireShot(const FVector& AimStart, const FVector& AimDir, const FVector& Muzzle, float SpreadDeg, FShotResult& OutResult)
{
	OutResult = FShotResult();
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor)
	{
		return false;
	}

	if (!Weapons.IsValidIndex(Current))
	{
		return false;
	}

	FWeaponAmmo& A = Ammo[Current];
	if (A.Mag <= 0)
	{
		if (Now() - LastEmptyClickTime > 0.35f)
		{
			LastEmptyClickTime = Now();
			RupturaAssets::PlaySound3D(this, TEXT("vazio"), Muzzle, 0.7f);
		}
		StartReload();
		return false;
	}
	if (!CanFire())
	{
		return false;
	}

	const FWeaponDef& Def = Weapons[Current];
	LastFireTime = Now();
	--A.Mag;
	Bloom = FMath::Min(Def.MaxBloom, Bloom + Def.BloomPerShot);

	FCollisionQueryParams Query(SCENE_QUERY_STAT(RupturaShot), false, OwnerActor);
	Query.bReturnPhysicalMaterial = false;

	const FVector Dir = FMath::VRandCone(AimDir.GetSafeNormal(), FMath::DegreesToRadians(FMath::Max(0.f, SpreadDeg)));
	const FVector AimEnd = AimStart + Dir * Def.Range;

	// 1) Onde a mira encosta no mundo.
	FHitResult AimHit;
	const FVector AimPoint = World->LineTraceSingleByChannel(AimHit, AimStart, AimEnd, ECC_Pawn, Query) ? AimHit.ImpactPoint : AimEnd;

	// 2) Tiro real saindo do cano.
	const FVector ShotDir = (AimPoint - Muzzle).GetSafeNormal();
	const FVector ShotEnd = AimPoint + ShotDir * 25.f;
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Muzzle, ShotEnd, ECC_Pawn, Query);
	OutResult.bHit = bHit;
	OutResult.End = bHit ? Hit.ImpactPoint : ShotEnd;

	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	AController* InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (bHit && Hit.GetActor())
	{
		AActor* Target = Hit.GetActor();
		OutResult.HitActor = Target;
		UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>();
		const bool bTargetIsPawn = Target->IsA<APawn>();
		OutResult.bHitPawn = bTargetIsPawn && TargetHealth && !TargetHealth->IsDead();

		if (!AreAllies(OwnerActor, Target))
		{
			float Damage = Def.Damage;
			if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Target))
			{
				const float Mult = Enemy->GetDamageMultiplierForHit(Hit, ShotDir);
				OutResult.bHeadshot = Mult > 1.9f && !Enemy->IsBoss();
				Damage *= Mult;
			}
			const bool bWasAlive = TargetHealth && !TargetHealth->IsDead();
			UGameplayStatics::ApplyPointDamage(Target, Damage, ShotDir, Hit, InstigatorController, OwnerActor, UDamageType::StaticClass());
			OutResult.bKilled = bWasAlive && TargetHealth->IsDead();
		}
		RupturaFx::Impact(World, Hit.ImpactPoint, Hit.ImpactNormal, OutResult.bHitPawn);
		RupturaAssets::PlaySound3D(this, OutResult.bHitPawn ? FName(TEXT("impacto_corpo")) : FName(TEXT("impacto")), Hit.ImpactPoint, 0.6f, FMath::FRandRange(0.9f, 1.15f));
	}

	RupturaFx::Tracer(World, Muzzle + ShotDir * 20.f, OutResult.End, Def.TracerColor);
	RupturaFx::MuzzleFlash(World, Muzzle, ShotDir.Rotation(), FLinearColor(1.f, 0.7f, 0.3f), Def.GunLength > 80.f ? 1.4f : 1.f);
	RupturaAssets::PlaySound3D(this, Def.FireSound, Muzzle, 1.f, FMath::FRandRange(0.95f, 1.05f));

	// Ruído para a percepção auditiva da IA.
	UAISense_Hearing::ReportNoiseEvent(World, Muzzle, Def.Loudness, OwnerActor, 0.f, TEXT("Tiro"));

	if (A.Mag <= 0)
	{
		StartReload();
	}
	return true;
}

bool UWeaponComponent::StartReload()
{
	if (!Weapons.IsValidIndex(Current) || bReloading)
	{
		return false;
	}
	FWeaponAmmo& A = Ammo[Current];
	const FWeaponDef& Def = Weapons[Current];
	if (A.Mag >= Def.MagSize || (!bInfiniteReserve && A.Reserve <= 0))
	{
		return false;
	}
	bReloading = true;
	ReloadStart = Now();
	ReloadEnd = ReloadStart + Def.ReloadTime;
	if (const AActor* Owner = GetOwner())
	{
		RupturaAssets::PlaySound3D(this, TEXT("recarga"), Owner->GetActorLocation(), 0.8f);
	}
	return true;
}

void UWeaponComponent::CancelReload()
{
	bReloading = false;
}

void UWeaponComponent::FinishReload()
{
	bReloading = false;
	if (!Weapons.IsValidIndex(Current))
	{
		return;
	}
	FWeaponAmmo& A = Ammo[Current];
	const int32 Needed = Weapons[Current].MagSize - A.Mag;
	const int32 Taken = bInfiniteReserve ? Needed : FMath::Min(Needed, A.Reserve);
	A.Mag += Taken;
	if (!bInfiniteReserve)
	{
		A.Reserve -= Taken;
	}
}

void UWeaponComponent::Equip(int32 Index)
{
	if (!Weapons.IsValidIndex(Index) || Index == Current)
	{
		return;
	}
	CancelReload();
	Current = Index;
	LastFireTime = Now() - Weapons[Index].FireInterval + 0.25f; // pequeno atraso ao sacar
	if (const AActor* Owner = GetOwner())
	{
		RupturaAssets::PlaySound3D(this, TEXT("troca_arma"), Owner->GetActorLocation(), 0.7f);
	}
}

int32 UWeaponComponent::AddReserve(int32 Index, int32 Amount)
{
	if (!Weapons.IsValidIndex(Index))
	{
		return 0;
	}
	FWeaponAmmo& A = Ammo[Index];
	const int32 Before = A.Reserve;
	A.Reserve = FMath::Min(Weapons[Index].MaxReserve, A.Reserve + Amount);
	return A.Reserve - Before;
}

bool UWeaponComponent::IsReserveFull(int32 Index) const
{
	return !Weapons.IsValidIndex(Index) || Ammo[Index].Reserve >= Weapons[Index].MaxReserve;
}

void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bReloading && Now() >= ReloadEnd)
	{
		FinishReload();
	}

	// A dispersão volta ao normal pouco depois de parar de atirar.
	if (Now() - LastFireTime > 0.12f)
	{
		Bloom = FMath::FInterpTo(Bloom, 0.f, DeltaTime, 6.f);
	}
}
