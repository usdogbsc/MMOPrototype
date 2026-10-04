// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Types/SlateEnums.h"

class UWidgetTree;
class UTextBlock;
class UProgressBar;
class UBorder;

/** Shared look for the prototype MMO HUD widgets built in C++ */
namespace MMOUI
{
	namespace Colors
	{
		extern const FLinearColor Panel;
		extern const FLinearColor PanelOutline;
		extern const FLinearColor BarBack;
		extern const FLinearColor PlayerHealth;
		extern const FLinearColor EnemyHealth;
		extern const FLinearColor HealthChip;
		extern const FLinearColor XP;
		extern const FLinearColor Hostile;
		extern const FLinearColor Gold;
		extern const FLinearColor Dead;
		extern const FLinearColor Error;
		extern const FLinearColor TextDim;
	}

	FSlateFontInfo Font(int32 Size, bool bBold = false);

	FSlateBrush RoundedBrush(const FLinearColor& Color, float Radius, const FLinearColor& OutlineColor = FLinearColor::Transparent, float OutlineWidth = 0.0f);

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = false, ETextJustify::Type Justify = ETextJustify::Left);

	UProgressBar* MakeBar(UWidgetTree* Tree, const FLinearColor& FillColor, const FLinearColor& BackgroundColor, float Radius = 3.0f);

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color, float Radius, const FMargin& Padding, const FLinearColor& OutlineColor = FLinearColor::Transparent, float OutlineWidth = 0.0f);
}
