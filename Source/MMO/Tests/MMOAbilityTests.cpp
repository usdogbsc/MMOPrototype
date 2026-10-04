// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Combat/MMOAbilityComponent.h"
#include "Combat/MMOAbilityDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOAbilityUnlockTest, "MMO.Abilities.UnlocksAndDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOAbilityUnlockTest::RunTest(const FString& Parameters)
{
	auto Make = [](FName Id, int32 Level)
	{
		UMMOAbilityDefinition* Ability = NewObject<UMMOAbilityDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Ability->AbilityId = Id;
		Ability->RequiredLevel = Level;
		return Ability;
	};
	UMMOAbilityDefinition* Strike = Make(TEXT("AT_Strike"), 2);
	UMMOAbilityDefinition* Bash = Make(TEXT("AT_Bash"), 3);
	UMMOAbilityDefinition* Heal = Make(TEXT("AT_Heal"), 4);
	const TArray<UMMOAbilityDefinition*> Set = { Strike, Bash, Heal };

	TestEqual(TEXT("Nothing at level 1"), UMMOAbilityComponent::GetUnlocked(Set, 1).Num(), 0);
	TestTrue(TEXT("Level 2 learns the first"), UMMOAbilityComponent::GetUnlocked(Set, 2) == TArray<UMMOAbilityDefinition*>{ Strike });
	TestEqual(TEXT("Level 3 knows two"), UMMOAbilityComponent::GetUnlocked(Set, 3).Num(), 2);
	TestEqual(TEXT("Level 20 knows all"), UMMOAbilityComponent::GetUnlocked(Set, 20).Num(), 3);

	Strike->WeaponDamageMultiplier = 1.0f;
	Strike->BonusDamage = 4.0f;
	TestEqual(TEXT("Weapon roll plus bonus"), Strike->ComputeDamage(12.0f), 16.0f);
	Bash->WeaponDamageMultiplier = 0.5f;
	TestEqual(TEXT("Half weapon damage"), Bash->ComputeDamage(12.0f), 6.0f);
	TestTrue(TEXT("Damage abilities are offensive"), Strike->DealsDamage() && Strike->IsOffensive());

	Heal->TargetType = EMMOAbilityTarget::Self;
	Heal->SelfHealFraction = 0.3f;
	TestTrue(TEXT("Self heal is not offensive and deals no damage"), !Heal->IsOffensive() && !Heal->DealsDamage());
	return true;
}

#endif
