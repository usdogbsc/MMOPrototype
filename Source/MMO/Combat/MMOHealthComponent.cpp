// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MMOHealthComponent.h"

FMMOAnyCombatEventSignature UMMOHealthComponent::OnAnyCombatEvent;

UMMOHealthComponent::UMMOHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

void UMMOHealthComponent::InitializeComponent()
{
	Super::InitializeComponent();

	CurrentHealth = MaxHealth;
}

float UMMOHealthComponent::ApplyDamage(float Amount, AActor* DamageInstigator)
{
	if (Amount <= 0.0f || IsDead())
	{
		return 0.0f;
	}

	if (bInvulnerable)
	{
		OnAnyCombatEvent.Broadcast(this, EMMOCombatEvent::Immune, 0.0f);
		return 0.0f;
	}

	// all damage is physical for now, so armor always applies
	const float Mitigated = Amount * (1.0f - GetDamageReduction());
	const float Applied = FMath::Min(Mitigated, CurrentHealth);
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Mitigated);
	LastInstigator = DamageInstigator;

	OnAnyCombatEvent.Broadcast(this, EMMOCombatEvent::Damage, Applied);
	OnDamaged.Broadcast(Applied, DamageInstigator);
	BroadcastHealthChanged();

	if (IsDead())
	{
		OnAnyCombatEvent.Broadcast(this, EMMOCombatEvent::Death, 0.0f);
		OnDeath.Broadcast(DamageInstigator);
	}

	return Applied;
}

float UMMOHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.0f || IsDead())
	{
		return 0.0f;
	}

	const float Healed = FMath::Min(Amount, MaxHealth - CurrentHealth);
	if (Healed <= 0.0f)
	{
		return 0.0f;
	}

	CurrentHealth += Healed;
	OnAnyCombatEvent.Broadcast(this, EMMOCombatEvent::Heal, Healed);
	BroadcastHealthChanged();
	return Healed;
}

void UMMOHealthComponent::SetMaxHealth(float NewMaxHealth, bool bFillToMax)
{
	MaxHealth = FMath::Max(1.0f, NewMaxHealth);
	CurrentHealth = bFillToMax ? MaxHealth : FMath::Min(CurrentHealth, MaxHealth);
	BroadcastHealthChanged();
}

void UMMOHealthComponent::ResetHealth()
{
	CurrentHealth = MaxHealth;
	bInvulnerable = false;
	LastInstigator.Reset();
	BroadcastHealthChanged();
}

void UMMOHealthComponent::BroadcastHealthChanged()
{
	OnHealthChanged.Broadcast(this, CurrentHealth, MaxHealth);
}
