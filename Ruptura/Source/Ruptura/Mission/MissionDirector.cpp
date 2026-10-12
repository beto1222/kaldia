#include "Mission/MissionDirector.h"
#include "Mission/MissionActors.h"
#include "Mission/SecurityDoor.h"
#include "Mission/Pickup.h"
#include "AI/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "AI/CombatRobot.h"
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

	const FName GroupRua(TEXT("Rua"));
	const FName GroupPatrulha(TEXT("Patrulha"));
	const FName GroupSubsolo(TEXT("Subsolo"));
	const FName GroupReforco1(TEXT("Reforco1"));
	const FName GroupReforco2(TEXT("Reforco2"));
	const FName GroupChefe(TEXT("Chefe"));

	constexpr float StreetZ = 110.f;
	constexpr float SubZ = RupturaLayout::SubFloorZ + 100.f;

	FVector Street(float X, float Y) { return FVector(X, Y, StreetZ); }
	FVector Sub(float X, float Y) { return FVector(X, Y, SubZ); }
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

	// Áudio ambiente e trilha dinâmica (silêncio se os sons não tiverem sido importados).
	RainLoop = RupturaAssets::CreateLoop2D(this, TEXT("chuva_loop"), StartCheckpoint >= EMissionStage::DesativarSeguranca ? 0.05f : 0.5f);
	MusicExplore = RupturaAssets::CreateLoop2D(this, TEXT("musica_exploracao_loop"), 0.45f);
	MusicCombat = RupturaAssets::CreateLoop2D(this, TEXT("musica_combate_loop"), 0.f);

	Stage = StartCheckpoint;
	OnStageEntered(Stage);
	EvaluateProgress();
}

// ---------------------------------------------------------------------------------------------
// Criação dos elementos da missão
// ---------------------------------------------------------------------------------------------

AEnemyCharacter* AMissionDirector::SpawnEnemy(TSubclassOf<AEnemyCharacter> Class, const FVector& Location, float Yaw, FName Group, const TArray<FVector>& Patrol, bool bElite)
{
	UWorld* World = GetWorld();
	const FTransform T(FRotator(0.f, Yaw, 0.f), Location);
	AEnemyCharacter* Enemy = World->SpawnActorDeferred<AEnemyCharacter>(Class, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Enemy)
	{
		Enemy->Setup(Group, Patrol, bElite);
		UGameplayStatics::FinishSpawningActor(Enemy, T);
	}
	return Enemy;
}

APickup* AMissionDirector::SpawnPickup(EPickupType Type, const FVector& Location, int32 Amount, float RespawnTime)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APickup* P = GetWorld()->SpawnActor<APickup>(APickup::StaticClass(), Location, FRotator::ZeroRotator, Params);
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

	// Zonas de objetivo
	SpawnZone(ZoneRua, FVector(2500.f, 0.f, 150.f), FVector(120.f, 1300.f, 300.f));
	SpawnZone(ZonePortao, FVector(5250.f, 0.f, 150.f), FVector(150.f, 1300.f, 300.f));
	SpawnZone(ZoneSubsolo, FVector(10250.f, 0.f, RupturaLayout::SubFloorZ + 150.f), FVector(150.f, 400.f, 250.f));
	SpawnZone(ZoneReator, FVector(13450.f, 0.f, RupturaLayout::SubFloorZ + 200.f), FVector(250.f, 1900.f, 350.f));
	SpawnZone(ZoneExtracao, FVector(100.f, 0.f, 150.f), FVector(300.f, 1000.f, 300.f));

	// Portas
	BunkerDoor = World->SpawnActor<ASecurityDoor>(ASecurityDoor::StaticClass(), FVector(RupturaLayout::BunkerDoorX, 0.f, 0.f), FRotator::ZeroRotator, Params);
	if (BunkerDoor)
	{
		BunkerDoor->Configure(400.f, 350.f, true, false, FText::FromString(TEXT("PORTA DO BUNKER")));
		if (CP >= EMissionStage::DesativarSeguranca)
		{
			BunkerDoor->Open(true);
		}
	}
	BlastDoor = World->SpawnActor<ASecurityDoor>(ASecurityDoor::StaticClass(), FVector(RupturaLayout::Hall1EndX, 0.f, RupturaLayout::SubFloorZ), FRotator::ZeroRotator, Params);
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

	// Cartão de acesso
	if (CP < EMissionStage::DesativarSeguranca)
	{
		Keycard = World->SpawnActor<AKeycardPickup>(AKeycardPickup::StaticClass(), RupturaLayout::KeycardLocation(), FRotator::ZeroRotator, Params);
	}

	// Núcleo de energia
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

	// Inimigos
	if (CP < EMissionStage::DesativarSeguranca)
	{
		SpawnEnemy(AEnemyCharacter::StaticClass(), Street(3800.f, -900.f), 90.f, GroupRua,
			{ Street(3800.f, -900.f), Street(4500.f, -300.f), Street(3200.f, -300.f) }, false);

		SpawnEnemy(AEnemyCharacter::StaticClass(), Street(6000.f, 900.f), 0.f, GroupPatrulha,
			{ Street(6000.f, 900.f), Street(7200.f, 900.f), Street(7200.f, -900.f), Street(6000.f, -900.f) }, false);
		SpawnEnemy(AEnemyCharacter::StaticClass(), Street(7400.f, -1300.f), 180.f, GroupPatrulha,
			{ Street(7400.f, -1300.f), Street(6200.f, -2100.f), Street(5800.f, -1300.f) }, false);
		SpawnEnemy(AEnemyCharacter::StaticClass(), Street(6600.f, 0.f), 180.f, GroupPatrulha,
			{ Street(6600.f, 0.f), Street(6600.f, 1300.f) }, false);
		SpawnEnemy(AEnemyCharacter::StaticClass(), Street(7600.f, 500.f), 180.f, GroupPatrulha,
			{ Street(7600.f, 500.f), Street(7600.f, -500.f) }, true);
	}
	if (CP < EMissionStage::EnfrentarRobo)
	{
		SpawnEnemy(AEnemyCharacter::StaticClass(), Sub(11200.f, 900.f), 180.f, GroupSubsolo,
			{ Sub(11200.f, 900.f), Sub(12400.f, 900.f) }, false);
		SpawnEnemy(AEnemyCharacter::StaticClass(), Sub(12000.f, -900.f), 180.f, GroupSubsolo,
			{ Sub(12000.f, -900.f), Sub(10800.f, -900.f) }, true);
		SpawnEnemy(AEnemyCharacter::StaticClass(), Sub(12600.f, 300.f), 180.f, GroupSubsolo, {}, false);
	}
	if (CP < EMissionStage::DestruirNucleo)
	{
		Boss = Cast<ACombatRobot>(SpawnEnemy(ACombatRobot::StaticClass(), RupturaLayout::BossSpawn(), 180.f, GroupChefe, {}, false));
	}

	// Suprimentos
	SpawnPickup(EPickupType::Vida, FVector(3000.f, 1050.f, 15.f), 35, 0.f);
	SpawnPickup(EPickupType::Municao, FVector(4600.f, 250.f, 0.f), 60, 0.f);
	SpawnPickup(EPickupType::Granada, FVector(4650.f, -250.f, 0.f), 2, 0.f);
	SpawnPickup(EPickupType::Vida, FVector(7250.f, 1700.f, 0.f), 50, 0.f);
	SpawnPickup(EPickupType::Municao, FVector(6900.f, -1600.f, 0.f), 60, 0.f);
	SpawnPickup(EPickupType::Municao, FVector(10700.f, -1300.f, RupturaLayout::SubFloorZ), 60, 0.f);
	SpawnPickup(EPickupType::Vida, FVector(12100.f, 2300.f, RupturaLayout::SubFloorZ), 50, 0.f);
	SpawnPickup(EPickupType::Granada, FVector(12450.f, 2250.f, RupturaLayout::SubFloorZ), 2, 0.f);
	// Arena do chefe: itens que reaparecem
	SpawnPickup(EPickupType::Vida, FVector(13700.f, 1700.f, RupturaLayout::SubFloorZ), 40, 30.f);
	SpawnPickup(EPickupType::Municao, FVector(13700.f, -1700.f, RupturaLayout::SubFloorZ), 60, 20.f);
	SpawnPickup(EPickupType::Municao, FVector(16500.f, 1500.f, RupturaLayout::SubFloorZ), 60, 20.f);
	SpawnPickup(EPickupType::Granada, FVector(13600.f, 600.f, RupturaLayout::SubFloorZ), 1, 25.f);
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
		ShowBanner(FText::FromString(TEXT("NOVO OBJETIVO")), FRupturaTuning::StageText(NewStage), 3.5f);
		RupturaAssets::PlaySound2D(this, TEXT("objetivo"), 0.7f);
	}

	switch (NewStage)
	{
	case EMissionStage::Explorar:
		ShowBanner(FText::FromString(TEXT("MISSÃO 01 — A CIDADE MORTA")), FRupturaTuning::StageText(NewStage), 4.f);
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
		ShowBanner(FText::FromString(TEXT("EXTRAÇÃO CONFIRMADA")), FText::FromString(TEXT("Kael saiu da zona de colapso")), 4.f);
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
			RupturaAssets::PlaySound2D(this, TEXT("explosao"), 1.f, 0.6f);
			if (AKaelCharacter* Kael = Cast<AKaelCharacter>(P))
			{
				Kael->AddTrauma(1.f);
			}
		}
		if (AKaelPlayerController* PC = GetPC())
		{
			PC->OnMissionFailed(FText::FromString(TEXT("O núcleo colapsou antes da extração.")));
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
				PC->ShowToast(FText::FromString(TEXT("Checkpoint salvo")), 2.f);
			}
		}
	}
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
		ShowBanner(FText::FromString(TEXT("COLOSSO DESTRUÍDO")), FText::FromString(TEXT("O escudo do núcleo caiu")), 3.f);
	}
	else if (Enemy && Enemy->GetGroup() == GroupPatrulha && Stage == EMissionStage::EnfrentarPatrulha)
	{
		const int32 Left = CountAlive(GroupPatrulha);
		if (Left > 0)
		{
			ShowHint(FText::FromString(FString::Printf(TEXT("Patrulha: %d restante(s)"), Left)));
		}
	}
	EvaluateProgress();
}

void AMissionDirector::OnKeycardTaken()
{
	bKeycard = true;
	ShowHint(FText::FromString(TEXT("Cartão de acesso obtido — porta do bunker liberada")));
	EvaluateProgress();
}

void AMissionDirector::OnTerminalHacked()
{
	bTerminalHacked = true;
	if (BlastDoor)
	{
		BlastDoor->Open();
	}

	// Reforços saem do salão do reator na direção do jogador.
	if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		SpawnEnemy(AEnemyCharacter::StaticClass(), Sub(13700.f, 700.f), 180.f, GroupReforco1, {}, false);
		SpawnEnemy(AEnemyCharacter::StaticClass(), Sub(13700.f, -700.f), 180.f, GroupReforco1, {}, false);
		AlertGroupTo(GroupReforco1, P->GetActorLocation());
	}
	ShowBanner(FText::FromString(TEXT("SEGURANÇA DESATIVADA")), FText::FromString(TEXT("Porta do reator aberta — contatos se aproximando")), 3.f);
	EvaluateProgress();
}

void AMissionDirector::OnBossEnraged()
{
	ShowBanner(FText::FromString(TEXT("ALERTA")), FText::FromString(TEXT("COLOSSO em modo de combate máximo")), 2.5f);
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
		PC->ShowToast(Text, 3.f);
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
		PC->PlayCinematic(BossLoc + FVector(-1300.f, -700.f, 250.f), BossLoc + FVector(-900.f, -450.f, 120.f), BossLoc + FVector(0.f, 0.f, 120.f), 3.f,
			FText::FromString(TEXT("UNIDADE DE COMBATE T-9 \"COLOSSO\" — ATIVADA")));
	}
	RupturaAssets::PlaySound2D(this, TEXT("robo_ativar"), 1.f, 0.9f);
	GetWorldTimerManager().SetTimer(BossTimer, this, &AMissionDirector::ActivateBossNow, 3.f, false);
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
	float Duration = 150.f;
	if (const URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		Duration = GI->Difficulty == ERupturaDifficulty::Facil ? 180.f : (GI->Difficulty == ERupturaDifficulty::Dificil ? 130.f : 150.f);
	}
	EscapeEnd = GetWorld()->GetTimeSeconds() + Duration;

	if (City)
	{
		City->SetAlarm(true);
	}
	if (!AlarmLoop)
	{
		AlarmLoop = RupturaAssets::CreateLoop2D(this, TEXT("alarme_loop"), 0.3f);
	}
	SpawnEscapeReinforcements();
}

void AMissionDirector::SpawnEscapeReinforcements()
{
	SpawnEnemy(AEnemyCharacter::StaticClass(), Sub(11000.f, 1000.f), 0.f, GroupReforco2, {}, false);
	SpawnEnemy(AEnemyCharacter::StaticClass(), Sub(11000.f, -1000.f), 0.f, GroupReforco2, {}, true);
	SpawnEnemy(AEnemyCharacter::StaticClass(), Street(6500.f, 0.f), 0.f, GroupReforco2, { Street(6500.f, 0.f), Street(7500.f, 0.f) }, false);
	SpawnEnemy(AEnemyCharacter::StaticClass(), Street(7000.f, 1500.f), 0.f, GroupReforco2, { Street(7000.f, 1500.f), Street(7600.f, 600.f) }, false);
	SpawnEnemy(AEnemyCharacter::StaticClass(), Street(7000.f, -1500.f), 0.f, GroupReforco2, { Street(7000.f, -1500.f), Street(7600.f, -600.f) }, true);
}

// ---------------------------------------------------------------------------------------------
// Tick, música e consultas do HUD
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

	bool bCombat = Stage == EMissionStage::Escapar;
	if (!bCombat)
	{
		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsDead() || FVector::Dist(It->GetActorLocation(), Player->GetActorLocation()) > 5000.f)
			{
				continue;
			}
			if (It->IsBoss() ? It->IsActive() : It->GetAlert() == EEnemyAlert::Combate)
			{
				bCombat = true;
				break;
			}
		}
	}

	if (bCombat != bCombatMusic)
	{
		bCombatMusic = bCombat;
		if (MusicCombat)
		{
			MusicCombat->AdjustVolume(bCombat ? 1.f : 3.f, bCombat ? 0.7f : 0.f);
		}
		if (MusicExplore)
		{
			MusicExplore->AdjustVolume(bCombat ? 1.f : 3.f, bCombat ? 0.12f : 0.45f);
		}
	}

	const bool bUnder = Player->GetActorLocation().Z < -250.f || Player->GetActorLocation().X > RupturaLayout::BunkerDoorX + 200.f;
	if (bUnder != bUnderground)
	{
		bUnderground = bUnder;
		if (RainLoop)
		{
			RainLoop->AdjustVolume(2.f, bUnder ? 0.05f : 0.5f);
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
	OutAlpha = FMath::Min(FMath::Clamp(Age / 0.3f, 0.f, 1.f), FMath::Clamp((BannerDuration - Age) / 0.6f, 0.f, 1.f));
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
		return FString::Printf(TEXT("Inimigos restantes: %d"), CountAlive(GroupPatrulha));
	case EMissionStage::RecuperarCartao:
		return TEXT("Posto da guarda, lado norte da instalação");
	case EMissionStage::EntrarSubsolo:
		return (BunkerDoor && BunkerDoor->IsOpen()) ? TEXT("Desça a rampa do bunker") : TEXT("Use o cartão na porta do bunker (fundo da instalação)");
	case EMissionStage::DesativarSeguranca:
		return TEXT("Sala de controle ao norte da ala subterrânea");
	case EMissionStage::EnfrentarRobo:
		return TEXT("Salão do reator — atire no núcleo laranja das costas");
	case EMissionStage::DestruirNucleo:
		return (Core && Core->IsShielded()) ? TEXT("Escudo ativo") : TEXT("Escudo desligado — destrua o núcleo");
	case EMissionStage::Escapar:
		return TEXT("Volte pela rampa até a rua — reforços a caminho");
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
		OutLocation = (BunkerDoor && BunkerDoor->IsOpen()) ? FVector(10250.f, 0.f, RupturaLayout::SubFloorZ + 150.f) : FVector(RupturaLayout::BunkerDoorX - 50.f, 0.f, 180.f);
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
		// Guia por etapas: saída do reator -> túnel -> porta do bunker -> extração.
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
