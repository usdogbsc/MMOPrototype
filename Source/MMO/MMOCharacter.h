// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Combat/MMOMeleeAttacker.h"
#include "MMOCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USoundBase;
class UNiagaraSystem;
class UCameraShakeBase;
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
class AMMOCharacter : public ACharacter, public IMMOMeleeAttacker
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

	/** Toggle auto-attack. If unset, a runtime action bound to 1 is created */
	UPROPERTY(EditAnywhere, Category="Input|Combat")
	TObjectPtr<UInputAction> BasicAttackAction;

	/** Clear the current target. If unset, a runtime action bound to Escape is created */
	UPROPERTY(EditAnywhere, Category="Input|Combat")
	TObjectPtr<UInputAction> ClearTargetAction;

	/** Camera zoom (axis). If unset, a runtime action bound to the mouse wheel is created */
	UPROPERTY(EditAnywhere, Category="Input|Camera")
	TObjectPtr<UInputAction> ZoomAction;

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

	/** Camera distance at spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0, Units="cm"))
	float CameraDefaultDistance = 550.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0, Units="cm"))
	float CameraMinDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0, Units="cm"))
	float CameraMaxDistance = 1100.0f;

	/** Distance change per mouse wheel notch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0, Units="cm"))
	float CameraZoomStep = 75.0f;

	/** How quickly the camera eases to the requested zoom */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0))
	float CameraZoomSpeed = 8.0f;

	/** Raises the camera a little so targets in front of the character stay visible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
	FVector CameraSocketOffset = FVector(0.0f, 0.0f, 55.0f);

	/** Camera follow smoothing (0 disables lag) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin=0))
	float CameraLagSpeed = 14.0f;

	/** Initial downward camera pitch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(Units="deg"))
	float CameraInitialPitch = -15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Audio")
	TSoftObjectPtr<USoundBase> SwingSound;

	/** Played when Basic Attack connects */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Audio")
	TSoftObjectPtr<USoundBase> MeleeImpactSound;

	/** Played when the player takes damage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Audio")
	TSoftObjectPtr<USoundBase> HurtSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Audio")
	TSoftObjectPtr<USoundBase> LevelUpSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Audio")
	TSoftObjectPtr<USoundBase> AutoAttackOnSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Audio")
	TSoftObjectPtr<USoundBase> AutoAttackOffSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Audio")
	TSoftObjectPtr<USoundBase> DeathSound;

	/** Effect spawned on the target when Basic Attack connects */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Effects")
	TSoftObjectPtr<UNiagaraSystem> MeleeImpactEffect;

	/** Small camera shake when Basic Attack connects */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Effects")
	TSoftClassPtr<UCameraShakeBase> MeleeImpactCameraShake;

	/** Camera shake when the player takes damage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Effects")
	TSoftClassPtr<UCameraShakeBase> HurtCameraShake;

	/** Loaded presentation assets */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundBase>> LoadedSounds;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedMeleeImpactEffect;

	UPROPERTY(Transient)
	TSubclassOf<UCameraShakeBase> LoadedMeleeImpactCameraShake;

	UPROPERTY(Transient)
	TSubclassOf<UCameraShakeBase> LoadedHurtCameraShake;

	/** Zoom distance the camera is easing toward */
	float DesiredCameraDistance = 550.0f;

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

	UFUNCTION()
	void HandleSwingStarted(AActor* Target);

	UFUNCTION()
	void HandleAutoAttackChanged(bool bActive);

	void TickRegeneration();

	/** Mouse wheel zoom input */
	void Zoom(const FInputActionValue& Value);

	/** Plays a loaded presentation sound by key (see BeginPlay) */
	void PlayPresentationSound(FName Key, const FVector* Location = nullptr) const;

	void PlayCameraShake(TSubclassOf<UCameraShakeBase> Shake, float Scale) const;

public:

	virtual void Tick(float DeltaSeconds) override;

	//~ Begin IMMOMeleeAttacker
	virtual void NotifyMeleeHitFrame() override;
	//~ End IMMOMeleeAttacker

protected:

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

	/** Toggles auto-attack on the current target */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoBasicAttack();

	/** Clears the current target */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoClearTarget();

	/** Zooms the camera (positive = in) */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoZoom(float Amount);

	/** Distance the camera is easing toward */
	float GetDesiredCameraDistance() const { return DesiredCameraDistance; }

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

