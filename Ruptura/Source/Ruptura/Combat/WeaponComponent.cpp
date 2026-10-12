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

	// Munição é escassa: cada tiro conta e o estampido atrai os infectados.
	FWeaponDef Pistol;
	Pistol.DisplayName = FText::FromString(TEXT("PISTOLA 9 MM"));
	Pistol.Damage = 30.f; Pistol.FireInterval = 0.22f; Pistol.MagSize = 12; Pistol.MaxReserve = 48;
	Pistol.HipSpreadDeg = 2.2f; Pistol.AimSpreadDeg = 0.5f; Pistol.BloomPerShot = 0.8f; Pistol.MaxBloom = 2.5f;
	Pistol.RecoilPitch = 1.8f; Pistol.RecoilYaw = 0.6f; Pistol.Range = 6000.f; Pistol.bAutomatic = false;
	Pistol.ReloadTime = 1.6f; Pistol.GunLength = 32.f; Pistol.FireSound = TEXT("tiro_pistola"); Pistol.Loudness = 1.f;
	Weapons.Add(Pistol);
	Ammo.Add(MakeAmmo(12, 12));

	FWeaponDef Shotgun;
	Shotgun.DisplayName = FText::FromString(TEXT("ESPINGARDA CAL. 12"));
	Shotgun.Damage = 13.f; Shotgun.Pellets = 9; Shotgun.PelletSpreadDeg = 4.5f;
	Shotgun.FireInterval = 0.95f; Shotgun.MagSize = 5; Shotgun.MaxReserve = 15;
	Shotgun.HipSpreadDeg = 1.5f; Shotgun.AimSpreadDeg = 0.5f; Shotgun.BloomPerShot = 0.f; Shotgun.MaxBloom = 0.f;
	Shotgun.RecoilPitch = 5.5f; Shotgun.RecoilYaw = 1.2f; Shotgun.Range = 2600.f; Shotgun.bAutomatic = false;
	Shotgun.ReloadTime = 3.f; Shotgun.GunLength = 78.f; Shotgun.FireSound = TEXT("tiro_espingarda"); Shotgun.Loudness = 1.f;
	Weapons.Add(Shotgun);
	Ammo.Add(MakeAmmo(3, 2));

	Current = 0;
	bInfiniteReserve = false;
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

void UWeaponComponent::FireProjectile(const FWeaponDef& Def, const FVector& AimStart, const FVector& Dir, const FVector& Muzzle, FShotResult& OutResult, bool bEffects)
{
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();

	FCollisionQueryParams Query(SCENE_QUERY_STAT(RupturaShot), false, OwnerActor);
	Query.bReturnPhysicalMaterial = false;

	// 1) Onde a mira encosta no mundo.
	const FVector AimEnd = AimStart + Dir * Def.Range;
	FHitResult AimHit;
	const FVector AimPoint = World->LineTraceSingleByChannel(AimHit, AimStart, AimEnd, ECC_Pawn, Query) ? AimHit.ImpactPoint : AimEnd;

	// 2) Projétil real saindo do cano: obstáculos na frente da arma bloqueiam.
	const FVector ShotDir = (AimPoint - Muzzle).GetSafeNormal();
	const FVector ShotEnd = AimPoint + ShotDir * 25.f;
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Muzzle, ShotEnd, ECC_Pawn, Query);
	OutResult.bHit |= bHit;
	OutResult.End = bHit ? Hit.ImpactPoint : ShotEnd;
	if (!bHit || !Hit.GetActor())
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	AController* InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	AActor* Target = Hit.GetActor();
	OutResult.HitActor = Target;
	UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>();
	const bool bFleshHit = Target->IsA<APawn>() && TargetHealth && !TargetHealth->IsDead();
	OutResult.bHitPawn |= bFleshHit;

	if (!AreAllies(OwnerActor, Target))
	{
		float Damage = Def.Damage;
		if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Target))
		{
			const float Mult = Enemy->GetDamageMultiplierForHit(Hit, ShotDir);
			OutResult.bHeadshot |= Mult > 1.9f && !Enemy->IsBoss();
			Damage *= Mult;
		}
		const bool bWasAlive = TargetHealth && !TargetHealth->IsDead();
		UGameplayStatics::ApplyPointDamage(Target, Damage, ShotDir, Hit, InstigatorController, OwnerActor, UDamageType::StaticClass());
		OutResult.bKilled |= bWasAlive && TargetHealth->IsDead();
	}
	if (bEffects)
	{
		RupturaFx::Impact(World, Hit.ImpactPoint, Hit.ImpactNormal, bFleshHit);
		RupturaAssets::PlaySound3D(this, bFleshHit ? FName(TEXT("impacto_corpo")) : FName(TEXT("impacto")), Hit.ImpactPoint, 0.6f, FMath::FRandRange(0.9f, 1.15f));
	}
}

bool UWeaponComponent::FireShot(const FVector& AimStart, const FVector& AimDir, const FVector& Muzzle, float SpreadDeg, FShotResult& OutResult)
{
	OutResult = FShotResult();
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor || !Weapons.IsValidIndex(Current))
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

	// A dispersão desvia o centro do disparo; os chumbos se abrem em volta desse centro.
	const FVector Center = FMath::VRandCone(AimDir.GetSafeNormal(), FMath::DegreesToRadians(FMath::Max(0.f, SpreadDeg)));
	const int32 Count = FMath::Max(1, Def.Pellets);
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Dir = Count > 1 ? FMath::VRandCone(Center, FMath::DegreesToRadians(Def.PelletSpreadDeg)) : Center;
		// Efeitos só nos primeiros chumbos: o resultado visual é o mesmo e poupa centenas de atores.
		FireProjectile(Def, AimStart, Dir, Muzzle, OutResult, i < 4);
	}

	// Sem traçantes (munição comum não brilha): só o clarão do cano, que ilumina o escuro por um instante.
	RupturaFx::MuzzleFlash(World, Muzzle, Center.Rotation(), FLinearColor(1.f, 0.68f, 0.32f), Count > 1 ? 1.6f : 1.f);
	RupturaAssets::PlaySound3D(this, Def.FireSound, Muzzle, 1.f, FMath::FRandRange(0.95f, 1.05f));

	// Estampido para a audição da IA: tiros atraem os infectados de longe.
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
