// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/MMOGreyWolf.h"
#include "Combat/MMOHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AMMOGreyWolf::AMMOGreyWolf()
{
	DisplayName = NSLOCTEXT("MMOCreature", "GreyWolf", "Grey Wolf");
	CreatureLevel = 1;

	GetHealth()->MaxHealth = 60.0f;
	AttackDamage = 6.0f;
	AttackRange = 90.0f;
	AttackCooldown = 2.0f;
	AggroRange = 900.0f;
	LeashRange = 2200.0f;
	ChaseSpeed = 420.0f;
	ReturnSpeed = 700.0f;
	XPReward = 40;
	CorpseDuration = 4.0f;
	RespawnDelay = 8.0f;

	// capsule bottom sits at Z = -50 relative to the actor
	GetCapsuleComponent()->InitCapsuleSize(50.0f, 50.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	const FLinearColor Fur(0.30f, 0.30f, 0.32f);
	const FLinearColor LightFur(0.45f, 0.44f, 0.43f);
	const FLinearColor DarkFur(0.18f, 0.18f, 0.20f);
	const FLinearColor Eye(1.0f, 0.55f, 0.05f);
	const FLinearColor Nose(0.02f, 0.02f, 0.02f);

	// basic shapes are 100cm, so scale 1.0 == 1m
	AddBodyPart(TEXT("Body"), CubeMesh.Object, FVector(0, 0, 7), FRotator::ZeroRotator, FVector(1.10f, 0.42f, 0.40f), Fur);
	AddBodyPart(TEXT("Chest"), CubeMesh.Object, FVector(38, 0, 12), FRotator::ZeroRotator, FVector(0.40f, 0.46f, 0.46f), LightFur);
	AddBodyPart(TEXT("Head"), CubeMesh.Object, FVector(66, 0, 32), FRotator::ZeroRotator, FVector(0.34f, 0.32f, 0.30f), Fur);
	AddBodyPart(TEXT("Snout"), CubeMesh.Object, FVector(92, 0, 24), FRotator::ZeroRotator, FVector(0.26f, 0.18f, 0.14f), LightFur);
	AddBodyPart(TEXT("Nose"), SphereMesh.Object, FVector(105, 0, 28), FRotator::ZeroRotator, FVector(0.07f), Nose);
	AddBodyPart(TEXT("EyeL"), SphereMesh.Object, FVector(82, -9, 38), FRotator::ZeroRotator, FVector(0.06f), Eye);
	AddBodyPart(TEXT("EyeR"), SphereMesh.Object, FVector(82, 9, 38), FRotator::ZeroRotator, FVector(0.06f), Eye);
	AddBodyPart(TEXT("EarL"), ConeMesh.Object, FVector(60, -10, 54), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.16f), DarkFur);
	AddBodyPart(TEXT("EarR"), ConeMesh.Object, FVector(60, 10, 54), FRotator::ZeroRotator, FVector(0.10f, 0.10f, 0.16f), DarkFur);
	AddBodyPart(TEXT("Tail"), CylinderMesh.Object, FVector(-78, 0, 8), FRotator(-60, 0, 0), FVector(0.09f, 0.09f, 0.45f), DarkFur);

	const FVector LegScale(0.12f, 0.12f, 0.38f);
	AddBodyPart(TEXT("LegFL"), CylinderMesh.Object, FVector(38, -15, -31), FRotator::ZeroRotator, LegScale, DarkFur);
	AddBodyPart(TEXT("LegFR"), CylinderMesh.Object, FVector(38, 15, -31), FRotator::ZeroRotator, LegScale, DarkFur);
	AddBodyPart(TEXT("LegBL"), CylinderMesh.Object, FVector(-40, -15, -31), FRotator::ZeroRotator, LegScale, DarkFur);
	AddBodyPart(TEXT("LegBR"), CylinderMesh.Object, FVector(-40, 15, -31), FRotator::ZeroRotator, LegScale, DarkFur);
}
