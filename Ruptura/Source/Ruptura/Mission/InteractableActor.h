#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "InteractableActor.generated.h"

class AKaelCharacter;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Qualquer coisa com que Kael pode interagir com a tecla E (portas, terminais, objetos). */
class RUPTURA_API IInteractable
{
	GENERATED_BODY()

public:
	virtual FText GetInteractPrompt(const AKaelCharacter* Kael) const = 0;
	virtual bool CanInteract(const AKaelCharacter* Kael) const = 0;
	/** 0 = toque; > 0 = segurar E por esse tempo. */
	virtual float GetInteractHoldTime() const = 0;
	virtual void Interact(AKaelCharacter* Kael) = 0;
	virtual FVector GetInteractLocation() const = 0;
};

/** Base para atores interativos; Kael procura objetos desta classe à sua frente. */
UCLASS(Abstract)
class RUPTURA_API AInteractableActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AInteractableActor();

	virtual FText GetInteractPrompt(const AKaelCharacter* Kael) const override PURE_VIRTUAL(AInteractableActor::GetInteractPrompt, return FText::GetEmpty(););
	virtual bool CanInteract(const AKaelCharacter* Kael) const override { return true; }
	virtual float GetInteractHoldTime() const override { return 0.f; }
	virtual void Interact(AKaelCharacter* Kael) override PURE_VIRTUAL(AInteractableActor::Interact, );
	virtual FVector GetInteractLocation() const override { return GetActorLocation(); }

	/** Distância máxima (cm) para aparecer o aviso de interação. */
	UPROPERTY(EditAnywhere, Category = "Interacao")
	float InteractRadius = 230.f;
};
