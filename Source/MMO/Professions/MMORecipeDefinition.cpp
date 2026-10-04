// Copyright Epic Games, Inc. All Rights Reserved.

#include "Professions/MMORecipeDefinition.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "UObject/UObjectIterator.h"

int32 UMMORecipeDefinition::GetMaxCraftable(const UMMOInventoryComponent* Inventory) const
{
	if (!Inventory || Ingredients.Num() == 0)
	{
		return 0;
	}
	int32 Max = TNumericLimits<int32>::Max();
	for (const FMMOIngredient& Ingredient : Ingredients)
	{
		if (!Ingredient.Item || Ingredient.Quantity <= 0)
		{
			return 0;
		}
		Max = FMath::Min(Max, Inventory->CountItem(Ingredient.Item) / Ingredient.Quantity);
	}
	return Max;
}

UMMORecipeDefinition* UMMORecipeDefinition::FindById(FName InRecipeId)
{
	if (InRecipeId.IsNone())
	{
		return nullptr;
	}
	for (TObjectIterator<UMMORecipeDefinition> It; It; ++It)
	{
		if (It->RecipeId == InRecipeId && !It->HasAnyFlags(RF_ClassDefaultObject))
		{
			return *It;
		}
	}
	const FString Name = FString::Printf(TEXT("DA_Recipe_%s"), *InRecipeId.ToString());
	return LoadObject<UMMORecipeDefinition>(nullptr, *FString::Printf(TEXT("/Game/MMO/Recipes/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn);
}
