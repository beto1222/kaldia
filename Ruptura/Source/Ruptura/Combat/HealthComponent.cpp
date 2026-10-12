#include "Combat/HealthComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleAnyDamage);
	}
}

void UHealthComponent::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	AActor* InstigatorActor = InstigatedBy ? InstigatedBy->GetPawn() : nullptr;
	if (!InstigatorActor && DamageCauser)
	{
		InstigatorActor = DamageCauser->GetInstigator() ? DamageCauser->GetInstigator() : DamageCauser;
	}
	ApplyDamageAmount(Damage, InstigatorActor);
}

float UHealthComponent::ApplyDamageAmount(float Amount, AActor* InstigatorActor)
{
	if (Amount <= 0.f || bInvulnerable || IsDead())
	{
		return 0.f;
	}

	const float Applied = FMath::Min(Health, Amount * DamageMultiplier);
	Health -= Applied;
	if (const UWorld* World = GetWorld())
	{
		LastDamageTime = World->GetTimeSeconds();
	}

	OnHealthChanged.Broadcast(this, Health, -Applied, InstigatorActor);
	if (Health <= 0.f)
	{
		Health = 0.f;
		OnDeath.Broadcast(this, InstigatorActor);
	}
	return Applied;
}

void UHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.f || IsDead())
	{
		return;
	}
	const float Before = Health;
	Health = FMath::Min(MaxHealth, Health + Amount);
	if (Health != Before)
	{
		OnHealthChanged.Broadcast(this, Health, Health - Before, nullptr);
	}
}

void UHealthComponent::SetMaxHealth(float NewMax, bool bRefill)
{
	MaxHealth = FMath::Max(1.f, NewMax);
	Health = bRefill ? MaxHealth : FMath::Min(Health, MaxHealth);
}
