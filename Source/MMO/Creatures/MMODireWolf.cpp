// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/MMODireWolf.h"
#include "Combat/MMOHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Items/MMOLootTable.h"

AMMODireWolf::AMMODireWolf()
{
	DisplayName = NSLOCTEXT("MMOCreature", "DireWolf", "Dire Wolf");
	CreatureLevel = 4;

	GetHealth()->MaxHealth = 170.0f;
	AttackDamage = 12.0f;
	AttackCooldown = 2.2f;
	AggroRange = 1100.0f;
	LeashRange = 2600.0f;
	ChaseSpeed = 440.0f;
	XPReward = 130;
	RespawnDelay = 25.0f;
	WanderRadius = 700.0f;
	WanderSpeed = 140.0f;

	LootTable = TSoftObjectPtr<UMMOLootTable>(FSoftObjectPath(TEXT("/Game/MMO/Loot/DA_Loot_DireWolf.DA_Loot_DireWolf")));

	// a third bigger: scaling the root scales the capsule, body and reach together
	GetCapsuleComponent()->SetRelativeScale3D(FVector(1.35f));

	// darker, rust-tinged coat with red eyes
	for (int32 i = 0; i < BodyPartColors.Num(); ++i)
	{
		const FLinearColor Original = BodyPartColors[i];
		const bool bIsEye = Original.R > 0.9f && Original.G > 0.3f && Original.B < 0.1f;
		BodyPartColors[i] = bIsEye ? FLinearColor(1.0f, 0.04f, 0.02f) : FLinearColor(Original.R * 0.7f + 0.01f, Original.G * 0.6f, Original.B * 0.55f);
	}
}
