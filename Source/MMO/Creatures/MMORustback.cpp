// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/MMORustback.h"
#include "Combat/MMOHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Items/MMOLootTable.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AMMORustback::AMMORustback()
{
	DisplayName = NSLOCTEXT("MMOCreature", "Rustback", "Rustback Skitterer");
	CreatureLevel = 4;

	GetHealth()->MaxHealth = 95.0f;
	AttackDamage = 8.0f;
	AttackRange = 85.0f;
	AttackCooldown = 1.8f;
	AggroRange = 800.0f;
	LeashRange = 2000.0f;
	ChaseSpeed = 380.0f;
	ReturnSpeed = 700.0f;
	WanderRadius = 300.0f;
	WanderSpeed = 120.0f;
	XPReward = 70;
	QuestTag = TEXT("Rustback");
	RespawnDelay = 40.0f;

	LootTable = TSoftObjectPtr<UMMOLootTable>(FSoftObjectPath(TEXT("/Game/MMO/Loot/DA_Loot_Rustback.DA_Loot_Rustback")));
	AggroSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/S_MMO_BeetleChitter.S_MMO_BeetleChitter")));
	AttackHitSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/S_MMO_BeetleBite.S_MMO_BeetleBite")));
	DeathSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/S_MMO_BeetleDeath.S_MMO_BeetleDeath")));

	// low and wide: capsule bottom at Z = -35
	GetCapsuleComponent()->InitCapsuleSize(48.0f, 35.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	const FLinearColor Shell(0.26f, 0.075f, 0.018f);
	const FLinearColor ShellDark(0.07f, 0.03f, 0.015f);
	const FLinearColor Under(0.11f, 0.09f, 0.07f);
	const FLinearColor Eye(1.0f, 0.55f, 0.05f);

	// carapace with ridged plates
	AddBodyPart(TEXT("Underside"), SphereMesh.Object, nullptr, FVector(-4, 0, -8), FRotator::ZeroRotator, FVector(0.92f, 0.64f, 0.26f), Under);
	AddBodyPart(TEXT("Shell"), SphereMesh.Object, nullptr, FVector(-6, 0, 2), FRotator::ZeroRotator, FVector(0.9f, 0.66f, 0.36f), Shell);
	AddBodyPart(TEXT("Ridge"), CubeMesh.Object, nullptr, FVector(-6, 0, 18), FRotator::ZeroRotator, FVector(0.7f, 0.06f, 0.06f), ShellDark);
	AddBodyPart(TEXT("PlateA"), CubeMesh.Object, nullptr, FVector(10, 0, 14), FRotator::ZeroRotator, FVector(0.05f, 0.58f, 0.1f), ShellDark);
	AddBodyPart(TEXT("PlateB"), CubeMesh.Object, nullptr, FVector(-18, 0, 14), FRotator::ZeroRotator, FVector(0.05f, 0.6f, 0.1f), ShellDark);

	// abdomen on the tail joint (sways as it walks)
	TailPivot = AddPivot(TEXT("TailPivot"), nullptr, FVector(-42, 0, 2));
	AddBodyPart(TEXT("Abdomen"), SphereMesh.Object, TailPivot, FVector(-18, 0, -2), FRotator::ZeroRotator, FVector(0.42f, 0.5f, 0.3f), Shell);

	// head with glowing eyes and mandibles on the jaw joint
	HeadPivot = AddPivot(TEXT("HeadPivot"), nullptr, FVector(38, 0, 0));
	AddBodyPart(TEXT("Head"), SphereMesh.Object, HeadPivot, FVector(12, 0, 0), FRotator::ZeroRotator, FVector(0.32f, 0.38f, 0.24f), ShellDark);
	AddBodyPart(TEXT("EyeL"), SphereMesh.Object, HeadPivot, FVector(22, -11, 7), FRotator::ZeroRotator, FVector(0.07f), Eye);
	AddBodyPart(TEXT("EyeR"), SphereMesh.Object, HeadPivot, FVector(22, 11, 7), FRotator::ZeroRotator, FVector(0.07f), Eye);
	AddBodyPart(TEXT("AntennaL"), CylinderMesh.Object, HeadPivot, FVector(26, -8, 16), FRotator(-50, -20, 0), FVector(0.025f, 0.025f, 0.3f), ShellDark);
	AddBodyPart(TEXT("AntennaR"), CylinderMesh.Object, HeadPivot, FVector(26, 8, 16), FRotator(-50, 20, 0), FVector(0.025f, 0.025f, 0.3f), ShellDark);

	JawPivot = AddPivot(TEXT("JawPivot"), HeadPivot, FVector(24, 0, -4));
	AddBodyPart(TEXT("MandibleL"), ConeMesh.Object, JawPivot, FVector(10, -9, 0), FRotator(-90, 0, 15), FVector(0.08f, 0.08f, 0.26f), Under);
	AddBodyPart(TEXT("MandibleR"), ConeMesh.Object, JawPivot, FVector(10, 9, 0), FRotator(-90, 0, -15), FVector(0.08f, 0.08f, 0.26f), Under);

	// six legs splayed outward; the base class swings alternate legs while walking
	for (int32 i = 0; i < 6; ++i)
	{
		const int32 Row = i / 2;
		const float Side = (i % 2 == 0) ? -1.0f : 1.0f;
		const FVector Root(22.0f - Row * 22.0f, Side * 22.0f, -8.0f);
		USceneComponent* Hip = AddPivot(*FString::Printf(TEXT("Hip%d"), i), nullptr, Root);
		AddBodyPart(*FString::Printf(TEXT("Thigh%d"), i), CylinderMesh.Object, Hip, FVector(0, Side * 16.0f, 2.0f), FRotator(0, 0, Side * 70.0f), FVector(0.05f, 0.05f, 0.34f), ShellDark);
		AddBodyPart(*FString::Printf(TEXT("Shin%d"), i), CylinderMesh.Object, Hip, FVector(0, Side * 34.0f, -13.0f), FRotator(0, 0, Side * -25.0f), FVector(0.04f, 0.04f, 0.3f), ShellDark);
		LegPivots.Add(Hip);
	}
}

void AMMORustback::ConfigureAsBrood()
{
	DisplayName = NSLOCTEXT("MMOCreature", "Rustling", "Rustling");
	CreatureLevel = 3;
	GetHealth()->MaxHealth = 40.0f;
	AttackDamage = 5.0f;
	AttackCooldown = 1.6f;
	XPReward = 15;
	QuestTag = TEXT("Rustling");
	LootTable.Reset();
	WanderRadius = 0.0f;
	bRespawns = false;
	LootableCorpseDuration = 0.0f;
	GetCapsuleComponent()->SetRelativeScale3D(FVector(0.7f));
	for (FLinearColor& Color : BodyPartColors)
	{
		Color = FLinearColor(Color.R * 1.3f, Color.G * 1.1f, Color.B, 1.0f);
	}
}
