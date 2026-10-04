// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MMOCombatComponent.h"
#include "Animation/MMOAnimNotify_MeleeHit.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOTargetable.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MMOCombat"

UMMOCombatComponent::UMMOCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SwingAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/MMO/Animations/A_MMO_PlayerBasicAttack.A_MMO_PlayerBasicAttack")));
}

void UMMOCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	LoadedSwingAnimation = SwingAnimation.LoadSynchronous();
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

	if (!bAutoAttackActive)
	{
		return;
	}

	const UMMOHealthComponent* OwnerHealth = GetOwner()->FindComponentByClass<UMMOHealthComponent>();
	const UMMOHealthComponent* TargetHealth = GetTargetHealth(CurrentTarget);
	if ((OwnerHealth && OwnerHealth->IsDead()) || !TargetHealth || TargetHealth->IsDead())
	{
		// target died (or we did): auto-attack ends, target stays selected per the targeting rules above
		StopAutoAttack();
		return;
	}

	if (bSwingPending || GetBasicAttackCooldownRemaining() > 0.0f)
	{
		return;
	}

	if (IsTargetInRange())
	{
		BeginSwing();
	}
	else
	{
		// stay active and resume automatically once back in range
		WarnOutOfRange();
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
	else
	{
		StopAutoAttack();
	}

	OnTargetChanged.Broadcast(CurrentTarget);
}

void UMMOCombatComponent::TargetFromView()
{
	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);

	// first: whatever creature capsule is directly under the crosshair
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MMOTargetTrace), false, GetOwner());
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(Hits, ViewLocation, ViewLocation + ViewRotation.Vector() * (TargetingRange + 500.0f),
		FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(30.0f), Params);

	for (const FHitResult& Hit : Hits)
	{
		const IMMOTargetable* Targetable = Cast<IMMOTargetable>(Hit.GetActor());
		if (Targetable && Targetable->IsTargetable())
		{
			SetTarget(Hit.GetActor());
			return;
		}
	}

	// otherwise: the living target closest to the crosshair within a small cone
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

EMMOAttackResult UMMOCombatComponent::ToggleAutoAttack()
{
	if (bAutoAttackActive)
	{
		StopAutoAttack();
		return EMMOAttackResult::Success;
	}

	return StartAutoAttack();
}

EMMOAttackResult UMMOCombatComponent::StartAutoAttack()
{
	const UMMOHealthComponent* OwnerHealth = GetOwner()->FindComponentByClass<UMMOHealthComponent>();
	if (OwnerHealth && OwnerHealth->IsDead())
	{
		return EMMOAttackResult::AttackerDead;
	}

	// convenience: with no target, pick the nearest enemy in front of us
	if (!CurrentTarget)
	{
		CycleTarget();
	}

	const UMMOHealthComponent* TargetHealth = GetTargetHealth(CurrentTarget);
	if (!CurrentTarget || !TargetHealth)
	{
		return Fail(EMMOAttackResult::NoTarget, LOCTEXT("NoTarget", "You have no target."));
	}

	if (TargetHealth->IsDead())
	{
		return Fail(EMMOAttackResult::TargetDead, LOCTEXT("TargetDead", "Your target is dead."));
	}

	if (!bAutoAttackActive)
	{
		bAutoAttackActive = true;
		OnAutoAttackChanged.Broadcast(true);
	}

	if (!IsTargetInRange())
	{
		LastOutOfRangeMessageTime = -1000.0;
		WarnOutOfRange();
		return EMMOAttackResult::OutOfRange;
	}

	return EMMOAttackResult::Success;
}

void UMMOCombatComponent::StopAutoAttack()
{
	if (bAutoAttackActive)
	{
		bAutoAttackActive = false;
		OnAutoAttackChanged.Broadcast(false);
	}
}

void UMMOCombatComponent::BeginSwing()
{
	AActor* Owner = GetOwner();
	LastSwingTime = GetWorld()->GetTimeSeconds();
	bSwingPending = true;
	PendingSwingTarget = CurrentTarget;

	// face the target when standing still. While moving, the player keeps full control of facing
	if (Owner->GetVelocity().Size2D() < 10.0f)
	{
		FVector ToTarget = CurrentTarget->GetActorLocation() - Owner->GetActorLocation();
		ToTarget.Z = 0.0f;
		if (!ToTarget.IsNearlyZero())
		{
			Owner->SetActorRotation(ToTarget.Rotation());
		}
	}

	// play the swing; its MMO Melee Hit notify will call NotifyMeleeHitFrame
	float HitTime = -1.0f;
	const ACharacter* Character = Cast<ACharacter>(Owner);
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && LoadedSwingAnimation)
	{
		if (AnimInstance->PlaySlotAnimationAsDynamicMontage(LoadedSwingAnimation, SwingSlotName, 0.05f, 0.2f, SwingPlayRate))
		{
			HitTime = UMMOAnimNotify_MeleeHit::FindHitTime(LoadedSwingAnimation);
		}
	}

	// safety net: if the notify never fires (missing notify, interrupted montage), resolve anyway
	const float ResolveDelay = HitTime >= 0.0f ? HitTime / SwingPlayRate + 0.25f : FallbackHitDelay;
	bLastHitFromNotify = false;
	GetWorld()->GetTimerManager().SetTimer(SwingResolveTimer, this, &UMMOCombatComponent::ResolveSwing, FMath::Max(0.01f, ResolveDelay), false);

	OnSwingStarted.Broadcast(CurrentTarget);
}

void UMMOCombatComponent::PlayAbilityAnimation(float PlayRate)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && LoadedSwingAnimation && !bSwingPending)
	{
		AnimInstance->PlaySlotAnimationAsDynamicMontage(LoadedSwingAnimation, SwingSlotName, 0.05f, 0.2f, PlayRate);
	}
}

void UMMOCombatComponent::NotifyMeleeHitFrame()
{
	if (bSwingPending)
	{
		bLastHitFromNotify = true;
		ResolveSwing();
	}
}

void UMMOCombatComponent::ResolveSwing()
{
	if (!bSwingPending)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(SwingResolveTimer);
	bSwingPending = false;

	AActor* Target = PendingSwingTarget.Get();
	PendingSwingTarget.Reset();

	UMMOHealthComponent* TargetHealth = GetTargetHealth(Target);
	if (!TargetHealth || TargetHealth->IsDead())
	{
		return;
	}

	// re-validate reach at the moment of impact
	if (GetEdgeDistance(GetOwner(), Target) > BasicAttackRange + HitRangeTolerance)
	{
		WarnOutOfRange();
		return;
	}

	const float Applied = TargetHealth->ApplyDamage(RollDamage(), GetOwner());
	OnBasicAttack.Broadcast(Target, Applied);

	// killing blow: auto-attack ends right away
	if (TargetHealth->IsDead() && Target == CurrentTarget)
	{
		StopAutoAttack();
	}
}

void UMMOCombatComponent::WarnOutOfRange()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastOutOfRangeMessageTime < OutOfRangeMessageInterval)
	{
		return;
	}

	LastOutOfRangeMessageTime = Now;
	++OutOfRangeWarningCount;
	OnCombatError.Broadcast(LOCTEXT("OutOfRange", "Out of Range"));
}

void UMMOCombatComponent::GetDamageRange(float& OutMin, float& OutMax) const
{
	OutMin = WeaponDamageMin + BonusDamage;
	OutMax = FMath::Max(WeaponDamageMin, WeaponDamageMax) + BonusDamage;
}

float UMMOCombatComponent::RollDamage() const
{
	float Min, Max;
	GetDamageRange(Min, Max);
	return static_cast<float>(FMath::RandRange(FMath::RoundToInt(Min), FMath::RoundToInt(Max)));
}

bool UMMOCombatComponent::IsTargetInRange() const
{
	return CurrentTarget && GetEdgeDistance(GetOwner(), CurrentTarget) <= BasicAttackRange;
}

float UMMOCombatComponent::GetBasicAttackCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, static_cast<float>(LastSwingTime + BasicAttackCooldown - World->GetTimeSeconds()));
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

EMMOAttackResult UMMOCombatComponent::Fail(EMMOAttackResult Result, const FText& Message)
{
	OnCombatError.Broadcast(Message);
	return Result;
}

#undef LOCTEXT_NAMESPACE
