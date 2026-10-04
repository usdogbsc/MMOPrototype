// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOCombatComponent.generated.h"

class IMMOTargetable;
class UMMOHealthComponent;
class UAnimSequenceBase;

/** Outcome of an attack request */
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOSwingSignature, AActor*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOAutoAttackChangedSignature, bool, bActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOCombatErrorSignature, const FText&, Message);

/**
 *  Player-side targeting and classic MMO auto-attack.
 *  While auto-attack is active, a melee swing starts whenever the swing timer is ready and the target is in range.
 *  Each swing's damage is applied on the attack animation's hit frame (UMMOAnimNotify_MeleeHit),
 *  after re-validating the target and range.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOCombatComponent();

	/** Weapon damage range rolled per swing. Set by the owner from the equipped weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Basic Attack", meta=(ClampMin=0))
	float WeaponDamageMin = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Basic Attack", meta=(ClampMin=0))
	float WeaponDamageMax = 5.0f;

	/** Flat damage added to every swing (level and gear bonuses) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Basic Attack", meta=(ClampMin=0))
	float BonusDamage = 0.0f;

	/** Max gap between the attacker's and target's collision edges to start a swing, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Basic Attack", meta=(ClampMin=0, Units="cm"))
	float BasicAttackRange = 150.0f;

	/** Seconds between swings (the swing timer) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Basic Attack", meta=(ClampMin=0.1, Units="s", DisplayName="Swing Interval"))
	float BasicAttackCooldown = 1.8f;

	/** Extra range allowed at the hit frame, so a target stepping back mid-swing is still hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Basic Attack", meta=(ClampMin=0, Units="cm"))
	float HitRangeTolerance = 60.0f;

	/** Minimum seconds between repeated "Out of Range" messages */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Basic Attack", meta=(ClampMin=0, Units="s"))
	float OutOfRangeMessageInterval = 2.0f;

	/** Swing animation. Its MMO Melee Hit notify times the damage. Root motion should be disabled so the player can keep moving */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Animation")
	TSoftObjectPtr<UAnimSequenceBase> SwingAnimation;

	/** Anim blueprint slot used for the swing. Use an upper-body slot to swing while running */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Animation")
	FName SwingSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Animation", meta=(ClampMin=0.1))
	float SwingPlayRate = 1.0f;

	/** Hit timing used only when the swing animation is missing or has no MMO Melee Hit notify */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Animation", meta=(ClampMin=0, Units="s"))
	float FallbackHitDelay = 0.35f;

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

	/** A swing started (animation begins) */
	UPROPERTY(BlueprintAssignable, Category="Combat")
	FMMOSwingSignature OnSwingStarted;

	/** A swing connected and dealt damage */
	UPROPERTY(BlueprintAssignable, Category="Combat")
	FMMOBasicAttackSignature OnBasicAttack;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FMMOAutoAttackChangedSignature OnAutoAttackChanged;

	/** Player-facing error such as "Out of Range" */
	UPROPERTY(BlueprintAssignable, Category="Combat")
	FMMOCombatErrorSignature OnCombatError;

protected:

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Combat")
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Combat")
	bool bAutoAttackActive = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> LoadedSwingAnimation;

	/** World time the last swing started */
	double LastSwingTime = -1000.0;

	/** True between a swing starting and its hit frame */
	bool bSwingPending = false;

	TWeakObjectPtr<AActor> PendingSwingTarget;

	/** True if the last hit was timed by the animation notify (false = fallback timer) */
	bool bLastHitFromNotify = false;

	double LastOutOfRangeMessageTime = -1000.0;

	/** How many "Out of Range" warnings have been shown (for testing the throttle) */
	int32 OutOfRangeWarningCount = 0;

	FTimerHandle SwingResolveTimer;

public:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Selects a target. Passing null or an untargetable actor clears the target (and stops auto-attack) */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void SetTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category="Combat")
	void ClearTarget() { SetTarget(nullptr); }

	/** Targets whatever the camera is aimed at. Clears the target if nothing is aimed at */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void TargetFromView();

	/** Cycles through living targets in front of the camera, nearest first. Returns true if a target was selected */
	UFUNCTION(BlueprintCallable, Category="Combat")
	bool CycleTarget();

	/** Turns auto-attack on (auto-selecting a nearby target if needed) or off */
	UFUNCTION(BlueprintCallable, Category="Combat")
	EMMOAttackResult ToggleAutoAttack();

	UFUNCTION(BlueprintCallable, Category="Combat")
	EMMOAttackResult StartAutoAttack();

	UFUNCTION(BlueprintCallable, Category="Combat")
	void StopAutoAttack();

	/** Called on the swing animation's hit frame */
	void NotifyMeleeHitFrame();

	UFUNCTION(BlueprintPure, Category="Combat")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsAutoAttacking() const { return bAutoAttackActive; }

	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsSwingPending() const { return bSwingPending; }

	/** True if there is a target and it is within Basic Attack range */
	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsTargetInRange() const;

	/** Seconds until the swing timer is ready */
	UFUNCTION(BlueprintPure, Category="Combat")
	float GetBasicAttackCooldownRemaining() const;

	/** Total per-swing damage range including bonuses */
	UFUNCTION(BlueprintPure, Category="Combat")
	void GetDamageRange(float& OutMin, float& OutMax) const;

	/** Rolls one swing's damage */
	float RollDamage() const;

	/** World time of the last swing (used for out-of-combat checks) */
	double GetLastAttackTime() const { return LastSwingTime; }

	bool WasLastHitFromNotify() const { return bLastHitFromNotify; }

	int32 GetOutOfRangeWarningCount() const { return OutOfRangeWarningCount; }

	/** Gap between two actors' collision edges on the horizontal plane */
	static float GetEdgeDistance(const AActor* A, const AActor* B);

	/** Returns the health component of a targetable actor, or null */
	static UMMOHealthComponent* GetTargetHealth(const AActor* Target);

protected:

	/** Starts a swing at the current target */
	void BeginSwing();

	/** Applies the pending swing's damage if the target is still alive and in reach */
	void ResolveSwing();

	void WarnOutOfRange();

	/** Gathers living targetable actors within range, with their angle from the view direction */
	void GatherCandidates(float MaxAngleDegrees, TArray<TPair<AActor*, float>>& OutCandidates) const;

	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	EMMOAttackResult Fail(EMMOAttackResult Result, const FText& Message);
};
