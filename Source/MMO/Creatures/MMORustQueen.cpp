// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/MMORustQueen.h"
#include "Creatures/MMOCreatureAIController.h"
#include "Combat/MMOHealthComponent.h"
#include "World/MMOTelegraph.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Items/MMOLootTable.h"
#include "Kismet/GameplayStatics.h"
#include "MMO.h"

#define LOCTEXT_NAMESPACE "MMOBoss"

FMMOBossEmote AMMORustQueen::OnBossEmote;

AMMORustQueen::AMMORustQueen()
{
	DisplayName = LOCTEXT("Name", "Grindmaw the Rust Queen");
	CreatureLevel = 6;
	bIsBoss = true;

	GetHealth()->MaxHealth = 1100.0f;
	AttackDamage = 15.0f;
	AttackRange = 120.0f;
	AttackCooldown = 2.4f;
	AggroRange = 1300.0f;
	LeashRange = 2600.0f;
	ChaseSpeed = 330.0f;
	WanderRadius = 0.0f;
	XPReward = 600;
	QuestTag = TEXT("RustQueen");
	RespawnDelay = 300.0f;
	LootableCorpseDuration = 300.0f;
	NameplateDistance = 5000.0f;

	LootTable = TSoftObjectPtr<UMMOLootTable>(FSoftObjectPath(TEXT("/Game/MMO/Loot/DA_Loot_RustQueen.DA_Loot_RustQueen")));

	// a huge, ember-veined queen
	GetCapsuleComponent()->SetRelativeScale3D(FVector(2.4f));
	for (int32 i = 0; i < BodyPartColors.Num(); ++i)
	{
		const FLinearColor Original = BodyPartColors[i];
		const bool bIsEye = Original.R > 0.9f && Original.G > 0.4f;
		BodyPartColors[i] = bIsEye ? FLinearColor(1.0f, 0.12f, 0.02f) : FLinearColor(Original.R * 1.15f + 0.03f, Original.G * 0.8f, Original.B * 0.7f);
	}
}

void AMMORustQueen::BeginPlay()
{
	Super::BeginPlay();
	BaseAttackCooldown = AttackCooldown;
}

void AMMORustQueen::Emote(const FText& Text)
{
	UE_LOG(LogMMO, Log, TEXT("%s"), *Text.ToString());
	OnBossEmote.Broadcast(this, Text);
}

void AMMORustQueen::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsDead())
	{
		return;
	}

	const bool bInCombat = IsInCombat();
	const AMMOCreatureAIController* AI = Cast<AMMOCreatureAIController>(GetController());
	AActor* Target = AI ? AI->GetThreatTarget() : nullptr;
	const double Now = GetWorld()->GetTimeSeconds();

	if (bInCombat && !bWasInCombat)
	{
		FightStart = Now;
		NextEruptionTime = Now + FirstEruptionDelay;
		Emote(LOCTEXT("Pull", "Grindmaw shrieks: \"Fresh meat for the brood!\""));
	}
	bWasInCombat = bInCombat;
	if (!bInCombat || !Target)
	{
		return;
	}

	// Rust Eruption under the current target
	if (Now >= NextEruptionTime && !ActiveTelegraph.IsValid())
	{
		SpawnEruption(Target);
		NextEruptionTime = Now + EruptionInterval;
	}

	// brood at each health threshold
	const float HealthFraction = GetHealth()->GetHealthPercent();
	if (BroodThresholds.IsValidIndex(NextBroodThreshold) && HealthFraction <= BroodThresholds[NextBroodThreshold])
	{
		++NextBroodThreshold;
		SummonBrood(Target);
	}

	if (!bEnraged && HealthFraction <= EnrageThreshold)
	{
		bEnraged = true;
		AttackCooldown = BaseAttackCooldown * EnrageAttackSpeed;
		Emote(LOCTEXT("Enrage", "Grindmaw becomes enraged!"));
	}
}

void AMMORustQueen::SpawnEruption(AActor* Target)
{
	FVector Location = Target->GetActorLocation();
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MMOEruption), false, Target);
	Params.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByChannel(Ground, Location, Location - FVector(0.0f, 0.0f, 600.0f), ECC_Visibility, Params))
	{
		Location = Ground.ImpactPoint + FVector(0.0f, 0.0f, 2.0f);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AMMOTelegraph* Telegraph = GetWorld()->SpawnActor<AMMOTelegraph>(AMMOTelegraph::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams))
	{
		Telegraph->Arm(EruptionRadius, EruptionWarning, EruptionDamage, this);
		ActiveTelegraph = Telegraph;
	}
}

void AMMORustQueen::SummonBrood(AActor* Target)
{
	Emote(LOCTEXT("Brood", "Grindmaw calls her brood! Rustlings burrow out of the walls!"));
	for (int32 i = 0; i < BroodCount; ++i)
	{
		const float Angle = 360.0f * i / FMath::Max(1, BroodCount) + 30.0f;
		const FVector Offset = FRotator(0.0f, Angle, 0.0f).Vector() * 420.0f;
		const FTransform BroodTransform(FRotator(0.0f, Angle + 180.0f, 0.0f), GetActorLocation() + Offset + FVector(0.0f, 0.0f, 20.0f));

		AMMORustback* Rustling = GetWorld()->SpawnActorDeferred<AMMORustback>(AMMORustback::StaticClass(), BroodTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Rustling)
		{
			continue;
		}
		Rustling->ConfigureAsBrood();
		UGameplayStatics::FinishSpawningActor(Rustling, BroodTransform);
		if (AMMOCreatureAIController* RustlingAI = Cast<AMMOCreatureAIController>(Rustling->GetController()))
		{
			RustlingAI->NotifyDamagedBy(Target);
		}
		Brood.Add(Rustling);
	}
}

void AMMORustQueen::ClearAdds()
{
	for (const TWeakObjectPtr<AMMOCreature>& Add : Brood)
	{
		if (Add.IsValid())
		{
			Add->Destroy();
		}
	}
	Brood.Reset();
	if (ActiveTelegraph.IsValid())
	{
		ActiveTelegraph->Destroy();
	}
	ActiveTelegraph.Reset();
}

void AMMORustQueen::NotifyCombatReset()
{
	Super::NotifyCombatReset();
	ClearAdds();
	NextBroodThreshold = 0;
	bEnraged = false;
	bWasInCombat = false;
	AttackCooldown = BaseAttackCooldown;
	UE_LOG(LogMMO, Log, TEXT("%s reset"), *DisplayName.ToString());
}

void AMMORustQueen::NotifyDied()
{
	Super::NotifyDied();
	ClearAdds();
	Emote(LOCTEXT("Death", "Grindmaw the Rust Queen collapses. The mine falls silent."));
}

#undef LOCTEXT_NAMESPACE
