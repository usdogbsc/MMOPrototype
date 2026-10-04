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
#include "Items/MMOVendor.h"
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
class UMMOQuestLogComponent;
class UMMOCooldownComponent;
class UMMOActionBarComponent;
class UMMOAbilityComponent;
class UMMOAbilityDefinition;
class UMMOProfessionComponent;
class AMMONPC;
class UStaticMeshComponent;
class AMMOCreature;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UENUM()
enum class EMMOUseItemResult : uint8
{
	Success,
	NotUsable,
	NotInBackpack,
	OnCooldown,
	InCombat,
	FullHealth,
	Dead
};

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

	/** Accepted and completed quests */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOQuestLogComponent> QuestLog;

	/** Item (and later ability) cooldowns */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOCooldownComponent> Cooldowns;

	/** Known abilities, casting and their effects */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOAbilityComponent> Abilities;

	/** Gathering / crafting skills and activities */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOProfessionComponent> Professions;

	/** Hotbar keys 2-9 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMOActionBarComponent> ActionBar;

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

	/** Toggle the quest log. If unset, a runtime action bound to L is created */
	UPROPERTY(EditAnywhere, Category="Input|Items")
	TObjectPtr<UInputAction> QuestLogAction;

	/** Toggle the abilities window. If unset, a runtime action bound to K is created */
	UPROPERTY(EditAnywhere, Category="Input|Items")
	TObjectPtr<UInputAction> AbilitiesAction;

	/** Hotbar slot keys 2-9. Created at runtime */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> ActionSlotActions;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items|Audio")
	TSoftObjectPtr<USoundBase> DrinkSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items|Audio")
	TSoftObjectPtr<USoundBase> EatSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Audio")
	TSoftObjectPtr<USoundBase> AbilitySound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Audio")
	TSoftObjectPtr<USoundBase> HealSound;

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

	/** Seconds between autosaves (important events such as quest turn-ins and level ups save within a few seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Save", meta=(ClampMin=5, Units="s"))
	float AutoSaveInterval = 30.0f;

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
	FTimerHandle SaveTimer;

	/** First-session guide progress ("Kill", "Loot", "Backpack", "Abilities") */
	TSet<FName> TutorialFlags;

	/** World time an ability last went off (counts as combat) */
	double LastAbilityTime = -1000.0;

	/** Food heal-over-time in progress */
	float FoodHealPerSecond = 0.0f;
	double FoodEndTime = 0.0;

	/** The merchant whose window is open, and what was sold to them this session */
	TWeakObjectPtr<AMMONPC> ActiveVendor;
	TArray<FMMOItemStack> Buyback;

	/** Saving starts once the saved game (if any) has been loaded, so a failed load never overwrites it */
	bool bSaveReady = false;
	bool bSaveRequested = false;
	double LastSaveTime = 0.0;

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

	/** Loads the local save (next tick after BeginPlay, once the controller is set) */
	void LoadSavedGame();

	/** Saves now if a save was requested or the autosave interval passed */
	void TickAutoSave();

	/** Marks the character as needing a save soon */
	UFUNCTION()
	void RequestSave();

	UFUNCTION()
	void HandleLocationDiscoveredForSave(class AMMODiscoveryZone* Zone, int32 XPAwarded);

	UFUNCTION()
	void HandleLevelUpForSave(int32 NewLevel);

	UFUNCTION()
	void HandleAbilityLearned(UMMOAbilityDefinition* Ability);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	/** The NPC / object (IMMOInteractable) under the mouse cursor, and how far along the ray it is */
	AActor* GetInteractableUnderCursor(float* OutDistance = nullptr) const;

	/** Nearest usable interactable within its interact range */
	AActor* FindNearestInteractable() const;

	/** Uses an IMMOInteractable (talks to an NPC...). Reports "too far" etc. Returns true if it was used */
	bool TryInteract(AActor* Target);

	/** Loots the nearest corpse in reach (opens the loot window) */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoInteract();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoToggleInventory();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoToggleCharacter();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoToggleQuestLog();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoToggleAbilities();

	/** Called by the ability component when an ability goes off: animation, sound, combat state */
	void NotifyAbilityExecuted(UMMOAbilityDefinition* Ability, AActor* Target);

	/** Turns to face Other (abilities) */
	void FaceActor(const AActor* Other);

	/** Floating text at a world location, e.g. "Stunned" */
	void ShowWorldText(const FVector& Location, const FString& Text, const FLinearColor& Color) const;

	/** Guide progress (saved with the character) */
	void MarkTutorial(FName Flag) { TutorialFlags.Add(Flag); }
	bool HasTutorial(FName Flag) const { return TutorialFlags.Contains(Flag); }
	const TSet<FName>& GetTutorialFlags() const { return TutorialFlags; }
	void RestoreTutorialFlags(const TSet<FName>& Flags) { TutorialFlags = Flags; }

	/** True while an ability cast is in progress */
	bool IsCasting() const;

	/** What the cast bar shows: true while casting, gathering or crafting */
	bool GetActiveCast(FText& OutName, float& OutProgress) const;

	/** Uses hotbar slot 0-7 (keys 2-9). Returns true if something happened */
	bool UseActionSlot(int32 Index);

	/** Uses one of the item (potion, food...). Reports problems through OnPlayerMessage */
	EMMOUseItemResult UseItem(UMMOItemDefinition* Item);

	/** Right-click / double-click on a backpack item: sell it (merchant open), use it, or equip it */
	void UseOrEquipInventorySlot(int32 SlotIndex);

	/** True if the player hit or was hit within OutOfCombatDelay */
	bool IsInCombat() const;

	/** Seconds of eating left (0 if not eating) */
	float GetFoodRemaining() const;

	/** Merchant trading. The HUD sets the active vendor when the merchant window opens */
	void SetActiveVendor(AMMONPC* Vendor);
	AMMONPC* GetActiveVendor() const { return ActiveVendor.Get(); }
	bool CanTradeWith(const AMMONPC* Vendor) const;
	EMMOVendorResult BuyFromVendor(int32 EntryIndex, int32 Quantity = 1);
	EMMOVendorResult SellInventorySlot(int32 SlotIndex);
	EMMOVendorResult BuybackItem(int32 BuybackIndex);
	const TArray<FMMOItemStack>& GetBuyback() const { return Buyback; }

	/** Fired after any trade so the merchant window can refresh */
	FSimpleMulticastDelegate OnTradeChanged;

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

	/** Re-derives stats and visuals after a save game was applied, then sets health */
	void RefreshAfterLoad(float SavedHealth);

	/** Writes the local save immediately (if saving is enabled). Returns true if written */
	bool SaveNow();

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

	FORCEINLINE UMMOQuestLogComponent* GetQuestLog() const { return QuestLog; }

	FORCEINLINE UMMOCooldownComponent* GetCooldowns() const { return Cooldowns; }

	FORCEINLINE UMMOActionBarComponent* GetActionBar() const { return ActionBar; }

	FORCEINLINE UMMOAbilityComponent* GetAbilities() const { return Abilities; }

	FORCEINLINE UMMOProfessionComponent* GetProfessions() const { return Professions; }
};

