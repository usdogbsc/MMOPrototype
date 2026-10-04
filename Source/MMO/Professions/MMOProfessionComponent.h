// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Professions/MMOProfessionTypes.h"
#include "MMOProfessionComponent.generated.h"

class AMMOCharacter;
class AMMOGatherNode;
class AMMOCraftingStation;
class UMMORecipeDefinition;
class UMMOInventoryComponent;

UENUM(BlueprintType)
enum class EMMOProfessionResult : uint8
{
	Started,
	Busy,
	SkillTooLow,
	MissingIngredients,
	InventoryFull,
	Unavailable,
	Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMMOSkillChangedSignature, EMMOProfession, Profession, int32, NewSkill);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOCraftedSignature, UMMORecipeDefinition*, Recipe);

/**
 *  Gathering and crafting: skill levels per profession, and the timed activity in progress
 *  (gathering a node or crafting a queue of recipes). Moving, taking damage or using an ability interrupts it.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOProfessionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOProfessionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Professions", meta=(ClampMin=0, Units="cm"))
	float MoveTolerance = 30.0f;

	UPROPERTY(BlueprintAssignable, Category="Professions")
	FMMOSkillChangedSignature OnSkillChanged;

	UPROPERTY(BlueprintAssignable, Category="Professions")
	FMMOCraftedSignature OnCrafted;

	/** Fired whenever the activity starts, finishes or is interrupted (UI refresh) */
	FSimpleMulticastDelegate OnActivityChanged;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	int32 GetSkill(EMMOProfession Profession) const;
	void SetSkill(EMMOProfession Profession, int32 Value);
	const TArray<int32>& GetSkills() const { return Skills; }
	void RestoreSkills(const TArray<int32>& InSkills);

	/** Raises a skill for doing something that requires RequiredSkill. Returns the gain */
	int32 TrySkillUp(EMMOProfession Profession, int32 RequiredSkill);

	EMMOProfessionResult StartGather(AMMOGatherNode* Node);

	/** Crafts Recipe Count times in a row (fewer if ingredients run out) */
	EMMOProfessionResult StartCraft(UMMORecipeDefinition* Recipe, int32 Count, AMMOCraftingStation* Station);

	/** Why Recipe can't be crafted once right now (Started if it can) */
	EMMOProfessionResult CanCraft(const UMMORecipeDefinition* Recipe) const;

	bool IsBusy() const { return ActivityDuration > 0.0f; }
	void Interrupt(bool bShowMessage);

	/** Cast bar text and progress while busy */
	bool GetActivity(FText& OutName, float& OutProgress) const;

	/** Crafts left in the current queue, including the one in progress */
	int32 GetCraftsRemaining() const { return CraftRecipe ? CraftsRemaining : 0; }

protected:

	UPROPERTY(VisibleInstanceOnly, Category="Professions")
	TArray<int32> Skills;

	/** Current activity */
	TWeakObjectPtr<AMMOGatherNode> GatherNode;
	TWeakObjectPtr<AMMOCraftingStation> CraftStation;

	UPROPERTY(Transient)
	TObjectPtr<UMMORecipeDefinition> CraftRecipe;

	int32 CraftsRemaining = 0;
	FText ActivityName;
	float ActivityDuration = 0.0f;
	double ActivityStart = 0.0;
	FVector ActivityLocation = FVector::ZeroVector;

	AMMOCharacter* GetCharacter() const;
	UMMOInventoryComponent* GetInventory() const;
	void BeginActivity(const FText& Name, float Duration);
	void EndActivity();
	void FinishGather();
	void FinishCraft();
};
