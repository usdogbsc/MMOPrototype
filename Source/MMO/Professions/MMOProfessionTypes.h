// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MMOProfessionTypes.generated.h"

/** Gathering and crafting skills */
UENUM(BlueprintType)
enum class EMMOProfession : uint8
{
	Mining,
	Herbalism,
	Smithing,
	Alchemy,
	Cooking,
	Count UMETA(Hidden)
};

/** How useful a node / recipe is for raising skill, shown as a color like classic MMOs */
UENUM(BlueprintType)
enum class EMMOSkillDifficulty : uint8
{
	/** Can't do it yet */
	TooHard,
	/** Always raises skill */
	Orange,
	Yellow,
	Green,
	/** Never raises skill anymore */
	Grey
};

namespace MMOProfessions
{
	static constexpr int32 MaxSkill = 50;

	FText GetName(EMMOProfession Profession);

	/** Difficulty of something that requires RequiredSkill, for a character with Skill */
	EMMOSkillDifficulty GetDifficulty(int32 Skill, int32 RequiredSkill);

	/** Deterministic skill gain: orange, yellow and green give +1, grey gives nothing */
	int32 GetSkillGain(int32 Skill, int32 RequiredSkill);

	FLinearColor GetDifficultyColor(EMMOSkillDifficulty Difficulty);
}
