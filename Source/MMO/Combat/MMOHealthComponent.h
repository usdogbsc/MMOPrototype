// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOHealthComponent.generated.h"

class UMMOHealthComponent;

/** Combat events broadcast globally so presentation (floating combat text) can react without knowing every actor */
UENUM(BlueprintType)
enum class EMMOCombatEvent : uint8
{
	Damage,
	Heal,
	Immune,
	Death
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMMOHealthChangedSignature, UMMOHealthComponent*, HealthComponent, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMMODamagedSignature, float, Amount, AActor*, DamageInstigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMODeathSignature, AActor*, Killer);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FMMOAnyCombatEventSignature, const UMMOHealthComponent* /*Component*/, EMMOCombatEvent /*Event*/, float /*Amount*/);

/**
 *  Health, damage and death for any combatant (player or creature).
 *  Written so CurrentHealth could later become a replicated property without changing callers.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOHealthComponent();

	/** Maximum health at spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta=(ClampMin=1))
	float MaxHealth = 100.0f;

protected:

	/** Current health */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Health")
	float CurrentHealth = 100.0f;

	/** If true, incoming damage is ignored (e.g. a creature evading back to its spawn) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Health")
	bool bInvulnerable = false;

	/** Who last damaged us. Used to award XP on death */
	TWeakObjectPtr<AActor> LastInstigator;

public:

	/** Called whenever current or max health changes */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FMMOHealthChangedSignature OnHealthChanged;

	/** Called when damage is actually applied */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FMMODamagedSignature OnDamaged;

	/** Called once when health reaches zero */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FMMODeathSignature OnDeath;

	/** Global combat event stream for presentation only */
	static FMMOAnyCombatEventSignature OnAnyCombatEvent;

	virtual void InitializeComponent() override;

	/** Applies damage, clamped so health never goes below zero. Returns the amount applied */
	UFUNCTION(BlueprintCallable, Category="Health")
	float ApplyDamage(float Amount, AActor* DamageInstigator);

	/** Restores health, clamped to max. Returns the amount healed */
	UFUNCTION(BlueprintCallable, Category="Health")
	float Heal(float Amount);

	/** Changes max health, optionally filling to the new max */
	UFUNCTION(BlueprintCallable, Category="Health")
	void SetMaxHealth(float NewMaxHealth, bool bFillToMax);

	/** Restores full health and clears death state */
	UFUNCTION(BlueprintCallable, Category="Health")
	void ResetHealth();

	UFUNCTION(BlueprintCallable, Category="Health")
	void SetInvulnerable(bool bNewInvulnerable) { bInvulnerable = bNewInvulnerable; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDead() const { return CurrentHealth <= 0.0f; }

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsInvulnerable() const { return bInvulnerable; }

	AActor* GetLastInstigator() const { return LastInstigator.Get(); }

protected:

	void BroadcastHealthChanged();
};
