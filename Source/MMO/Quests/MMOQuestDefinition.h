// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MMOQuestDefinition.generated.h"

class UMMOItemDefinition;

UENUM(BlueprintType)
enum class EMMOQuestObjectiveType : uint8
{
	/** Kill creatures whose QuestTag matches TargetId */
	Kill,
	/** Have items with ItemId == TargetId in the backpack (removed on turn-in) */
	Collect,
	/** Discover the location whose LocationId == TargetId */
	Discover
};

USTRUCT(BlueprintType)
struct FMMOQuestObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	EMMOQuestObjectiveType Type = EMMOQuestObjectiveType::Kill;

	/** Creature quest tag, item id or location id */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(ClampMin=1))
	int32 Count = 1;

	/** Tracker text, e.g. "Grey Wolves slain" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FText Description;
};

USTRUCT(BlueprintType)
struct FMMOItemReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	TObjectPtr<UMMOItemDefinition> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(ClampMin=1))
	int32 Quantity = 1;
};

/**
 *  Data asset describing one quest: who gives it, what to do, what it pays.
 *  Assets live in /Game/MMO/Quests and are named DA_Quest_<QuestId>.
 */
UCLASS(BlueprintType)
class UMMOQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FText Title;

	/** What the giver says when offering the quest */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(MultiLine=true))
	FText Description;

	/** Short summary shown in the quest log */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(MultiLine=true))
	FText Summary;

	/** What the turn-in NPC says while the quest is still in progress */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(MultiLine=true))
	FText ProgressText;

	/** What the turn-in NPC says when you complete it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(MultiLine=true))
	FText CompletionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(ClampMin=1))
	int32 RecommendedLevel = 1;

	/** NPC ids that offer and accept the quest */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FName GiverId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	FName TurnInId;

	/** Must be turned in before this quest is offered */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	TObjectPtr<UMMOQuestDefinition> Prerequisite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	TArray<FMMOQuestObjective> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Rewards", meta=(ClampMin=0))
	int32 RewardXP = 100;

	/** Copper */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Rewards", meta=(ClampMin=0))
	int32 RewardCurrency = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Rewards")
	TArray<FMMOItemReward> RewardItems;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("MMOQuest"), GetFName()); }

	/** Loaded quest by id (or /Game/MMO/Quests/DA_Quest_<QuestId>) */
	static UMMOQuestDefinition* FindById(FName InQuestId);
};
