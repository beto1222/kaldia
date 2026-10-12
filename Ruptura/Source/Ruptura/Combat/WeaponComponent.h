#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

USTRUCT(BlueprintType)
struct FWeaponDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") FText DisplayName;
	/** Dano por projétil (por chumbo, na espingarda). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float Damage = 20.f;
	/** Projéteis por disparo (1 = bala; > 1 = chumbos espalhados pelo cone de dispersão). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") int32 Pellets = 1;
	/** Abertura do cone de chumbos (graus), somada à dispersão normal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float PelletSpreadDeg = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float FireInterval = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") int32 MagSize = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") int32 MaxReserve = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float HipSpreadDeg = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float AimSpreadDeg = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float BloomPerShot = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float MaxBloom = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float RecoilPitch = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float RecoilYaw = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float Range = 8000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") bool bAutomatic = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float ReloadTime = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float GunLength = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") FName FireSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma") float Loudness = 1.f;
};

USTRUCT(BlueprintType)
struct FWeaponAmmo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arma") int32 Mag = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Arma") int32 Reserve = 0;
};

struct FShotResult
{
	bool bHit = false;
	bool bHitPawn = false;
	bool bKilled = false;
	bool bHeadshot = false;
	FVector End = FVector::ZeroVector;
	TWeakObjectPtr<AActor> HitActor;
};

/** Armas de disparo instantâneo (hitscan) de Kael: munição escassa, recarga, dispersão, chumbos e efeitos. */
UCLASS(ClassGroup = (Ruptura), meta = (BlueprintSpawnableComponent))
class RUPTURA_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void InitPlayerLoadout();

	/**
	 * Dispara uma vez. AimStart/AimDir definem para onde se mira (câmera ou olhos);
	 * o tiro real sai de Muzzle até o ponto mirado, então coberturas na frente do cano bloqueiam.
	 */
	bool FireShot(const FVector& AimStart, const FVector& AimDir, const FVector& Muzzle, float SpreadDeg, FShotResult& OutResult);

	bool CanFire() const;
	bool StartReload();
	void CancelReload();
	void Equip(int32 Index);
	int32 AddReserve(int32 Index, int32 Amount);
	bool IsReserveFull(int32 Index) const;

	float GetSpread(bool bAiming, bool bMoving) const;
	float GetReloadProgress() const;
	bool IsReloading() const { return bReloading; }
	int32 GetCurrentIndex() const { return Current; }
	int32 GetNumWeapons() const { return Weapons.Num(); }
	const FWeaponDef& GetDef() const;
	const FWeaponAmmo& GetAmmo() const;
	const FWeaponDef& GetDefAt(int32 Index) const;
	const FWeaponAmmo& GetAmmoAt(int32 Index) const;

	/** Munição de reserva infinita (útil para testes). */
	UPROPERTY(EditAnywhere, Category = "Arma")
	bool bInfiniteReserve = false;

protected:
	UPROPERTY(EditAnywhere, Category = "Arma")
	TArray<FWeaponDef> Weapons;

	UPROPERTY(VisibleAnywhere, Category = "Arma")
	TArray<FWeaponAmmo> Ammo;

	int32 Current = 0;
	float LastFireTime = -100.f;
	float LastEmptyClickTime = -100.f;
	bool bReloading = false;
	float ReloadStart = 0.f;
	float ReloadEnd = 0.f;
	float Bloom = 0.f;

	float Now() const;
	void FinishReload();
	/** Um projétil: traço da mira e traço real saindo do cano. Acumula o resultado em OutResult. */
	void FireProjectile(const FWeaponDef& Def, const FVector& AimStart, const FVector& Dir, const FVector& Muzzle, FShotResult& OutResult, bool bEffects);
	static bool AreAllies(const AActor* A, const AActor* B);
};
