#include "Core/RupturaTypes.h"

FEnemyTuning FRupturaTuning::ForDifficulty(ERupturaDifficulty Difficulty, bool bElite)
{
	FEnemyTuning T;
	switch (Difficulty)
	{
	case ERupturaDifficulty::Facil:
		T.MaxHealth = 70.f; T.ShotDamage = 5.f; T.SpreadDeg = 7.f; T.ReactionTime = 1.0f;
		T.DetectionRate = 0.6f; T.FlankChance = 0.15f; T.BurstTime = 1.0f; T.FireInterval = 0.2f;
		break;
	case ERupturaDifficulty::Dificil:
		T.MaxHealth = 130.f; T.ShotDamage = 11.f; T.SpreadDeg = 2.8f; T.ReactionTime = 0.35f;
		T.DetectionRate = 1.5f; T.FlankChance = 0.6f; T.BurstTime = 1.8f; T.FireInterval = 0.12f;
		break;
	default:
		break; // Normal = valores padrão da struct
	}

	if (bElite)
	{
		T.MaxHealth *= 1.5f;
		T.SpreadDeg *= 0.7f;
		T.ReactionTime *= 0.7f;
		T.FlankChance = FMath::Min(0.9f, T.FlankChance + 0.25f);
	}
	return T;
}

FText FRupturaTuning::DifficultyName(ERupturaDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case ERupturaDifficulty::Facil: return FText::FromString(TEXT("FÁCIL"));
	case ERupturaDifficulty::Dificil: return FText::FromString(TEXT("DIFÍCIL"));
	default: return FText::FromString(TEXT("NORMAL"));
	}
}

FText FRupturaTuning::StageText(EMissionStage Stage)
{
	switch (Stage)
	{
	case EMissionStage::Explorar: return FText::FromString(TEXT("Explore a rua de entrada"));
	case EMissionStage::EncontrarInstalacao: return FText::FromString(TEXT("Encontre a instalação militar"));
	case EMissionStage::EnfrentarPatrulha: return FText::FromString(TEXT("Elimine a patrulha inimiga"));
	case EMissionStage::RecuperarCartao: return FText::FromString(TEXT("Recupere o cartão de acesso no posto da guarda"));
	case EMissionStage::EntrarSubsolo: return FText::FromString(TEXT("Entre na instalação subterrânea"));
	case EMissionStage::DesativarSeguranca: return FText::FromString(TEXT("Desative o sistema de segurança no terminal"));
	case EMissionStage::EnfrentarRobo: return FText::FromString(TEXT("Derrote o robô de combate COLOSSO"));
	case EMissionStage::DestruirNucleo: return FText::FromString(TEXT("Destrua o núcleo de energia"));
	case EMissionStage::Escapar: return FText::FromString(TEXT("Escape para o ponto de extração na rua"));
	case EMissionStage::Concluida: return FText::FromString(TEXT("Missão concluída"));
	case EMissionStage::Falhou: return FText::FromString(TEXT("Missão fracassada"));
	default: return FText::GetEmpty();
	}
}
