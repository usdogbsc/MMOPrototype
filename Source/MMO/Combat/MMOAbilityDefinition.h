// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MMOAbilityDefinition.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EMMOAbilityTarget : uint8
{
	/** The selected enemy, within Range */
	Enemy,
	/** The caster */
	Self,
	/** Every living enemy in a cone in front of the caster, within AreaRadius */
	EnemiesInFront
};

/**
 *  A player ability (data asset /Game/MMO/Abilities/DA_Ability_<AbilityId>).
 *  Effects are simple data: weapon-based damage, a bleed, a stun and a self heal can be combined.
 */
UCLASS(BlueprintType)
class UMMOAbilityDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability")
	FName AbilityId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability")
	TObjectPtr<UTexture2D> Icon;

	/** Learned automatically on reaching this level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin=1))
	int32 RequiredLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability")
	EMMOAbilityTarget TargetType = EMMOAbilityTarget::Enemy;

	/** Reach for Enemy abilities (0 = melee, same as Basic Attack) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin=0, Units="cm"))
	float Range = 0.0f;

	/** Reach of EnemiesInFront abilities */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin=0, Units="cm"))
	float AreaRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin=0, Units="s"))
	float Cooldown = 6.0f;

	/** 0 = instant. Moving interrupts a cast */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin=0, Units="s"))
	float CastTime = 0.0f;

	/** Starts the shared global cooldown */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability")
	bool bTriggersGlobalCooldown = true;

	/** Damage = weapon swing roll x WeaponDamageMultiplier + BonusDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability|Effects", meta=(ClampMin=0))
	float WeaponDamageMultiplier = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability|Effects", meta=(ClampMin=0))
	float BonusDamage = 0.0f;

	/** Total bleed damage dealt over BleedDuration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability|Effects", meta=(ClampMin=0))
	float BleedDamage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability|Effects", meta=(ClampMin=0, Units="s"))
	float BleedDuration = 0.0f;

	/** The target can't move or attack for this long */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability|Effects", meta=(ClampMin=0, Units="s"))
	float StunDuration = 0.0f;

	/** Fraction of max health restored to the caster */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability|Effects", meta=(ClampMin=0, ClampMax=1))
	float SelfHealFraction = 0.0f;

	bool IsOffensive() const { return TargetType != EMMOAbilityTarget::Self; }
	bool DealsDamage() const { return WeaponDamageMultiplier > 0.0f || BonusDamage > 0.0f; }

	/** Direct damage for one weapon roll */
	float ComputeDamage(float WeaponRoll) const { return WeaponRoll * WeaponDamageMultiplier + BonusDamage; }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("MMOAbility"), GetFName()); }

	/** Loaded abilities first, then /Game/MMO/Abilities/DA_Ability_<AbilityId> */
	static UMMOAbilityDefinition* FindById(FName InAbilityId);
};
