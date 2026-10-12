#include "Player/KaelPlayerController.h"
#include "Player/KaelCharacter.h"
#include "AI/EnemyCharacter.h"
#include "Core/RupturaGameInstance.h"
#include "Core/RupturaAssets.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	// Trajeto da câmera de abertura: desce sobre a rua em direção à instalação militar.
	const FVector IntroFrom(-700.f, 250.f, 950.f);
	const FVector IntroTo(900.f, -250.f, 330.f);
	const FVector IntroLook(6500.f, 0.f, 250.f);
	constexpr float IntroDuration = 10.f;
}

AKaelPlayerController::AKaelPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
}

void AKaelPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	BuildInput();
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->RemoveMappingContext(Mapping);
			Subsystem->AddMappingContext(Mapping, 1);
		}
	}

	URupturaGameInstance* GI = URupturaGameInstance::Get(this);
	if (GI && !GI->bIntroSeen && GI->CheckpointStage == EMissionStage::Explorar)
	{
		GI->bIntroSeen = true;
		PlayCinematic(IntroFrom, IntroTo, IntroLook, IntroDuration, FText::FromString(TEXT("Setor 7 — 02:40. Tempestade sobre a cidade evacuada.")), true);
	}
	else
	{
		HelpAutoUntil = GetWorld()->GetTimeSeconds() + 8.f;
	}
}

void AKaelPlayerController::BuildInput()
{
	if (Mapping)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* Name)
	{
		UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
		Action->ValueType = EInputActionValueType::Boolean;
		return Action;
	};

	IA_Skip = MakeAction(TEXT("IA_PularCena"));
	IA_Help = MakeAction(TEXT("IA_Ajuda"));
	IA_Restart = MakeAction(TEXT("IA_Reiniciar"));
	IA_Q0 = MakeAction(TEXT("IA_QualidadeBaixa"));
	IA_Q1 = MakeAction(TEXT("IA_QualidadeMedia"));
	IA_Q2 = MakeAction(TEXT("IA_QualidadeAlta"));
	IA_Q3 = MakeAction(TEXT("IA_QualidadeEpica"));
	IA_D0 = MakeAction(TEXT("IA_DificuldadeFacil"));
	IA_D1 = MakeAction(TEXT("IA_DificuldadeNormal"));
	IA_D2 = MakeAction(TEXT("IA_DificuldadeDificil"));

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Sistema"));
	Mapping->MapKey(IA_Skip, EKeys::SpaceBar);
	Mapping->MapKey(IA_Skip, EKeys::Enter);
	Mapping->MapKey(IA_Skip, EKeys::Gamepad_Special_Right);
	Mapping->MapKey(IA_Help, EKeys::H);
	Mapping->MapKey(IA_Restart, EKeys::Enter);
	Mapping->MapKey(IA_Restart, EKeys::Gamepad_Special_Right);
	Mapping->MapKey(IA_Q0, EKeys::F5);
	Mapping->MapKey(IA_Q1, EKeys::F6);
	Mapping->MapKey(IA_Q2, EKeys::F7);
	Mapping->MapKey(IA_Q3, EKeys::F8);
	Mapping->MapKey(IA_D0, EKeys::F1);
	Mapping->MapKey(IA_D1, EKeys::F2);
	Mapping->MapKey(IA_D2, EKeys::F3);
}

void AKaelPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInput();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		return;
	}
	EIC->BindAction(IA_Skip, ETriggerEvent::Started, this, &AKaelPlayerController::Input_Skip);
	EIC->BindAction(IA_Help, ETriggerEvent::Started, this, &AKaelPlayerController::Input_Help);
	EIC->BindAction(IA_Restart, ETriggerEvent::Started, this, &AKaelPlayerController::Input_Restart);
	EIC->BindAction(IA_Q0, ETriggerEvent::Started, this, &AKaelPlayerController::Input_QualityLow);
	EIC->BindAction(IA_Q1, ETriggerEvent::Started, this, &AKaelPlayerController::Input_QualityMedium);
	EIC->BindAction(IA_Q2, ETriggerEvent::Started, this, &AKaelPlayerController::Input_QualityHigh);
	EIC->BindAction(IA_Q3, ETriggerEvent::Started, this, &AKaelPlayerController::Input_QualityEpic);
	EIC->BindAction(IA_D0, ETriggerEvent::Started, this, &AKaelPlayerController::Input_DifficultyEasy);
	EIC->BindAction(IA_D1, ETriggerEvent::Started, this, &AKaelPlayerController::Input_DifficultyNormal);
	EIC->BindAction(IA_D2, ETriggerEvent::Started, this, &AKaelPlayerController::Input_DifficultyHard);
}

// ---------------------------------------------------------------------------------------------
// Cinemáticas
// ---------------------------------------------------------------------------------------------

void AKaelPlayerController::PlayCinematic(const FVector& From, const FVector& To, const FVector& LookAt, float Duration, const FText& Caption, bool bIsIntro)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!CineCamera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CineCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), From, (LookAt - From).Rotation(), Params);
		if (CineCamera)
		{
			CineCamera->GetCameraComponent()->SetFieldOfView(68.f);
			CineCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		}
	}
	if (!CineCamera)
	{
		return;
	}

	bCinematic = true;
	bIntro = bIsIntro;
	CineFrom = From;
	CineTo = To;
	CineLook = LookAt;
	CineStart = World->GetTimeSeconds();
	CineDuration = FMath::Max(0.5f, Duration);
	CineCaption = Caption;

	CineCamera->SetActorLocationAndRotation(From, (LookAt - From).Rotation());
	SetViewTargetWithBlend(CineCamera, bIsIntro ? 0.f : 0.6f, VTBlend_Cubic);
	LockPawn(true);
}

float AKaelPlayerController::GetCinematicTime() const
{
	return bCinematic ? GetWorld()->GetTimeSeconds() - CineStart : 0.f;
}

void AKaelPlayerController::EndCinematic()
{
	if (!bCinematic)
	{
		return;
	}
	const bool bWasIntro = bIntro;
	bCinematic = false;
	bIntro = false;

	if (APawn* P = GetPawn())
	{
		SetViewTargetWithBlend(P, 1.0f, VTBlend_Cubic);
		if (bWasIntro)
		{
			SetControlRotation(FRotator(-8.f, P->GetActorRotation().Yaw, 0.f));
		}
	}
	LockPawn(false);

	if (bWasIntro)
	{
		HelpAutoUntil = GetWorld()->GetTimeSeconds() + 20.f;
	}
}

void AKaelPlayerController::LockPawn(bool bLock)
{
	if (AKaelCharacter* Kael = Cast<AKaelCharacter>(GetPawn()))
	{
		if (!bLock && (Kael->IsDead() || EndState != EEndState::None))
		{
			return;
		}
		Kael->SetInputLocked(bLock);
	}
}

void AKaelPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bCinematic || !CineCamera)
	{
		return;
	}

	LockPawn(true); // o pawn pode ter sido possuído depois do início da cena

	const float Alpha = FMath::Clamp(GetCinematicTime() / CineDuration, 0.f, 1.f);
	const float Smooth = FMath::SmoothStep(0.f, 1.f, Alpha);
	const FVector Loc = FMath::Lerp(CineFrom, CineTo, Smooth);
	CineCamera->SetActorLocationAndRotation(Loc, (CineLook - Loc).Rotation());

	if (Alpha >= 1.f)
	{
		EndCinematic();
	}
}

// ---------------------------------------------------------------------------------------------
// Fim de jogo e reinício
// ---------------------------------------------------------------------------------------------

void AKaelPlayerController::OnKaelDied()
{
	if (EndState != EEndState::None)
	{
		return;
	}
	EndState = EEndState::Dead;
	EndStateTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &AKaelPlayerController::ReloadLevel, 4.5f, false);
}

void AKaelPlayerController::OnMissionComplete(float InFinalTime)
{
	if (EndState != EEndState::None)
	{
		return;
	}
	EndState = EEndState::Complete;
	EndStateTime = GetWorld()->GetTimeSeconds();
	FinalTime = InFinalTime;
	LockPawn(true);
	if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		GI->CheckpointStage = EMissionStage::Explorar;
	}
}

void AKaelPlayerController::OnMissionFailed(const FText& Reason)
{
	if (EndState != EEndState::None)
	{
		return;
	}
	EndState = EEndState::Failed;
	EndStateTime = GetWorld()->GetTimeSeconds();
	FailReason = Reason;
	LockPawn(true);
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &AKaelPlayerController::ReloadLevel, 6.f, false);
}

float AKaelPlayerController::GetEndStateAge() const
{
	return EndState == EEndState::None ? 0.f : GetWorld()->GetTimeSeconds() - EndStateTime;
}

void AKaelPlayerController::ReloadLevel()
{
	if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		if (EndState != EEndState::Complete)
		{
			GI->AccumulatedTime += GetWorld()->GetTimeSeconds();
		}
	}
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void AKaelPlayerController::Input_Restart()
{
	if (EndState == EEndState::Complete)
	{
		if (GetEndStateAge() < 1.5f)
		{
			return;
		}
		if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
		{
			GI->ResetRun();
		}
		ReloadLevel();
	}
	else if (EndState == EEndState::Dead || EndState == EEndState::Failed)
	{
		if (GetEndStateAge() > 1.f)
		{
			GetWorldTimerManager().ClearTimer(ReloadTimer);
			ReloadLevel();
		}
	}
}

void AKaelPlayerController::Input_Skip()
{
	if (bCinematic && GetCinematicTime() > 0.5f)
	{
		EndCinematic();
	}
}

// ---------------------------------------------------------------------------------------------
// Ajuda, qualidade e dificuldade
// ---------------------------------------------------------------------------------------------

void AKaelPlayerController::Input_Help()
{
	if (IsHelpVisible())
	{
		bHelpToggled = false;
		HelpAutoUntil = 0.f;
	}
	else
	{
		bHelpToggled = true;
	}
}

bool AKaelPlayerController::IsHelpVisible() const
{
	return !bCinematic && EndState == EEndState::None && (bHelpToggled || GetWorld()->GetTimeSeconds() < HelpAutoUntil);
}

void AKaelPlayerController::ShowToast(const FText& Text, float Duration)
{
	ToastText = Text;
	ToastStart = GetWorld()->GetTimeSeconds();
	ToastDuration = Duration;
}

bool AKaelPlayerController::GetToast(FText& OutText, float& OutAlpha) const
{
	const float Age = GetWorld()->GetTimeSeconds() - ToastStart;
	if (Age > ToastDuration)
	{
		return false;
	}
	OutText = ToastText;
	OutAlpha = FMath::Clamp((ToastDuration - Age) / 0.4f, 0.f, 1.f);
	return true;
}

void AKaelPlayerController::Input_QualityLow() { SetQuality(0); }
void AKaelPlayerController::Input_QualityMedium() { SetQuality(1); }
void AKaelPlayerController::Input_QualityHigh() { SetQuality(2); }
void AKaelPlayerController::Input_QualityEpic() { SetQuality(3); }

void AKaelPlayerController::SetQuality(int32 Level)
{
	if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
	{
		Settings->SetOverallScalabilityLevel(Level);
		Settings->ApplySettings(false);
	}
	static const TCHAR* Names[] = { TEXT("BAIXA (Low)"), TEXT("MÉDIA (Medium)"), TEXT("ALTA (High)"), TEXT("ÉPICA (Epic)") };
	ShowToast(FText::FromString(FString::Printf(TEXT("Qualidade gráfica: %s"), Names[FMath::Clamp(Level, 0, 3)])));
}

void AKaelPlayerController::Input_DifficultyEasy() { SetDifficulty(ERupturaDifficulty::Facil); }
void AKaelPlayerController::Input_DifficultyNormal() { SetDifficulty(ERupturaDifficulty::Normal); }
void AKaelPlayerController::Input_DifficultyHard() { SetDifficulty(ERupturaDifficulty::Dificil); }

void AKaelPlayerController::SetDifficulty(ERupturaDifficulty Difficulty)
{
	if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		GI->Difficulty = Difficulty;
	}
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		It->ApplyDifficulty(Difficulty);
	}
	ShowToast(FText::FromString(FString::Printf(TEXT("Dificuldade: %s"), *FRupturaTuning::DifficultyName(Difficulty).ToString())));
}
