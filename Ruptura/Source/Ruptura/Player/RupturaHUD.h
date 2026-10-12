#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RupturaHUD.generated.h"

class AKaelCharacter;
class AKaelPlayerController;
class AMissionDirector;
class UFont;

/** HUD em português desenhado via Canvas (não depende de assets UMG). */
UCLASS()
class RUPTURA_API ARupturaHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	void DrawIntro(AKaelPlayerController* PC);
	void DrawCinematic(AKaelPlayerController* PC);
	void DrawDamageOverlay(AKaelCharacter* Kael);
	void DrawCrosshair(AKaelCharacter* Kael);
	void DrawVitals(AKaelCharacter* Kael);
	void DrawWeaponInfo(AKaelCharacter* Kael);
	void DrawObjectives(AMissionDirector* Director);
	void DrawObjectiveMarker(AMissionDirector* Director);
	void DrawEnemyMarkers();
	void DrawInteraction(AKaelCharacter* Kael);
	void DrawBossBar(AMissionDirector* Director);
	void DrawCountdown(AMissionDirector* Director);
	void DrawBanner(AMissionDirector* Director);
	void DrawToast(AKaelPlayerController* PC);
	void DrawHelp();
	void DrawEndScreen(AKaelPlayerController* PC, AMissionDirector* Director);

	// Utilitários
	void Text(const FString& Str, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale, bool bCenter = false, bool bShadow = true);
	void Bar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, const FLinearColor& Back);
	bool WorldToScreen(const FVector& World, FVector2D& OutScreen, bool& bOutBehind) const;

	float U = 1.f; // escala da interface (1.0 a 1080p)
	float W = 1920.f;
	float H = 1080.f;
};
