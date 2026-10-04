// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/MMOCreature.h"
#include "Creatures/MMOCreatureAIController.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "MMO.h"

namespace MMOCreature
{
	static const FName ColorParam(TEXT("Color"));
	static const float HitFlashDuration = 0.18f;
	static const float LungeDuration = 0.35f;
}

AMMOCreature::AMMOCreature()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AMMOCreatureAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;

	// creatures are built from static parts, so the inherited skeletal mesh is unused
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Health = CreateDefaultSubobject<UMMOHealthComponent>(TEXT("Health"));

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	TargetRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetRing"));
	TargetRing->SetupAttachment(RootComponent);
	TargetRing->SetStaticMesh(CylinderMesh.Object);
	TargetRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetRing->SetCastShadow(false);
	TargetRing->SetHiddenInGame(true);

	DisplayName = NSLOCTEXT("MMOCreature", "DefaultName", "Creature");
}

void AMMOCreature::BeginPlay()
{
	Super::BeginPlay();

	SpawnTransform = GetActorTransform();
	GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;

	// size the target ring to the capsule and sit it on the ground
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	TargetRing->SetRelativeLocation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 1.0f));
	TargetRing->SetRelativeScale3D(FVector(Radius * 2.6f / 100.0f, Radius * 2.6f / 100.0f, 0.01f));

	if (UMaterialInstanceDynamic* RingMaterial = TargetRing->CreateDynamicMaterialInstance(0))
	{
		RingMaterial->SetVectorParameterValue(MMOCreature::ColorParam, FLinearColor(0.9f, 0.08f, 0.05f));
	}

	BodyPartMaterials.Reset();
	for (int32 i = 0; i < BodyParts.Num(); ++i)
	{
		UMaterialInstanceDynamic* Material = BodyParts[i]->CreateDynamicMaterialInstance(0);
		if (Material)
		{
			Material->SetVectorParameterValue(MMOCreature::ColorParam, BodyPartColors[i]);
		}
		BodyPartMaterials.Add(Material);
	}

	Health->OnDamaged.AddDynamic(this, &AMMOCreature::HandleDamaged);
	Health->OnDeath.AddDynamic(this, &AMMOCreature::HandleDeath);
}

UStaticMeshComponent* AMMOCreature::AddBodyPart(FName Name, UStaticMesh* PartMesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(VisualRoot);
	Part->SetStaticMesh(PartMesh);
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);

	BodyParts.Add(Part);
	BodyPartColors.Add(Color);
	return Part;
}

bool AMMOCreature::IsTargetable() const
{
	return !bIsDead && !IsHidden();
}

void AMMOCreature::SetTargeted(bool bTargeted)
{
	TargetRing->SetHiddenInGame(!bTargeted);
}

FVector AMMOCreature::GetNameplateLocation() const
{
	return GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 45.0f);
}

void AMMOCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdatePresentation(DeltaSeconds);
}

bool AMMOCreature::CanAttack(const AActor* Target) const
{
	if (bIsDead || !Target)
	{
		return false;
	}

	if (GetWorld()->GetTimeSeconds() < LastAttackTime + AttackCooldown)
	{
		return false;
	}

	return UMMOCombatComponent::GetEdgeDistance(this, Target) <= AttackRange;
}

void AMMOCreature::PerformAttack(AActor* Target)
{
	if (bIsDead || !Target)
	{
		return;
	}

	LastAttackTime = GetWorld()->GetTimeSeconds();
	LungeTime = MMOCreature::LungeDuration;

	if (UMMOHealthComponent* TargetHealth = Target->FindComponentByClass<UMMOHealthComponent>())
	{
		TargetHealth->ApplyDamage(AttackDamage, this);
	}
}

void AMMOCreature::SetEvading(bool bEvading)
{
	Health->SetInvulnerable(bEvading);
	GetCharacterMovement()->MaxWalkSpeed = bEvading ? ReturnSpeed : ChaseSpeed;
}

void AMMOCreature::HandleDamaged(float Amount, AActor* DamageInstigator)
{
	HitFlashTime = MMOCreature::HitFlashDuration;

	if (AMMOCreatureAIController* AI = Cast<AMMOCreatureAIController>(GetController()))
	{
		AI->NotifyDamagedBy(DamageInstigator);
	}
}

void AMMOCreature::HandleDeath(AActor* Killer)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	HitFlashTime = 0.0f;
	LungeTime = 0.0f;

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// corpses don't block the player
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	if (AMMOCreatureAIController* AI = Cast<AMMOCreatureAIController>(GetController()))
	{
		AI->NotifyPawnDied();
	}

	// award XP exactly once, guarded by bIsDead above
	if (Killer)
	{
		if (UMMOProgressionComponent* Progression = Killer->FindComponentByClass<UMMOProgressionComponent>())
		{
			Progression->AddXP(XPReward);
		}
	}

	UE_LOG(LogMMO, Log, TEXT("%s died (killer: %s, XP reward: %d)"), *GetName(), *GetNameSafe(Killer), XPReward);

	GetWorldTimerManager().SetTimer(CorpseTimer, this, &AMMOCreature::HideCorpse, FMath::Max(0.01f, CorpseDuration), false);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMMOCreature::Respawn, FMath::Max(CorpseDuration + 0.1f, RespawnDelay), false);
}

void AMMOCreature::HideCorpse()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	TargetRing->SetHiddenInGame(true);
}

void AMMOCreature::Respawn()
{
	TeleportTo(SpawnTransform.GetLocation(), SpawnTransform.Rotator(), false, true);

	bIsDead = false;
	DeathBlend = 0.0f;
	LastAttackTime = -1000.0;

	Health->ResetHealth();
	SetEvading(false);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SetActorEnableCollision(true);
	SetActorHiddenInGame(false);
	TargetRing->SetHiddenInGame(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	if (AMMOCreatureAIController* AI = Cast<AMMOCreatureAIController>(GetController()))
	{
		AI->NotifyPawnRespawned();
	}

	UE_LOG(LogMMO, Log, TEXT("%s respawned"), *GetName());
}

void AMMOCreature::UpdatePresentation(float DeltaSeconds)
{
	AnimTime += DeltaSeconds;
	HitFlashTime = FMath::Max(0.0f, HitFlashTime - DeltaSeconds);
	LungeTime = FMath::Max(0.0f, LungeTime - DeltaSeconds);
	DeathBlend = bIsDead ? FMath::Min(1.0f, DeathBlend + DeltaSeconds * 4.0f) : 0.0f;

	FVector Offset = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;

	// trot bob while moving
	const float Speed = GetVelocity().Size2D();
	if (!bIsDead && Speed > 10.0f)
	{
		Offset.Z += FMath::Abs(FMath::Sin(AnimTime * Speed * 0.035f)) * 6.0f;
	}

	// attack lunge: quick forward snap with a head dip, then recover
	if (LungeTime > 0.0f)
	{
		const float Alpha = 1.0f - LungeTime / MMOCreature::LungeDuration;
		const float Curve = FMath::Sin(Alpha * PI);
		Offset.X += Curve * 45.0f;
		Rotation.Pitch -= Curve * 15.0f;
	}

	// hit reaction: small knockback
	if (HitFlashTime > 0.0f)
	{
		Offset.X -= (HitFlashTime / MMOCreature::HitFlashDuration) * 12.0f;
	}

	// death: roll onto its side
	if (bIsDead)
	{
		const float Ease = FMath::InterpEaseOut(0.0f, 1.0f, DeathBlend, 2.0f);
		Rotation.Roll = Ease * 90.0f;
		Offset.Z -= Ease * 20.0f;
	}

	VisualRoot->SetRelativeLocationAndRotation(Offset, Rotation);

	// color: white flash on hit, darkened when dead
	for (int32 i = 0; i < BodyPartMaterials.Num(); ++i)
	{
		if (!BodyPartMaterials[i])
		{
			continue;
		}

		FLinearColor Color = BodyPartColors[i];
		if (HitFlashTime > 0.0f)
		{
			Color = FLinearColor::LerpUsingHSV(Color, FLinearColor(1.0f, 0.85f, 0.85f), HitFlashTime / MMOCreature::HitFlashDuration);
		}
		if (bIsDead)
		{
			Color = Color * FMath::Lerp(1.0f, 0.35f, DeathBlend);
			Color.A = 1.0f;
		}
		BodyPartMaterials[i]->SetVectorParameterValue(MMOCreature::ColorParam, Color);
	}
}
