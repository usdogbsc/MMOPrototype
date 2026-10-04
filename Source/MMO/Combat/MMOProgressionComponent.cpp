// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MMOProgressionComponent.h"

UMMOProgressionComponent::UMMOProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

void UMMOProgressionComponent::InitializeComponent()
{
	Super::InitializeComponent();

	ResetProgression();
}

void UMMOProgressionComponent::ResetProgression()
{
	Level = FMath::Clamp(StartingLevel, 1, MaxLevel);
	CurrentXP = 0;
}

void UMMOProgressionComponent::RestoreProgress(int32 InLevel, int32 InXP)
{
	Level = FMath::Clamp(InLevel, 1, MaxLevel);
	CurrentXP = IsMaxLevel() ? 0 : FMath::Clamp(InXP, 0, GetXPToNextLevel() - 1);
	OnXPChanged.Broadcast(CurrentXP, GetXPToNextLevel(), 0);
}

int32 UMMOProgressionComponent::AddXP(int32 Amount)
{
	if (Amount <= 0 || IsMaxLevel())
	{
		return 0;
	}

	CurrentXP += Amount;

	int32 LevelsGained = 0;
	while (!IsMaxLevel() && CurrentXP >= GetXPToNextLevel())
	{
		CurrentXP -= GetXPToNextLevel();
		++Level;
		++LevelsGained;
		OnLevelUp.Broadcast(Level);
	}

	if (IsMaxLevel())
	{
		CurrentXP = 0;
	}

	OnXPChanged.Broadcast(CurrentXP, GetXPToNextLevel(), Amount);
	return LevelsGained;
}

int32 UMMOProgressionComponent::GetXPRequiredForLevel(int32 InLevel) const
{
	const int32 Exponent = FMath::Max(0, InLevel - 1);
	return FMath::Max(1, FMath::RoundToInt(BaseXPRequirement * FMath::Pow(XPGrowthFactor, static_cast<float>(Exponent))));
}
