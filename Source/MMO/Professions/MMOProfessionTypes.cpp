// Copyright Epic Games, Inc. All Rights Reserved.

#include "Professions/MMOProfessionTypes.h"

#define LOCTEXT_NAMESPACE "MMOProfessions"

namespace MMOProfessions
{
	FText GetName(EMMOProfession Profession)
	{
		switch (Profession)
		{
		case EMMOProfession::Mining: return LOCTEXT("Mining", "Mining");
		case EMMOProfession::Herbalism: return LOCTEXT("Herbalism", "Herbalism");
		case EMMOProfession::Smithing: return LOCTEXT("Smithing", "Smithing");
		case EMMOProfession::Alchemy: return LOCTEXT("Alchemy", "Alchemy");
		case EMMOProfession::Cooking: return LOCTEXT("Cooking", "Cooking");
		default: return FText::GetEmpty();
		}
	}

	EMMOSkillDifficulty GetDifficulty(int32 Skill, int32 RequiredSkill)
	{
		if (Skill < RequiredSkill)
		{
			return EMMOSkillDifficulty::TooHard;
		}
		const int32 Above = Skill - RequiredSkill;
		if (Above < 5)
		{
			return EMMOSkillDifficulty::Orange;
		}
		if (Above < 10)
		{
			return EMMOSkillDifficulty::Yellow;
		}
		if (Above < 15)
		{
			return EMMOSkillDifficulty::Green;
		}
		return EMMOSkillDifficulty::Grey;
	}

	int32 GetSkillGain(int32 Skill, int32 RequiredSkill)
	{
		if (Skill >= MaxSkill)
		{
			return 0;
		}
		const EMMOSkillDifficulty Difficulty = GetDifficulty(Skill, RequiredSkill);
		return Difficulty == EMMOSkillDifficulty::Orange || Difficulty == EMMOSkillDifficulty::Yellow || Difficulty == EMMOSkillDifficulty::Green ? 1 : 0;
	}

	FLinearColor GetDifficultyColor(EMMOSkillDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case EMMOSkillDifficulty::TooHard: return FLinearColor(0.85f, 0.2f, 0.2f);
		case EMMOSkillDifficulty::Orange: return FLinearColor(1.0f, 0.5f, 0.15f);
		case EMMOSkillDifficulty::Yellow: return FLinearColor(1.0f, 0.9f, 0.2f);
		case EMMOSkillDifficulty::Green: return FLinearColor(0.3f, 0.9f, 0.3f);
		default: return FLinearColor(0.6f, 0.6f, 0.6f);
		}
	}
}

#undef LOCTEXT_NAMESPACE
