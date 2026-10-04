// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOCombatComponent.generated.h"

class IMMOTargetable;
class UMMOHealthComponent;

/** Outcome of a basic attack request */
UENUM(BlueprintType)
enum class EMMOAttackResult : uint8
{
	Success,
	NoTarget,
	TargetDead,
	OutOfRange,
	OnCooldown,
	AttackerDead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOTargetChangedSignature, AActor*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMMOBasicAttackSignature, AActor*, Target, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOCombatErrorSignature, const FText&, Message);

/**
 *  Player-side targeting and the Basic Attack action.
 *  Owns the current combat target and the rules for attacking it (living target, range, cooldown).
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOCombatComponent();

	/** Damage dealt by Basic Attack at level 1 (level bonuses are applied by the owner) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Basic Attack", meta=(ClampMin=0))
	float BasicAttackDamage = 12.0f;

	/** Max gap between the attacker's and target's collision edges, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Basic Attack", meta=(ClampMin=0, Units="cm"))
	float BasicAttackRange = 150.0f;

	/** Seconds between Basic Attacks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Basic Attack", meta=(ClampMin=0.1, Units="s"))
	float BasicAttackCooldown = 1.5f;

	/** Max distance for selecting a target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Targeting", meta=(ClampMin=0, Units="cm"))
	float TargetingRange = 3000.0f;

	/** Half-angle of the cone around the camera's aim used by "target under crosshair" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Targeting", meta=(ClampMin=1, ClampMax=90, Units="deg"))
	float CrosshairTargetAngle = 12.0f;

	/** Half-angle of the cone in front of the camera used by cycle targeting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Targeting", meta=(ClampMin=1, ClampMax=180, Units="deg"))
	float CycleTargetAngle = 80.0f;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FMMOTargetChangedSignature OnTargetChanged;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FMMOBasicAttackSignature OnBasicAttack;

	/** Player-facing error such as "Target is out of range" */
	UPROPERTY(BlueprintAssignable, Category="Combat")
	FMMOCombatErrorSignature OnCombatError;

protected:

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Combat")
	TObjectPtr<AActor> CurrentTarget;

	/** World time of the last Basic Attack */
	double LastAttackTime = -1000.0;

public:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Selects a target. Passing null or an untargetable actor clears the target */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void SetTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category="Combat")
	void ClearTarget() { SetTarget(nullptr); }

	/** Targets whatever is closest to the camera's aim. Clears the target if nothing is aimed at */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void TargetFromView();

	/** Cycles through living targets in front of the camera, nearest first. Returns true if a target was selected */
	UFUNCTION(BlueprintCallable, Category="Combat")
	bool CycleTarget();

	/** Attempts the Basic Attack on the current target. Auto-selects a nearby target if none */
	UFUNCTION(BlueprintCallable, Category="Combat")
	EMMOAttackResult TryBasicAttack();

	UFUNCTION(BlueprintPure, Category="Combat")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	UFUNCTION(BlueprintPure, Category="Combat")
	float GetBasicAttackCooldownRemaining() const;

	/** World time of the last attack this component made (used for out-of-combat checks) */
	double GetLastAttackTime() const { return LastAttackTime; }

	/** Gap between two actors' collision edges on the horizontal plane */
	static float GetEdgeDistance(const AActor* A, const AActor* B);

	/** Returns the health component of a targetable actor, or null */
	static UMMOHealthComponent* GetTargetHealth(const AActor* Target);

protected:

	/** Gathers living targetable actors within range, with their angle from the view direction */
	void GatherCandidates(float MaxAngleDegrees, TArray<TPair<AActor*, float>>& OutCandidates) const;

	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	EMMOAttackResult FailAttack(EMMOAttackResult Result, const FText& Message);
};
