#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/SoldierBody.h"
#include "KaelCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class UAudioComponent;
class UHealthComponent;
class UWeaponComponent;
class UInputAction;
class UInputMappingContext;
class UAIPerceptionStimuliSourceComponent;
class AInteractableActor;
struct FInputActionValue;

/**
 * KAEL — sobrevivente controlável. Câmera sobre o ombro bem fechada (estilo survival horror) ou
 * em primeira pessoa (V). Lanterna com bateria, fôlego limitado, medo (batimentos, respiração,
 * tremor na mira, distorção da imagem), faca para quando a munição acaba.
 */
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

	// --- API usada por HUD, missão, IA e combate ---
	UHealthComponent* GetHealth() const { return Health; }
	UWeaponComponent* GetWeapon() const { return Weapon; }
	bool IsAiming() const { return bAiming; }
	bool IsDead() const { return bDead; }
	bool HasKeycard() const { return bHasKeycard; }
	void GiveKeycard() { bHasKeycard = true; }
	int32 GetGrenades() const { return Grenades; }
	int32 AddGrenades(int32 Amount);
	bool IsGrenadesFull() const { return Grenades >= MaxGrenades; }

	// Lanterna
	bool IsFlashlightOn() const { return bFlashlightOn && Battery > 0.f; }
	float GetBattery() const { return Battery; }
	/** Soma carga (0..1). Retorna quanto foi efetivamente adicionado. */
	float AddBattery(float Amount);
	bool IsBatteryFull() const { return Battery >= 0.99f; }
	FVector GetFlashlightLocation() const;
	FVector GetFlashlightDirection() const;
	float GetLastFlashlightToggle() const { return LastFlashlightToggle; }
	/** Interferência elétrica (o Hospedeiro por perto, apagões): a lanterna falha. */
	void SetFlashlightInterference(float Amount) { Interference = FMath::Max(Interference, FMath::Clamp(Amount, 0.f, 1.f)); }

	// Corpo e mente
	float GetStamina() const { return Stamina; }
	bool IsExhausted() const { return bExhausted; }
	float GetFear() const { return Fear; }
	/** Susto instantâneo (aparições, rugidos, portas batendo). */
	void AddFear(float Amount);
	bool IsFirstPerson() const { return bFirstPerson; }

	void AddTrauma(float Amount);
	void SetInputLocked(bool bLocked);
	bool IsInputLocked() const { return bInputLocked; }

	/** Para o HUD: texto da interação em foco e progresso de "segurar E". */
	bool GetInteractionInfo(FText& OutPrompt, float& OutProgress, bool& bOutCanInteract) const;

	float GetDamageFlash() const { return DamageFlash; }
	float GetLastDamageTime() const { return LastDamageTime; }
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
	void Input_Melee();
	void Input_Flashlight();
	void Input_ToggleView();
	void Input_InteractStart();
	void Input_InteractStop();

	// Lógica por frame
	void UpdateMovementMode(float DeltaSeconds);
	void UpdateStamina(float DeltaSeconds);
	void UpdateFear(float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	void UpdatePostProcess(float DeltaSeconds);
	void UpdateAimSway(float DeltaSeconds);
	void UpdateFiring();
	void UpdateInteraction(float DeltaSeconds);
	void UpdateBodyPose(float DeltaSeconds);
	void UpdateFootsteps(float DeltaSeconds);
	void UpdateAimPoint();
	void UpdateFlashlight(float DeltaSeconds);
	void UpdateHeartAndBreath(float DeltaSeconds);

	void EquipWeapon(int32 Index);
	void SetFirstPerson(bool bEnable);
	FVector GetMuzzleLocation() const;

	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* Component, float NewHealth, float Delta, AActor* InstigatorActor);

	UFUNCTION()
	void HandleDeath(UHealthComponent* Component, AActor* Killer);

	// Componentes
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<UCameraComponent> FollowCamera;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<USpotLightComponent> Flashlight;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<UHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<UWeaponComponent> Weapon;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<USceneComponent> BodyRoot;
	UPROPERTY(VisibleAnywhere, Category = "Kael") TObjectPtr<UAIPerceptionStimuliSourceComponent> StimuliSource;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> BreathLoop;

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
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Melee;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Flashlight;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_View;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Interact;

	// Ajustes
	UPROPERTY(EditAnywhere, Category = "Kael|Movimento") float WalkSpeed = 330.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Movimento") float SprintSpeed = 600.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Movimento") float AimSpeed = 190.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Movimento") float DodgeStrength = 900.f;
	/** Segundos de corrida com o fôlego cheio. */
	UPROPERTY(EditAnywhere, Category = "Kael|Movimento") float SprintDuration = 6.5f;
	UPROPERTY(EditAnywhere, Category = "Kael|Camera") float MouseSensitivity = 1.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Camera") float PadSensitivity = 150.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Camera") bool bStartInFirstPerson = false;
	UPROPERTY(EditAnywhere, Category = "Kael|Lanterna") float FlashlightCandelas = 700.f;
	/** Segundos de lanterna acesa com a bateria cheia. */
	UPROPERTY(EditAnywhere, Category = "Kael|Lanterna") float BatteryLife = 420.f;
	UPROPERTY(EditAnywhere, Category = "Kael|Combate") int32 MaxGrenades = 2;
	UPROPERTY(EditAnywhere, Category = "Kael|Combate") float KnifeDamage = 34.f;

	// Estado
	bool bAiming = false;
	bool bSprinting = false;
	bool bFireHeld = false;
	bool bSemiShotQueued = false;
	bool bDead = false;
	bool bHasKeycard = false;
	bool bInputLocked = false;
	bool bInteractHeld = false;
	bool bInteractConsumed = false;
	bool bDodging = false;
	bool bFirstPerson = false;
	bool bFlashlightOn = true;
	bool bExhausted = false;
	int32 Grenades = 1;

	float Battery = 0.8f;
	float Interference = 0.f;
	float LastFlashlightToggle = -100.f;
	float FlickerUntil = 0.f;
	float Stamina = 1.f;
	float LastSprintTime = -100.f;
	float Fear = 0.f;
	float FearTarget = 0.f;
	float FearScanTimer = 0.f;
	float HeartTimer = 0.f;

	float Trauma = 0.f;
	float DamageFlash = 0.f;
	float LastDamageTime = -100.f;
	float LastShotTime = -100.f;
	float NextGrenadeTime = 0.f;
	float NextMeleeTime = 0.f;
	float MeleeTime = -100.f;
	float DodgeEndTime = 0.f;
	float NextDodgeTime = 0.f;
	float DodgeSpin = 0.f;
	float StaggerUntil = 0.f;
	float InteractProgress = 0.f;
	float FootstepTimer = 0.f;
	float BobPhase = 0.f;
	float DeathTime = 0.f;

	FVector AimPoint = FVector::ZeroVector;
	FRotator AppliedSway = FRotator::ZeroRotator;
	float SwayAmp = 0.f;

	TWeakObjectPtr<AInteractableActor> FocusedInteractable;
};
