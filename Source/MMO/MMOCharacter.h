// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Combat/MMOMeleeAttacker.h"
#include "Combat/MMOCombatComponent.h"
#include "Items/MMOItemTypes.h"
#include "Items/MMOEquipmentComponent.h"
#include "Items/MMOLootContainerComponent.h"
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
class UMMOInventoryComponent;
class UMMOEquipmentComponent;
class UMMOLootContainerComponent;
class UMMOExplorationComponent;
class UStaticMeshComponent;
class AMMOCreature;
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

	/** Backpack */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOInventoryComponent> Inventory;

	/** Worn items */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOEquipmentComponent> Equipment;

	/** Zone discovery and ambience */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOExplorationComponent> Exploration;

	/** Visual for the main-hand item (uses the item's Equipped Mesh, if any) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MainHandMesh;
	
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

	/** Left click: select what is under the cursor (drag to turn the camera). If unset, a runtime action bound to Left Mouse Button is created */
	UPROPERTY(EditAnywhere, Category="Input|Combat")
	TObjectPtr<UInputAction> TargetAction;

	/** Right click: attack an enemy or loot a corpse under the cursor (drag to turn the camera). If unset, a runtime action bound to Right Mouse Button is created */
	UPROPERTY(EditAnywhere, Category="Input|Combat")
	TObjectPtr<UInputAction> RightClickAction;

	/** Mouse movement (in look-axis units) while a button is held before a click counts as a camera drag */
	UPROPERTY(EditAnywhere, Category="Input|Combat", meta=(ClampMin=0))
	float ClickDragThreshold = 4.0f;

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

	/** Loot a nearby corpse. If unset, a runtime action bound to F is created */
	UPROPERTY(EditAnywhere, Category="Input|Items")
	TObjectPtr<UInputAction> InteractAction;

	/** Toggle the backpack. If unset, a runtime action bound to B and I is created */
	UPROPERTY(EditAnywhere, Category="Input|Items")
	TObjectPtr<UInputAction> InventoryAction;

	/** Toggle the character window. If unset, a runtime action bound to C is created */
	UPROPERTY(EditAnywhere, Category="Input|Items")
	TObjectPtr<UInputAction> CharacterAction;

	/** How close the player must be to loot a corpse */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items", meta=(ClampMin=0, Units="cm"))
	float InteractRange = 350.0f;

	/** Socket the main-hand visual attaches to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	FName MainHandSocket = TEXT("weapon_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items|Audio")
	TSoftObjectPtr<USoundBase> LootSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items|Audio")
	TSoftObjectPtr<USoundBase> RareLootSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items|Audio")
	TSoftObjectPtr<USoundBase> CoinSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items|Audio")
	TSoftObjectPtr<USoundBase> EquipSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items|Audio")
	TSoftObjectPtr<USoundBase> ErrorSound;

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

	/** Mouse buttons held for camera dragging, and how far the mouse moved since each was pressed */
	bool bLeftMouseHeld = false;
	bool bRightMouseHeld = false;
	float LeftMouseDrag = 0.0f;
	float RightMouseDrag = 0.0f;

	/** Base max health captured at BeginPlay, before level and gear bonuses */
	float BaseMaxHealth = 100.0f;

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

	/** Rebuilds max health, armor and swing damage from level + equipment */
	void RecalculateStats(bool bFillHealth);

	UFUNCTION()
	void HandleEquipmentChanged();

	UFUNCTION()
	void HandleItemsReceived(UMMOItemDefinition* Item, int32 Quantity);

	UFUNCTION()
	void HandleCurrencyReceived(int32 Amount);

	/** Updates the main-hand visual from the equipped weapon */
	void RefreshEquipmentVisuals();

	/** Finds the corpse the player is trying to loot: under the cursor, the current target, or the nearest one */
	AMMOCreature* FindLootableCorpse(bool& bOutTooFar) const;

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

	/** Mouse look: turns the camera only while a mouse button is held (the cursor is free otherwise) */
	void MouseLook(const FInputActionValue& Value);

	void OnLeftMousePressed();
	void OnLeftMouseReleased();
	void OnRightMousePressed();
	void OnRightMouseReleased();

	/** Shows a hand over lootable corpses and crosshairs over enemies */
	void UpdateHoverCursor();

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

	/** The creature under the mouse cursor, if any */
	AMMOCreature* GetCreatureUnderCursor() const;

	/** Nearest visible creature whose body the ray passes through */
	static AMMOCreature* FindCreatureAlongRay(const UWorld* World, const FVector& Origin, const FVector& Direction, float MaxDistance, const AActor* Ignore);

	/** Right-click behaviour: loot a corpse, or target and auto-attack a living enemy */
	void InteractWith(AMMOCreature* Creature);

	/** Loots the nearest corpse in reach (opens the loot window) */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoInteract();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoToggleInventory();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoToggleCharacter();

	/** Item actions used by the UI and debug tools. Each reports problems through OnPlayerMessage */
	EMMOEquipResult EquipInventorySlot(int32 SlotIndex);
	EMMOEquipResult UnequipSlot(EMMOEquipmentSlot Slot, int32 PreferredInventorySlot = INDEX_NONE);
	bool MoveInventorySlot(int32 FromIndex, int32 ToIndex);
	EMMOLootResult LootItem(UMMOLootContainerComponent* Container, const FGuid& InstanceId);
	EMMOLootResult LootCurrency(UMMOLootContainerComponent* Container);
	EMMOLootResult LootAll(UMMOLootContainerComponent* Container);

	/** True if the container's owner is still a lootable corpse within reach */
	bool CanReachLoot(const UMMOLootContainerComponent* Container) const;

	/** Shows a red message such as "Inventory Full" */
	void ShowPlayerMessage(const FText& Message, bool bPlayErrorSound = true);

	/** Player-facing messages from item actions (shown like combat errors) */
	UPROPERTY(BlueprintAssignable, Category="Items")
	FMMOCombatErrorSignature OnPlayerMessage;

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

	FORCEINLINE UMMOInventoryComponent* GetInventory() const { return Inventory; }

	FORCEINLINE UMMOEquipmentComponent* GetEquipment() const { return Equipment; }

	FORCEINLINE UMMOExplorationComponent* GetExploration() const { return Exploration; }
};

