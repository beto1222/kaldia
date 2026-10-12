#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/SoldierBody.h"
#include "KaelCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UHealthComponent;
class UWeaponComponent;
class UInputAction;
class UInputMappingContext;
class UAIPerceptionStimuliSourceComponent;
class AInteractableActor;
struct FInputActionValue;

/** KAEL — soldado controlável em terceira pessoa com câmera sobre o ombro. */
UCLASS()
class RUPTURA_API AKaelCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AKaelCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual bool CanJumpInternal_Implementation() const override;

	// --- API usada por HUD, missão e combate ---
	UHealthComponent* GetHealth() const { return Health; }
	UWeaponComponent* GetWeapon() const { return Weapon; }
	bool IsAiming() const { return bAiming; }
	bool IsInCover() const { return bInCover; }
	bool IsDead() const { return bDead; }
	bool HasKeycard() const { return bHasKeycard; }
	void GiveKeycard() { bHasKeycard = true; }
	int32 GetGrenades() const { return Grenades; }
	int32 AddGrenades(int32 Amount);
	bool IsGrenadesFull() const { return Grenades >= MaxGrenades; }

	void AddTrauma(float Amount);
	void SetInputLocked(bool bLocked);
	bool IsInputLocked() const { return bInputLocked; }

	/** Para o HUD: texto da interação em foco e progresso de "segurar E". */
	bool GetInteractionInfo(FText& OutPrompt, float& OutProgress, bool& bOutCanInteract) const;

	float GetDamageFlash() const { return DamageFlash; }
	float GetLastDamageTime() const { return LastDamageTime; }
	FVector GetLastDamageSource() const { return LastDamageSource; }
	float GetHitMarkerTime() const { return HitMarkerTime; }
	bool WasLastHitKill() const { return bLastHitKill; }
	bool IsAimingAtEnemy() const { return bAimingAtEnemy; }
	float GetCurrentSpreadDeg() const;

protected:
	virtual void BeginPlay() override;

	// Entrada
	void BuildInput();
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_LookPad(const FInputActionValue& Value);
	void Input_JumpStart();
	void Input_JumpStop();
	void Input_SprintStart();
	void Input_SprintStop();
	void Input_Crouch();
	void Input_Dodge();
	void Input_AimStart();
	void Input_AimStop();
	void Input_FireStart();
	void Input_FireStop();
	void Input_Reload();
	void Input_Grenade();
	void Input_Weapon1();
	void Input_Weapon2();
	void Input_SwitchWeapon();
	void Input_Cover();
	void Input_InteractStart();
	void Input_InteractStop();

	// Lógica por frame
	void UpdateMovementMode(float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	void UpdateFiring();
	void UpdateInteraction(float DeltaSeconds);
	void UpdateCover(float DeltaSeconds);
	void UpdateBodyPose(float DeltaSeconds);
	void UpdateFootsteps(float DeltaSeconds);
	void UpdateAimTarget();

	void EquipWeapon(int32 Index);
	bool TryEnterCover();
	void ExitCover();
	bool HasWallAt(const FVector& Point, float Height) const;
	FVector GetMuzzleLocation() const;

	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* Component, float NewHealth, float Delta, AActor* InstigatorActor);

	UFUNCTION()
	void HandleDeath(UHealthComponent* Component, AActor* Killer);

	// Componentes
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<UCameraComponent> FollowCamera;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<UHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<UWeaponComponent> Weapon;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<USceneComponent> BodyRoot;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<UAIPerceptionStimuliSourceComponent> StimuliSource;

	UPROPERTY() FSoldierBody Body;

	// Input criado em tempo de execução
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Move;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Look;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_LookPad;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Jump;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Sprint;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Crouch;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Dodge;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Aim;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Fire;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Reload;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Grenade;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Weapon1;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Weapon2;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_SwitchWeapon;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Cover;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Interact;

	// Ajustes
	UPROPERTY(EditAnywhere, Category = "Kael|Movimento") float JogSpeed = 400.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Movimento") float SprintSpeed = 640.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Movimento") float AimSpeed = 260.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Movimento") float DodgeStrength = 1250.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Camera") float MouseSensitivity = 1.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Camera") float PadSensitivity = 160.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Combate") int32 MaxGrenades = 4;

	// Estado
	bool bAiming = false;
	bool bSprinting = false;
	bool bFireHeld = false;
	bool bSemiShotQueued = false;
	bool bInCover = false;
	bool bCoverLow = false;
	FVector CoverNormal = FVector::ForwardVector;
	bool bDead = false;
	bool bHasKeycard = false;
	bool bInputLocked = false;
	bool bInteractHeld = false;
	bool bInteractConsumed = false;
	bool bAimingAtEnemy = false;
	bool bLastHitKill = false;
	bool bDodging = false;
	int32 Grenades = 2;

	float Trauma = 0.f;
	float DamageFlash = 0.f;
	float LastDamageTime = -100.f;
	FVector LastDamageSource = FVector::ZeroVector;
	float LastShotTime = -100.f;
	float HitMarkerTime = -100.f;
	float NextGrenadeTime = 0.f;
	float DodgeEndTime = 0.f;
	float NextDodgeTime = 0.f;
	float DodgeSpin = 0.f;
	float StaggerUntil = 0.f;
	float InteractProgress = 0.f;
	float FootstepTimer = 0.f;
	float DeathTime = 0.f;
	float NoiseTime = 0.f;

	TWeakObjectPtr<AInteractableActor> FocusedInteractable;
};
