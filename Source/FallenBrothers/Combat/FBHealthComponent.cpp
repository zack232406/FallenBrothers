// Fallen Brothers - Health component.

#include "Combat/FBHealthComponent.h"

#include "Engine/World.h"

UFBHealthComponent::UFBHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFBHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
}

void UFBHealthComponent::ApplyDamage(float Amount, AActor* Causer)
{
	if (Amount <= 0.f || IsDead())
	{
		return;
	}

	const float Old = Health;
	Health = FMath::Max(0.f, Health - Amount);
	LastDamageTime = GetWorld()->GetTimeSeconds();
	OnHealthChanged.Broadcast(Health, Health - Old);

	if (Health <= 0.f)
	{
		OnDeath.Broadcast();
	}
}

void UFBHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.f || IsDead())
	{
		return;
	}
	const float Old = Health;
	Health = FMath::Min(MaxHealth, Health + Amount);
	if (Health != Old)
	{
		OnHealthChanged.Broadcast(Health, Health - Old);
	}
}

float UFBHealthComponent::GetTimeSinceDamage() const
{
	return GetWorld()->GetTimeSeconds() - LastDamageTime;
}

void UFBHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsDead() && Health < MaxHealth && GetTimeSinceDamage() > RegenDelay)
	{
		Health = FMath::Min(MaxHealth, Health + RegenRate * DeltaTime);
	}
}

void UFBHealthComponent::Revive(float Percent)
{
	const float Old = Health;
	Health = FMath::Clamp(MaxHealth * Percent, 1.f, MaxHealth);
	LastDamageTime = GetWorld()->GetTimeSeconds();
	OnHealthChanged.Broadcast(Health, Health - Old);
}
