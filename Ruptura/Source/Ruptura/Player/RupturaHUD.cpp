#include "Player/RupturaHUD.h"
#include "Player/KaelCharacter.h"
#include "Player/KaelPlayerController.h"
#include "Mission/MissionDirector.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Core/RupturaGameInstance.h"
#include "Core/RupturaTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	const FLinearColor ColText(0.86f, 0.84f, 0.8f, 1.f);
	const FLinearColor ColDim(0.5f, 0.5f, 0.5f, 0.85f);
	const FLinearColor ColBlood(0.62f, 0.04f, 0.03f, 1.f);
	const FLinearColor ColAmber(0.9f, 0.65f, 0.3f, 1.f);
	const FLinearColor ColPale(0.72f, 0.78f, 0.75f, 1.f);
	const FLinearColor ColCalm(0.55f, 0.72f, 0.5f, 1.f);
	const FLinearColor ColPanel(0.f, 0.f, 0.f, 0.6f);

	FString FormatTime(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
	}

	FLinearColor WithAlpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, C.A * FMath::Clamp(A, 0.f, 1.f));
	}

	UFont* SmallFont() { return GEngine ? GEngine->GetSmallFont() : nullptr; }
	UFont* MediumFont() { return GEngine ? GEngine->GetMediumFont() : nullptr; }
	UFont* LargeFont() { return GEngine ? GEngine->GetLargeFont() : nullptr; }
}

void ARupturaHUD::Text(const FString& Str, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale, bool bCenter, bool bShadow)
{
	if (Str.IsEmpty() || Color.A <= 0.f)
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
		DrawText(Str, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.85f), X + 2.f * U, Y + 2.f * U, Font, Scale);
	}
	DrawText(Str, Color, X, Y, Font, Scale);
}

void ARupturaHUD::Typewriter(const FString& Str, float Age, float CharsPerSecond, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
{
	if (Age <= 0.f)
	{
		return;
	}
	const int32 Count = FMath::Min(Str.Len(), FMath::FloorToInt(Age * CharsPerSecond));
	// Centraliza pelo texto completo para as letras não "andarem" enquanto aparecem.
	float TW = 0.f, TH = 0.f;
	GetTextSize(Str, TW, TH, Font, Scale);
	Text(Str.Left(Count), Color, X - TW * 0.5f, Y, Font, Scale, false);
}

void ARupturaHUD::Bar(float X, float Y, float BW, float BH, float Fraction, const FLinearColor& Fill, const FLinearColor& Back)
{
	DrawRect(Back, X, Y, BW, BH);
	DrawRect(Fill, X, Y, BW * FMath::Clamp(Fraction, 0.f, 1.f), BH);
}

void ARupturaHUD::EdgeVignette(const FLinearColor& Color, float Strength)
{
	if (Strength <= 0.01f)
	{
		return;
	}
	constexpr int32 Layers = 7;
	for (int32 i = 0; i < Layers; ++i)
	{
		const float B = (Layers - i) * 26.f * U;
		const FLinearColor C = WithAlpha(Color, Strength * 0.22f);
		DrawRect(C, 0.f, 0.f, W, B);
		DrawRect(C, 0.f, H - B, W, B);
		DrawRect(C, 0.f, B, B, H - 2.f * B);
		DrawRect(C, W - B, B, B, H - 2.f * B);
	}
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
		OutScreen = FVector2D(Side > 0.f ? W - 70.f * U : 70.f * U, H * 0.5f);
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

	const bool bStatus = PC && PC->IsStatusHeld() && Kael && !Kael->IsDead();

	if (Kael)
	{
		DrawDamageOverlay(Kael);
		if (!Kael->IsDead())
		{
			DrawCrosshair(Kael);
			DrawContextInfo(Kael);
			DrawInteraction(Kael);
		}
	}
	if (Director)
	{
		FText BannerTitle, BannerSub;
		float BannerAlpha = 0.f;
		if (bStatus || Director->GetBanner(BannerTitle, BannerSub, BannerAlpha))
		{
			DrawObjectiveMarker(Director);
		}
		if (bStatus)
		{
			DrawObjectives(Director);
		}
		DrawCountdown(Director);
		DrawBanner(Director);
	}
	if (bStatus)
	{
		DrawStatus(Kael);
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
// Cenas
// ---------------------------------------------------------------------------------------------

void ARupturaHUD::DrawIntro(AKaelPlayerController* PC)
{
	const float T = PC->GetCinematicTime();
	const float BarH = 120.f * U;

	// Começa no preto absoluto.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f - FMath::Clamp(T / 3.f, 0.f, 1.f)), 0.f, 0.f, W, H);
	DrawRect(FLinearColor::Black, 0.f, 0.f, W, BarH);
	DrawRect(FLinearColor::Black, 0.f, H - BarH, W, BarH);

	// Título com falhas, como uma lâmpada morrendo.
	const float Flicker = FMath::PerlinNoise1D(T * 7.f) > -0.35f ? 1.f : 0.25f;
	const float TitleAlpha = FMath::Clamp((T - 2.f) / 2.f, 0.f, 1.f) * FMath::Clamp((8.f - T) / 1.2f, 0.f, 1.f) * Flicker;
	Text(TEXT("R U P T U R A"), WithAlpha(ColText, TitleAlpha), W * 0.5f, H * 0.36f, LargeFont(), 2.8f * U, true);
	Text(TEXT("PROTOCOLO ZERO"), WithAlpha(ColDim, TitleAlpha), W * 0.5f, H * 0.36f + 78.f * U, MediumFont(), 1.4f * U, true);
	Text(TEXT("A CIDADE MORTA"), WithAlpha(ColBlood, TitleAlpha), W * 0.5f, H * 0.36f + 118.f * U, MediumFont(), 1.6f * U, true);

	// Narração em máquina de escrever.
	const float LineY = H - BarH + 34.f * U;
	const FLinearColor LineColor(0.8f, 0.78f, 0.74f, 1.f);
	if (T >= 8.f && T < 11.8f)
	{
		Typewriter(PC->GetCinematicCaption().ToString(), T - 8.f, 32.f, LineColor, W * 0.5f, LineY, MediumFont(), 1.25f * U);
	}
	else if (T >= 11.8f && T < 15.4f)
	{
		Typewriter(TEXT("O último sinal de socorro veio da instalação militar. Alguém ainda está transmitindo."), T - 11.8f, 38.f, LineColor, W * 0.5f, LineY, MediumFont(), 1.25f * U);
	}
	else if (T >= 15.4f)
	{
		Typewriter(TEXT("Não confie no que você ouvir no escuro."), T - 15.4f, 30.f, WithAlpha(ColBlood, 1.f), W * 0.5f, LineY, MediumFont(), 1.25f * U);
	}

	ERupturaDifficulty Diff = ERupturaDifficulty::Normal;
	if (const URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		Diff = GI->Difficulty;
	}
	const FString Options = FString::Printf(TEXT("ESPAÇO: pular abertura   |   F1/F2/F3: dificuldade (%s)   |   F5–F8: qualidade   |   use fones de ouvido"),
		*FRupturaTuning::DifficultyName(Diff).ToString());
	Text(Options, ColDim, W * 0.5f, 44.f * U, SmallFont(), 1.2f * U, true);
}

void ARupturaHUD::DrawCinematic(AKaelPlayerController* PC)
{
	const float BarH = 100.f * U;
	DrawRect(FLinearColor::Black, 0.f, 0.f, W, BarH);
	DrawRect(FLinearColor::Black, 0.f, H - BarH, W, BarH);
	Typewriter(PC->GetCinematicCaption().ToString(), PC->GetCinematicTime(), 30.f, ColBlood, W * 0.5f, H - BarH + 30.f * U, MediumFont(), 1.4f * U);
}

// ---------------------------------------------------------------------------------------------
// Jogo
// ---------------------------------------------------------------------------------------------

void ARupturaHUD::DrawDamageOverlay(AKaelCharacter* Kael)
{
	const float Flash = Kael->GetDamageFlash();
	const float HealthFrac = Kael->GetHealth() ? Kael->GetHealth()->GetFraction() : 1.f;
	const float Now = GetWorld()->GetTimeSeconds();

	float Edge = Flash * 0.8f;
	if (HealthFrac < 0.45f && !Kael->IsDead())
	{
		// Bordas de sangue que pulsam com o coração.
		const float Beat = FMath::Pow(FMath::Max(0.f, FMath::Sin(Now * 5.f)), 6.f);
		Edge = FMath::Max(Edge, (0.45f - HealthFrac) * 1.8f * (0.75f + 0.25f * Beat));
	}
	EdgeVignette(FLinearColor(0.28f, 0.f, 0.f, 1.f), FMath::Clamp(Edge, 0.f, 1.f));
	if (Flash > 0.05f)
	{
		DrawRect(FLinearColor(0.3f, 0.f, 0.f, Flash * 0.18f), 0.f, 0.f, W, H);
	}
	// Medo extremo: a visão periférica escurece.
	EdgeVignette(FLinearColor::Black, FMath::Clamp((Kael->GetFear() - 0.5f) * 1.2f, 0.f, 0.6f));
}

void ARupturaHUD::DrawCrosshair(AKaelCharacter* Kael)
{
	if (!Kael->IsAiming())
	{
		return; // sem mira na tela fora da pontaria
	}
	const float Cx = W * 0.5f;
	const float Cy = H * 0.5f;
	const float Fov = (PlayerOwner && PlayerOwner->PlayerCameraManager) ? PlayerOwner->PlayerCameraManager->GetFOVAngle() : 70.f;
	const float PixelsPerDeg = W / FMath::Max(10.f, Fov);
	const float Gap = 5.f * U + Kael->GetCurrentSpreadDeg() * PixelsPerDeg * 0.5f;
	const float Len = 7.f * U;
	const FLinearColor C(0.85f, 0.85f, 0.82f, 0.55f);
	const float Th = 1.5f * U;

	DrawLine(Cx - Gap - Len, Cy, Cx - Gap, Cy, C, Th);
	DrawLine(Cx + Gap, Cy, Cx + Gap + Len, Cy, C, Th);
	DrawLine(Cx, Cy + Gap, Cx, Cy + Gap + Len, C, Th);
	DrawRect(FLinearColor(0.9f, 0.9f, 0.88f, 0.75f), Cx - 1.5f * U, Cy - 1.5f * U, 3.f * U, 3.f * U);
}

void ARupturaHUD::DrawContextInfo(AKaelCharacter* Kael)
{
	const float Now = GetWorld()->GetTimeSeconds();

	// Munição: só ao mirar, recarregar ou com o pente vazio.
	if (const UWeaponComponent* Weapon = Kael->GetWeapon())
	{
		if (Weapon->GetNumWeapons() > 0)
		{
			const FWeaponAmmo& Ammo = Weapon->GetAmmo();
			if (Kael->IsAiming() || Weapon->IsReloading() || Ammo.Mag == 0)
			{
				const FWeaponDef& Def = Weapon->GetDef();
				const float X = W - 340.f * U;
				const float Y = H - 140.f * U;
				Text(Def.DisplayName.ToString(), ColDim, X, Y, SmallFont(), 1.1f * U);
				const bool bLow = Ammo.Mag <= FMath::Max(1, Def.MagSize / 4);
				Text(FString::Printf(TEXT("%d"), Ammo.Mag), bLow ? ColBlood : ColText, X, Y + 24.f * U, LargeFont(), 1.3f * U);
				Text(FString::Printf(TEXT("/ %d"), Ammo.Reserve), ColDim, X + 70.f * U, Y + 40.f * U, MediumFont(), 1.1f * U);
				if (Weapon->IsReloading())
				{
					Bar(X, Y + 84.f * U, 220.f * U, 3.f * U, Weapon->GetReloadProgress(), ColText, FLinearColor(0.1f, 0.1f, 0.1f, 0.6f));
				}
				else if (Ammo.Mag == 0 && Ammo.Reserve == 0)
				{
					Text(TEXT("sem munição — Q faca  |  1/2 trocar"), ColBlood, X, Y + 80.f * U, SmallFont(), 1.05f * U);
				}
			}
		}
	}

	// Lanterna: aparece ao ligar/desligar e pisca quando a bateria está no fim.
	const float Battery = Kael->GetBattery();
	const bool bRecentToggle = Now - Kael->GetLastFlashlightToggle() < 2.5f;
	const bool bLowBattery = Battery < 0.2f;
	if (bRecentToggle || bLowBattery)
	{
		float Alpha = bRecentToggle ? FMath::Clamp((2.5f - (Now - Kael->GetLastFlashlightToggle())) / 0.6f, 0.f, 1.f) : 0.f;
		if (bLowBattery)
		{
			Alpha = FMath::Max(Alpha, 0.45f + 0.45f * FMath::Sin(Now * 4.f));
		}
		const float X = 44.f * U;
		const float Y = H - 96.f * U;
		const FLinearColor C = bLowBattery ? ColBlood : ColPale;
		Text(Kael->IsFlashlightOn() ? TEXT("LANTERNA") : TEXT("LANTERNA (desligada)"), WithAlpha(ColDim, Alpha), X, Y, SmallFont(), 1.05f * U);
		for (int32 i = 0; i < 5; ++i)
		{
			const bool bFilled = Battery > (i + 0.5f) / 5.f;
			DrawRect(WithAlpha(bFilled ? C : FLinearColor(0.15f, 0.15f, 0.15f, 1.f), Alpha * 0.85f), X + i * 30.f * U, Y + 26.f * U, 26.f * U, 8.f * U);
		}
	}

	// Fôlego: barra fina no centro inferior enquanto se recupera.
	const float Stamina = Kael->GetStamina();
	if (Stamina < 0.99f)
	{
		const float BW = 240.f * U;
		const float Alpha = FMath::Clamp((1.f - Stamina) * 3.f, 0.f, 0.8f);
		const FLinearColor Fill = Kael->IsExhausted() ? WithAlpha(ColBlood, Alpha) : WithAlpha(ColPale, Alpha);
		Bar((W - BW) * 0.5f, H - 60.f * U, BW, 3.f * U, Stamina, Fill, FLinearColor(0.f, 0.f, 0.f, Alpha * 0.5f));
	}
}

void ARupturaHUD::DrawStatus(AKaelCharacter* Kael)
{
	if (!Kael)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float X = 44.f * U;
	float Y = H - 330.f * U;
	DrawRect(ColPanel, X - 16.f * U, Y - 14.f * U, 520.f * U, 300.f * U);

	const float Frac = Kael->GetHealth() ? Kael->GetHealth()->GetFraction() : 1.f;
	FString Cond = TEXT("ESTÁVEL");
	FLinearColor CondColor = ColCalm;
	if (Frac <= 0.33f)
	{
		Cond = TEXT("CRÍTICO");
		CondColor = WithAlpha(ColBlood, 0.6f + 0.4f * FMath::Abs(FMath::Sin(Now * 4.f)));
	}
	else if (Frac <= 0.66f)
	{
		Cond = TEXT("FERIDO");
		CondColor = ColAmber;
	}
	Text(TEXT("CONDIÇÃO"), ColDim, X, Y, SmallFont(), 1.1f * U);
	Text(Cond, CondColor, X + 140.f * U, Y - 6.f * U, MediumFont(), 1.3f * U);
	Y += 46.f * U;

	if (const UWeaponComponent* Weapon = Kael->GetWeapon())
	{
		for (int32 i = 0; i < Weapon->GetNumWeapons(); ++i)
		{
			const FWeaponDef& Def = Weapon->GetDefAt(i);
			const FWeaponAmmo& Ammo = Weapon->GetAmmoAt(i);
			const bool bCurrent = i == Weapon->GetCurrentIndex();
			Text(FString::Printf(TEXT("%s%d  %s"), bCurrent ? TEXT("> ") : TEXT("  "), i + 1, *Def.DisplayName.ToString()), bCurrent ? ColText : ColDim, X, Y, SmallFont(), 1.15f * U);
			Text(FString::Printf(TEXT("%d | %d"), Ammo.Mag, Ammo.Reserve), bCurrent ? ColText : ColDim, X + 380.f * U, Y, SmallFont(), 1.15f * U);
			Y += 30.f * U;
		}
	}
	Text(FString::Printf(TEXT("   GRANADAS       %d"), Kael->GetGrenades()), ColDim, X, Y, SmallFont(), 1.15f * U);
	Y += 30.f * U;
	Text(FString::Printf(TEXT("   LANTERNA       %d%%%s"), FMath::RoundToInt(Kael->GetBattery() * 100.f), Kael->IsFlashlightOn() ? TEXT("") : TEXT("  (desligada)")),
		Kael->GetBattery() < 0.2f ? ColBlood : ColDim, X, Y, SmallFont(), 1.15f * U);
	Y += 30.f * U;
	if (Kael->HasKeycard())
	{
		Text(TEXT("   CARTÃO DE ACESSO"), ColPale, X, Y, SmallFont(), 1.15f * U);
		Y += 30.f * U;
	}
	Y += 10.f * U;
	Text(TEXT("H — controles"), WithAlpha(ColDim, 0.7f), X, Y, SmallFont(), 1.f * U);
}

void ARupturaHUD::DrawObjectives(AMissionDirector* Director)
{
	const float X = 44.f * U;
	float Y = 44.f * U;
	const EMissionStage Stage = Director->GetStage();

	DrawRect(ColPanel, X - 16.f * U, Y - 14.f * U, 600.f * U, 112.f * U);
	Text(TEXT("SETOR 7 — A CIDADE MORTA"), ColDim, X, Y, SmallFont(), 1.2f * U);
	Y += 32.f * U;
	if (Stage <= EMissionStage::Escapar)
	{
		Text(FRupturaTuning::StageText(Stage).ToString(), ColText, X, Y, SmallFont(), 1.3f * U);
		const FString Detail = Director->GetObjectiveDetail();
		if (!Detail.IsEmpty())
		{
			Text(Detail, ColAmber, X, Y + 30.f * U, SmallFont(), 1.05f * U);
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

	const float R = 9.f * U;
	const FLinearColor C(0.85f, 0.8f, 0.7f, 0.7f);
	DrawLine(S.X, S.Y - R, S.X + R, S.Y, C, 2.f * U);
	DrawLine(S.X + R, S.Y, S.X, S.Y + R, C, 2.f * U);
	DrawLine(S.X, S.Y + R, S.X - R, S.Y, C, 2.f * U);
	DrawLine(S.X - R, S.Y, S.X, S.Y - R, C, 2.f * U);
	const float Meters = FVector::Dist(Target, PlayerOwner->GetPawn()->GetActorLocation()) / 100.f;
	Text(FString::Printf(TEXT("%.0f m"), Meters), C, S.X, S.Y + 14.f * U, SmallFont(), 1.f * U, true);
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
	const float Y = H * 0.68f;
	const FString Str = bCan ? FString::Printf(TEXT("[E]  %s"), *Prompt.ToString()) : Prompt.ToString();
	Text(Str, bCan ? ColText : ColBlood, W * 0.5f, Y, MediumFont(), 1.1f * U, true);
	if (Progress > 0.f)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Str, TW, TH, MediumFont(), 1.1f * U);
		Bar(W * 0.5f - TW * 0.5f, Y + TH + 6.f * U, TW, 3.f * U, Progress, ColText, FLinearColor(0.1f, 0.1f, 0.1f, 0.7f));
	}
}

void ARupturaHUD::DrawCountdown(AMissionDirector* Director)
{
	const float Left = Director->GetEscapeTimeLeft();
	if (Left < 0.f)
	{
		return;
	}
	const float Pulse = Left < 30.f ? (0.6f + 0.4f * FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f))) : 0.9f;
	Text(TEXT("COLAPSO ESTRUTURAL"), WithAlpha(ColBlood, Pulse), W * 0.5f, 60.f * U, SmallFont(), 1.3f * U, true);
	Text(FormatTime(Left), WithAlpha(ColText, Pulse), W * 0.5f, 88.f * U, LargeFont(), 1.5f * U, true);
}

void ARupturaHUD::DrawBanner(AMissionDirector* Director)
{
	FText Title, Sub;
	float Alpha = 0.f;
	if (!Director->GetBanner(Title, Sub, Alpha))
	{
		return;
	}
	const float Y = H * 0.2f;
	Text(Title.ToString(), WithAlpha(ColBlood, Alpha), W * 0.5f, Y, SmallFont(), 1.3f * U, true);
	Text(Sub.ToString(), WithAlpha(ColText, Alpha), W * 0.5f, Y + 30.f * U, MediumFont(), 1.3f * U, true);
}

void ARupturaHUD::DrawToast(AKaelPlayerController* PC)
{
	FText Msg;
	float Alpha = 0.f;
	if (PC->GetToast(Msg, Alpha))
	{
		Text(Msg.ToString(), WithAlpha(FLinearColor(0.85f, 0.82f, 0.75f, 1.f), Alpha), W * 0.5f, H * 0.78f, MediumFont(), 1.1f * U, true);
	}
}

void ARupturaHUD::DrawHelp()
{
	static const TCHAR* Lines[] = {
		TEXT("CONTROLES  (H oculta/mostra)"),
		TEXT("WASD mover   |   Mouse olhar   |   Shift correr (fôlego limitado)"),
		TEXT("C agachar (silencioso)   |   Espaço pular   |   Alt esquivar"),
		TEXT("F lanterna   |   V primeira/terceira pessoa"),
		TEXT("Botão direito mirar   |   Botão esquerdo atirar   |   R recarregar"),
		TEXT("1/2 ou roda trocar arma   |   Q faca   |   G granada"),
		TEXT("E interagir (segure em terminais)   |   Tab estado e objetivo"),
		TEXT("F1–F3 dificuldade   |   F5–F8 qualidade gráfica"),
		TEXT("Luz e tiros atraem os infectados. No escuro, agachado, você é quase invisível."),
	};
	const float X = W - 760.f * U;
	float Y = 170.f * U;
	DrawRect(ColPanel, X - 16.f * U, Y - 12.f * U, 740.f * U, 30.f * U * (float)UE_ARRAY_COUNT(Lines) + 20.f * U);
	for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Lines); ++i)
	{
		const FLinearColor C = i == 0 ? ColDim : (i == (int32)UE_ARRAY_COUNT(Lines) - 1 ? ColAmber : ColText);
		Text(Lines[i], C, X, Y, SmallFont(), 1.05f * U);
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
	const float Now = GetWorld()->GetTimeSeconds();

	if (State == AKaelPlayerController::EEndState::Dead)
	{
		// Escurece devagar enquanto o corpo cai.
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.94f * FMath::Clamp((Age - 0.4f) / 2.5f, 0.f, 1.f)), 0.f, 0.f, W, H);
		const float A = FMath::Clamp((Age - 1.6f) / 1.5f, 0.f, 1.f) * (FMath::PerlinNoise1D(Now * 6.f) > -0.4f ? 1.f : 0.4f);
		Text(TEXT("VOCÊ MORREU"), WithAlpha(ColBlood, A), W * 0.5f, H * 0.4f, LargeFont(), 2.6f * U, true);
		Text(TEXT("A cidade ganhou mais um morador."), WithAlpha(ColDim, A), W * 0.5f, H * 0.4f + 92.f * U, MediumFont(), 1.2f * U, true);
		if (Age > 3.f)
		{
			Text(TEXT("Voltando ao último ponto seguro...   (ENTER para agora)"), WithAlpha(ColDim, 0.8f), W * 0.5f, H * 0.82f, SmallFont(), 1.15f * U, true);
		}
		return;
	}

	const float Fade = FMath::Clamp(Age / 1.5f, 0.f, 1.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.85f * Fade), 0.f, 0.f, W, H);

	if (State == AKaelPlayerController::EEndState::Failed)
	{
		Text(TEXT("SOTERRADO"), WithAlpha(ColBlood, Fade), W * 0.5f, H * 0.4f, LargeFont(), 2.4f * U, true);
		Text(PC->GetFailReason().ToString(), WithAlpha(ColText, Fade), W * 0.5f, H * 0.4f + 90.f * U, MediumFont(), 1.2f * U, true);
		Text(TEXT("ENTER — tentar de novo do último ponto seguro"), WithAlpha(ColDim, Fade), W * 0.5f, H * 0.4f + 130.f * U, SmallFont(), 1.2f * U, true);
		return;
	}

	// Concluída
	Text(TEXT("VOCÊ SOBREVIVEU"), WithAlpha(ColText, Fade), W * 0.5f, H * 0.26f, LargeFont(), 2.6f * U, true);
	Text(TEXT("O núcleo do Protocolo Zero virou cinza. O sinal de socorro continua tocando."), WithAlpha(ColDim, Fade), W * 0.5f, H * 0.26f + 90.f * U, MediumFont(), 1.15f * U, true);

	if (const URupturaGameInstance* GI = URupturaGameInstance::Get(this))
	{
		const float Accuracy = GI->ShotsFired > 0 ? 100.f * GI->ShotsHit / GI->ShotsFired : 0.f;
		const FString Stats[] = {
			FString::Printf(TEXT("Tempo:   %s"), *FormatTime(PC->GetFinalTime())),
			FString::Printf(TEXT("Infectados abatidos:   %d"), GI->Kills),
			FString::Printf(TEXT("Precisão:   %.0f%%"), Accuracy),
			FString::Printf(TEXT("Mortes:   %d"), GI->Deaths),
			FString::Printf(TEXT("Dificuldade:   %s"), *FRupturaTuning::DifficultyName(GI->Difficulty).ToString()),
		};
		float Y = H * 0.26f + 160.f * U;
		for (const FString& Line : Stats)
		{
			Text(Line, WithAlpha(ColText, Fade * 0.9f), W * 0.5f, Y, MediumFont(), 1.1f * U, true);
			Y += 40.f * U;
		}
	}
	if (Age > 1.5f)
	{
		Text(TEXT("ENTER — jogar novamente"), ColAmber, W * 0.5f, H * 0.82f, MediumFont(), 1.15f * U, true);
	}
}
