// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Professions/MMOProfessionTypes.h"
#include "MMORecipeDefinition.generated.h"

class UMMOItemDefinition;
class UMMOInventoryComponent;

USTRUCT(BlueprintType)
struct FMMOIngredient
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe")
	TObjectPtr<UMMOItemDefinition> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe", meta=(ClampMin=1))
	int32 Quantity = 1;
};

/** A crafting recipe (data asset /Game/MMO/Recipes/DA_Recipe_<RecipeId>), made at a station of its profession */
UCLASS(BlueprintType)
class UMMORecipeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe")
	FName RecipeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe")
	EMMOProfession Profession = EMMOProfession::Smithing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe", meta=(ClampMin=0))
	int32 RequiredSkill = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe")
	TObjectPtr<UMMOItemDefinition> Output;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe", meta=(ClampMin=1))
	int32 OutputQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe")
	TArray<FMMOIngredient> Ingredients;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recipe", meta=(ClampMin=0, Units="s"))
	float CraftTime = 2.0f;

	/** How many times the ingredients in Inventory allow this to be crafted */
	int32 GetMaxCraftable(const UMMOInventoryComponent* Inventory) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("MMORecipe"), GetFName()); }

	static UMMORecipeDefinition* FindById(FName InRecipeId);
};
