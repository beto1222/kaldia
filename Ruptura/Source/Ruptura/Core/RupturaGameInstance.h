#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/RupturaTypes.h"
#include "RupturaGameInstance.generated.h"

/** Guarda o que sobrevive ao recarregamento do mapa: checkpoint, dificuldade e estatísticas. */
UCLASS()
class RUPTURA_API URupturaGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Ruptura")
	EMissionStage CheckpointStage = EMissionStage::Explorar;

	UPROPERTY(BlueprintReadWrite, Category = "Ruptura")
	ERupturaDifficulty Difficulty = ERupturaDifficulty::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "Ruptura") int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ruptura") int32 ShotsFired = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ruptura") int32 ShotsHit = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ruptura") int32 Deaths = 0;
	/** Tempo de missão acumulado de tentativas anteriores (recarregamentos por checkpoint). */
	UPROPERTY(BlueprintReadOnly, Category = "Ruptura") float AccumulatedTime = 0.f;
	/** A cena de abertura só toca uma vez por tentativa (não repete ao morrer). */
	UPROPERTY(BlueprintReadOnly, Category = "Ruptura") bool bIntroSeen = false;

	void ResetRun()
	{
		bIntroSeen = false;
		CheckpointStage = EMissionStage::Explorar;
		Kills = ShotsFired = ShotsHit = Deaths = 0;
		AccumulatedTime = 0.f;
	}

	static URupturaGameInstance* Get(const UObject* WorldContext);
};
