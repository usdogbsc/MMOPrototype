// Copyright Epic Games, Inc. All Rights Reserved.

#include "Professions/MMOProfessionComponent.h"
#include "Professions/MMORecipeDefinition.h"
#include "World/MMOGatherNode.h"
#include "World/MMOCraftingStation.h"
#include "MMOCharacter.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "MMO.h"

#define LOCTEXT_NAMESPACE "MMOProfessions"

UMMOProfessionComponent::UMMOProfessionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	Skills.Init(1, static_cast<int32>(EMMOProfession::Count));
}

AMMOCharacter* UMMOProfessionComponent::GetCharacter() const
{
	return Cast<AMMOCharacter>(GetOwner());
}

UMMOInventoryComponent* UMMOProfessionComponent::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UMMOInventoryComponent>() : nullptr;
}

int32 UMMOProfessionComponent::GetSkill(EMMOProfession Profession) const
{
	const int32 Index = static_cast<int32>(Profession);
	return Skills.IsValidIndex(Index) ? Skills[Index] : 0;
}

void UMMOProfessionComponent::SetSkill(EMMOProfession Profession, int32 Value)
{
	const int32 Index = static_cast<int32>(Profession);
	if (Skills.IsValidIndex(Index))
	{
		Skills[Index] = FMath::Clamp(Value, 1, MMOProfessions::MaxSkill);
		OnSkillChanged.Broadcast(Profession, Skills[Index]);
	}
}

void UMMOProfessionComponent::RestoreSkills(const TArray<int32>& InSkills)
{
	Skills.Init(1, static_cast<int32>(EMMOProfession::Count));
	for (int32 Index = 0; Index < Skills.Num() && Index < InSkills.Num(); ++Index)
	{
		Skills[Index] = FMath::Clamp(InSkills[Index], 1, MMOProfessions::MaxSkill);
	}
	OnActivityChanged.Broadcast();
}

int32 UMMOProfessionComponent::TrySkillUp(EMMOProfession Profession, int32 RequiredSkill)
{
	const int32 Gain = MMOProfessions::GetSkillGain(GetSkill(Profession), RequiredSkill);
	if (Gain > 0)
	{
		SetSkill(Profession, GetSkill(Profession) + Gain);
		if (AMMOCharacter* Character = GetCharacter())
		{
			Character->ShowWorldText(Character->GetActorLocation() + FVector(0.0f, 0.0f, 140.0f),
				FString::Printf(TEXT("%s %d"), *MMOProfessions::GetName(Profession).ToString(), GetSkill(Profession)), FLinearColor(0.45f, 0.75f, 1.0f));
		}
	}
	return Gain;
}

void UMMOProfessionComponent::BeginActivity(const FText& Name, float Duration)
{
	ActivityName = Name;
	ActivityDuration = FMath::Max(0.05f, Duration);
	ActivityStart = GetWorld()->GetTimeSeconds();
	ActivityLocation = GetOwner()->GetActorLocation();
	OnActivityChanged.Broadcast();
}

void UMMOProfessionComponent::EndActivity()
{
	ActivityDuration = 0.0f;
	GatherNode.Reset();
	CraftRecipe = nullptr;
	CraftStation.Reset();
	CraftsRemaining = 0;
	OnActivityChanged.Broadcast();
}

bool UMMOProfessionComponent::GetActivity(FText& OutName, float& OutProgress) const
{
	if (!IsBusy())
	{
		return false;
	}
	OutName = ActivityName;
	OutProgress = FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - ActivityStart) / ActivityDuration), 0.0f, 1.0f);
	return true;
}

void UMMOProfessionComponent::Interrupt(bool bShowMessage)
{
	if (!IsBusy())
	{
		return;
	}
	EndActivity();
	if (bShowMessage)
	{
		if (AMMOCharacter* Character = GetCharacter())
		{
			Character->ShowPlayerMessage(LOCTEXT("Interrupted", "Interrupted"));
		}
	}
}

EMMOProfessionResult UMMOProfessionComponent::StartGather(AMMOGatherNode* Node)
{
	AMMOCharacter* Character = GetCharacter();
	if (!Character || Character->IsDead())
	{
		return EMMOProfessionResult::Dead;
	}
	if (!Node || Node->IsDepleted())
	{
		return EMMOProfessionResult::Unavailable;
	}
	if (IsBusy() || Character->IsCasting())
	{
		Character->ShowPlayerMessage(LOCTEXT("Busy", "You are already doing something."));
		return EMMOProfessionResult::Busy;
	}
	if (GetSkill(Node->Profession) < Node->RequiredSkill)
	{
		Character->ShowPlayerMessage(FText::Format(LOCTEXT("SkillTooLow", "Requires {0} {1}."), MMOProfessions::GetName(Node->Profession), Node->RequiredSkill));
		return EMMOProfessionResult::SkillTooLow;
	}
	UMMOItemDefinition* Yield = UMMOItemDefinition::FindById(Node->YieldItemId);
	if (!Yield || GetInventory()->GetAddableQuantity(Yield, 1) < 1)
	{
		Character->ShowPlayerMessage(LOCTEXT("BagFull", "Inventory is full."));
		return EMMOProfessionResult::InventoryFull;
	}

	GatherNode = Node;
	Character->FaceActor(Node);
	const FText Verb = Node->Profession == EMMOProfession::Mining ? LOCTEXT("Mining", "Mining") : LOCTEXT("Gathering", "Gathering");
	BeginActivity(FText::Format(LOCTEXT("GatherName", "{0} {1}"), Verb, Node->DisplayName), Node->GatherTime);
	return EMMOProfessionResult::Started;
}

void UMMOProfessionComponent::FinishGather()
{
	AMMOGatherNode* Node = GatherNode.Get();
	UMMOInventoryComponent* Inventory = GetInventory();
	EndActivity();
	if (!Node || Node->IsDepleted() || !Inventory)
	{
		return;
	}

	UMMOItemDefinition* Yield = UMMOItemDefinition::FindById(Node->YieldItemId);
	const int32 Amount = FMath::RandRange(Node->MinYield, FMath::Max(Node->MinYield, Node->MaxYield));
	const int32 Added = Yield ? Inventory->AddItem(Yield, Amount, true) : 0;
	if (Added <= 0)
	{
		if (AMMOCharacter* Character = GetCharacter())
		{
			Character->ShowPlayerMessage(LOCTEXT("BagFull", "Inventory is full."));
		}
		return;
	}
	if (UMMOItemDefinition* Bonus = UMMOItemDefinition::FindById(Node->BonusItemId))
	{
		if (FMath::FRand() < Node->BonusChance)
		{
			Inventory->AddItem(Bonus, 1, true);
		}
	}

	TrySkillUp(Node->Profession, Node->RequiredSkill);
	Node->Deplete();
	UE_LOG(LogMMO, Log, TEXT("Gathered %d x %s from %s"), Added, *Yield->DisplayName.ToString(), *Node->DisplayName.ToString());
}

EMMOProfessionResult UMMOProfessionComponent::CanCraft(const UMMORecipeDefinition* Recipe) const
{
	const AMMOCharacter* Character = GetCharacter();
	if (!Character || Character->IsDead())
	{
		return EMMOProfessionResult::Dead;
	}
	if (!Recipe || !Recipe->Output)
	{
		return EMMOProfessionResult::Unavailable;
	}
	if (GetSkill(Recipe->Profession) < Recipe->RequiredSkill)
	{
		return EMMOProfessionResult::SkillTooLow;
	}
	if (Recipe->GetMaxCraftable(GetInventory()) < 1)
	{
		return EMMOProfessionResult::MissingIngredients;
	}
	// the result must fit (ingredients used up may free a slot, but keep the rule simple and safe)
	if (GetInventory()->GetAddableQuantity(Recipe->Output, Recipe->OutputQuantity) < Recipe->OutputQuantity)
	{
		return EMMOProfessionResult::InventoryFull;
	}
	return EMMOProfessionResult::Started;
}

EMMOProfessionResult UMMOProfessionComponent::StartCraft(UMMORecipeDefinition* Recipe, int32 Count, AMMOCraftingStation* Station)
{
	AMMOCharacter* Character = GetCharacter();
	if (!Character)
	{
		return EMMOProfessionResult::Dead;
	}
	if (IsBusy() || Character->IsCasting())
	{
		Character->ShowPlayerMessage(LOCTEXT("Busy", "You are already doing something."));
		return EMMOProfessionResult::Busy;
	}
	if (Station && Station->Profession != Recipe->Profession)
	{
		return EMMOProfessionResult::Unavailable;
	}

	const EMMOProfessionResult Result = CanCraft(Recipe);
	switch (Result)
	{
	case EMMOProfessionResult::SkillTooLow:
		Character->ShowPlayerMessage(FText::Format(LOCTEXT("RecipeSkill", "Requires {0} {1}."), MMOProfessions::GetName(Recipe->Profession), Recipe->RequiredSkill));
		return Result;
	case EMMOProfessionResult::MissingIngredients:
		Character->ShowPlayerMessage(LOCTEXT("Missing", "Missing reagents."));
		return Result;
	case EMMOProfessionResult::InventoryFull:
		Character->ShowPlayerMessage(LOCTEXT("BagFull", "Inventory is full."));
		return Result;
	case EMMOProfessionResult::Started:
		break;
	default:
		return Result;
	}

	CraftRecipe = Recipe;
	CraftStation = Station;
	CraftsRemaining = FMath::Clamp(Count, 1, Recipe->GetMaxCraftable(GetInventory()));
	BeginActivity(Recipe->Output->DisplayName, Recipe->CraftTime);
	return EMMOProfessionResult::Started;
}

void UMMOProfessionComponent::FinishCraft()
{
	UMMORecipeDefinition* Recipe = CraftRecipe;
	UMMOInventoryComponent* Inventory = GetInventory();
	if (!Recipe || !Inventory || CanCraft(Recipe) != EMMOProfessionResult::Started)
	{
		EndActivity();
		return;
	}

	for (const FMMOIngredient& Ingredient : Recipe->Ingredients)
	{
		Inventory->RemoveItem(Ingredient.Item, Ingredient.Quantity);
	}
	Inventory->AddItem(Recipe->Output, Recipe->OutputQuantity, true);
	TrySkillUp(Recipe->Profession, Recipe->RequiredSkill);
	OnCrafted.Broadcast(Recipe);
	UE_LOG(LogMMO, Log, TEXT("Crafted %d x %s"), Recipe->OutputQuantity, *Recipe->Output->DisplayName.ToString());

	// keep going through the queue while ingredients last
	--CraftsRemaining;
	if (CraftsRemaining > 0 && CanCraft(Recipe) == EMMOProfessionResult::Started)
	{
		AMMOCraftingStation* Station = CraftStation.Get();
		const int32 Left = CraftsRemaining;
		ActivityDuration = 0.0f;
		CraftRecipe = Recipe;
		CraftStation = Station;
		CraftsRemaining = Left;
		BeginActivity(Recipe->Output->DisplayName, Recipe->CraftTime);
		return;
	}
	EndActivity();
}

void UMMOProfessionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AMMOCharacter* Character = GetCharacter();
	if (!IsBusy() || !Character)
	{
		return;
	}

	// gathering and crafting need the player to stand still (and the station to still be in reach)
	const bool bMoved = FVector::Dist2D(Character->GetActorLocation(), ActivityLocation) > MoveTolerance;
	const bool bLeftStation = CraftRecipe && CraftStation.IsValid() && !CraftStation->IsInReach(Character);
	if (Character->IsDead() || bMoved || bLeftStation)
	{
		Interrupt(!Character->IsDead());
		return;
	}

	if (GetWorld()->GetTimeSeconds() - ActivityStart >= ActivityDuration)
	{
		if (GatherNode.IsValid())
		{
			FinishGather();
		}
		else
		{
			FinishCraft();
		}
	}
}

#undef LOCTEXT_NAMESPACE
