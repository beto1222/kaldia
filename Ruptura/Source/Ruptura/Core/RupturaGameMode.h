#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RupturaGameMode.generated.h"

class AMissionDirector;
class ACityBuilder;

/**
 * Modo de jogo: constrói a fase (CityBuilder), cria o roteiro (MissionDirector)
 * e posiciona Kael no checkpoint atual. Funciona em qualquer mapa vazio.
 */
UCLASS()
class RUPTURA_API ARupturaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARupturaGameMode();

	virtual void StartPlay() override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;

	AMissionDirector* GetDirector() const { return Director; }
	ACityBuilder* GetCity() const { return City; }

protected:
	UPROPERTY(Transient) TObjectPtr<AMissionDirector> Director;
	UPROPERTY(Transient) TObjectPtr<ACityBuilder> City;
};
