// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOAbilityComponent.generated.h"

class AMMOCharacter;
class UMMOAbilityDefinition;

UENUM(BlueprintType)
enum class EMMOAbilityResult : uint8
{
	Success,
	CastStarted,
	NotKnown,
	NotReady,
	NoTarget,
	OutOfRange,
	AlreadyCasting,
	Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOAbilityLearnedSignature, UMMOAbilityDefinition*, Ability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOAbilityUsedSignature, UMMOAbilityDefinition*, Ability);

/**
 *  The player's abilities: which are known (by level), using them (cooldowns, global cooldown, range, cast times)
 *  and applying their effects. Owned by AMMOCharacter.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOAbilityComponent();

	/** Every ability this character can learn, in display order */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Abilities")
	TArray<TSoftObjectPtr<UMMOAbilityDefinition>> AbilitySet;

	/** Shared cooldown started by most abilities */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Abilities", meta=(ClampMin=0, Units="s"))
	float GlobalCooldown = 1.5f;

	/** Moving further than this from where a cast started interrupts it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Abilities", meta=(ClampMin=0, Units="cm"))
	float CastMoveTolerance = 30.0f;

	UPROPERTY(BlueprintAssignable, Category="Abilities")
	FMMOAbilityLearnedSignature OnAbilityLearned;

	UPROPERTY(BlueprintAssignable, Category="Abilities")
	FMMOAbilityUsedSignature OnAbilityUsed;

	static const FName GlobalCooldownKey;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** All abilities in the set, loaded */
	const TArray<TObjectPtr<UMMOAbilityDefinition>>& GetAllAbilities() const { return AllAbilities; }

	/** Abilities from Set that a character of Level knows */
	static TArray<UMMOAbilityDefinition*> GetUnlocked(const TArray<UMMOAbilityDefinition*>& Set, int32 Level);

	/** Updates known abilities for Level. Newly learned ones are announced (OnAbilityLearned) when bAnnounce */
	void RefreshKnown(int32 Level, bool bAnnounce);

	bool IsKnown(const UMMOAbilityDefinition* Ability) const { return Known.Contains(Ability); }
	const TArray<TObjectPtr<UMMOAbilityDefinition>>& GetKnown() const { return Known; }

	EMMOAbilityResult UseAbility(UMMOAbilityDefinition* Ability);

	/** Seconds until the ability can be used (its own cooldown or the global cooldown, whichever is longer), and that cooldown's length */
	void GetCooldown(const UMMOAbilityDefinition* Ability, float& OutRemaining, float& OutDuration) const;

	/** False if the ability needs an enemy target that is missing or out of reach */
	bool IsTargetInRange(const UMMOAbilityDefinition* Ability) const;

	bool IsCasting() const { return CastingAbility != nullptr; }
	UMMOAbilityDefinition* GetCastingAbility() const { return CastingAbility; }
	float GetCastProgress() const;
	void InterruptCast(bool bShowMessage = true);

protected:

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMMOAbilityDefinition>> AllAbilities;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMMOAbilityDefinition>> Known;

	UPROPERTY(Transient)
	TObjectPtr<UMMOAbilityDefinition> CastingAbility;

	TWeakObjectPtr<AActor> CastTarget;
	double CastStartTime = 0.0;
	FVector CastStartLocation = FVector::ZeroVector;

	AMMOCharacter* GetCharacter() const;
	float GetReach(const UMMOAbilityDefinition* Ability) const;
	void LoadAbilitySet();

	/** Applies the ability now */
	void Execute(UMMOAbilityDefinition* Ability, AActor* Target);
	void DamageTarget(UMMOAbilityDefinition* Ability, AActor* Target);
};
