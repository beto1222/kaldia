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

/** Etapas do roteiro. Os nomes internos são da versão original; os textos exibidos ficam em StageText. */
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

/** Parâmetros dos infectados derivados da dificuldade. */
USTRUCT(BlueprintType)
struct FEnemyTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float MaxHealth = 110.f;
	/** Dano de cada golpe corpo a corpo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float MeleeDamage = 20.f;
	/** Tempo mínimo entre o início de dois golpes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float AttackInterval = 1.5f;
	/** Duração da preparação do golpe (janela para o jogador reagir). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float AttackWindup = 0.45f;
	/** Velocidade ao perseguir o jogador (cm/s). Kael corre a 600. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float ChaseSpeed = 430.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float DetectionRate = 1.f;
	/** Fração do dano recebido que vira atordoamento (interrompe ataques). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ruptura") float StaggerResist = 1.f;
};

struct RUPTURA_API FRupturaTuning
{
	static FEnemyTuning ForDifficulty(ERupturaDifficulty Difficulty, bool bElite);
	static FText DifficultyName(ERupturaDifficulty Difficulty);
	static FText StageText(EMissionStage Stage);
};
