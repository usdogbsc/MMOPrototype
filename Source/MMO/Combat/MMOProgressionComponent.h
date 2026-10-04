// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOProgressionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMMOXPChangedSignature, int32, CurrentXP, int32, XPToNextLevel, int32, XPGained);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOLevelUpSignature, int32, NewLevel);

/**
 *  Level and experience tracking.
 *  XP required for level L -> L+1 = round(BaseXPRequirement * XPGrowthFactor^(L-1)), i.e. 100, 150, 225, 338...
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOProgressionComponent();

	/** Level at spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin=1))
	int32 StartingLevel = 1;

	/** Level cap. XP stops accumulating at the cap */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin=1))
	int32 MaxLevel = 20;

	/** XP required to go from level 1 to level 2 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin=1))
	int32 BaseXPRequirement = 100;

	/** Multiplier applied to the XP requirement for each subsequent level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin=1.0))
	float XPGrowthFactor = 1.5f;

protected:

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Progression")
	int32 Level = 1;

	/** XP earned toward the next level (overflow is carried over on level up) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Progression")
	int32 CurrentXP = 0;

public:

	UPROPERTY(BlueprintAssignable, Category="Progression")
	FMMOXPChangedSignature OnXPChanged;

	/** Broadcast once per level gained */
	UPROPERTY(BlueprintAssignable, Category="Progression")
	FMMOLevelUpSignature OnLevelUp;

	virtual void InitializeComponent() override;

	/** Resets to the starting level with zero XP */
	UFUNCTION(BlueprintCallable, Category="Progression")
	void ResetProgression();

	/** Adds XP, levelling up as many times as needed and carrying overflow. Returns the number of levels gained */
	UFUNCTION(BlueprintCallable, Category="Progression")
	int32 AddXP(int32 Amount);

	/** XP needed to advance from InLevel to InLevel + 1 */
	UFUNCTION(BlueprintPure, Category="Progression")
	int32 GetXPRequiredForLevel(int32 InLevel) const;

	UFUNCTION(BlueprintPure, Category="Progression")
	int32 GetXPToNextLevel() const { return GetXPRequiredForLevel(Level); }

	UFUNCTION(BlueprintPure, Category="Progression")
	int32 GetLevel() const { return Level; }

	UFUNCTION(BlueprintPure, Category="Progression")
	int32 GetCurrentXP() const { return CurrentXP; }

	UFUNCTION(BlueprintPure, Category="Progression")
	bool IsMaxLevel() const { return Level >= MaxLevel; }
};
