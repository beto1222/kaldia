#include "Mission/MissionDirector.h"
#include "Mission/MissionActors.h"
#include "Mission/SecurityDoor.h"
#include "Mission/Pickup.h"
#include "AI/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "AI/Abomination.h"
#include "Player/KaelCharacter.h"
#include "Player/KaelPlayerController.h"
#include "World/CityBuilder.h"
#include "World/LevelLayout.h"
#include "Core/RupturaGameMode.h"
#include "Core/RupturaGameInstance.h"
#include "Core/RupturaAssets.h"
#include "Core/FxActor.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	const FName ZoneRua(TEXT("Rua"));
	const FName ZonePortao(TEXT("Portao"));
	const FName ZoneSubsolo(TEXT("Subsolo"));
	const FName ZoneReator(TEXT("Reator"));
	const FName ZoneExtracao(TEXT("Extracao"));
	const FName ZoneApagao(TEXT("EvApagao"));
	const FName ZoneTunel(TEXT("EvTunel"));
	const FName ZoneAla(TEXT("EvAla"));

	const FName GroupRua(TEXT("Rua"));
	const FName GroupPatrulha(TEXT("Patrulha"));
	const FName GroupDormentes(TEXT("Dormentes"));
	const FName GroupSubsolo(TEXT("Subsolo"));
	const FName GroupSusto(TEXT("Susto"));
	const FName GroupReforco1(TEXT("Reforco1"));
	const FName GroupReforco2(TEXT("Reforco2"));
	const FName GroupChefe(TEXT("Chefe"));

	constexpr float StreetZ = 110.f;
	constexpr float SubZ = RupturaLayout::SubFloorZ + 100.f;

	FVector Street(float X, float Y) { return FVector(X, Y, StreetZ); }
	FVector Sub(float X, float Y) { return FVector(X, Y, SubZ); }

	// Áreas usadas pelos apagões.
	FBox StreetBlackoutArea() { return FBox(FVector(1900.f, -1500.f, -100.f), FVector(4300.f, 1500.f, 1500.f)); }
	FBox GuardPostArea() { return FBox(FVector(6780.f, 1480.f, 0.f), FVector(7420.f, 2120.f, 330.f)); }
	FBox TunnelArea() { return FBox(FVector(RupturaLayout::BunkerDoorX, -600.f, -1000.f), FVector(RupturaLayout::RampEndX, 600.f, 600.f)); }
	FBox WingArea() { return FBox(FVector(RupturaLayout::RampEndX, -1800.f, -1000.f), FVector(RupturaLayout::Hall1EndX, 2800.f, 0.f)); }
}

AMissionDirector::AMissionDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
}

AMissionDirector* AMissionDirector::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	if (ARupturaGameMode* GM = World->GetAuthGameMode<ARupturaGameMode>())
	{
		return GM->GetDirector();
	}
	return nullptr;
}

FTransform AMissionDirector::GetCheckpointTransform(EMissionStage CheckpointStage)
{
	switch (CheckpointStage)
	{
	case EMissionStage::DesativarSeguranca: return FTransform(FRotator(0.f, 0.f, 0.f), Sub(10350.f, 0.f));
	case EMissionStage::EnfrentarRobo: return FTransform(FRotator(0.f, 0.f, 0.f), Sub(12650.f, 0.f));
	case EMissionStage::DestruirNucleo: return FTransform(FRotator(0.f, 0.f, 0.f), Sub(13700.f, 0.f));
	case EMissionStage::Escapar: return FTransform(FRotator(0.f, 180.f, 0.f), Sub(15000.f, 900.f));
	default: return FTransform(FRotator(0.f, 0.f, 0.f), Street(200.f, 0.f));
	}
}

AKaelPlayerController* AMissionDirector::GetPC() const
{
	return Cast<AKaelPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
}

void AMissionDirector::BeginPlay()
{
	Super::BeginPlay();

	URupturaGameInstance* GI = URupturaGameInstance::Get(this);
	StartCheckpoint = GI ? GI->CheckpointStage : EMissionStage::Explorar;
	if (StartCheckpoint >= EMissionStage::Concluida)
	{
		StartCheckpoint = EMissionStage::Explorar;
	}

	// Estado implícito de um checkpoint: tudo antes dele já foi feito.
	bReachedStreet = StartCheckpoint > EMissionStage::Explorar;
	bReachedGate = StartCheckpoint > EMissionStage::EncontrarInstalacao;
	bKeycard = StartCheckpoint > EMissionStage::RecuperarCartao;
	bReachedUnderground = StartCheckpoint > EMissionStage::EntrarSubsolo;
	bTerminalHacked = StartCheckpoint > EMissionStage::DesativarSeguranca;
	bBossDead = StartCheckpoint > EMissionStage::EnfrentarRobo;
	bCoreDestroyed = StartCheckpoint > EMissionStage::DestruirNucleo;
	bBossIntroPlayed = bBossDead;
	bPendingKeycardGive = bKeycard;

	SpawnMissionActors(StartCheckpoint);
	ApplyCheckpointWorldState(StartCheckpoint);

	// Áudio ambiente e trilha dinâmica (silêncio se os sons não tiverem sido importados).
	RainLoop = RupturaAssets::CreateLoop2D(this, TEXT("chuva_loop"), StartCheckpoint >= EMissionStage::DesativarSeguranca ? 0.05f : 0.45f);
	Ambience = RupturaAssets::CreateLoop2D(this, TEXT("ambiente_loop"), 0.5f);
	ChaseMusic = RupturaAssets::CreateLoop2D(this, TEXT("perseguicao_loop"), 0.01f);
	if (StartCheckpoint > EMissionStage::DesativarSeguranca)
	{
		AlarmLoop = RupturaAssets::CreateLoop2D(this, TEXT("alarme_loop"), 0.12f);
	}

	Stage = StartCheckpoint;
	OnStageEntered(Stage);
	EvaluateProgress();
}

// ---------------------------------------------------------------------------------------------
// Criação dos elementos da missão
// ---------------------------------------------------------------------------------------------

AEnemyCharacter* AMissionDirector::SpawnEnemy(TSubclassOf<AEnemyCharacter> Class, const FVector& Location, float Yaw, FName Group, const TArray<FVector>& Patrol, bool bElite, bool bDormant)
{
	UWorld* World = GetWorld();
	const FTransform T(FRotator(0.f, Yaw, 0.f), Location);
	AEnemyCharacter* Enemy = World->SpawnActorDeferred<AEnemyCharacter>(Class, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Enemy)
	{
		Enemy->Setup(Group, Patrol, bElite, bDormant);
		UGameplayStatics::FinishSpawningActor(Enemy, T);
	}
	return Enemy;
}

APickup* AMissionDirector::SpawnPickup(EPickupType Type, const FVector& Location, int32 Amount, float RespawnTime)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APickup* P = GetWorld()->SpawnActor<APickup>(APickup::StaticClass(), Location, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Params);
	if (P)
	{
		P->Configure(Type, Amount, RespawnTime);
	}
	return P;
}

AObjectiveZone* AMissionDirector::SpawnZone(FName ZoneId, const FVector& Center, const FVector& Extent)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AObjectiveZone* Zone = GetWorld()->SpawnActor<AObjectiveZone>(AObjectiveZone::StaticClass(), Center, FRotator::ZeroRotator, Params);
	if (Zone)
	{
		Zone->Configure(ZoneId, Extent);
	}
	return Zone;
}

void AMissionDirector::SpawnMissionActors(EMissionStage CP)
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const float SubFloor = RupturaLayout::SubFloorZ;

	// Zonas de objetivo e de sustos
	SpawnZone(ZoneRua, FVector(2500.f, 0.f, 150.f), FVector(120.f, 1300.f, 300.f));
	SpawnZone(ZonePortao, FVector(5250.f, 0.f, 150.f), FVector(150.f, 1300.f, 300.f));
	SpawnZone(ZoneSubsolo, FVector(10250.f, 0.f, SubFloor + 150.f), FVector(150.f, 400.f, 250.f));
	SpawnZone(ZoneReator, FVector(13450.f, 0.f, SubFloor + 200.f), FVector(250.f, 1900.f, 350.f));
	SpawnZone(ZoneExtracao, FVector(100.f, 0.f, 150.f), FVector(300.f, 1000.f, 300.f));
	SpawnZone(ZoneApagao, FVector(1700.f, 0.f, 150.f), FVector(100.f, 1300.f, 300.f));
	SpawnZone(ZoneTunel, FVector(9300.f, 0.f, -380.f), FVector(120.f, 400.f, 300.f));
	SpawnZone(ZoneAla, FVector(10900.f, 0.f, SubFloor + 150.f), FVector(100.f, 1500.f, 250.f));

	// Portas
	BunkerDoor = World->SpawnActor<ASecurityDoor>(ASecurityDoor::StaticClass(), FVector(RupturaLayout::BunkerDoorX, 0.f, 0.f), FRotator::ZeroRotator, Params);
	if (BunkerDoor)
	{
		BunkerDoor->Configure(400.f, 350.f, true, false, FText::FromString(TEXT("PORTA DO BUNKER")));
	}
	BlastDoor = World->SpawnActor<ASecurityDoor>(ASecurityDoor::StaticClass(), FVector(RupturaLayout::Hall1EndX, 0.f, SubFloor), FRotator::ZeroRotator, Params);
	if (BlastDoor)
	{
		BlastDoor->Configure(400.f, 380.f, false, true, FText::FromString(TEXT("PORTA BLINDADA DO REATOR")));
		if (CP >= EMissionStage::EnfrentarRobo)
		{
			BlastDoor->Open(true);
		}
	}

	// Terminal (frente voltada para -Y, para quem entra na sala)
	Terminal = World->SpawnActor<ASecurityTerminal>(ASecurityTerminal::StaticClass(), RupturaLayout::TerminalLocation(), FRotator(0.f, -90.f, 0.f), Params);
	if (Terminal && CP >= EMissionStage::EnfrentarRobo)
	{
		Terminal->SetHacked(true);
	}

	// Crachá de acesso
	if (CP < EMissionStage::DesativarSeguranca)
	{
		Keycard = World->SpawnActor<AKeycardPickup>(AKeycardPickup::StaticClass(), RupturaLayout::KeycardLocation(), FRotator::ZeroRotator, Params);
	}

	// Núcleo do Protocolo Zero
	Core = World->SpawnActor<APowerCore>(APowerCore::StaticClass(), RupturaLayout::CoreLocation(), FRotator::ZeroRotator, Params);
	if (Core)
	{
		if (CP >= EMissionStage::DestruirNucleo)
		{
			Core->SetShield(false);
		}
		if (CP >= EMissionStage::Escapar)
		{
			Core->SetDestroyedState();
		}
	}

	const TSubclassOf<AEnemyCharacter> Infected = AEnemyCharacter::StaticClass();

	// Avenida e pátio
	if (CP < EMissionStage::DesativarSeguranca)
	{
		SpawnEnemy(Infected, Street(2300.f, -700.f), 90.f, GroupRua, { Street(2300.f, -700.f), Street(3000.f, -300.f), Street(2600.f, 600.f) }, false);
		SpawnEnemy(Infected, Street(1250.f, 520.f), 30.f, GroupRua, {}, false, true);     // caído entre os carros
		SpawnEnemy(Infected, Street(3100.f, -450.f), 200.f, GroupRua, {}, false, true);   // caído junto ao carro queimado
		SpawnEnemy(Infected, Street(3900.f, 1150.f), 90.f, GroupRua, {}, false);          // parado, de frente para a parede
		SpawnEnemy(Infected, Street(4500.f, -300.f), 180.f, GroupRua, { Street(4500.f, -300.f), Street(4700.f, 600.f) }, false);

		SpawnEnemy(Infected, Street(6000.f, 900.f), 0.f, GroupPatrulha,
			{ Street(6000.f, 900.f), Street(7200.f, 900.f), Street(7200.f, -900.f), Street(6000.f, -900.f) }, false);
		SpawnEnemy(Infected, Street(7400.f, -1300.f), 180.f, GroupPatrulha,
			{ Street(7400.f, -1300.f), Street(6200.f, -2100.f), Street(5800.f, -1300.f) }, false);
		SpawnEnemy(Infected, Street(6600.f, 0.f), 180.f, GroupPatrulha, { Street(6600.f, 0.f), Street(6600.f, 1300.f) }, false);
		SpawnEnemy(Infected, Street(7600.f, 500.f), 180.f, GroupPatrulha, { Street(7600.f, 500.f), Street(7600.f, -500.f) }, true);
		SpawnEnemy(Infected, Street(6300.f, -1700.f), 140.f, GroupPatrulha, {}, false);
		SpawnEnemy(Infected, Street(6900.f, -600.f), 60.f, GroupDormentes, {}, false, true);
	}
	// Ala subterrânea
	if (CP < EMissionStage::EnfrentarRobo)
	{
		SpawnEnemy(Infected, Sub(11200.f, 900.f), 180.f, GroupSubsolo, { Sub(11200.f, 900.f), Sub(12400.f, 900.f) }, false);
		SpawnEnemy(Infected, Sub(12000.f, -900.f), 180.f, GroupSubsolo, { Sub(12000.f, -900.f), Sub(10800.f, -900.f) }, true);
		SpawnEnemy(Infected, Sub(12600.f, 300.f), 180.f, GroupSubsolo, {}, false);
		SpawnEnemy(Infected, Sub(10800.f, -1300.f), 90.f, GroupSubsolo, {}, false, true);
		// Caído dentro da sala de controle: levanta enquanto você mexe no terminal.
		SpawnEnemy(Infected, Sub(12100.f, 2100.f), 0.f, GroupSubsolo, {}, false, true);
	}
	// O Hospedeiro
	if (CP < EMissionStage::DestruirNucleo)
	{
		Boss = Cast<AAbomination>(SpawnEnemy(AAbomination::StaticClass(), RupturaLayout::BossSpawn(), 180.f, GroupChefe, {}, false));
	}

	// Suprimentos: escassos. Cada bala conta.
	SpawnPickup(EPickupType::Vida, FVector(3000.f, 1050.f, 14.f), 30, 0.f);
	SpawnPickup(EPickupType::Municao, FVector(4600.f, 250.f, 0.f), 8, 0.f);
	SpawnPickup(EPickupType::Bateria, FVector(2050.f, -1050.f, 14.f), 40, 0.f);
	SpawnPickup(EPickupType::Granada, FVector(4650.f, -250.f, 0.f), 1, 0.f);
	SpawnPickup(EPickupType::Vida, FVector(7250.f, 1700.f, 0.f), 40, 0.f);
	SpawnPickup(EPickupType::Municao, FVector(6900.f, -1600.f, 0.f), 10, 0.f);
	SpawnPickup(EPickupType::Bateria, FVector(6150.f, 1450.f, 0.f), 35, 0.f);
	SpawnPickup(EPickupType::Municao, FVector(10700.f, -1300.f, SubFloor), 10, 0.f);
	SpawnPickup(EPickupType::Vida, FVector(12100.f, 2300.f, SubFloor), 40, 0.f);
	SpawnPickup(EPickupType::Bateria, FVector(12450.f, 2250.f, SubFloor), 40, 0.f);
	SpawnPickup(EPickupType::Municao, FVector(12650.f, 2200.f, SubFloor), 8, 0.f);
	// Salão do reator: itens que reaparecem (a luta é longa)
	SpawnPickup(EPickupType::Vida, FVector(13700.f, 1700.f, SubFloor), 35, 35.f);
	SpawnPickup(EPickupType::Municao, FVector(13700.f, -1700.f, SubFloor), 10, 25.f);
	SpawnPickup(EPickupType::Municao, FVector(16500.f, 1500.f, SubFloor), 10, 25.f);
	SpawnPickup(EPickupType::Bateria, FVector(14000.f, -1600.f, SubFloor), 30, 45.f);
	SpawnPickup(EPickupType::Granada, FVector(13600.f, 600.f, SubFloor), 1, 30.f);
}

void AMissionDirector::ApplyCheckpointWorldState(EMissionStage CP)
{
	// Sustos que já aconteceram antes deste checkpoint não se repetem, e o mundo fica como eles deixaram.
	if (CP >= EMissionStage::DesativarSeguranca)
	{
		FiredEvents.Add(ZoneApagao);
		FiredEvents.Add(ZoneTunel);
		FiredEvents.Add(ZoneAla);
		if (City)
		{
			City->BlackoutArea(StreetBlackoutArea(), FVector::ZeroVector, 0.f, 0.f, false);
			City->BlackoutArea(GuardPostArea(), FVector::ZeroVector, 0.f, 0.f, false);
			City->BlackoutArea(TunnelArea(), FVector::ZeroVector, 0.f, 0.f, false);
		}
		if (BunkerDoor && CP == EMissionStage::DesativarSeguranca)
		{
			BunkerDoor->Close(true, true);
		}
	}
	if (CP >= EMissionStage::EnfrentarRobo)
	{
		if (City)
		{
			City->BlackoutArea(WingArea(), FVector::ZeroVector, 0.f, 0.f, false);
			City->SetAlarm(true);
		}
		if (BunkerDoor)
		{
			BunkerDoor->Open(true);
		}
	}
}

int32 AMissionDirector::CountAlive(FName Group) const
{
	int32 Count = 0;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetGroup() == Group && !It->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

void AMissionDirector::AlertGroupTo(FName Group, const FVector& Location)
{
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetGroup() == Group && !It->IsDead())
		{
			if (AEnemyAIController* AI = Cast<AEnemyAIController>(It->GetController()))
			{
				AI->AlertTo(Location, true);
			}
		}
	}
}

bool AMissionDirector::IsPlayerUnderground() const
{
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	return P && (P->GetActorLocation().Z < -250.f || P->GetActorLocation().X > RupturaLayout::BunkerDoorX + 200.f);
}

// ---------------------------------------------------------------------------------------------
// Progressão
// ---------------------------------------------------------------------------------------------

bool AMissionDirector::IsStageConditionMet(EMissionStage Check) const
{
	switch (Check)
	{
	case EMissionStage::Explorar: return bReachedStreet;
	case EMissionStage::EncontrarInstalacao: return bReachedGate;
	case EMissionStage::EnfrentarPatrulha: return CountAlive(GroupPatrulha) == 0;
	case EMissionStage::RecuperarCartao: return bKeycard;
	case EMissionStage::EntrarSubsolo: return bReachedUnderground;
	case EMissionStage::DesativarSeguranca: return bTerminalHacked;
	case EMissionStage::EnfrentarRobo: return bBossDead;
	case EMissionStage::DestruirNucleo: return bCoreDestroyed;
	case EMissionStage::Escapar: return bExtracted;
	default: return false;
	}
}

void AMissionDirector::EvaluateProgress()
{
	if (bEvaluating)
	{
		return;
	}
	bEvaluating = true;
	while (Stage < EMissionStage::Concluida && IsStageConditionMet(Stage))
	{
		SetStage(static_cast<EMissionStage>(static_cast<uint8>(Stage) + 1));
	}
	bEvaluating = false;
}

void AMissionDirector::SetStage(EMissionStage NewStage)
{
	if (NewStage == Stage)
	{
		return;
	}
	Stage = NewStage;
	OnStageEntered(NewStage);
}

void AMissionDirector::OnStageEntered(EMissionStage NewStage)
{
	if (NewStage < EMissionStage::Concluida && NewStage != EMissionStage::Explorar)
	{
		ShowBanner(FText::FromString(TEXT("OBJETIVO")), FRupturaTuning::StageText(NewStage), 4.f);
		RupturaAssets::PlaySound2D(this, TEXT("objetivo"), 0.5f);
	}

	switch (NewStage)
	{
	case EMissionStage::Explorar:
		ShowBanner(FText::FromString(TEXT("RÁDIO — SINAL INTERMITENTE")),
			FText::FromString(TEXT("\"...se alguém ouvir isso... não venha para a instalação... não acenda as luzes...\"")), 7.f);
		RupturaAssets::PlaySound2D(this, TEXT("estatica"), 0.6f);
		break;

	case EMissionStage::DesativarSeguranca:
	case EMissionStage::EnfrentarRobo:
	case EMissionStage::DestruirNucleo:
		SaveCheckpoint(NewStage);
		break;

	case EMissionStage::Escapar:
		SaveCheckpoint(NewStage);
		StartEscape();
		break;

	case EMissionStage::Concluida:
	{
		ShowBanner(FText::FromString(TEXT("A AVENIDA")), FText::FromString(TEXT("Você saiu. Mas o rádio continua chiando...")), 4.f);
		EscapeEnd = -1.f;
		if (AlarmLoop)
		{
			AlarmLoop->FadeOut(2.f, 0.f);
		}
		if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
		{
			GI->CheckpointStage = EMissionStage::Explorar;
		}
		if (AKaelPlayerController* PC = GetPC())
		{
			PC->OnMissionComplete(GetMissionTime());
		}
		break;
	}

	case EMissionStage::Falhou:
	{
		EscapeEnd = -1.f;
		if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			RupturaFx::Explosion(GetWorld(), P->GetActorLocation() + P->GetActorForwardVector() * 900.f + FVector(0.f, 0.f, 200.f), 900.f);
			RupturaFx::CeilingDust(GetWorld(), P->GetActorLocation() + FVector(0.f, 0.f, 500.f));
			RupturaAssets::PlaySound2D(this, TEXT("desabamento"), 1.f, 0.7f);
			if (AKaelCharacter* Kael = Cast<AKaelCharacter>(P))
			{
				Kael->AddTrauma(1.f);
			}
		}
		if (AKaelPlayerController* PC = GetPC())
		{
			PC->OnMissionFailed(FText::FromString(TEXT("O subsolo desabou antes que você saísse.")));
		}
		break;
	}

	default:
		break;
	}
}

void AMissionDirector::SaveCheckpoint(EMissionStage CheckpointStage)
{
	if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		if (GI->CheckpointStage != CheckpointStage)
		{
			GI->CheckpointStage = CheckpointStage;
			if (AKaelPlayerController* PC = GetPC())
			{
				PC->ShowToast(FText::FromString(TEXT("Progresso salvo")), 2.f);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Sustos roteirizados
// ---------------------------------------------------------------------------------------------

bool AMissionDirector::FireEventOnce(FName EventId)
{
	if (FiredEvents.Contains(EventId))
	{
		return false;
	}
	FiredEvents.Add(EventId);
	return true;
}

void AMissionDirector::PlayScare(FName Sound, const FVector& Location, float Fear, float Volume)
{
	RupturaAssets::PlaySound3D(this, Sound, Location, Volume);
	if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Kael->AddFear(Fear);
	}
}

void AMissionDirector::SpawnApparition()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// No meio da avenida, recortado contra as luzes vermelhas do portão.
	GetWorld()->SpawnActor<AApparition>(AApparition::StaticClass(), FVector(4650.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f), Params);
}

void AMissionDirector::OnApparitionVanished(const FVector& Location)
{
	if (City)
	{
		City->PopLightNear(Location, 1500.f);
	}
}

void AMissionDirector::SpawnClosetScare()
{
	// O armário do posto da guarda não estava vazio.
	SpawnEnemy(AEnemyCharacter::StaticClass(), Street(7000.f, 1650.f), 90.f, GroupSusto, {}, false);
	if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		AlertGroupTo(GroupSusto, P->GetActorLocation());
	}
	PlayScare(TEXT("susto"), FVector(6950.f, 1900.f, 120.f), 0.7f);
	RupturaAssets::PlaySound3D(this, TEXT("porta_batida"), FVector(6900.f, 2000.f, 100.f), 0.8f, 1.3f);
}

void AMissionDirector::TunnelBanging()
{
	PlayScare(TEXT("batidas"), FVector(RupturaLayout::BunkerDoorX - 30.f, 0.f, 150.f), 0.35f);
}

// ---------------------------------------------------------------------------------------------
// Eventos
// ---------------------------------------------------------------------------------------------

void AMissionDirector::OnZoneEntered(FName ZoneId)
{
	if (ZoneId == ZoneRua)
	{
		bReachedStreet = true;
	}
	else if (ZoneId == ZonePortao)
	{
		bReachedGate = true;
		if (FireEventOnce(ZonePortao))
		{
			PlayScare(TEXT("grito_distante"), FVector(7600.f, -2200.f, 300.f), 0.25f);
		}
	}
	else if (ZoneId == ZoneSubsolo)
	{
		bReachedUnderground = true;
	}
	else if (ZoneId == ZoneReator)
	{
		if (Stage >= EMissionStage::EnfrentarRobo && Boss && !Boss->IsDead() && !Boss->IsActive() && !bBossIntroPlayed)
		{
			PlayBossIntro();
		}
	}
	else if (ZoneId == ZoneExtracao)
	{
		if (Stage == EMissionStage::Escapar)
		{
			bExtracted = true;
		}
	}
	else if (ZoneId == ZoneApagao && Stage < EMissionStage::Escapar && FireEventOnce(ZoneApagao))
	{
		// Os postes morrem um a um, do fundo da avenida na direção do jogador...
		if (City)
		{
			City->BlackoutArea(StreetBlackoutArea(), FVector(4300.f, 0.f, 600.f), 0.3f, 0.45f, true);
		}
		// ...e no fim da escuridão há alguém parado.
		GetWorldTimerManager().SetTimer(EventTimer, this, &AMissionDirector::SpawnApparition, 2.4f, false);
	}
	else if (ZoneId == ZoneTunel && Stage < EMissionStage::Escapar && FireEventOnce(ZoneTunel))
	{
		// A porta bate atrás do jogador e as luzes da rampa se apagam em sequência.
		if (BunkerDoor)
		{
			BunkerDoor->Close(false, true);
		}
		if (City)
		{
			City->BlackoutArea(TunnelArea(), FVector(RupturaLayout::BunkerDoorX, 0.f, 0.f), 0.6f, 0.35f, true);
		}
		if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Kael->AddFear(0.45f);
			Kael->AddTrauma(0.2f);
		}
		ShowHint(FText::FromString(TEXT("A porta do bunker travou atrás de você.")));
		GetWorldTimerManager().SetTimer(EventTimer2, this, &AMissionDirector::TunnelBanging, 3.f, false);
	}
	else if (ZoneId == ZoneAla && Stage < EMissionStage::Escapar && FireEventOnce(ZoneAla))
	{
		// Algo enorme bate na porta do reator, do lado de dentro.
		if (City)
		{
			City->Flicker(2.5f);
		}
		PlayScare(TEXT("batidas"), FVector(RupturaLayout::Hall1EndX - 30.f, 0.f, RupturaLayout::SubFloorZ + 200.f), 0.4f);
		RupturaAssets::PlaySound3D(this, TEXT("hospedeiro_rugido"), FVector(RupturaLayout::Hall1EndX + 600.f, 0.f, RupturaLayout::SubFloorZ + 200.f), 0.6f, 0.8f);
	}
	EvaluateProgress();
}

void AMissionDirector::OnEnemyKilled(AEnemyCharacter* Enemy, AActor* Killer)
{
	if (Killer && Killer == UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (URupturaGameInstance* GI = URupturaGameInstance::Get(this))
		{
			++GI->Kills;
		}
	}

	if (Enemy && Enemy->IsBoss())
	{
		bBossDead = true;
		GetWorldTimerManager().ClearTimer(BossTimer);
		if (Core)
		{
			Core->SetShield(false);
		}
		ShowBanner(FText::FromString(TEXT("O HOSPEDEIRO ESTÁ MORTO")), FText::FromString(TEXT("A membrana em volta do núcleo se rompeu")), 4.f);
	}
	else if (Enemy && Enemy->GetGroup() == GroupPatrulha && Stage == EMissionStage::EnfrentarPatrulha)
	{
		const int32 Left = CountAlive(GroupPatrulha);
		if (Left > 0)
		{
			ShowHint(FText::FromString(FString::Printf(TEXT("Ainda há %d no pátio"), Left)));
		}
	}
	EvaluateProgress();
}

void AMissionDirector::OnKeycardTaken()
{
	bKeycard = true;
	ShowHint(FText::FromString(TEXT("Crachá de acesso obtido — a porta do bunker vai abrir")));

	// A luz do posto estoura e o armário se abre.
	if (City)
	{
		City->BlackoutArea(GuardPostArea(), FVector(7100.f, 1800.f, 270.f), 0.f, 0.05f, true);
	}
	GetWorldTimerManager().SetTimer(EventTimer, this, &AMissionDirector::SpawnClosetScare, 1.3f, false);
	EvaluateProgress();
}

void AMissionDirector::OnTerminalHacked()
{
	bTerminalHacked = true;
	if (BlastDoor)
	{
		BlastDoor->Open();
	}
	if (BunkerDoor)
	{
		BunkerDoor->Open(); // a saída volta a ter energia
	}

	// A energia volta... e sobrecarrega: as luzes brancas estouram, só as vermelhas de emergência ficam.
	if (City)
	{
		City->BlackoutArea(WingArea(), RupturaLayout::TerminalLocation(), 0.2f, 0.12f, true);
		City->SetAlarm(true);
	}
	if (!AlarmLoop)
	{
		AlarmLoop = RupturaAssets::CreateLoop2D(this, TEXT("alarme_loop"), 0.12f);
	}

	// A porta do reator se abre e eles saem de lá.
	if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const TSubclassOf<AEnemyCharacter> Infected = AEnemyCharacter::StaticClass();
		SpawnEnemy(Infected, Sub(13700.f, 700.f), 180.f, GroupReforco1, {}, false);
		SpawnEnemy(Infected, Sub(13700.f, -700.f), 180.f, GroupReforco1, {}, false);
		SpawnEnemy(Infected, Sub(13900.f, 0.f), 180.f, GroupReforco1, {}, true);
		AlertGroupTo(GroupReforco1, P->GetActorLocation());
	}
	ShowBanner(FText::FromString(TEXT("ENERGIA DE EMERGÊNCIA")), FText::FromString(TEXT("A porta do reator se abriu. Alguma coisa está vindo de lá.")), 4.f);
	EvaluateProgress();
}

void AMissionDirector::OnBossEnraged()
{
	ShowBanner(FText::FromString(TEXT("ELE ESTÁ SANGRANDO")), FText::FromString(TEXT("E ficou mais rápido")), 2.5f);
	ShowHint(FText::FromString(TEXT("Faça ele investir contra um pilar — atordoado, o coração fica exposto")));
	if (City)
	{
		City->Flicker(3.f);
	}
}

void AMissionDirector::OnCoreDestroyed()
{
	bCoreDestroyed = true;
	EvaluateProgress();
}

void AMissionDirector::ShowHint(const FText& Text)
{
	if (AKaelPlayerController* PC = GetPC())
	{
		PC->ShowToast(Text, 3.5f);
	}
}

void AMissionDirector::PlayBossIntro()
{
	bBossIntroPlayed = true;
	if (!Boss)
	{
		return;
	}
	const FVector BossLoc = Boss->GetActorLocation();
	if (AKaelPlayerController* PC = GetPC())
	{
		PC->PlayCinematic(BossLoc + FVector(-1200.f, -600.f, 120.f), BossLoc + FVector(-750.f, -300.f, 60.f), BossLoc + FVector(0.f, 0.f, 150.f), 4.f,
			FText::FromString(TEXT("O HOSPEDEIRO — o que sobrou da equipe do Protocolo Zero")));
	}
	if (City)
	{
		City->Flicker(4.f);
	}
	RupturaAssets::PlaySound2D(this, TEXT("hospedeiro_rugido"), 1.f, 0.8f);
	GetWorldTimerManager().SetTimer(BossTimer, this, &AMissionDirector::ActivateBossNow, 4.f, false);
}

void AMissionDirector::ActivateBossNow()
{
	if (Boss && !Boss->IsDead())
	{
		Boss->Activate();
	}
}

void AMissionDirector::StartEscape()
{
	float Duration = 160.f;
	if (const URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		Duration = GI->Difficulty == ERupturaDifficulty::Facil ? 200.f : (GI->Difficulty == ERupturaDifficulty::Dificil ? 135.f : 160.f);
	}
	EscapeEnd = GetWorld()->GetTimeSeconds() + Duration;

	if (City)
	{
		City->SetAlarm(true);
	}
	if (!AlarmLoop)
	{
		AlarmLoop = RupturaAssets::CreateLoop2D(this, TEXT("alarme_loop"), 0.35f);
	}
	else
	{
		AlarmLoop->AdjustVolume(1.f, 0.35f);
	}
	if (BunkerDoor)
	{
		BunkerDoor->Open(true);
	}
	SpawnEscapeReinforcements();
}

void AMissionDirector::SpawnEscapeReinforcements()
{
	// Tudo o que ainda estava vivo na cidade foi atraído pelo barulho.
	const TSubclassOf<AEnemyCharacter> Infected = AEnemyCharacter::StaticClass();
	SpawnEnemy(Infected, Sub(11000.f, 1000.f), 0.f, GroupReforco2, {}, false);
	SpawnEnemy(Infected, Sub(11000.f, -1000.f), 0.f, GroupReforco2, {}, true);
	SpawnEnemy(Infected, Sub(11600.f, 0.f), 0.f, GroupReforco2, {}, false, true);
	SpawnEnemy(Infected, Street(6500.f, 0.f), 0.f, GroupReforco2, { Street(6500.f, 0.f), Street(7500.f, 0.f) }, false);
	SpawnEnemy(Infected, Street(7000.f, 1500.f), 0.f, GroupReforco2, { Street(7000.f, 1500.f), Street(7600.f, 600.f) }, false);
	SpawnEnemy(Infected, Street(7000.f, -1500.f), 0.f, GroupReforco2, { Street(7000.f, -1500.f), Street(7600.f, -600.f) }, true);
	SpawnEnemy(Infected, Street(3500.f, 300.f), 0.f, GroupReforco2, {}, false, true);
	SpawnEnemy(Infected, Street(2000.f, -400.f), 0.f, GroupReforco2, { Street(2000.f, -400.f), Street(1200.f, 300.f) }, false);
}

// ---------------------------------------------------------------------------------------------
// Tick, música, sustos ambientes e consultas do HUD
// ---------------------------------------------------------------------------------------------

void AMissionDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bPendingKeycardGive)
	{
		if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Kael->GiveKeycard();
			bPendingKeycardGive = false;
		}
	}

	if (Stage == EMissionStage::Escapar && EscapeEnd > 0.f && GetWorld()->GetTimeSeconds() >= EscapeEnd)
	{
		SetStage(EMissionStage::Falhou);
	}

	UpdateMusic(DeltaSeconds);
	UpdateAmbientScares(DeltaSeconds);
	UpdateCollapse(DeltaSeconds);
}

void AMissionDirector::UpdateMusic(float DeltaSeconds)
{
	MusicCheckTimer -= DeltaSeconds;
	if (MusicCheckTimer > 0.f)
	{
		return;
	}
	MusicCheckTimer = 0.5f;

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	bool bChase = Stage == EMissionStage::Escapar;
	if (!bChase)
	{
		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsDead() || FVector::Dist(It->GetActorLocation(), Player->GetActorLocation()) > 3500.f)
			{
				continue;
			}
			if (It->IsBoss() ? It->IsActive() : It->GetAlert() == EEnemyAlert::Combate)
			{
				bChase = true;
				break;
			}
		}
	}

	if (bChase != bChaseMusic)
	{
		bChaseMusic = bChase;
		if (ChaseMusic)
		{
			ChaseMusic->AdjustVolume(bChase ? 0.6f : 4.f, bChase ? 0.65f : 0.01f);
		}
		if (Ambience)
		{
			Ambience->AdjustVolume(bChase ? 1.f : 4.f, bChase ? 0.2f : 0.5f);
		}
	}

	const bool bUnder = IsPlayerUnderground();
	if (bUnder != bUnderground)
	{
		bUnderground = bUnder;
		if (RainLoop)
		{
			RainLoop->AdjustVolume(2.f, bUnder ? 0.05f : 0.45f);
		}
	}
}

void AMissionDirector::UpdateAmbientScares(float DeltaSeconds)
{
	if (Stage >= EMissionStage::Concluida || bChaseMusic)
	{
		return;
	}
	AmbientTimer -= DeltaSeconds;
	if (AmbientTimer > 0.f)
	{
		return;
	}
	AmbientTimer = FMath::FRandRange(22.f, 50.f);

	const AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Kael || Kael->IsDead() || (GetPC() && GetPC()->IsInCinematic()))
	{
		return;
	}

	// Algo que você ouve... atrás de você.
	static const TCHAR* Sounds[] = { TEXT("sussurro"), TEXT("passos_atras"), TEXT("metal_rangido"), TEXT("grito_distante"), TEXT("batidas"), TEXT("choro") };
	const FName Sound(Sounds[FMath::RandRange(0, (int32)UE_ARRAY_COUNT(Sounds) - 1)]);
	FVector Forward = Kael->GetControlRotation().Vector().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = Kael->GetActorForwardVector();
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const bool bFar = Sound == FName(TEXT("grito_distante"));
	const FVector Where = Kael->GetActorLocation() - Forward * FMath::FRandRange(bFar ? 2500.f : 500.f, bFar ? 4000.f : 1300.f)
		+ Right * FMath::FRandRange(-600.f, 600.f) + FVector(0.f, 0.f, FMath::FRandRange(-50.f, 150.f));
	RupturaAssets::PlaySound3D(this, Sound, Where, bFar ? 1.f : 0.8f, FMath::FRandRange(0.9f, 1.1f));

	if (City && FMath::FRand() < 0.25f)
	{
		City->Flicker(FMath::FRandRange(0.4f, 1.2f));
	}
}

void AMissionDirector::UpdateCollapse(float DeltaSeconds)
{
	if (Stage != EMissionStage::Escapar || !IsPlayerUnderground())
	{
		return;
	}
	CollapseTimer -= DeltaSeconds;
	if (CollapseTimer > 0.f)
	{
		return;
	}
	CollapseTimer = FMath::FRandRange(2.f, 4.5f);

	if (AKaelCharacter* Kael = Cast<AKaelCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		const FVector Where = Kael->GetActorLocation() + FVector(FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-500.f, 500.f), 550.f);
		RupturaFx::CeilingDust(GetWorld(), Where);
		RupturaAssets::PlaySound3D(this, TEXT("desabamento"), Where, 1.f, FMath::FRandRange(0.8f, 1.1f));
		Kael->AddTrauma(0.3f);
		if (City)
		{
			City->Flicker(0.6f);
		}
	}
}

void AMissionDirector::ShowBanner(const FText& Title, const FText& SubText, float Duration)
{
	BannerTitle = Title;
	BannerSub = SubText;
	BannerStart = GetWorld()->GetTimeSeconds();
	BannerDuration = Duration;
}

bool AMissionDirector::GetBanner(FText& OutTitle, FText& OutSub, float& OutAlpha) const
{
	const float Age = GetWorld()->GetTimeSeconds() - BannerStart;
	if (Age < 0.f || Age > BannerDuration)
	{
		return false;
	}
	OutTitle = BannerTitle;
	OutSub = BannerSub;
	OutAlpha = FMath::Min(FMath::Clamp(Age / 0.6f, 0.f, 1.f), FMath::Clamp((BannerDuration - Age) / 0.8f, 0.f, 1.f));
	return true;
}

float AMissionDirector::GetEscapeTimeLeft() const
{
	if (Stage != EMissionStage::Escapar || EscapeEnd < 0.f)
	{
		return -1.f;
	}
	return FMath::Max(0.f, EscapeEnd - GetWorld()->GetTimeSeconds());
}

AEnemyCharacter* AMissionDirector::GetActiveBoss() const
{
	return (Boss && Boss->IsActive() && !Boss->IsDead()) ? Boss.Get() : nullptr;
}

float AMissionDirector::GetMissionTime() const
{
	const URupturaGameInstance* GI = URupturaGameInstance::Get(this);
	return (GI ? GI->AccumulatedTime : 0.f) + GetWorld()->GetTimeSeconds();
}

FString AMissionDirector::GetObjectiveDetail() const
{
	switch (Stage)
	{
	case EMissionStage::EnfrentarPatrulha:
		return FString::Printf(TEXT("Infectados no pátio: %d"), CountAlive(GroupPatrulha));
	case EMissionStage::RecuperarCartao:
		return TEXT("Posto da guarda, canto norte do pátio");
	case EMissionStage::EntrarSubsolo:
		return ((BunkerDoor && BunkerDoor->IsOpen()) || FiredEvents.Contains(ZoneTunel)) ? TEXT("Desça a rampa do bunker") : TEXT("Use o crachá na porta do bunker, no fundo do pátio");
	case EMissionStage::DesativarSeguranca:
		return TEXT("Sala de controle, ao norte da ala subterrânea");
	case EMissionStage::EnfrentarRobo:
		return TEXT("Faça ele investir contra um pilar e atire no coração, nas costas");
	case EMissionStage::DestruirNucleo:
		return (Core && Core->IsShielded()) ? TEXT("A membrana ainda protege o núcleo") : TEXT("Atire no núcleo até ele romper");
	case EMissionStage::Escapar:
		return TEXT("Suba a rampa e volte para a avenida — eles estão vindo");
	default:
		return FString();
	}
}

bool AMissionDirector::GetObjectiveLocation(FVector& OutLocation) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector P = Player ? Player->GetActorLocation() : FVector::ZeroVector;

	switch (Stage)
	{
	case EMissionStage::Explorar:
		OutLocation = FVector(2500.f, 0.f, 150.f);
		return true;
	case EMissionStage::EncontrarInstalacao:
		OutLocation = FVector(5400.f, 0.f, 200.f);
		return true;
	case EMissionStage::EnfrentarPatrulha:
	{
		float Best = TNumericLimits<float>::Max();
		bool bFound = false;
		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetGroup() == GroupPatrulha && !It->IsDead())
			{
				const float D = FVector::Dist(It->GetActorLocation(), P);
				if (D < Best)
				{
					Best = D;
					OutLocation = It->GetActorLocation() + FVector(0.f, 0.f, 120.f);
					bFound = true;
				}
			}
		}
		return bFound;
	}
	case EMissionStage::RecuperarCartao:
		OutLocation = RupturaLayout::KeycardLocation() + FVector(0.f, 0.f, 40.f);
		return true;
	case EMissionStage::EntrarSubsolo:
		OutLocation = ((BunkerDoor && BunkerDoor->IsOpen()) || FiredEvents.Contains(ZoneTunel)) ? FVector(10250.f, 0.f, RupturaLayout::SubFloorZ + 150.f) : FVector(RupturaLayout::BunkerDoorX - 50.f, 0.f, 180.f);
		return true;
	case EMissionStage::DesativarSeguranca:
		OutLocation = RupturaLayout::TerminalLocation() + FVector(0.f, -60.f, 150.f);
		return true;
	case EMissionStage::EnfrentarRobo:
		if (Boss && !Boss->IsDead())
		{
			OutLocation = Boss->GetActorLocation() + FVector(0.f, 0.f, 250.f);
		}
		else
		{
			OutLocation = FVector(RupturaLayout::Hall1EndX, 0.f, RupturaLayout::SubFloorZ + 200.f);
		}
		return true;
	case EMissionStage::DestruirNucleo:
		OutLocation = RupturaLayout::CoreLocation() + FVector(0.f, 0.f, 300.f);
		return true;
	case EMissionStage::Escapar:
		// Guia por etapas: saída do reator -> rampa -> porta do bunker -> avenida.
		if (P.X > RupturaLayout::RampEndX + 200.f)
		{
			OutLocation = FVector(RupturaLayout::RampEndX, 0.f, RupturaLayout::SubFloorZ + 150.f);
		}
		else if (P.X > RupturaLayout::BunkerDoorX)
		{
			OutLocation = FVector(RupturaLayout::BunkerDoorX - 100.f, 0.f, 150.f);
		}
		else
		{
			OutLocation = RupturaLayout::ExtractionLocation() + FVector(0.f, 0.f, 150.f);
		}
		return true;
	default:
		return false;
	}
}
