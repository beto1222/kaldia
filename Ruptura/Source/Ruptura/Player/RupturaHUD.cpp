#include "Player/RupturaHUD.h"
#include "Player/KaelCharacter.h"
#include "Player/KaelPlayerController.h"
#include "Mission/MissionDirector.h"
#include "AI/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Core/RupturaGameInstance.h"
#include "Core/RupturaTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"

namespace
{
	const FLinearColor ColWhite(0.92f, 0.95f, 0.97f, 1.f);
	const FLinearColor ColDim(0.55f, 0.6f, 0.65f, 0.9f);
	const FLinearColor ColCyan(0.25f, 0.85f, 1.f, 1.f);
	const FLinearColor ColAmber(1.f, 0.7f, 0.2f, 1.f);
	const FLinearColor ColRed(1.f, 0.22f, 0.18f, 1.f);
	const FLinearColor ColGreen(0.35f, 1.f, 0.5f, 1.f);
	const FLinearColor ColPanel(0.f, 0.02f, 0.04f, 0.55f);

	FString FormatTime(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
	}

	UFont* SmallFont() { return GEngine ? GEngine->GetSmallFont() : nullptr; }
	UFont* MediumFont() { return GEngine ? GEngine->GetMediumFont() : nullptr; }
	UFont* LargeFont() { return GEngine ? GEngine->GetLargeFont() : nullptr; }
}

void ARupturaHUD::Text(const FString& Str, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale, bool bCenter, bool bShadow)
{
	if (Str.IsEmpty())
	{
		return;
	}
	float TW = 0.f, TH = 0.f;
	if (bCenter)
	{
		GetTextSize(Str, TW, TH, Font, Scale);
		X -= TW * 0.5f;
	}
	if (bShadow)
	{
		DrawText(Str, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + 2.f * U, Y + 2.f * U, Font, Scale);
	}
	DrawText(Str, Color, X, Y, Font, Scale);
}

void ARupturaHUD::Bar(float X, float Y, float BW, float BH, float Fraction, const FLinearColor& Fill, const FLinearColor& Back)
{
	DrawRect(Back, X, Y, BW, BH);
	DrawRect(Fill, X, Y, BW * FMath::Clamp(Fraction, 0.f, 1.f), BH);
}

bool ARupturaHUD::WorldToScreen(const FVector& WorldLoc, FVector2D& OutScreen, bool& bOutBehind) const
{
	if (!PlayerOwner || !PlayerOwner->PlayerCameraManager || !Canvas)
	{
		return false;
	}
	const FVector CamLoc = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FRotator CamRot = PlayerOwner->PlayerCameraManager->GetCameraRotation();
	const FVector Dir = WorldLoc - CamLoc;
	bOutBehind = FVector::DotProduct(Dir, CamRot.Vector()) <= 0.f;
	if (!bOutBehind)
	{
		const FVector P = Canvas->Project(WorldLoc, false);
		OutScreen = FVector2D(P.X, P.Y);
	}
	else
	{
		const float Side = FVector::DotProduct(Dir, FRotationMatrix(CamRot).GetUnitAxis(EAxis::Y));
		OutScreen = FVector2D(Side > 0.f ? W - 70.f * U : 70.f * U, H - 140.f * U);
	}
	return true;
}

void ARupturaHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	W = Canvas->ClipX;
	H = Canvas->ClipY;
	U = H / 1080.f;

	AKaelPlayerController* PC = Cast<AKaelPlayerController>(PlayerOwner);
	AKaelCharacter* Kael = PC ? Cast<AKaelCharacter>(PC->GetPawn()) : nullptr;
	AMissionDirector* Director = AMissionDirector::Get(this);

	if (PC && PC->IsIntro())
	{
		DrawIntro(PC);
		DrawToast(PC);
		return;
	}
	if (PC && PC->IsInCinematic())
	{
		DrawCinematic(PC);
		return;
	}

	if (Kael)
	{
		DrawDamageOverlay(Kael);
		DrawEnemyMarkers();
		if (Director)
		{
			DrawObjectiveMarker(Director);
		}
		if (!Kael->IsDead())
		{
			DrawCrosshair(Kael);
		}
		DrawVitals(Kael);
		DrawWeaponInfo(Kael);
		DrawInteraction(Kael);
	}
	if (Director)
	{
		DrawObjectives(Director);
		DrawBossBar(Director);
		DrawCountdown(Director);
		DrawBanner(Director);
	}
	if (PC)
	{
		DrawToast(PC);
		if (PC->IsHelpVisible())
		{
			DrawHelp();
		}
		DrawEndScreen(PC, Director);
	}
}

// ---------------------------------------------------------------------------------------------

void ARupturaHUD::DrawIntro(AKaelPlayerController* PC)
{
	const float T = PC->GetCinematicTime();
	const float BarH = 110.f * U;
	DrawRect(FLinearColor::Black, 0.f, 0.f, W, BarH);
	DrawRect(FLinearColor::Black, 0.f, H - BarH, W, BarH);

	const float TitleAlpha = FMath::Clamp((T - 1.f) / 1.5f, 0.f, 1.f) * FMath::Clamp((9.5f - T) / 1.f, 0.f, 1.f);
	Text(TEXT("RUPTURA: PROTOCOLO ZERO"), FLinearColor(0.95f, 0.97f, 1.f, TitleAlpha), W * 0.5f, H * 0.36f, LargeFont(), 2.6f * U, true);
	Text(TEXT("MISSÃO 01 — A CIDADE MORTA"), FLinearColor(ColCyan.R, ColCyan.G, ColCyan.B, TitleAlpha), W * 0.5f, H * 0.36f + 80.f * U, MediumFont(), 1.8f * U, true);

	const float CapAlpha = FMath::Clamp((T - 3.f) / 1.f, 0.f, 1.f);
	Text(PC->GetCinematicCaption().ToString(), FLinearColor(0.85f, 0.88f, 0.9f, CapAlpha), W * 0.5f, H - BarH + 30.f * U, MediumFont(), 1.3f * U, true);

	ERupturaDifficulty Diff = ERupturaDifficulty::Normal;
	if (const URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		Diff = GI->Difficulty;
	}
	const FString Options = FString::Printf(TEXT("ESPAÇO: pular abertura    |    F1/F2/F3: dificuldade (%s)    |    F5–F8: qualidade gráfica"),
		*FRupturaTuning::DifficultyName(Diff).ToString());
	Text(Options, ColDim, W * 0.5f, 40.f * U, SmallFont(), 1.3f * U, true);
}

void ARupturaHUD::DrawCinematic(AKaelPlayerController* PC)
{
	const float BarH = 90.f * U;
	DrawRect(FLinearColor::Black, 0.f, 0.f, W, BarH);
	DrawRect(FLinearColor::Black, 0.f, H - BarH, W, BarH);
	Text(PC->GetCinematicCaption().ToString(), ColAmber, W * 0.5f, H - BarH + 25.f * U, MediumFont(), 1.5f * U, true);
}

void ARupturaHUD::DrawDamageOverlay(AKaelCharacter* Kael)
{
	const float Flash = Kael->GetDamageFlash();
	const float HealthFrac = Kael->GetHealth() ? Kael->GetHealth()->GetFraction() : 1.f;
	const float Now = GetWorld()->GetTimeSeconds();

	float Edge = Flash * 0.45f;
	if (HealthFrac < 0.35f && !Kael->IsDead())
	{
		Edge = FMath::Max(Edge, (0.35f - HealthFrac) * 1.4f * (0.75f + 0.25f * FMath::Sin(Now * 5.f)));
	}
	if (Edge > 0.01f)
	{
		const FLinearColor C(0.6f, 0.f, 0.f, FMath::Clamp(Edge, 0.f, 0.7f));
		const float B = 90.f * U;
		DrawRect(C, 0.f, 0.f, W, B);
		DrawRect(C, 0.f, H - B, W, B);
		DrawRect(C, 0.f, B, B, H - 2.f * B);
		DrawRect(C, W - B, B, B, H - 2.f * B);
		DrawRect(FLinearColor(0.4f, 0.f, 0.f, Edge * 0.25f), 0.f, 0.f, W, H);
	}

	// Indicador de direção do dano
	const float Age = Now - Kael->GetLastDamageTime();
	if (Age < 1.6f && PlayerOwner && PlayerOwner->PlayerCameraManager)
	{
		const FVector ToSource = Kael->GetLastDamageSource() - Kael->GetActorLocation();
		const float CamYaw = PlayerOwner->PlayerCameraManager->GetCameraRotation().Yaw;
		const float Angle = FMath::DegreesToRadians(FRotator::NormalizeAxis(ToSource.Rotation().Yaw - CamYaw));
		const FVector2D Center(W * 0.5f, H * 0.5f);
		const FVector2D Dir(FMath::Sin(Angle), -FMath::Cos(Angle));
		const FVector2D Perp(-Dir.Y, Dir.X);
		const FVector2D Tip = Center + Dir * 190.f * U;
		const FVector2D BaseL = Center + Dir * 150.f * U + Perp * 26.f * U;
		const FVector2D BaseR = Center + Dir * 150.f * U - Perp * 26.f * U;
		const FLinearColor C(1.f, 0.15f, 0.1f, 1.f - Age / 1.6f);
		DrawLine(Tip.X, Tip.Y, BaseL.X, BaseL.Y, C, 4.f * U);
		DrawLine(Tip.X, Tip.Y, BaseR.X, BaseR.Y, C, 4.f * U);
		DrawLine(BaseL.X, BaseL.Y, BaseR.X, BaseR.Y, C, 4.f * U);
	}
}

void ARupturaHUD::DrawCrosshair(AKaelCharacter* Kael)
{
	const float Cx = W * 0.5f;
	const float Cy = H * 0.5f;
	const float Fov = (PlayerOwner && PlayerOwner->PlayerCameraManager) ? PlayerOwner->PlayerCameraManager->GetFOVAngle() : 90.f;
	const float PixelsPerDeg = W / FMath::Max(10.f, Fov);
	const float Gap = 6.f * U + Kael->GetCurrentSpreadDeg() * PixelsPerDeg * 0.5f;
	const float Len = 10.f * U;
	const FLinearColor C = Kael->IsAimingAtEnemy() ? ColRed : FLinearColor(1.f, 1.f, 1.f, 0.9f);
	const float Th = 2.f * U;

	DrawLine(Cx - Gap - Len, Cy, Cx - Gap, Cy, C, Th);
	DrawLine(Cx + Gap, Cy, Cx + Gap + Len, Cy, C, Th);
	DrawLine(Cx, Cy - Gap - Len, Cx, Cy - Gap, C, Th);
	DrawLine(Cx, Cy + Gap, Cx, Cy + Gap + Len, C, Th);
	DrawRect(C, Cx - 1.5f * U, Cy - 1.5f * U, 3.f * U, 3.f * U);

	// Marcador de acerto
	const float HitAge = GetWorld()->GetTimeSeconds() - Kael->GetHitMarkerTime();
	if (HitAge < 0.25f)
	{
		const FLinearColor HC = Kael->WasLastHitKill() ? ColRed : FLinearColor(1.f, 1.f, 1.f, 1.f - HitAge / 0.25f);
		const float A = 8.f * U, B = 16.f * U;
		DrawLine(Cx - B, Cy - B, Cx - A, Cy - A, HC, 2.5f * U);
		DrawLine(Cx + B, Cy - B, Cx + A, Cy - A, HC, 2.5f * U);
		DrawLine(Cx - B, Cy + B, Cx - A, Cy + A, HC, 2.5f * U);
		DrawLine(Cx + B, Cy + B, Cx + A, Cy + A, HC, 2.5f * U);
	}
}

void ARupturaHUD::DrawVitals(AKaelCharacter* Kael)
{
	const UHealthComponent* HealthComp = Kael->GetHealth();
	if (!HealthComp)
	{
		return;
	}
	const float X = 40.f * U;
	const float Y = H - 120.f * U;
	DrawRect(ColPanel, X - 12.f * U, Y - 12.f * U, 420.f * U, 96.f * U);

	const float Frac = HealthComp->GetFraction();
	const FLinearColor Fill = Frac > 0.5f ? ColGreen : (Frac > 0.25f ? ColAmber : ColRed);
	Text(TEXT("KAEL"), ColCyan, X, Y, MediumFont(), 1.1f * U);
	Text(FString::Printf(TEXT("%d"), FMath::CeilToInt(HealthComp->Health)), ColWhite, X + 330.f * U, Y, MediumFont(), 1.1f * U);
	Bar(X, Y + 32.f * U, 390.f * U, 14.f * U, Frac, Fill, FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));

	FString Status;
	if (Kael->HasKeycard())
	{
		Status += TEXT("CARTÃO DE ACESSO  ");
	}
	if (Kael->IsInCover())
	{
		Status += TEXT("EM COBERTURA");
	}
	Text(Status, ColGreen, X, Y + 54.f * U, SmallFont(), 1.2f * U);
}

void ARupturaHUD::DrawWeaponInfo(AKaelCharacter* Kael)
{
	const UWeaponComponent* Weapon = Kael->GetWeapon();
	if (!Weapon || Weapon->GetNumWeapons() == 0)
	{
		return;
	}
	const FWeaponDef& Def = Weapon->GetDef();
	const FWeaponAmmo& Ammo = Weapon->GetAmmo();

	const float PW = 420.f * U;
	const float X = W - PW - 28.f * U;
	const float Y = H - 132.f * U;
	DrawRect(ColPanel, X, Y, PW, 108.f * U);

	Text(Def.DisplayName.ToString(), ColCyan, X + 16.f * U, Y + 10.f * U, SmallFont(), 1.25f * U);
	const bool bLow = Ammo.Mag <= FMath::Max(1, Def.MagSize / 4);
	Text(FString::Printf(TEXT("%d"), Ammo.Mag), bLow ? ColRed : ColWhite, X + 16.f * U, Y + 34.f * U, LargeFont(), 1.6f * U);
	Text(FString::Printf(TEXT("/ %d"), Ammo.Reserve), ColDim, X + 120.f * U, Y + 50.f * U, MediumFont(), 1.2f * U);
	Text(FString::Printf(TEXT("GRANADAS  %d"), Kael->GetGrenades()), ColAmber, X + 250.f * U, Y + 52.f * U, SmallFont(), 1.25f * U);

	if (Weapon->IsReloading())
	{
		Text(TEXT("RECARREGANDO"), ColAmber, X + 16.f * U, Y + 86.f * U, SmallFont(), 1.1f * U);
		Bar(X + 160.f * U, Y + 92.f * U, PW - 180.f * U, 6.f * U, Weapon->GetReloadProgress(), ColAmber, FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
	}
	else if (Ammo.Mag == 0 && Ammo.Reserve == 0)
	{
		Text(TEXT("SEM MUNIÇÃO — troque de arma (1/2)"), ColRed, X + 16.f * U, Y + 86.f * U, SmallFont(), 1.1f * U);
	}
	else if (bLow)
	{
		Text(TEXT("R — recarregar"), ColRed, X + 16.f * U, Y + 86.f * U, SmallFont(), 1.1f * U);
	}
}

void ARupturaHUD::DrawObjectives(AMissionDirector* Director)
{
	const float X = 40.f * U;
	float Y = 40.f * U;
	const EMissionStage Stage = Director->GetStage();

	DrawRect(ColPanel, X - 14.f * U, Y - 12.f * U, 560.f * U, 74.f * U + 30.f * U * FMath::Min((int32)Stage, 3));
	Text(TEXT("MISSÃO 01 — A CIDADE MORTA"), ColCyan, X, Y, SmallFont(), 1.3f * U);
	Y += 30.f * U;

	// Mostra até 3 objetivos concluídos mais recentes e o atual.
	const int32 Current = FMath::Min((int32)Stage, (int32)EMissionStage::Escapar);
	for (int32 i = FMath::Max(0, Current - 3); i < Current; ++i)
	{
		Text(FString::Printf(TEXT("[X] %s"), *FRupturaTuning::StageText((EMissionStage)i).ToString()), ColDim, X, Y, SmallFont(), 1.15f * U);
		Y += 30.f * U;
	}
	if (Stage <= EMissionStage::Escapar)
	{
		const float Pulse = 0.8f + 0.2f * FMath::Sin(GetWorld()->GetTimeSeconds() * 4.f);
		Text(FString::Printf(TEXT(">  %s"), *FRupturaTuning::StageText(Stage).ToString()), FLinearColor(1.f, 1.f, 1.f, Pulse), X, Y, SmallFont(), 1.25f * U);
		FString Detail = Director->GetObjectiveDetail();
		if (!Detail.IsEmpty())
		{
			Text(Detail, ColAmber, X + 26.f * U, Y + 26.f * U, SmallFont(), 1.05f * U);
		}
	}
}

void ARupturaHUD::DrawObjectiveMarker(AMissionDirector* Director)
{
	FVector Target;
	if (!Director->GetObjectiveLocation(Target) || !PlayerOwner || !PlayerOwner->GetPawn())
	{
		return;
	}
	FVector2D S;
	bool bBehind = false;
	if (!WorldToScreen(Target, S, bBehind))
	{
		return;
	}
	S.X = FMath::Clamp(S.X, 60.f * U, W - 60.f * U);
	S.Y = FMath::Clamp(S.Y, 160.f * U, H - 150.f * U);

	const float R = 12.f * U;
	const FLinearColor C = ColAmber;
	DrawLine(S.X, S.Y - R, S.X + R, S.Y, C, 3.f * U);
	DrawLine(S.X + R, S.Y, S.X, S.Y + R, C, 3.f * U);
	DrawLine(S.X, S.Y + R, S.X - R, S.Y, C, 3.f * U);
	DrawLine(S.X - R, S.Y, S.X, S.Y - R, C, 3.f * U);
	const float Meters = FVector::Dist(Target, PlayerOwner->GetPawn()->GetActorLocation()) / 100.f;
	Text(FString::Printf(TEXT("%.0f m"), Meters), C, S.X, S.Y + 16.f * U, SmallFont(), 1.1f * U, true);
}

void ARupturaHUD::DrawEnemyMarkers()
{
	const APawn* Player = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
	if (!Player)
	{
		return;
	}
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (Enemy->IsDead() || Enemy->IsBoss())
		{
			continue;
		}
		const EEnemyAlert Alert = Enemy->GetAlert();
		if (Alert == EEnemyAlert::Calmo || FVector::Dist(Enemy->GetActorLocation(), Player->GetActorLocation()) > 4500.f)
		{
			continue;
		}
		FVector2D S;
		bool bBehind = false;
		if (!WorldToScreen(Enemy->GetActorLocation() + FVector(0.f, 0.f, 130.f), S, bBehind) || bBehind)
		{
			continue;
		}
		const bool bCombat = Alert == EEnemyAlert::Combate;
		const FLinearColor C = bCombat ? ColRed : ColAmber;
		Text(bCombat ? TEXT("!") : TEXT("?"), C, S.X, S.Y, LargeFont(), (bCombat ? 1.4f : 1.1f) * U, true);
		if (!bCombat)
		{
			Bar(S.X - 20.f * U, S.Y + 40.f * U, 40.f * U, 4.f * U, Enemy->GetDetection(), ColAmber, FLinearColor(0.f, 0.f, 0.f, 0.6f));
		}
	}
}

void ARupturaHUD::DrawInteraction(AKaelCharacter* Kael)
{
	FText Prompt;
	float Progress = 0.f;
	bool bCan = false;
	if (!Kael->GetInteractionInfo(Prompt, Progress, bCan))
	{
		return;
	}
	const float Y = H * 0.66f;
	const FString Str = bCan ? FString::Printf(TEXT("[E]  %s"), *Prompt.ToString()) : Prompt.ToString();
	float TW = 0.f, TH = 0.f;
	GetTextSize(Str, TW, TH, MediumFont(), 1.2f * U);
	DrawRect(ColPanel, W * 0.5f - TW * 0.5f - 18.f * U, Y - 10.f * U, TW + 36.f * U, TH + 20.f * U + (Progress > 0.f ? 14.f * U : 0.f));
	Text(Str, bCan ? ColWhite : ColRed, W * 0.5f, Y, MediumFont(), 1.2f * U, true);
	if (Progress > 0.f)
	{
		Bar(W * 0.5f - TW * 0.5f, Y + TH + 6.f * U, TW, 6.f * U, Progress, ColCyan, FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
	}
}

void ARupturaHUD::DrawBossBar(AMissionDirector* Director)
{
	const AEnemyCharacter* Boss = Director->GetActiveBoss();
	if (!Boss || !Boss->GetHealth())
	{
		return;
	}
	const float BW = 760.f * U;
	const float X = (W - BW) * 0.5f;
	const float Y = 48.f * U;
	Text(TEXT("UNIDADE DE COMBATE T-9 \"COLOSSO\""), ColRed, W * 0.5f, Y - 32.f * U, SmallFont(), 1.3f * U, true);
	Bar(X, Y, BW, 14.f * U, Boss->GetHealth()->GetFraction(), ColRed, FLinearColor(0.08f, 0.f, 0.f, 0.8f));
	Text(TEXT("Ponto fraco: núcleo laranja nas costas"), ColDim, W * 0.5f, Y + 20.f * U, SmallFont(), 1.0f * U, true);
}

void ARupturaHUD::DrawCountdown(AMissionDirector* Director)
{
	const float Left = Director->GetEscapeTimeLeft();
	if (Left < 0.f)
	{
		return;
	}
	const float Pulse = Left < 30.f ? (0.6f + 0.4f * FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f))) : 1.f;
	Text(TEXT("NÚCLEO EM COLAPSO — EVACUE"), FLinearColor(1.f, 0.3f, 0.2f, Pulse), W * 0.5f, 110.f * U, MediumFont(), 1.3f * U, true);
	Text(FormatTime(Left), FLinearColor(1.f, 0.3f, 0.2f, Pulse), W * 0.5f, 145.f * U, LargeFont(), 1.8f * U, true);
}

void ARupturaHUD::DrawBanner(AMissionDirector* Director)
{
	FText Title, Sub;
	float Alpha = 0.f;
	if (!Director->GetBanner(Title, Sub, Alpha))
	{
		return;
	}
	const float Y = H * 0.22f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), 0.f, Y - 14.f * U, W, 100.f * U);
	Text(Title.ToString(), FLinearColor(ColCyan.R, ColCyan.G, ColCyan.B, Alpha), W * 0.5f, Y, SmallFont(), 1.4f * U, true);
	Text(Sub.ToString(), FLinearColor(1.f, 1.f, 1.f, Alpha), W * 0.5f, Y + 32.f * U, MediumFont(), 1.4f * U, true);
}

void ARupturaHUD::DrawToast(AKaelPlayerController* PC)
{
	FText Msg;
	float Alpha = 0.f;
	if (PC->GetToast(Msg, Alpha))
	{
		Text(Msg.ToString(), FLinearColor(1.f, 0.9f, 0.6f, Alpha), W * 0.5f, H * 0.78f, MediumFont(), 1.2f * U, true);
	}
}

void ARupturaHUD::DrawHelp()
{
	static const TCHAR* Lines[] = {
		TEXT("CONTROLES  (H oculta/mostra)"),
		TEXT("WASD mover   |   Mouse olhar   |   Shift correr"),
		TEXT("Espaço pular   |   C agachar   |   Alt/V esquivar"),
		TEXT("Q cobertura (encostado numa parede)"),
		TEXT("Botão direito mirar   |   Botão esquerdo atirar"),
		TEXT("R recarregar   |   1/2 ou roda trocar arma   |   G granada"),
		TEXT("E interagir (segure em terminais)"),
		TEXT("F1–F3 dificuldade   |   F5–F8 qualidade gráfica"),
	};
	const float X = W - 600.f * U;
	float Y = 190.f * U;
	DrawRect(ColPanel, X - 16.f * U, Y - 12.f * U, 580.f * U, 30.f * U * (float)UE_ARRAY_COUNT(Lines) + 20.f * U);
	for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Lines); ++i)
	{
		Text(Lines[i], i == 0 ? ColCyan : ColWhite, X, Y, SmallFont(), 1.1f * U);
		Y += 30.f * U;
	}
}

void ARupturaHUD::DrawEndScreen(AKaelPlayerController* PC, AMissionDirector* Director)
{
	const AKaelPlayerController::EEndState State = PC->GetEndState();
	if (State == AKaelPlayerController::EEndState::None)
	{
		return;
	}
	const float Age = PC->GetEndStateAge();
	const float Fade = FMath::Clamp(Age / 1.2f, 0.f, 1.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f * Fade), 0.f, 0.f, W, H);

	if (State == AKaelPlayerController::EEndState::Dead)
	{
		Text(TEXT("KAEL ABATIDO"), FLinearColor(ColRed.R, ColRed.G, ColRed.B, Fade), W * 0.5f, H * 0.4f, LargeFont(), 2.4f * U, true);
		Text(TEXT("Retornando ao último checkpoint...   (ENTER para agora)"), FLinearColor(1.f, 1.f, 1.f, Fade), W * 0.5f, H * 0.4f + 90.f * U, MediumFont(), 1.2f * U, true);
		return;
	}
	if (State == AKaelPlayerController::EEndState::Failed)
	{
		Text(TEXT("MISSÃO FRACASSADA"), FLinearColor(ColRed.R, ColRed.G, ColRed.B, Fade), W * 0.5f, H * 0.4f, LargeFont(), 2.4f * U, true);
		Text(PC->GetFailReason().ToString(), FLinearColor(1.f, 1.f, 1.f, Fade), W * 0.5f, H * 0.4f + 90.f * U, MediumFont(), 1.2f * U, true);
		Text(TEXT("ENTER — tentar novamente do checkpoint"), FLinearColor(ColDim.R, ColDim.G, ColDim.B, Fade), W * 0.5f, H * 0.4f + 130.f * U, SmallFont(), 1.2f * U, true);
		return;
	}

	// Concluída
	Text(TEXT("MISSÃO CONCLUÍDA"), FLinearColor(ColCyan.R, ColCyan.G, ColCyan.B, Fade), W * 0.5f, H * 0.28f, LargeFont(), 2.6f * U, true);
	Text(TEXT("A CIDADE MORTA — núcleo destruído, Kael extraído."), FLinearColor(1.f, 1.f, 1.f, Fade), W * 0.5f, H * 0.28f + 90.f * U, MediumFont(), 1.2f * U, true);

	const URupturaGameInstance* GI = URupturaGameInstance::Get(this);
	if (GI)
	{
		const float Accuracy = GI->ShotsFired > 0 ? 100.f * GI->ShotsHit / GI->ShotsFired : 0.f;
		const FString Stats[] = {
			FString::Printf(TEXT("Tempo de missão:   %s"), *FormatTime(PC->GetFinalTime())),
			FString::Printf(TEXT("Inimigos abatidos:   %d"), GI->Kills),
			FString::Printf(TEXT("Precisão:   %.0f%%"), Accuracy),
			FString::Printf(TEXT("Mortes:   %d"), GI->Deaths),
			FString::Printf(TEXT("Dificuldade:   %s"), *FRupturaTuning::DifficultyName(GI->Difficulty).ToString()),
		};
		float Y = H * 0.28f + 160.f * U;
		for (const FString& Line : Stats)
		{
			Text(Line, FLinearColor(0.9f, 0.92f, 0.95f, Fade), W * 0.5f, Y, MediumFont(), 1.15f * U, true);
			Y += 40.f * U;
		}
	}
	if (Age > 1.5f)
	{
		Text(TEXT("ENTER — jogar novamente"), ColAmber, W * 0.5f, H * 0.82f, MediumFont(), 1.2f * U, true);
	}
}
