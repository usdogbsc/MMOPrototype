// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOTargetable.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

#define LOCTEXT_NAMESPACE "MMOCombat"

UMMOCombatComponent::UMMOCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UMMOCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// drop targets that despawned or wandered far away. Dead targets stay selected until their corpse is removed
	if (CurrentTarget)
	{
		const bool bGone = !IsValid(CurrentTarget) || CurrentTarget->IsHidden();
		const bool bTooFar = GetOwner() && FVector::Dist(GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation()) > TargetingRange * 1.5f;
		if (bGone || bTooFar)
		{
			ClearTarget();
		}
	}
}

void UMMOCombatComponent::SetTarget(AActor* NewTarget)
{
	IMMOTargetable* NewTargetable = Cast<IMMOTargetable>(NewTarget);
	if (!NewTargetable || NewTarget == GetOwner())
	{
		NewTarget = nullptr;
	}

	if (NewTarget == CurrentTarget)
	{
		return;
	}

	if (IMMOTargetable* OldTargetable = Cast<IMMOTargetable>(CurrentTarget))
	{
		OldTargetable->SetTargeted(false);
	}

	CurrentTarget = NewTarget;

	if (NewTargetable && NewTarget)
	{
		NewTargetable->SetTargeted(true);
	}

	OnTargetChanged.Broadcast(CurrentTarget);
}

void UMMOCombatComponent::TargetFromView()
{
	TArray<TPair<AActor*, float>> Candidates;
	GatherCandidates(CrosshairTargetAngle, Candidates);

	AActor* Best = nullptr;
	float BestAngle = TNumericLimits<float>::Max();
	for (const TPair<AActor*, float>& Candidate : Candidates)
	{
		if (Candidate.Value < BestAngle)
		{
			BestAngle = Candidate.Value;
			Best = Candidate.Key;
		}
	}

	// clicking on nothing clears the target, like most MMOs
	SetTarget(Best);
}

bool UMMOCombatComponent::CycleTarget()
{
	TArray<TPair<AActor*, float>> Candidates;
	GatherCandidates(CycleTargetAngle, Candidates);

	if (Candidates.IsEmpty())
	{
		return false;
	}

	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	Candidates.Sort([&OwnerLocation](const TPair<AActor*, float>& A, const TPair<AActor*, float>& B)
	{
		return FVector::DistSquared(OwnerLocation, A.Key->GetActorLocation()) < FVector::DistSquared(OwnerLocation, B.Key->GetActorLocation());
	});

	// pick the one after the current target, wrapping around
	int32 NextIndex = 0;
	const int32 CurrentIndex = Candidates.IndexOfByPredicate([this](const TPair<AActor*, float>& Candidate) { return Candidate.Key == CurrentTarget; });
	if (CurrentIndex != INDEX_NONE)
	{
		NextIndex = (CurrentIndex + 1) % Candidates.Num();
	}

	SetTarget(Candidates[NextIndex].Key);
	return true;
}

EMMOAttackResult UMMOCombatComponent::TryBasicAttack()
{
	AActor* Owner = GetOwner();
	const UMMOHealthComponent* OwnerHealth = Owner ? Owner->FindComponentByClass<UMMOHealthComponent>() : nullptr;
	if (!Owner || (OwnerHealth && OwnerHealth->IsDead()))
	{
		return EMMOAttackResult::AttackerDead;
	}

	// convenience: pressing attack with no target picks the nearest enemy in front of us
	if (!CurrentTarget)
	{
		CycleTarget();
	}

	UMMOHealthComponent* TargetHealth = GetTargetHealth(CurrentTarget);
	if (!CurrentTarget || !TargetHealth)
	{
		return FailAttack(EMMOAttackResult::NoTarget, LOCTEXT("NoTarget", "You have no target."));
	}

	if (TargetHealth->IsDead())
	{
		return FailAttack(EMMOAttackResult::TargetDead, LOCTEXT("TargetDead", "Your target is dead."));
	}

	if (GetBasicAttackCooldownRemaining() > 0.0f)
	{
		return FailAttack(EMMOAttackResult::OnCooldown, LOCTEXT("OnCooldown", "Basic Attack is not ready yet."));
	}

	if (GetEdgeDistance(Owner, CurrentTarget) > BasicAttackRange)
	{
		return FailAttack(EMMOAttackResult::OutOfRange, LOCTEXT("OutOfRange", "Target is out of range."));
	}

	// face the target so the swing reads correctly
	FVector ToTarget = CurrentTarget->GetActorLocation() - Owner->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (!ToTarget.IsNearlyZero())
	{
		Owner->SetActorRotation(ToTarget.Rotation());
	}

	LastAttackTime = GetWorld()->GetTimeSeconds();

	// keep a reference: the damage below may kill the target and trigger callbacks
	AActor* Target = CurrentTarget;
	const float Applied = TargetHealth->ApplyDamage(BasicAttackDamage, Owner);
	OnBasicAttack.Broadcast(Target, Applied);

	return EMMOAttackResult::Success;
}

float UMMOCombatComponent::GetBasicAttackCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, static_cast<float>(LastAttackTime + BasicAttackCooldown - World->GetTimeSeconds()));
}

float UMMOCombatComponent::GetEdgeDistance(const AActor* A, const AActor* B)
{
	if (!A || !B)
	{
		return TNumericLimits<float>::Max();
	}

	auto GetRadius = [](const AActor* Actor)
	{
		if (const ACharacter* Character = Cast<ACharacter>(Actor))
		{
			return Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
		}
		return Actor->GetSimpleCollisionRadius();
	};

	const float CenterDistance = FVector::Dist2D(A->GetActorLocation(), B->GetActorLocation());
	return FMath::Max(0.0f, CenterDistance - GetRadius(A) - GetRadius(B));
}

UMMOHealthComponent* UMMOCombatComponent::GetTargetHealth(const AActor* Target)
{
	const IMMOTargetable* Targetable = Cast<const IMMOTargetable>(Target);
	return Targetable ? Targetable->GetTargetHealth() : nullptr;
}

void UMMOCombatComponent::GatherCandidates(float MaxAngleDegrees, TArray<TPair<AActor*, float>>& OutCandidates) const
{
	OutCandidates.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(MaxAngleDegrees));

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		const IMMOTargetable* Targetable = Cast<IMMOTargetable>(Actor);
		if (!Targetable || Actor == Owner || !Targetable->IsTargetable())
		{
			continue;
		}

		if (FVector::Dist(Owner->GetActorLocation(), Actor->GetActorLocation()) > TargetingRange)
		{
			continue;
		}

		// aim at the body rather than the feet so low creatures are easy to select
		const FVector ToActor = (Targetable->GetNameplateLocation() - ViewLocation).GetSafeNormal();
		const float Dot = FVector::DotProduct(ViewDirection, ToActor);
		if (Dot < MinDot)
		{
			continue;
		}

		// require line of sight
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MMOTargetLOS), false, Owner);
		Params.AddIgnoredActor(Actor);
		if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, Actor->GetActorLocation(), ECC_Visibility, Params))
		{
			continue;
		}

		OutCandidates.Emplace(Actor, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f))));
	}
}

void UMMOCombatComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->GetController())
	{
		Pawn->GetController()->GetPlayerViewPoint(OutLocation, OutRotation);
		return;
	}

	GetOwner()->GetActorEyesViewPoint(OutLocation, OutRotation);
}

EMMOAttackResult UMMOCombatComponent::FailAttack(EMMOAttackResult Result, const FText& Message)
{
	OnCombatError.Broadcast(Message);
	return Result;
}

#undef LOCTEXT_NAMESPACE
