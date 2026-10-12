#pragma once

#include "CoreMinimal.h"
#include "RupturaTypes.generated.h"

UENUM(BlueprintType)
enum class ERupturaDifficulty : uint8
{
	Facil,
	Normal,
	Dificil
};

UENUM(BlueprintType)
enum class EMissionStage : uint8
{
	Explorar,
	EncontrarInstalacao,
	EnfrentarPatrulha,
	RecuperarCartao,
	EntrarSubsolo,
	DesativarSeguranca,
	EnfrentarRobo,
	DestruirNucleo,
	Escapar,
	Concluida,
	Falhou
};

UENUM(BlueprintType)
enum class EEnemyAlert : uint8
{
	Calmo,
	Suspeito,
	Combate,
	Buscando,
	Morto
};

/** Parâmetros de comportamento inimigo derivados da dificuldade. */
USTRUCT(BlueprintType)
struct FEnemyTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float MaxHealth = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float ShotDamage = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float SpreadDeg = 4.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float ReactionTime = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float DetectionRate = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float FlankChance = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float BurstTime = 1.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float FireInterval = 0.15f;
};

struct RUPTURA_API FRupturaTuning
{
	static FEnemyTuning ForDifficulty(ERupturaDifficulty Difficulty, bool bElite);
	static FText DifficultyName(ERupturaDifficulty Difficulty);
	static FText StageText(EMissionStage Stage);
};
