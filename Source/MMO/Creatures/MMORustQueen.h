// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/MMORustback.h"
#include "MMORustQueen.generated.h"

class AMMOTelegraph;

DECLARE_MULTICAST_DELEGATE_TwoParams(FMMOBossEmote, const AMMOCreature* /*Boss*/, const FText& /*Text*/);

/**
 *  Grindmaw, the Rust Queen: the Rustvein Mine boss.
 *  - Rust Eruption: every few seconds a glowing circle appears under her target and erupts after a short delay.
 *  - Brood: at 60% and 30% health she calls three Rustlings.
 *  - Enrage: below 25% health she attacks much faster.
 *  Leaving the fight (leash) resets everything: adds vanish, she heals, phases start over.
 */
UCLASS()
class AMMORustQueen : public AMMORustback
{
	GENERATED_BODY()

public:

	AMMORustQueen();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin=1, Units="s"))
	float EruptionInterval = 9.0f;

	/** Seconds into the fight before the first eruption */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin=0, Units="s"))
	float FirstEruptionDelay = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin=0, Units="s"))
	float EruptionWarning = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin=0, Units="cm"))
	float EruptionRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin=0))
	float EruptionDamage = 35.0f;

	/** Health fractions at which the brood is summoned */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
	TArray<float> BroodThresholds = { 0.6f, 0.3f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin=0))
	int32 BroodCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin=0, ClampMax=1))
	float EnrageThreshold = 0.25f;

	/** Attack cooldown multiplier while enraged */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin=0.1, ClampMax=1))
	float EnrageAttackSpeed = 0.6f;

	/** Emotes ("Grindmaw calls her brood!") for the HUD */
	static FMMOBossEmote OnBossEmote;

	bool IsEnraged() const { return bEnraged; }
	const TArray<TWeakObjectPtr<AMMOCreature>>& GetBrood() const { return Brood; }
	AMMOTelegraph* GetActiveTelegraph() const { return ActiveTelegraph.Get(); }

	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyCombatReset() override;
	virtual void NotifyDied() override;

protected:

	virtual void BeginPlay() override;

	TArray<TWeakObjectPtr<AMMOCreature>> Brood;
	TWeakObjectPtr<AMMOTelegraph> ActiveTelegraph;
	int32 NextBroodThreshold = 0;
	bool bEnraged = false;
	bool bWasInCombat = false;
	double FightStart = 0.0;
	double NextEruptionTime = 0.0;
	float BaseAttackCooldown = 2.4f;

	void SpawnEruption(AActor* Target);
	void SummonBrood(AActor* Target);
	void ClearAdds();
	void Emote(const FText& Text);
};
