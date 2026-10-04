// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "MMOCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UAnimSequenceBase;
class UMMOHealthComponent;
class UMMOProgressionComponent;
class UMMOCombatComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AMMOCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Player health */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOHealthComponent> Health;

	/** Level and XP */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOProgressionComponent> Progression;

	/** Targeting and Basic Attack */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOCombatComponent> Combat;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Target under the crosshair. If unset, a runtime action bound to Left Mouse Button is created */
	UPROPERTY(EditAnywhere, Category="Input|Combat")
	TObjectPtr<UInputAction> TargetAction;

	/** Cycle to the next nearby target. If unset, a runtime action bound to Tab is created */
	UPROPERTY(EditAnywhere, Category="Input|Combat")
	TObjectPtr<UInputAction> CycleTargetAction;

	/** Basic Attack. If unset, a runtime action bound to 1 is created */
	UPROPERTY(EditAnywhere, Category="Input|Combat")
	TObjectPtr<UInputAction> BasicAttackAction;

	/** Mapping context for the combat actions. If unset, one is created at runtime with the default keys */
	UPROPERTY(EditAnywhere, Category="Input|Combat")
	TObjectPtr<UInputMappingContext> CombatMappingContext;

	/** Max health gained per level above 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Leveling", meta=(ClampMin=0))
	float MaxHealthPerLevel = 10.0f;

	/** Basic Attack damage gained per level above 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Leveling", meta=(ClampMin=0))
	float AttackDamagePerLevel = 2.0f;

	/** Seconds after the last hit taken or dealt before health regenerates */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Regeneration", meta=(ClampMin=0, Units="s"))
	float OutOfCombatDelay = 6.0f;

	/** Health per second regenerated while out of combat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Regeneration", meta=(ClampMin=0))
	float OutOfCombatRegenPerSecond = 4.0f;

	/** Seconds between death and respawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Death", meta=(ClampMin=0, Units="s"))
	float RespawnDelay = 5.0f;

	/** Animation played on Basic Attack (uses the anim blueprint's DefaultSlot) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Presentation")
	TObjectPtr<UAnimSequenceBase> BasicAttackAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Presentation", meta=(ClampMin=0.1))
	float BasicAttackAnimationPlayRate = 1.4f;

	/** Base stats captured at BeginPlay, before level bonuses */
	float BaseMaxHealth = 100.0f;
	float BaseAttackDamage = 12.0f;

	/** Where the player respawns */
	FTransform RespawnTransform;

	/** World time when we last took damage */
	double LastDamageTakenTime = -1000.0;

	/** World time at which we will respawn (valid while dead) */
	double RespawnTime = 0.0;

	/** Mesh setup restored after ragdoll */
	FTransform MeshRelativeTransform;
	FName MeshCollisionProfile;

	FTimerHandle RegenTimer;
	FTimerHandle RespawnTimer;

public:

	/** Constructor */
	AMMOCharacter();	

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Creates default combat input actions/mapping for any that were not assigned in the Blueprint */
	void CreateDefaultCombatInput();

	/** Applies level-based stat bonuses */
	void ApplyLevelStats(int32 Level, bool bFillHealth);

	UFUNCTION()
	void HandleLevelUp(int32 NewLevel);

	UFUNCTION()
	void HandleDamaged(float Amount, AActor* DamageInstigator);

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	UFUNCTION()
	void HandleBasicAttack(AActor* Target, float Damage);

	void TickRegeneration();

	void RespawnPlayer();

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Targets the enemy under the crosshair */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoTarget();

	/** Cycles to the next nearby enemy */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoCycleTarget();

	/** Uses Basic Attack on the current target */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoBasicAttack();

	/** True while dead and waiting to respawn */
	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsDead() const;

	/** Seconds until respawn (0 if alive) */
	UFUNCTION(BlueprintPure, Category="Combat")
	float GetRespawnTimeRemaining() const;

	/** Seconds between death and respawn */
	float GetRespawnDelay() const { return RespawnDelay; }

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	FORCEINLINE UMMOHealthComponent* GetHealth() const { return Health; }

	FORCEINLINE UMMOProgressionComponent* GetProgression() const { return Progression; }

	FORCEINLINE UMMOCombatComponent* GetCombat() const { return Combat; }
};

