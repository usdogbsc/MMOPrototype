// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOProgressionCurveTest, "MMO.Combat.Progression.XPCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOProgressionCurveTest::RunTest(const FString& Parameters)
{
	UMMOProgressionComponent* Progression = NewObject<UMMOProgressionComponent>();
	Progression->ResetProgression();

	TestEqual(TEXT("Starts at level 1"), Progression->GetLevel(), 1);
	TestEqual(TEXT("Level 1 -> 2 requires 100"), Progression->GetXPRequiredForLevel(1), 100);
	TestEqual(TEXT("Level 2 -> 3 requires 150"), Progression->GetXPRequiredForLevel(2), 150);
	TestEqual(TEXT("Level 3 -> 4 requires 225"), Progression->GetXPRequiredForLevel(3), 225);
	TestEqual(TEXT("Level 4 -> 5 requires 338"), Progression->GetXPRequiredForLevel(4), 338);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOProgressionOverflowTest, "MMO.Combat.Progression.Overflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOProgressionOverflowTest::RunTest(const FString& Parameters)
{
	UMMOProgressionComponent* Progression = NewObject<UMMOProgressionComponent>();
	Progression->ResetProgression();

	// the example from the design brief: 90/100 + 30 => level 2 with 20/150
	Progression->AddXP(90);
	TestEqual(TEXT("90 XP stays level 1"), Progression->GetLevel(), 1);
	TestEqual(TEXT("90 XP banked"), Progression->GetCurrentXP(), 90);

	const int32 Gained = Progression->AddXP(30);
	TestEqual(TEXT("One level gained"), Gained, 1);
	TestEqual(TEXT("Now level 2"), Progression->GetLevel(), 2);
	TestEqual(TEXT("Overflow carried"), Progression->GetCurrentXP(), 20);
	TestEqual(TEXT("Next requirement 150"), Progression->GetXPToNextLevel(), 150);

	// a large award gains several levels at once: 20 + 400 = 420 -> L3 (270 left) -> L4 (45 left)
	TestEqual(TEXT("Two levels gained"), Progression->AddXP(400), 2);
	TestEqual(TEXT("Now level 4"), Progression->GetLevel(), 4);
	TestEqual(TEXT("Overflow after multi-level"), Progression->GetCurrentXP(), 45);

	TestEqual(TEXT("Zero XP ignored"), Progression->AddXP(0), 0);
	TestEqual(TEXT("Negative XP ignored"), Progression->AddXP(-50), 0);
	TestEqual(TEXT("XP unchanged"), Progression->GetCurrentXP(), 45);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOProgressionMaxLevelTest, "MMO.Combat.Progression.MaxLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOProgressionMaxLevelTest::RunTest(const FString& Parameters)
{
	UMMOProgressionComponent* Progression = NewObject<UMMOProgressionComponent>();
	Progression->MaxLevel = 3;
	Progression->ResetProgression();

	Progression->AddXP(100000);
	TestEqual(TEXT("Clamped to max level"), Progression->GetLevel(), 3);
	TestEqual(TEXT("No XP at max level"), Progression->GetCurrentXP(), 0);
	TestEqual(TEXT("Further XP ignored"), Progression->AddXP(100), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOHealthTest, "MMO.Combat.Health.DamageAndDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOHealthTest::RunTest(const FString& Parameters)
{
	UMMOHealthComponent* Health = NewObject<UMMOHealthComponent>();
	Health->MaxHealth = 60.0f;
	Health->ResetHealth();

	int32 DeathCount = 0;
	FDelegateHandle Handle = UMMOHealthComponent::OnAnyCombatEvent.AddLambda([&DeathCount, Health](const UMMOHealthComponent* Component, EMMOCombatEvent Event, float)
	{
		if (Component == Health && Event == EMMOCombatEvent::Death)
		{
			++DeathCount;
		}
	});

	TestEqual(TEXT("Damage applied"), Health->ApplyDamage(12.0f, nullptr), 12.0f);
	TestEqual(TEXT("Health reduced"), Health->GetCurrentHealth(), 48.0f);

	Health->SetInvulnerable(true);
	TestEqual(TEXT("Invulnerable ignores damage"), Health->ApplyDamage(12.0f, nullptr), 0.0f);
	Health->SetInvulnerable(false);

	TestEqual(TEXT("Overkill clamps applied damage"), Health->ApplyDamage(500.0f, nullptr), 48.0f);
	TestEqual(TEXT("Health never below zero"), Health->GetCurrentHealth(), 0.0f);
	TestTrue(TEXT("Is dead"), Health->IsDead());

	TestEqual(TEXT("No damage once dead"), Health->ApplyDamage(10.0f, nullptr), 0.0f);
	TestEqual(TEXT("No healing once dead"), Health->Heal(10.0f), 0.0f);
	TestEqual(TEXT("Death broadcast exactly once"), DeathCount, 1);

	Health->ResetHealth();
	TestFalse(TEXT("Alive after reset"), Health->IsDead());
	TestEqual(TEXT("Full health after reset"), Health->GetCurrentHealth(), 60.0f);

	UMMOHealthComponent::OnAnyCombatEvent.Remove(Handle);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
