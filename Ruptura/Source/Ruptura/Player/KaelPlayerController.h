#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/RupturaTypes.h"
#include "KaelPlayerController.generated.h"

class ACameraActor;
class UInputAction;
class UInputMappingContext;

/**
 * Controla cenas cinematográficas curtas (abertura e apresentação do chefe), telas de fim,
 * reinício por checkpoint, qualidade gráfica (F5–F8) e dificuldade (F1–F3).
 */
UCLASS()
class RUPTURA_API AKaelPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKaelPlayerController();

	enum class EEndState : uint8 { None, Dead, Complete, Failed };

	virtual void Tick(float DeltaSeconds) override;

	void PlayCinematic(const FVector& From, const FVector& To, const FVector& LookAt, float Duration, const FText& Caption, bool bIsIntro = false);
	bool IsInCinematic() const { return bCinematic; }
	bool IsIntro() const { return bCinematic && bIntro; }
	float GetCinematicTime() const;
	const FText& GetCinematicCaption() const { return CineCaption; }

	void OnKaelDied();
	void OnMissionComplete(float InFinalTime);
	void OnMissionFailed(const FText& Reason);

	EEndState GetEndState() const { return EndState; }
	float GetEndStateAge() const;
	const FText& GetFailReason() const { return FailReason; }
	float GetFinalTime() const { return FinalTime; }

	void ShowToast(const FText& Text, float Duration = 2.5f);
	bool GetToast(FText& OutText, float& OutAlpha) const;
	bool IsHelpVisible() const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	void BuildInput();
	void Input_Skip();
	void Input_Help();
	void Input_Restart();
	void Input_QualityLow();
	void Input_QualityMedium();
	void Input_QualityHigh();
	void Input_QualityEpic();
	void Input_DifficultyEasy();
	void Input_DifficultyNormal();
	void Input_DifficultyHard();

	void SetQuality(int32 Level);
	void SetDifficulty(ERupturaDifficulty Difficulty);
	void EndCinematic();
	void ReloadLevel();
	void LockPawn(bool bLock);

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Skip;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Help;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Restart;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Q0;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Q1;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Q2;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Q3;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_D0;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_D1;
	UPROPERTY(Transient) TObjectPtr<UInputAction> IA_D2;

	UPROPERTY(Transient) TObjectPtr<ACameraActor> CineCamera;

	bool bCinematic = false;
	bool bIntro = false;
	FVector CineFrom = FVector::ZeroVector;
	FVector CineTo = FVector::ZeroVector;
	FVector CineLook = FVector::ZeroVector;
	float CineStart = 0.f;
	float CineDuration = 1.f;
	FText CineCaption;

	EEndState EndState = EEndState::None;
	float EndStateTime = 0.f;
	FText FailReason;
	float FinalTime = 0.f;

	FText ToastText;
	float ToastStart = -100.f;
	float ToastDuration = 0.f;

	bool bHelpToggled = false;
	float HelpAutoUntil = 0.f;

	FTimerHandle ReloadTimer;
};
