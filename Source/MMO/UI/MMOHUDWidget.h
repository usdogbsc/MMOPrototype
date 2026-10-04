// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/MMOHealthComponent.h"
#include "MMOHUDWidget.generated.h"

class AMMOCharacter;
class UMMOUnitFrameWidget;
class UMMOHotbarSlotWidget;
class UCanvasPanel;
class UTextBlock;
class UImage;
class UWidget;
class UVerticalBox;
class UMMOInventoryWindowWidget;
class UMMOCharacterWindowWidget;
class UMMOLootWindowWidget;
class UMMOLootContainerComponent;
class UMMOItemDefinition;
class AMMODiscoveryZone;
class AMMONPC;
class UMMODialogueWindowWidget;
class UMMOQuestLogWindowWidget;
class UMMOVendorWindowWidget;
class UMMOActionSlotWidget;
class UHorizontalBox;
class UMMOAbilitiesWindowWidget;
class UMMOAbilityDefinition;
class UProgressBar;

/**
 *  Root MMO HUD: player frame, target frame, Basic Attack hotbar slot, floating combat text, banners and screen flashes.
 *  Builds a default layout in C++; a Widget Blueprint child can supply its own layout using the same widget names.
 */
UCLASS()
class UMMOHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Spawns a floating combat text at a world location */
	void AddFloatingText(const FVector& WorldLocation, const FString& Text, const FLinearColor& Color, int32 FontSize = 24, float Duration = 1.2f);

	/** True once the player and target frames have been built/bound */
	bool HasFrames() const { return PlayerFrame && TargetFrame; }

	UMMOUnitFrameWidget* GetTargetFrame() const { return TargetFrame; }

	/** Windows (managed by AMMOHUD, which also switches the mouse cursor on/off) */
	void SetInventoryOpen(bool bOpen);
	void SetCharacterOpen(bool bOpen);
	void OpenLoot(UMMOLootContainerComponent* Container);
	void CloseLoot();

	bool IsInventoryOpen() const;
	bool IsCharacterOpen() const;
	bool IsLootOpen() const;
	UMMOLootContainerComponent* GetOpenLoot() const;

	void OpenDialogue(AMMONPC* NPC);
	void CloseDialogue();
	bool IsDialogueOpen() const;
	AMMONPC* GetDialogueNPC() const;

	void OpenVendor(AMMONPC* Vendor);
	void CloseVendor();
	bool IsVendorOpen() const;
	AMMONPC* GetOpenVendor() const;
	UMMOVendorWindowWidget* GetVendorWindow() const { return VendorWindow; }

	/** Hotbar buttons for keys 2-9 */
	const TArray<TObjectPtr<UMMOActionSlotWidget>>& GetActionSlots() const { return ActionSlots; }

	void SetAbilitiesOpen(bool bOpen);
	bool IsAbilitiesOpen() const;
	UMMOAbilitiesWindowWidget* GetAbilitiesWindow() const { return AbilitiesWindow; }

	/** True while the cast bar is showing */
	bool IsCastBarVisible() const;

	void SetQuestLogOpen(bool bOpen);
	bool IsQuestLogOpen() const;

	UMMODialogueWindowWidget* GetDialogueWindow() const { return DialogueWindow; }
	UMMOQuestLogWindowWidget* GetQuestLogWindow() const { return QuestLogWindow; }

	/** Lines currently shown in the quest tracker (for tests) */
	TArray<FString> GetTrackerLines() const;

	UMMOInventoryWindowWidget* GetInventoryWindow() const { return InventoryWindow; }
	UMMOCharacterWindowWidget* GetCharacterWindow() const { return CharacterWindow; }
	UMMOLootWindowWidget* GetLootWindow() const { return LootWindow; }

	/** Binds to the owning player's character if it changed (also called by AMMOHUD every tick) */
	void SyncToOwningPawn();

	/** Text currently shown in the zone banner (empty when hidden) */
	FString GetZoneBannerText() const;

	/** Fired when a window closes itself (close button, loot emptied or out of reach) */
	FSimpleDelegate OnWindowClosed;

protected:

	/** Unit frame class used by the default layout */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	TSubclassOf<UMMOUnitFrameWidget> UnitFrameClass;

	/** Hotbar slot class used by the default layout */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	TSubclassOf<UMMOHotbarSlotWidget> HotbarSlotClass;

	/** Name shown in the player frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FText PlayerDisplayName = NSLOCTEXT("MMOHUD", "PlayerName", "Adventurer");

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOUnitFrameWidget> PlayerFrame;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOUnitFrameWidget> TargetFrame;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOHotbarSlotWidget> AttackSlot;

	/** Full-screen canvas that floating combat text is placed on */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCanvasPanel> FloatingTextLayer;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ErrorText;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> LevelUpBanner;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelUpSubtitle;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> DeathOverlay;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DeathCountdownText;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> DamageFlash;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> LevelUpFlash;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOInventoryWindowWidget> InventoryWindow;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOCharacterWindowWidget> CharacterWindow;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOLootWindowWidget> LootWindow;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMODialogueWindowWidget> DialogueWindow;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOQuestLogWindowWidget> QuestLogWindow;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOVendorWindowWidget> VendorWindow;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOAbilitiesWindowWidget> AbilitiesWindow;

	/** Cast bar above the hotbar */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> CastBar;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> CastBarFill;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CastBarText;

	/** Row holding Basic Attack and the item/ability buttons */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ActionBarRow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMMOActionSlotWidget>> ActionSlots;

	/** "Eating..." under the player frame */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** Accepted quests and their objectives, right side */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> QuestTracker;

	/** Quest progress / completion notices, upper-center */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> QuestToastText;

	float QuestToastTime = 0.0f;

	/** Recent pickups, bottom-right */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> LootFeed;

	/** Larger notice for Rare and better drops */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RareLootBanner;

	struct FLootFeedEntry
	{
		TWeakObjectPtr<UTextBlock> Widget;
		float Age = 0.0f;
	};

	/** Zone name / "Discovered:" banner, top-center */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ZoneTitleText;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ZoneSubtitleText;

	float ZoneBannerTime = 0.0f;

	TArray<FLootFeedEntry> LootFeedEntries;
	float RareBannerTime = 0.0f;

	struct FFloatingText
	{
		TWeakObjectPtr<UTextBlock> Widget;
		FVector WorldLocation;
		float Age = 0.0f;
		float Duration = 1.2f;
		float JitterX = 0.0f;
	};

	TArray<FFloatingText> FloatingTexts;

	float ErrorTime = 0.0f;
	float LevelUpTime = 0.0f;
	float DamageFlashTime = 0.0f;

	TWeakObjectPtr<AMMOCharacter> BoundCharacter;
	TWeakObjectPtr<AActor> LastTarget;
	FDelegateHandle CombatEventHandle;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildDefaultLayout();

	void BindToCharacter(AMMOCharacter* Character);
	void UnbindFromCharacter();

	void UpdateFrames(AMMOCharacter* Character);
	void UpdateFloatingTexts(float DeltaSeconds);
	void UpdateBanners(AMMOCharacter* Character, float DeltaSeconds);

	void HandleAnyCombatEvent(const UMMOHealthComponent* Component, EMMOCombatEvent Event, float Amount);

	UFUNCTION()
	void HandleCombatError(const FText& Message);

	UFUNCTION()
	void HandleLevelUp(int32 NewLevel);

	UFUNCTION()
	void HandleXPChanged(int32 CurrentXP, int32 XPToNextLevel, int32 XPGained);

	UFUNCTION()
	void HandlePlayerDamaged(float Amount, AActor* DamageInstigator);

	UFUNCTION()
	void HandleItemsReceived(UMMOItemDefinition* Item, int32 Quantity);

	UFUNCTION()
	void HandleCurrencyReceived(int32 Amount);

	UFUNCTION()
	void HandleZoneChanged(AMMODiscoveryZone* NewZone);

	UFUNCTION()
	void HandleLocationDiscovered(AMMODiscoveryZone* Zone, int32 XPAwarded);

	UFUNCTION()
	void HandleAbilityLearned(UMMOAbilityDefinition* Ability);

	void UpdateCastBar(AMMOCharacter* Character);

	UFUNCTION()
	void HandleQuestLogChanged();

	UFUNCTION()
	void HandleQuestMessage(const FText& Message, bool bImportant);

	void RebuildTracker();

	void ShowZoneBanner(const FString& Title, const FString& Subtitle, const FLinearColor& Color, float Duration);

	void AddLootFeedLine(const FString& Text, const FLinearColor& Color);
	void UpdateLootFeed(float DeltaSeconds);
	void CloseWindowFromWidget(UWidget* Window);
};
