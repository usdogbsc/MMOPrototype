// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Professions/MMOProfessionComponent.h"
#include "Professions/MMOProfessionTypes.h"
#include "Professions/MMORecipeDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOProfessionDifficultyTest, "MMO.Professions.DifficultyAndSkillGain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOProfessionDifficultyTest::RunTest(const FString& Parameters)
{
	using namespace MMOProfessions;
	TestEqual(TEXT("Below the requirement"), GetDifficulty(4, 5), EMMOSkillDifficulty::TooHard);
	TestEqual(TEXT("Just learned: orange"), GetDifficulty(5, 5), EMMOSkillDifficulty::Orange);
	TestEqual(TEXT("+5: yellow"), GetDifficulty(10, 5), EMMOSkillDifficulty::Yellow);
	TestEqual(TEXT("+10: green"), GetDifficulty(15, 5), EMMOSkillDifficulty::Green);
	TestEqual(TEXT("+15: grey"), GetDifficulty(20, 5), EMMOSkillDifficulty::Grey);

	TestEqual(TEXT("Orange gives a point"), GetSkillGain(5, 5), 1);
	TestEqual(TEXT("Green still gives a point"), GetSkillGain(19, 5), 1);
	TestEqual(TEXT("Grey gives nothing"), GetSkillGain(20, 5), 0);
	TestEqual(TEXT("Too hard gives nothing"), GetSkillGain(1, 5), 0);
	TestEqual(TEXT("Capped at the maximum"), GetSkillGain(MaxSkill, MaxSkill), 0);

	UMMOProfessionComponent* Professions = NewObject<UMMOProfessionComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	TestEqual(TEXT("Skills start at 1"), Professions->GetSkill(EMMOProfession::Mining), 1);
	Professions->SetSkill(EMMOProfession::Mining, 999);
	TestEqual(TEXT("Clamped to the maximum"), Professions->GetSkill(EMMOProfession::Mining), MaxSkill);
	Professions->RestoreSkills({ 3, 7 });
	TestTrue(TEXT("Restore fills missing professions with 1"), Professions->GetSkill(EMMOProfession::Herbalism) == 7 && Professions->GetSkill(EMMOProfession::Cooking) == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMORecipeCraftableTest, "MMO.Professions.RecipeMaxCraftable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMORecipeCraftableTest::RunTest(const FString& Parameters)
{
	auto MakeItem = [](FName Id)
	{
		UMMOItemDefinition* Item = NewObject<UMMOItemDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Item->ItemId = Id;
		Item->MaxStackSize = 20;
		return Item;
	};
	UMMOItemDefinition* Ore = MakeItem(TEXT("PT_Ore"));
	UMMOItemDefinition* Pelt = MakeItem(TEXT("PT_Pelt"));
	UMMOItemDefinition* Bar = MakeItem(TEXT("PT_Bar"));

	UMMORecipeDefinition* Recipe = NewObject<UMMORecipeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Recipe->Output = Bar;
	Recipe->Ingredients = { { Ore, 2 }, { Pelt, 1 } };

	UMMOInventoryComponent* Bag = NewObject<UMMOInventoryComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	Bag->Capacity = 6;
	Bag->EnsureSlots();
	TestEqual(TEXT("Nothing without reagents"), Recipe->GetMaxCraftable(Bag), 0);
	Bag->AddItem(Ore, 7);
	TestEqual(TEXT("Still nothing without the second reagent"), Recipe->GetMaxCraftable(Bag), 0);
	Bag->AddItem(Pelt, 5);
	TestEqual(TEXT("Limited by the scarcest reagent"), Recipe->GetMaxCraftable(Bag), 3);
	Recipe->Ingredients.Reset();
	TestEqual(TEXT("A recipe without reagents can't be crafted"), Recipe->GetMaxCraftable(Bag), 0);
	return true;
}

#endif
