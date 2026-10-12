#include "Core/RupturaTypes.h"

FEnemyTuning FRupturaTuning::ForDifficulty(ERupturaDifficulty Difficulty, bool bElite)
{
	FEnemyTuning T;
	switch (Difficulty)
	{
	case ERupturaDifficulty::Facil:
		T.MaxHealth = 80.f; T.MeleeDamage = 12.f; T.AttackInterval = 1.9f; T.AttackWindup = 0.6f;
		T.ChaseSpeed = 360.f; T.DetectionRate = 0.6f; T.StaggerResist = 0.7f;
		break;
	case ERupturaDifficulty::Dificil:
		T.MaxHealth = 150.f; T.MeleeDamage = 28.f; T.AttackInterval = 1.2f; T.AttackWindup = 0.36f;
		T.ChaseSpeed = 500.f; T.DetectionRate = 1.5f; T.StaggerResist = 1.4f;
		break;
	default:
		break; // Normal = valores padrão da struct
	}

	// Mutante: maior, mais resistente e mais rápido. Grita e atrai os outros.
	if (bElite)
	{
		T.MaxHealth *= 1.9f;
		T.MeleeDamage *= 1.35f;
		T.AttackWindup *= 0.8f;
		T.ChaseSpeed *= 1.15f;
		T.StaggerResist *= 1.8f;
	}
	return T;
}

FText FRupturaTuning::DifficultyName(ERupturaDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case ERupturaDifficulty::Facil: return FText::FromString(TEXT("HISTÓRIA"));
	case ERupturaDifficulty::Dificil: return FText::FromString(TEXT("PESADELO"));
	default: return FText::FromString(TEXT("SOBREVIVÊNCIA"));
	}
}

FText FRupturaTuning::StageText(EMissionStage Stage)
{
	switch (Stage)
	{
	case EMissionStage::Explorar: return FText::FromString(TEXT("Siga o sinal de socorro pela avenida"));
	case EMissionStage::EncontrarInstalacao: return FText::FromString(TEXT("Encontre a entrada da instalação militar"));
	case EMissionStage::EnfrentarPatrulha: return FText::FromString(TEXT("Sobreviva aos infectados do pátio"));
	case EMissionStage::RecuperarCartao: return FText::FromString(TEXT("Pegue o cartão de acesso no posto da guarda"));
	case EMissionStage::EntrarSubsolo: return FText::FromString(TEXT("Desça ao subsolo da instalação"));
	case EMissionStage::DesativarSeguranca: return FText::FromString(TEXT("Restaure a energia na sala de controle"));
	case EMissionStage::EnfrentarRobo: return FText::FromString(TEXT("Mate o HOSPEDEIRO"));
	case EMissionStage::DestruirNucleo: return FText::FromString(TEXT("Destrua o núcleo do Protocolo Zero"));
	case EMissionStage::Escapar: return FText::FromString(TEXT("Fuja para a avenida antes do colapso"));
	case EMissionStage::Concluida: return FText::FromString(TEXT("Você sobreviveu"));
	case EMissionStage::Falhou: return FText::FromString(TEXT("Soterrado"));
	default: return FText::GetEmpty();
	}
}
