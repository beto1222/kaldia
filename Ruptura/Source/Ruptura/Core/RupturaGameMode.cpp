#include "Core/RupturaGameMode.h"
#include "Core/RupturaGameInstance.h"
#include "Player/KaelCharacter.h"
#include "Player/KaelPlayerController.h"
#include "Player/RupturaHUD.h"
#include "Mission/MissionDirector.h"
#include "World/CityBuilder.h"
#include "Engine/World.h"

ARupturaGameMode::ARupturaGameMode()
{
	DefaultPawnClass = AKaelCharacter::StaticClass();
	PlayerControllerClass = AKaelPlayerController::StaticClass();
	HUDClass = ARupturaHUD::StaticClass();
}

void ARupturaGameMode::StartPlay()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// A geometria precisa existir antes do primeiro tick de física do jogador.
	City = World->SpawnActor<ACityBuilder>(ACityBuilder::StaticClass(), FTransform::Identity, Params);
	if (City)
	{
		City->Build();
	}

	Director = World->SpawnActor<AMissionDirector>(AMissionDirector::StaticClass(), FTransform::Identity, Params);
	if (Director)
	{
		Director->SetCityBuilder(City);
	}

	Super::StartPlay();
}

APawn* ARupturaGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	EMissionStage Checkpoint = EMissionStage::Explorar;
	if (const URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		Checkpoint = GI->CheckpointStage;
	}
	const FTransform SpawnTransform = AMissionDirector::GetCheckpointTransform(Checkpoint);
	if (NewPlayer)
	{
		NewPlayer->SetControlRotation(SpawnTransform.Rotator());
	}
	return SpawnDefaultPawnAtTransform(NewPlayer, SpawnTransform);
}

void ARupturaGameMode::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	// Usa a orientação do checkpoint (o mapa não precisa de PlayerStart).
	const APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	Super::FinishRestartPlayer(NewPlayer, Pawn ? Pawn->GetActorRotation() : StartRotation);
}
