#pragma once

#include "CoreMinimal.h"
#include "Mission/InteractableActor.h"
#include "SecurityDoor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/** Porta deslizante de duas folhas. Pode exigir cartão de acesso ou ficar travada pelo sistema de segurança. */
UCLASS()
class RUPTURA_API ASecurityDoor : public AInteractableActor
{
	GENERATED_BODY()

public:
	ASecurityDoor();

	virtual void Tick(float DeltaSeconds) override;

	void Configure(float InWidth, float InHeight, bool bNeedsKeycard, bool bLockedBySecurity, const FText& InName);
	void Unlock();
	void Open(bool bInstant = false);
	/** Fecha a porta (com estrondo). bLock = fica travada até ser aberta por código (falta de energia). */
	void Close(bool bInstant, bool bLock);
	bool IsOpen() const { return bOpen; }

	virtual FText GetInteractPrompt(const AKaelCharacter* Kael) const override;
	virtual bool CanInteract(const AKaelCharacter* Kael) const override;
	virtual void Interact(AKaelCharacter* Kael) override;
	virtual FVector GetInteractLocation() const override;

protected:
	void UpdateStatus();
	void LayoutPanels();

	UPROPERTY(VisibleAnywhere, Category = "Porta") TObjectPtr<UStaticMeshComponent> PanelL;
	UPROPERTY(VisibleAnywhere, Category = "Porta") TObjectPtr<UStaticMeshComponent> PanelR;
	UPROPERTY(VisibleAnywhere, Category = "Porta") TObjectPtr<UStaticMeshComponent> Header;
	UPROPERTY(VisibleAnywhere, Category = "Porta") TObjectPtr<UStaticMeshComponent> Lamp;
	UPROPERTY(VisibleAnywhere, Category = "Porta") TObjectPtr<UPointLightComponent> StatusLight;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LampMaterial;

	float Width = 400.f;
	float Height = 350.f;
	bool bRequiresKeycard = false;
	bool bLocked = false;
	bool bOpen = false;
	float OpenAlpha = 0.f;
	FText DoorName;
};
