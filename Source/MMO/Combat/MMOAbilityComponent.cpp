// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MMOAbilityComponent.h"
#include "Combat/MMOAbilityDefinition.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOCooldownComponent.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOTargetable.h"
#include "Creatures/MMOCreature.h"
#include "MMOCharacter.h"
#include "EngineUtils.h"
#include "MMO.h"

#define LOCTEXT_NAMESPACE "MMOAbilities"

const FName UMMOAbilityComponent::GlobalCooldownKey(TEXT("GlobalCooldown"));

UMMOAbilityComponent::UMMOAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	for (const TCHAR* Id : { TEXT("RendingStrike"), TEXT("ShoulderBash"), TEXT("SecondWind"), TEXT("CleavingArc") })
	{
		AbilitySet.Add(TSoftObjectPtr<UMMOAbilityDefinition>(FSoftObjectPath(FString::Printf(TEXT("/Game/MMO/Abilities/DA_Ability_%s.DA_Ability_%s"), Id, Id))));
	}
}

void UMMOAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadAbilitySet();
}

void UMMOAbilityComponent::LoadAbilitySet()
{
	if (AllAbilities.Num() > 0)
	{
		return;
	}
	for (const TSoftObjectPtr<UMMOAbilityDefinition>& Ref : AbilitySet)
	{
		if (UMMOAbilityDefinition* Ability = Ref.LoadSynchronous())
		{
			AllAbilities.Add(Ability);
		}
	}
}

AMMOCharacter* UMMOAbilityComponent::GetCharacter() const
{
	return Cast<AMMOCharacter>(GetOwner());
}

TArray<UMMOAbilityDefinition*> UMMOAbilityComponent::GetUnlocked(const TArray<UMMOAbilityDefinition*>& Set, int32 Level)
{
	return Set.FilterByPredicate([Level](const UMMOAbilityDefinition* Ability) { return Ability && Ability->RequiredLevel <= Level; });
}

void UMMOAbilityComponent::RefreshKnown(int32 Level, bool bAnnounce)
{
	LoadAbilitySet();
	TArray<UMMOAbilityDefinition*> Set;
	for (UMMOAbilityDefinition* Ability : AllAbilities)
	{
		Set.Add(Ability);
	}

	for (UMMOAbilityDefinition* Ability : GetUnlocked(Set, Level))
	{
		if (!Known.Contains(Ability))
		{
			Known.Add(Ability);
			if (bAnnounce)
			{
				OnAbilityLearned.Broadcast(Ability);
			}
		}
	}
	Known.RemoveAll([Level](const UMMOAbilityDefinition* Ability) { return !Ability || Ability->RequiredLevel > Level; });
}

float UMMOAbilityComponent::GetReach(const UMMOAbilityDefinition* Ability) const
{
	const AMMOCharacter* Character = GetCharacter();
	if (Ability->TargetType == EMMOAbilityTarget::EnemiesInFront)
	{
		return Ability->AreaRadius;
	}
	return Ability->Range > 0.0f ? Ability->Range : (Character ? Character->GetCombat()->BasicAttackRange : 150.0f);
}

bool UMMOAbilityComponent::IsTargetInRange(const UMMOAbilityDefinition* Ability) const
{
	if (!Ability || Ability->TargetType != EMMOAbilityTarget::Enemy)
	{
		return true;
	}
	const AMMOCharacter* Character = GetCharacter();
	const AActor* Target = Character ? Character->GetCombat()->GetCurrentTarget() : nullptr;
	const UMMOHealthComponent* TargetHealth = UMMOCombatComponent::GetTargetHealth(Target);
	return TargetHealth && !TargetHealth->IsDead() && UMMOCombatComponent::GetEdgeDistance(Character, Target) <= GetReach(Ability) + 10.0f;
}

void UMMOAbilityComponent::GetCooldown(const UMMOAbilityDefinition* Ability, float& OutRemaining, float& OutDuration) const
{
	OutRemaining = 0.0f;
	OutDuration = 0.0f;
	const AMMOCharacter* Character = GetCharacter();
	if (!Ability || !Character)
	{
		return;
	}
	const UMMOCooldownComponent* Cooldowns = Character->GetCooldowns();
	OutRemaining = Cooldowns->GetRemaining(Ability->AbilityId);
	OutDuration = Cooldowns->GetDuration(Ability->AbilityId);
	if (Ability->bTriggersGlobalCooldown && Cooldowns->GetRemaining(GlobalCooldownKey) > OutRemaining)
	{
		OutRemaining = Cooldowns->GetRemaining(GlobalCooldownKey);
		OutDuration = Cooldowns->GetDuration(GlobalCooldownKey);
	}
}

EMMOAbilityResult UMMOAbilityComponent::UseAbility(UMMOAbilityDefinition* Ability)
{
	AMMOCharacter* Character = GetCharacter();
	if (!Character || Character->IsDead())
	{
		return EMMOAbilityResult::Dead;
	}
	if (!Ability || !Known.Contains(Ability))
	{
		Character->ShowPlayerMessage(LOCTEXT("NotKnown", "You haven't learned that ability yet."));
		return EMMOAbilityResult::NotKnown;
	}
	if (IsCasting())
	{
		Character->ShowPlayerMessage(LOCTEXT("Casting", "You are already doing something."));
		return EMMOAbilityResult::AlreadyCasting;
	}

	float Remaining, Duration;
	GetCooldown(Ability, Remaining, Duration);
	if (Remaining > 0.0f)
	{
		Character->ShowPlayerMessage(LOCTEXT("NotReady", "That ability isn't ready yet."));
		return EMMOAbilityResult::NotReady;
	}

	AActor* Target = nullptr;
	if (Ability->TargetType == EMMOAbilityTarget::Enemy)
	{
		Target = Character->GetCombat()->GetCurrentTarget();
		const UMMOHealthComponent* TargetHealth = UMMOCombatComponent::GetTargetHealth(Target);
		if (!TargetHealth || TargetHealth->IsDead())
		{
			Character->ShowPlayerMessage(LOCTEXT("NoTarget", "You have no target."));
			return EMMOAbilityResult::NoTarget;
		}
		if (!IsTargetInRange(Ability))
		{
			Character->ShowPlayerMessage(LOCTEXT("OutOfRange", "Out of range."));
			return EMMOAbilityResult::OutOfRange;
		}
	}

	if (Ability->CastTime > 0.0f)
	{
		CastingAbility = Ability;
		CastTarget = Target;
		CastStartTime = GetWorld()->GetTimeSeconds();
		CastStartLocation = Character->GetActorLocation();
		if (Ability->bTriggersGlobalCooldown)
		{
			Character->GetCooldowns()->StartCooldown(GlobalCooldownKey, GlobalCooldown);
		}
		return EMMOAbilityResult::CastStarted;
	}

	Execute(Ability, Target);
	return EMMOAbilityResult::Success;
}

float UMMOAbilityComponent::GetCastProgress() const
{
	if (!CastingAbility || CastingAbility->CastTime <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - CastStartTime) / CastingAbility->CastTime), 0.0f, 1.0f);
}

void UMMOAbilityComponent::InterruptCast(bool bShowMessage)
{
	if (!CastingAbility)
	{
		return;
	}
	CastingAbility = nullptr;
	CastTarget.Reset();
	if (AMMOCharacter* Character = GetCharacter())
	{
		Character->GetCooldowns()->ClearCooldown(GlobalCooldownKey);
		if (bShowMessage)
		{
			Character->ShowPlayerMessage(LOCTEXT("Interrupted", "Interrupted"));
		}
	}
}

void UMMOAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AMMOCharacter* Character = GetCharacter();
	if (!CastingAbility || !Character)
	{
		return;
	}

	// casting needs the caster to stand still and stay alive
	if (Character->IsDead() || FVector::Dist2D(Character->GetActorLocation(), CastStartLocation) > CastMoveTolerance)
	{
		InterruptCast(!Character->IsDead());
		return;
	}

	if (GetCastProgress() >= 1.0f)
	{
		UMMOAbilityDefinition* Ability = CastingAbility;
		AActor* Target = CastTarget.Get();
		CastingAbility = nullptr;
		CastTarget.Reset();
		Execute(Ability, Target);
	}
}

void UMMOAbilityComponent::DamageTarget(UMMOAbilityDefinition* Ability, AActor* Target)
{
	AMMOCharacter* Character = GetCharacter();
	UMMOHealthComponent* TargetHealth = UMMOCombatComponent::GetTargetHealth(Target);
	if (!TargetHealth || TargetHealth->IsDead())
	{
		return;
	}

	if (Ability->DealsDamage())
	{
		TargetHealth->ApplyDamage(Ability->ComputeDamage(Character->GetCombat()->RollDamage()), Character);
	}

	AMMOCreature* Creature = Cast<AMMOCreature>(Target);
	if (Creature && !Creature->IsDead())
	{
		if (Ability->BleedDamage > 0.0f && Ability->BleedDuration > 0.0f)
		{
			Creature->ApplyBleed(Ability->BleedDamage, Ability->BleedDuration, Character);
		}
		if (Ability->StunDuration > 0.0f)
		{
			Creature->ApplyStun(Ability->StunDuration);
			Character->ShowWorldText(Creature->GetNameplateLocation() + FVector(0.0f, 0.0f, 25.0f), TEXT("Stunned"), FLinearColor(1.0f, 0.85f, 0.3f));
		}
	}
}

void UMMOAbilityComponent::Execute(UMMOAbilityDefinition* Ability, AActor* Target)
{
	AMMOCharacter* Character = GetCharacter();
	if (!Character || !Ability)
	{
		return;
	}

	UMMOCooldownComponent* Cooldowns = Character->GetCooldowns();
	Cooldowns->StartCooldown(Ability->AbilityId, Ability->Cooldown);
	if (Ability->bTriggersGlobalCooldown && Ability->CastTime <= 0.0f)
	{
		Cooldowns->StartCooldown(GlobalCooldownKey, GlobalCooldown);
	}

	switch (Ability->TargetType)
	{
	case EMMOAbilityTarget::Enemy:
		Character->FaceActor(Target);
		DamageTarget(Ability, Target);
		break;

	case EMMOAbilityTarget::EnemiesInFront:
	{
		const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
		int32 Hits = 0;
		for (TActorIterator<AMMOCreature> It(GetWorld()); It; ++It)
		{
			AMMOCreature* Creature = *It;
			if (Creature->IsDead() || !Creature->IsTargetable())
			{
				continue;
			}
			const FVector To = (Creature->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D();
			if (UMMOCombatComponent::GetEdgeDistance(Character, Creature) <= Ability->AreaRadius && FVector::DotProduct(Forward, To) >= 0.3f)
			{
				DamageTarget(Ability, Creature);
				++Hits;
			}
		}
		UE_LOG(LogMMO, Log, TEXT("%s hit %d enemies"), *Ability->DisplayName.ToString(), Hits);
		break;
	}

	case EMMOAbilityTarget::Self:
		break;
	}

	if (Ability->SelfHealFraction > 0.0f)
	{
		Character->GetHealth()->Heal(Character->GetHealth()->GetMaxHealth() * Ability->SelfHealFraction);
	}

	Character->NotifyAbilityExecuted(Ability, Target);
	OnAbilityUsed.Broadcast(Ability);
}

#undef LOCTEXT_NAMESPACE
