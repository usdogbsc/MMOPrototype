// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOQuestLogComponent.generated.h"

class UMMOQuestDefinition;
class UMMOInventoryComponent;
class AMMODiscoveryZone;

UENUM(BlueprintType)
enum class EMMOQuestState : uint8
{
	/** Prerequisites not met */
	Unavailable,
	/** Can be accepted */
	Available,
	/** Accepted, objectives not done */
	Active,
	/** Objectives done, waiting to be turned in */
	ReadyToTurnIn,
	/** Turned in */
	Completed
};

UENUM(BlueprintType)
enum class EMMOQuestResult : uint8
{
	Success,
	NotAvailable,
	LogFull,
	NotReady,
	InventoryFull
};

/** Progress on one accepted quest (kill/discover counts; collect counts are read from the backpack) */
USTRUCT(BlueprintType)
struct FMMOQuestProgress
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
	TObjectPtr<UMMOQuestDefinition> Quest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
	TArray<int32> Counts;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOQuestLogChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMMOQuestMessageSignature, const FText&, Message, bool, bImportant);

/**
 *  The player's quests: accepting, tracking objective progress, turning in for rewards.
 *  Kill progress is reported by creatures (QuestTag), discovery by the exploration component,
 *  collect progress is read live from the backpack.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOQuestLogComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOQuestLogComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quests", meta=(ClampMin=1))
	int32 MaxActiveQuests = 10;

	UPROPERTY(BlueprintAssignable, Category="Quests")
	FMMOQuestLogChangedSignature OnQuestLogChanged;

	/** Short player-facing updates: "Quest accepted", "Grey Wolves slain: 3/5", "Quest complete" */
	UPROPERTY(BlueprintAssignable, Category="Quests")
	FMMOQuestMessageSignature OnQuestMessage;

	virtual void BeginPlay() override;

	EMMOQuestState GetQuestState(const UMMOQuestDefinition* Quest) const;

	EMMOQuestResult AcceptQuest(UMMOQuestDefinition* Quest);
	bool AbandonQuest(const UMMOQuestDefinition* Quest);
	EMMOQuestResult TurnInQuest(UMMOQuestDefinition* Quest);

	/** Progress (clamped to the objective count) */
	int32 GetObjectiveProgress(const UMMOQuestDefinition* Quest, int32 ObjectiveIndex) const;
	bool AreObjectivesComplete(const UMMOQuestDefinition* Quest) const;

	/** Reported by creatures and the exploration component */
	void NotifyKill(FName QuestTag);
	void NotifyDiscovered(FName LocationId);

	const TArray<FMMOQuestProgress>& GetActiveQuests() const { return Active; }
	const TSet<FName>& GetCompletedQuestIds() const { return Completed; }
	bool HasCompleted(FName QuestId) const { return Completed.Contains(QuestId); }

	/** Replaces all quest state (used by the save system and tests) */
	void RestoreState(const TArray<FMMOQuestProgress>& InActive, const TSet<FName>& InCompleted);

	/** Wires up inventory/exploration listeners (called from BeginPlay; public for tests) */
	void BindSources(UMMOInventoryComponent* InInventory);

protected:

	UPROPERTY(VisibleInstanceOnly, Category="Quests")
	TArray<FMMOQuestProgress> Active;

	UPROPERTY(VisibleInstanceOnly, Category="Quests")
	TSet<FName> Completed;

	UPROPERTY(Transient)
	TObjectPtr<UMMOInventoryComponent> Inventory;

	/** Last known collect counts, to announce changes */
	TMap<FName, int32> LastCollectCounts;

	FMMOQuestProgress* FindActive(const UMMOQuestDefinition* Quest);
	const FMMOQuestProgress* FindActive(const UMMOQuestDefinition* Quest) const;

	bool IsDiscovered(FName LocationId) const;

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleLocationDiscovered(AMMODiscoveryZone* Zone, int32 XPAwarded);

	/** Announces "Objective: x/y" and "Quest complete" when progress changes */
	void AnnounceProgress(const UMMOQuestDefinition* Quest, int32 ObjectiveIndex, bool bWasComplete);

	void Message(const FText& Text, bool bImportant = false) { OnQuestMessage.Broadcast(Text, bImportant); }
};
