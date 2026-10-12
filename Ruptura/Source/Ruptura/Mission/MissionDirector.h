#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/RupturaTypes.h"
#include "Mission/Pickup.h"
#include "MissionDirector.generated.h"

class AEnemyCharacter;
class AAbomination;
class ASecurityDoor;
class ASecurityTerminal;
class APowerCore;
class AKeycardPickup;
class AObjectiveZone;
class ACityBuilder;
class AKaelPlayerController;
class UAudioComponent;

/**
 * Roteiro: cria os elementos jogáveis, acompanha os 9 objetivos, checkpoints, a horda, a fuga
 * cronometrada e os sustos roteirizados (apagões, aparição, porta que bate, armário...).
 * Também conduz o "diretor de medo": sons ambientes atrás do jogador, trilha de perseguição,
 * e o desabamento durante a fuga.
 */
UCLASS()
class RUPTURA_API AMissionDirector : public AActor
{
	GENERATED_BODY()

public:
	AMissionDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static AMissionDirector* Get(const UObject* WorldContext);
	static FTransform GetCheckpointTransform(EMissionStage CheckpointStage);

	void SetCityBuilder(ACityBuilder* InCity) { City = InCity; }

	// Eventos de jogo
	void OnZoneEntered(FName ZoneId);
	void OnEnemyKilled(AEnemyCharacter* Enemy, AActor* Killer);
	void OnKeycardTaken();
	void OnTerminalHacked();
	void OnBossEnraged();
	void OnCoreDestroyed();
	void OnApparitionVanished(const FVector& Location);
	void ShowHint(const FText& Text);

	// Consultas do HUD
	EMissionStage GetStage() const { return Stage; }
	FString GetObjectiveDetail() const;
	bool GetObjectiveLocation(FVector& OutLocation) const;
	bool GetBanner(FText& OutTitle, FText& OutSub, float& OutAlpha) const;
	float GetEscapeTimeLeft() const;
	AEnemyCharacter* GetActiveBoss() const;
	float GetMissionTime() const;

protected:
	void SpawnMissionActors(EMissionStage Checkpoint);
	void ApplyCheckpointWorldState(EMissionStage Checkpoint);
	AEnemyCharacter* SpawnEnemy(TSubclassOf<AEnemyCharacter> Class, const FVector& Location, float Yaw, FName Group, const TArray<FVector>& Patrol, bool bElite, bool bDormant = false);
	APickup* SpawnPickup(EPickupType Type, const FVector& Location, int32 Amount, float RespawnTime);
	AObjectiveZone* SpawnZone(FName ZoneId, const FVector& Center, const FVector& Extent);

	void SetStage(EMissionStage NewStage);
	void OnStageEntered(EMissionStage NewStage);
	bool IsStageConditionMet(EMissionStage Check) const;
	void EvaluateProgress();
	int32 CountAlive(FName Group) const;

	// Sustos roteirizados
	bool FireEventOnce(FName EventId);
	void SpawnApparition();
	void SpawnClosetScare();
	void TunnelBanging();
	void PlayScare(FName Sound, const FVector& Location, float Fear, float Volume = 1.f);

	void StartEscape();
	void SpawnEscapeReinforcements();
	void PlayBossIntro();
	void ActivateBossNow();
	void SaveCheckpoint(EMissionStage CheckpointStage);
	void ShowBanner(const FText& Title, const FText& SubText, float Duration);
	void UpdateMusic(float DeltaSeconds);
	void UpdateAmbientScares(float DeltaSeconds);
	void UpdateCollapse(float DeltaSeconds);
	void AlertGroupTo(FName Group, const FVector& Location);
	bool IsPlayerUnderground() const;
	AKaelPlayerController* GetPC() const;

	UPROPERTY(Transient) TObjectPtr<ACityBuilder> City;
	UPROPERTY(Transient) TObjectPtr<ASecurityDoor> BunkerDoor;
	UPROPERTY(Transient) TObjectPtr<ASecurityDoor> BlastDoor;
	UPROPERTY(Transient) TObjectPtr<ASecurityTerminal> Terminal;
	UPROPERTY(Transient) TObjectPtr<APowerCore> Core;
	UPROPERTY(Transient) TObjectPtr<AAbomination> Boss;
	UPROPERTY(Transient) TObjectPtr<AKeycardPickup> Keycard;

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Ambience;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> ChaseMusic;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RainLoop;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> AlarmLoop;

	EMissionStage Stage = EMissionStage::Explorar;
	EMissionStage StartCheckpoint = EMissionStage::Explorar;

	bool bReachedStreet = false;
	bool bReachedGate = false;
	bool bKeycard = false;
	bool bReachedUnderground = false;
	bool bTerminalHacked = false;
	bool bBossDead = false;
	bool bCoreDestroyed = false;
	bool bExtracted = false;
	bool bBossIntroPlayed = false;
	bool bPendingKeycardGive = false;
	bool bEvaluating = false;

	TSet<FName> FiredEvents;

	float EscapeEnd = -1.f;
	FText BannerTitle;
	FText BannerSub;
	float BannerStart = -100.f;
	float BannerDuration = 0.f;

	bool bChaseMusic = false;
	bool bUnderground = false;
	float MusicCheckTimer = 0.f;
	float AmbientTimer = 25.f;
	float CollapseTimer = 3.f;

	FTimerHandle BossTimer;
	FTimerHandle EventTimer;
	FTimerHandle EventTimer2;
};
