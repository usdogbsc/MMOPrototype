// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOHUDWidget.h"
#include "UI/MMOHotbarSlotWidget.h"
#include "UI/MMOCharacterWindowWidget.h"
#include "UI/MMOInventoryWindowWidget.h"
#include "UI/MMOLootWindowWidget.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Items/MMOLootContainerComponent.h"
#include "World/MMODiscoveryZone.h"
#include "World/MMOExplorationComponent.h"
#include "UI/MMOUIStyle.h"
#include "UI/MMOUnitFrameWidget.h"
#include "MMOCharacter.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Combat/MMOTargetable.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"

namespace MMOHUDLayout
{
	static UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Child, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child);
		CanvasSlot->SetAnchors(Anchors);
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetAutoSize(true);
		return CanvasSlot;
	}

	static UCanvasPanelSlot* Fill(UCanvasPanel* Canvas, UWidget* Child)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child);
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CanvasSlot->SetOffsets(FMargin(0.0f));
		return CanvasSlot;
	}

	static const float LevelUpDuration = 3.5f;
	static const float DamageFlashDuration = 0.35f;
	static const int32 MaxFloatingTexts = 40;
}

void UMMOHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	// only the windows take mouse input; everything else lets clicks through to the game
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UMMOHUDWidget::BuildDefaultLayout()
{
	using namespace MMOUI;
	using namespace MMOHUDLayout;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	// screen flashes sit underneath everything else
	DamageFlash = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DamageFlash"));
	DamageFlash->SetColorAndOpacity(FLinearColor(0.75f, 0.0f, 0.0f, 0.0f));
	Fill(Root, DamageFlash);

	LevelUpFlash = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LevelUpFlash"));
	LevelUpFlash->SetColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.25f, 0.0f));
	Fill(Root, LevelUpFlash);

	FloatingTextLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FloatingTextLayer"));
	Fill(Root, FloatingTextLayer);

	// unit frames, top-left
	const TSubclassOf<UMMOUnitFrameWidget> FrameClass = UnitFrameClass ? UnitFrameClass : TSubclassOf<UMMOUnitFrameWidget>(UMMOUnitFrameWidget::StaticClass());
	PlayerFrame = WidgetTree->ConstructWidget<UMMOUnitFrameWidget>(FrameClass, TEXT("PlayerFrame"));
	Place(Root, PlayerFrame, FAnchors(0.0f, 0.0f), FVector2D::ZeroVector, FVector2D(24.0f, 24.0f));

	TargetFrame = WidgetTree->ConstructWidget<UMMOUnitFrameWidget>(FrameClass, TEXT("TargetFrame"));
	Place(Root, TargetFrame, FAnchors(0.0f, 0.0f), FVector2D::ZeroVector, FVector2D(330.0f, 24.0f));

	UTextBlock* HelpText = MakeText(WidgetTree, TEXT("Left-click: select    Right-click: attack / loot    Hold a mouse button + drag: camera    1: auto attack    F: loot    Tab: next target    B: backpack    C: character    Esc: close / clear"), 11, Colors::TextDim);
	Place(Root, HelpText, FAnchors(0.0f, 0.0f), FVector2D::ZeroVector, FVector2D(26.0f, 122.0f));

	// hotbar, bottom-center
	const TSubclassOf<UMMOHotbarSlotWidget> SlotClass = HotbarSlotClass ? HotbarSlotClass : TSubclassOf<UMMOHotbarSlotWidget>(UMMOHotbarSlotWidget::StaticClass());
	AttackSlot = WidgetTree->ConstructWidget<UMMOHotbarSlotWidget>(SlotClass, TEXT("AttackSlot"));
	Place(Root, AttackSlot, FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -28.0f));


	// error line, upper-center
	ErrorText = MakeText(WidgetTree, TEXT(""), 18, Colors::Error, true, ETextJustify::Center);
	ErrorText->SetRenderOpacity(0.0f);
	Place(Root, ErrorText, FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 170.0f));

	// level-up banner
	UVerticalBox* LevelUpBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LevelUpBanner"));
	UTextBlock* LevelUpTitle = MakeText(WidgetTree, TEXT("LEVEL UP!"), 44, Colors::Gold, true, ETextJustify::Center);
	LevelUpSubtitle = MakeText(WidgetTree, TEXT(""), 20, FLinearColor::White, false, ETextJustify::Center);
	LevelUpBox->AddChildToVerticalBox(LevelUpTitle)->SetHorizontalAlignment(HAlign_Center);
	LevelUpBox->AddChildToVerticalBox(LevelUpSubtitle)->SetHorizontalAlignment(HAlign_Center);
	LevelUpBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	LevelUpBox->SetVisibility(ESlateVisibility::Collapsed);
	LevelUpBanner = LevelUpBox;
	Place(Root, LevelUpBox, FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 215.0f));

	// death overlay
	UBorder* DeathBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeathOverlay"));
	DeathBorder->SetBrushColor(FLinearColor(0.12f, 0.0f, 0.0f, 0.5f));
	DeathBorder->SetHorizontalAlignment(HAlign_Center);
	DeathBorder->SetVerticalAlignment(VAlign_Center);
	UVerticalBox* DeathBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	DeathBox->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("YOU HAVE DIED"), 40, Colors::Error, true, ETextJustify::Center))->SetHorizontalAlignment(HAlign_Center);
	DeathCountdownText = MakeText(WidgetTree, TEXT(""), 20, FLinearColor::White, false, ETextJustify::Center);
	DeathBox->AddChildToVerticalBox(DeathCountdownText)->SetHorizontalAlignment(HAlign_Center);
	DeathBorder->SetContent(DeathBox);
	DeathBorder->SetVisibility(ESlateVisibility::Collapsed);
	DeathOverlay = DeathBorder;
	Fill(Root, DeathBorder);

	// pickup feed and rare-drop banner
	LootFeed = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LootFeed"));
	Place(Root, LootFeed, FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-28.0f, -120.0f));

	RareLootBanner = MakeText(WidgetTree, TEXT(""), 26, MMOItems::GetRarityColor(EMMOItemRarity::Rare), true, ETextJustify::Center);
	RareLootBanner->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	RareLootBanner->SetRenderOpacity(0.0f);
	Place(Root, RareLootBanner, FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 330.0f));

	// zone name / discovery banner
	UVerticalBox* ZoneBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ZoneBanner"));
	ZoneTitleText = MakeText(WidgetTree, TEXT(""), 28, FLinearColor::White, true, ETextJustify::Center);
	ZoneSubtitleText = MakeText(WidgetTree, TEXT(""), 14, Colors::TextDim, false, ETextJustify::Center);
	ZoneBox->AddChildToVerticalBox(ZoneTitleText)->SetHorizontalAlignment(HAlign_Center);
	ZoneBox->AddChildToVerticalBox(ZoneSubtitleText)->SetHorizontalAlignment(HAlign_Center);
	ZoneBox->SetRenderOpacity(0.0f);
	Place(Root, ZoneBox, FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 92.0f));

	// everything above is display-only
	for (UWidget* Child : Root->GetAllChildren())
	{
		Child->SetVisibility(Child->GetVisibility() == ESlateVisibility::Collapsed ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// windows (interactive)
	CharacterWindow = WidgetTree->ConstructWidget<UMMOCharacterWindowWidget>(UMMOCharacterWindowWidget::StaticClass(), TEXT("CharacterWindow"));
	Place(Root, CharacterWindow, FAnchors(0.0f, 0.5f), FVector2D(0.0f, 0.5f), FVector2D(24.0f, 40.0f));

	InventoryWindow = WidgetTree->ConstructWidget<UMMOInventoryWindowWidget>(UMMOInventoryWindowWidget::StaticClass(), TEXT("InventoryWindow"));
	Place(Root, InventoryWindow, FAnchors(1.0f, 0.5f), FVector2D(1.0f, 0.5f), FVector2D(-24.0f, 20.0f));

	LootWindow = WidgetTree->ConstructWidget<UMMOLootWindowWidget>(UMMOLootWindowWidget::StaticClass(), TEXT("LootWindow"));
	Place(Root, LootWindow, FAnchors(0.5f, 0.5f), FVector2D(1.0f, 0.5f), FVector2D(-90.0f, 0.0f));

	for (UWidget* Window : { static_cast<UWidget*>(CharacterWindow), static_cast<UWidget*>(InventoryWindow), static_cast<UWidget*>(LootWindow) })
	{
		Window->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMMOHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PlayerFrame)
	{
		PlayerFrame->SetShowXP(true);
		PlayerFrame->SetHealthColor(MMOUI::Colors::PlayerHealth);
	}
	if (TargetFrame)
	{
		TargetFrame->SetShowXP(false);
		TargetFrame->SetHealthColor(MMOUI::Colors::EnemyHealth);
		TargetFrame->SetVisibility(ESlateVisibility::Collapsed);
	}

	CombatEventHandle = UMMOHealthComponent::OnAnyCombatEvent.AddUObject(this, &UMMOHUDWidget::HandleAnyCombatEvent);

	if (InventoryWindow)
	{
		InventoryWindow->OnCloseRequested.BindLambda([this]() { CloseWindowFromWidget(InventoryWindow); });
	}
	if (CharacterWindow)
	{
		CharacterWindow->OnCloseRequested.BindLambda([this]() { CloseWindowFromWidget(CharacterWindow); });
	}
	if (LootWindow)
	{
		LootWindow->OnCloseRequested.BindLambda([this]() { CloseWindowFromWidget(LootWindow); });
	}
}

void UMMOHUDWidget::NativeDestruct()
{
	UMMOHealthComponent::OnAnyCombatEvent.Remove(CombatEventHandle);
	UnbindFromCharacter();

	Super::NativeDestruct();
}

void UMMOHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	SyncToOwningPawn();
	AMMOCharacter* Character = BoundCharacter.Get();

	UpdateFrames(Character);
	UpdateFloatingTexts(InDeltaTime);
	UpdateBanners(Character, InDeltaTime);
	UpdateLootFeed(InDeltaTime);
}

void UMMOHUDWidget::SetInventoryOpen(bool bOpen)
{
	if (InventoryWindow)
	{
		if (bOpen)
		{
			InventoryWindow->Init(BoundCharacter.Get());
			InventoryWindow->Refresh();
		}
		InventoryWindow->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMMOHUDWidget::SetCharacterOpen(bool bOpen)
{
	if (CharacterWindow)
	{
		if (bOpen)
		{
			CharacterWindow->Init(BoundCharacter.Get());
			CharacterWindow->Refresh();
		}
		CharacterWindow->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMMOHUDWidget::OpenLoot(UMMOLootContainerComponent* Container)
{
	if (LootWindow && Container)
	{
		LootWindow->Open(BoundCharacter.Get(), Container);
		LootWindow->SetVisibility(ESlateVisibility::Visible);
	}
}

void UMMOHUDWidget::CloseLoot()
{
	if (LootWindow)
	{
		LootWindow->Close();
		LootWindow->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UMMOHUDWidget::IsInventoryOpen() const
{
	return InventoryWindow && InventoryWindow->GetVisibility() == ESlateVisibility::Visible;
}

bool UMMOHUDWidget::IsCharacterOpen() const
{
	return CharacterWindow && CharacterWindow->GetVisibility() == ESlateVisibility::Visible;
}

bool UMMOHUDWidget::IsLootOpen() const
{
	return LootWindow && LootWindow->GetVisibility() == ESlateVisibility::Visible;
}

UMMOLootContainerComponent* UMMOHUDWidget::GetOpenLoot() const
{
	return IsLootOpen() ? LootWindow->GetContainer() : nullptr;
}

void UMMOHUDWidget::CloseWindowFromWidget(UWidget* Window)
{
	if (Window == LootWindow)
	{
		CloseLoot();
	}
	else if (Window)
	{
		Window->SetVisibility(ESlateVisibility::Collapsed);
	}
	OnWindowClosed.ExecuteIfBound();
}

void UMMOHUDWidget::AddLootFeedLine(const FString& Text, const FLinearColor& Color)
{
	if (!LootFeed || !WidgetTree)
	{
		return;
	}

	if (LootFeedEntries.Num() >= 6)
	{
		if (UTextBlock* Oldest = LootFeedEntries[0].Widget.Get())
		{
			Oldest->RemoveFromParent();
		}
		LootFeedEntries.RemoveAt(0);
	}

	UTextBlock* Line = MMOUI::MakeText(WidgetTree, Text, 15, Color, true, ETextJustify::Right);
	LootFeed->AddChildToVerticalBox(Line)->SetHorizontalAlignment(HAlign_Right);

	FLootFeedEntry& Entry = LootFeedEntries.AddDefaulted_GetRef();
	Entry.Widget = Line;
}

void UMMOHUDWidget::UpdateLootFeed(float DeltaSeconds)
{
	for (int32 i = LootFeedEntries.Num() - 1; i >= 0; --i)
	{
		FLootFeedEntry& Entry = LootFeedEntries[i];
		Entry.Age += DeltaSeconds;
		UTextBlock* Line = Entry.Widget.Get();
		if (!Line || Entry.Age > 4.5f)
		{
			if (Line)
			{
				Line->RemoveFromParent();
			}
			LootFeedEntries.RemoveAt(i);
			continue;
		}
		Line->SetRenderOpacity(Entry.Age < 3.5f ? 1.0f : 1.0f - (Entry.Age - 3.5f));
	}

	if (ZoneTitleText && ZoneTitleText->GetParent())
	{
		ZoneBannerTime = FMath::Max(0.0f, ZoneBannerTime - DeltaSeconds);
		ZoneTitleText->GetParent()->SetRenderOpacity(FMath::Min(1.0f, ZoneBannerTime / 0.8f));
	}

	if (RareLootBanner)
	{
		RareBannerTime = FMath::Max(0.0f, RareBannerTime - DeltaSeconds);
		const float Elapsed = 3.0f - RareBannerTime;
		RareLootBanner->SetRenderOpacity(RareBannerTime <= 0.0f ? 0.0f : FMath::Min(1.0f, RareBannerTime / 0.6f));
		RareLootBanner->SetRenderScale(FVector2D(Elapsed < 0.2f ? 1.0f + (0.2f - Elapsed) * 1.5f : 1.0f));
	}
}

void UMMOHUDWidget::HandleItemsReceived(UMMOItemDefinition* Item, int32 Quantity)
{
	if (!Item)
	{
		return;
	}

	const FLinearColor Color = MMOItems::GetRarityColor(Item->Rarity);
	AddLootFeedLine(Quantity > 1 ? FString::Printf(TEXT("+ %s x%d"), *Item->DisplayName.ToString(), Quantity) : FString::Printf(TEXT("+ %s"), *Item->DisplayName.ToString()), Color);

	if (Item->Rarity >= EMMOItemRarity::Rare && RareLootBanner)
	{
		RareLootBanner->SetText(FText::FromString(FString::Printf(TEXT("%s ITEM:  %s"), *MMOItems::GetRarityText(Item->Rarity).ToString().ToUpper(), *Item->DisplayName.ToString())));
		RareLootBanner->SetColorAndOpacity(FSlateColor(Color));
		RareBannerTime = 3.0f;
	}
}

FString UMMOHUDWidget::GetZoneBannerText() const
{
	return ZoneTitleText && ZoneBannerTime > 0.0f ? ZoneTitleText->GetText().ToString() : FString();
}

void UMMOHUDWidget::ShowZoneBanner(const FString& Title, const FString& Subtitle, const FLinearColor& Color, float Duration)
{
	if (!ZoneTitleText || !ZoneSubtitleText)
	{
		return;
	}
	ZoneTitleText->SetText(FText::FromString(Title));
	ZoneTitleText->SetColorAndOpacity(FSlateColor(Color));
	ZoneSubtitleText->SetText(FText::FromString(Subtitle));
	ZoneBannerTime = Duration;
}

void UMMOHUDWidget::HandleZoneChanged(AMMODiscoveryZone* NewZone)
{
	if (NewZone)
	{
		ShowZoneBanner(NewZone->LocationName.ToString(), NewZone->Subtitle.ToString(), FLinearColor(0.95f, 0.93f, 0.85f), 3.0f);
	}
}

void UMMOHUDWidget::HandleLocationDiscovered(AMMODiscoveryZone* Zone, int32 XPAwarded)
{
	if (Zone)
	{
		const FString Subtitle = XPAwarded > 0 ? FString::Printf(TEXT("%s    +%d XP"), *Zone->Subtitle.ToString(), XPAwarded) : Zone->Subtitle.ToString();
		ShowZoneBanner(FString::Printf(TEXT("Discovered: %s"), *Zone->LocationName.ToString()), Subtitle, MMOUI::Colors::Gold, 4.5f);
	}
}

void UMMOHUDWidget::HandleCurrencyReceived(int32 Amount)
{
	AddLootFeedLine(FString::Printf(TEXT("+ %s"), *MMOItems::FormatCurrency(Amount)), MMOUI::Colors::Gold);
}

void UMMOHUDWidget::SyncToOwningPawn()
{
	AMMOCharacter* Character = Cast<AMMOCharacter>(GetOwningPlayerPawn());
	if (Character != BoundCharacter.Get())
	{
		BindToCharacter(Character);
	}
}

void UMMOHUDWidget::BindToCharacter(AMMOCharacter* Character)
{
	UnbindFromCharacter();
	BoundCharacter = Character;

	if (!Character)
	{
		return;
	}

	Character->GetCombat()->OnCombatError.AddDynamic(this, &UMMOHUDWidget::HandleCombatError);
	Character->GetProgression()->OnLevelUp.AddDynamic(this, &UMMOHUDWidget::HandleLevelUp);
	Character->GetProgression()->OnXPChanged.AddDynamic(this, &UMMOHUDWidget::HandleXPChanged);
	Character->GetHealth()->OnDamaged.AddDynamic(this, &UMMOHUDWidget::HandlePlayerDamaged);
	Character->GetInventory()->OnItemsReceived.AddDynamic(this, &UMMOHUDWidget::HandleItemsReceived);
	Character->GetInventory()->OnCurrencyReceived.AddDynamic(this, &UMMOHUDWidget::HandleCurrencyReceived);
	Character->OnPlayerMessage.AddDynamic(this, &UMMOHUDWidget::HandleCombatError);
	Character->GetExploration()->OnZoneChanged.AddDynamic(this, &UMMOHUDWidget::HandleZoneChanged);
	Character->GetExploration()->OnLocationDiscovered.AddDynamic(this, &UMMOHUDWidget::HandleLocationDiscovered);
	if (AMMODiscoveryZone* Zone = Character->GetExploration()->GetCurrentZone())
	{
		HandleZoneChanged(Zone);
	}

	if (InventoryWindow)
	{
		InventoryWindow->Init(Character);
	}
	if (CharacterWindow)
	{
		CharacterWindow->Init(Character);
	}
}

void UMMOHUDWidget::UnbindFromCharacter()
{
	if (AMMOCharacter* Character = BoundCharacter.Get())
	{
		Character->GetCombat()->OnCombatError.RemoveAll(this);
		Character->GetProgression()->OnLevelUp.RemoveAll(this);
		Character->GetProgression()->OnXPChanged.RemoveAll(this);
		Character->GetHealth()->OnDamaged.RemoveAll(this);
		Character->GetInventory()->OnItemsReceived.RemoveAll(this);
		Character->GetInventory()->OnCurrencyReceived.RemoveAll(this);
		Character->OnPlayerMessage.RemoveAll(this);
		Character->GetExploration()->OnZoneChanged.RemoveAll(this);
		Character->GetExploration()->OnLocationDiscovered.RemoveAll(this);
	}
	BoundCharacter.Reset();
}

void UMMOHUDWidget::UpdateFrames(AMMOCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	const UMMOHealthComponent* Health = Character->GetHealth();
	const UMMOProgressionComponent* Progression = Character->GetProgression();
	const UMMOCombatComponent* Combat = Character->GetCombat();

	if (PlayerFrame)
	{
		PlayerFrame->SetUnitName(PlayerDisplayName, FLinearColor::White);
		PlayerFrame->SetLevel(Progression->GetLevel());
		PlayerFrame->SetHealth(Health->GetCurrentHealth(), Health->GetMaxHealth());
		PlayerFrame->SetXP(Progression->GetCurrentXP(), Progression->GetXPToNextLevel(), Progression->IsMaxLevel());
	}

	if (TargetFrame)
	{
		AActor* Target = Combat->GetCurrentTarget();
		const IMMOTargetable* Targetable = Cast<IMMOTargetable>(Target);
		const UMMOHealthComponent* TargetHealth = Targetable ? Targetable->GetTargetHealth() : nullptr;

		if (TargetHealth)
		{
			const bool bDead = TargetHealth->IsDead();
			TargetFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
			TargetFrame->SetUnitName(Targetable->GetTargetDisplayName(), bDead ? MMOUI::Colors::Dead : MMOUI::Colors::Hostile);
			TargetFrame->SetLevel(Targetable->GetTargetLevel());
			TargetFrame->SetHealth(TargetHealth->GetCurrentHealth(), TargetHealth->GetMaxHealth(), bDead);
			if (Target != LastTarget.Get())
			{
				TargetFrame->ResetHealthChip();
			}
		}
		else
		{
			TargetFrame->SetVisibility(ESlateVisibility::Collapsed);
		}
		LastTarget = Target;
	}

	if (AttackSlot)
	{
		const bool bOutOfRange = Combat->GetCurrentTarget() && !Combat->IsTargetInRange();
		AttackSlot->SetSlotState(Combat->IsAutoAttacking(), bOutOfRange, Combat->GetBasicAttackCooldownRemaining(), Combat->BasicAttackCooldown);
	}
}

void UMMOHUDWidget::AddFloatingText(const FVector& WorldLocation, const FString& Text, const FLinearColor& Color, int32 FontSize, float Duration)
{
	if (!FloatingTextLayer || !WidgetTree)
	{
		return;
	}

	if (FloatingTexts.Num() >= MMOHUDLayout::MaxFloatingTexts)
	{
		if (UTextBlock* Oldest = FloatingTexts[0].Widget.Get())
		{
			Oldest->RemoveFromParent();
		}
		FloatingTexts.RemoveAt(0);
	}

	UTextBlock* Block = MMOUI::MakeText(WidgetTree, Text, FontSize, Color, true, ETextJustify::Center);
	Block->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UCanvasPanelSlot* TextSlot = FloatingTextLayer->AddChildToCanvas(Block);
	TextSlot->SetAutoSize(true);
	TextSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	Block->SetVisibility(ESlateVisibility::Hidden);

	FFloatingText& Entry = FloatingTexts.AddDefaulted_GetRef();
	Entry.Widget = Block;
	Entry.WorldLocation = WorldLocation;
	Entry.Duration = Duration;
	Entry.JitterX = FMath::FRandRange(-24.0f, 24.0f);
}

void UMMOHUDWidget::UpdateFloatingTexts(float DeltaSeconds)
{
	APlayerController* PC = GetOwningPlayer();

	for (int32 i = FloatingTexts.Num() - 1; i >= 0; --i)
	{
		FFloatingText& Entry = FloatingTexts[i];
		UTextBlock* Block = Entry.Widget.Get();
		Entry.Age += DeltaSeconds;

		if (!Block || Entry.Age >= Entry.Duration)
		{
			if (Block)
			{
				Block->RemoveFromParent();
			}
			FloatingTexts.RemoveAt(i);
			continue;
		}

		FVector2D ScreenPosition;
		if (!PC || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Entry.WorldLocation, ScreenPosition, false))
		{
			Block->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}

		const float T = Entry.Age / Entry.Duration;
		const float Rise = 70.0f * FMath::InterpEaseOut(0.0f, 1.0f, T, 2.0f);
		const float Pop = T < 0.12f ? 1.0f + (0.12f - T) * 4.0f : 1.0f;

		if (UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(Block->Slot))
		{
			TextSlot->SetPosition(ScreenPosition + FVector2D(Entry.JitterX, -30.0f - Rise));
		}
		Block->SetRenderScale(FVector2D(Pop));
		Block->SetRenderOpacity(T < 0.6f ? 1.0f : 1.0f - (T - 0.6f) / 0.4f);
		Block->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UMMOHUDWidget::UpdateBanners(AMMOCharacter* Character, float DeltaSeconds)
{
	using namespace MMOHUDLayout;

	if (ErrorText)
	{
		ErrorTime = FMath::Max(0.0f, ErrorTime - DeltaSeconds);
		ErrorText->SetRenderOpacity(FMath::Min(1.0f, ErrorTime / 0.4f));
	}

	if (DamageFlash)
	{
		DamageFlashTime = FMath::Max(0.0f, DamageFlashTime - DeltaSeconds);
		DamageFlash->SetColorAndOpacity(FLinearColor(0.75f, 0.0f, 0.0f, 0.14f * (DamageFlashTime / DamageFlashDuration)));
	}

	if (LevelUpTime > 0.0f)
	{
		const float Elapsed = LevelUpDuration - LevelUpTime;
		LevelUpTime = FMath::Max(0.0f, LevelUpTime - DeltaSeconds);

		if (LevelUpBanner)
		{
			const float Pop = Elapsed < 0.25f ? 1.0f + (0.25f - Elapsed) * 2.0f : 1.0f;
			LevelUpBanner->SetRenderScale(FVector2D(Pop));
			LevelUpBanner->SetRenderOpacity(LevelUpTime < 0.8f ? LevelUpTime / 0.8f : 1.0f);
			LevelUpBanner->SetVisibility(LevelUpTime > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (LevelUpFlash)
		{
			LevelUpFlash->SetColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.25f, Elapsed < 0.6f ? 0.22f * (1.0f - Elapsed / 0.6f) : 0.0f));
		}
	}

	if (DeathOverlay)
	{
		const bool bDead = Character && Character->IsDead();
		DeathOverlay->SetVisibility(bDead ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bDead && DeathCountdownText)
		{
			DeathCountdownText->SetText(FText::FromString(FString::Printf(TEXT("Respawning in %d..."), FMath::CeilToInt(Character->GetRespawnTimeRemaining()))));
		}
	}
}

void UMMOHUDWidget::HandleAnyCombatEvent(const UMMOHealthComponent* Component, EMMOCombatEvent Event, float Amount)
{
	const AActor* EventOwner = Component ? Component->GetOwner() : nullptr;
	if (!EventOwner || EventOwner->GetWorld() != GetWorld())
	{
		return;
	}

	const bool bIsPlayer = EventOwner == GetOwningPlayerPawn();
	const IMMOTargetable* Targetable = Cast<IMMOTargetable>(EventOwner);
	const FVector Location = Targetable ? Targetable->GetNameplateLocation() : EventOwner->GetActorLocation() + FVector(0.0f, 0.0f, 110.0f);

	switch (Event)
	{
	case EMMOCombatEvent::Damage:
		if (bIsPlayer)
		{
			AddFloatingText(Location, FString::Printf(TEXT("-%d"), FMath::RoundToInt(Amount)), FLinearColor(1.0f, 0.25f, 0.2f), 22);
		}
		else
		{
			AddFloatingText(Location, FString::Printf(TEXT("%d"), FMath::RoundToInt(Amount)), FLinearColor(1.0f, 0.93f, 0.45f), 30);
		}
		break;

	case EMMOCombatEvent::Immune:
		AddFloatingText(Location, TEXT("Evade"), FLinearColor(0.8f, 0.8f, 0.8f), 20);
		break;

	case EMMOCombatEvent::Death:
		if (!bIsPlayer)
		{
			AddFloatingText(Location + FVector(0.0f, 0.0f, 30.0f), TEXT("Killed"), MMOUI::Colors::Dead, 20, 1.5f);
		}
		break;

	case EMMOCombatEvent::Heal:
		break;
	}
}

void UMMOHUDWidget::HandleCombatError(const FText& Message)
{
	if (ErrorText)
	{
		ErrorText->SetText(Message);
	}
	ErrorTime = 1.6f;
}

void UMMOHUDWidget::HandleLevelUp(int32 NewLevel)
{
	LevelUpTime = MMOHUDLayout::LevelUpDuration;
	if (LevelUpSubtitle)
	{
		LevelUpSubtitle->SetText(FText::FromString(FString::Printf(TEXT("You have reached level %d"), NewLevel)));
	}
}

void UMMOHUDWidget::HandleXPChanged(int32 CurrentXP, int32 XPToNextLevel, int32 XPGained)
{
	if (XPGained > 0 && GetOwningPlayerPawn())
	{
		AddFloatingText(GetOwningPlayerPawn()->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f),
			FString::Printf(TEXT("+%d XP"), XPGained), FLinearColor(0.78f, 0.55f, 1.0f), 22, 2.0f);
	}
}

void UMMOHUDWidget::HandlePlayerDamaged(float Amount, AActor* DamageInstigator)
{
	DamageFlashTime = MMOHUDLayout::DamageFlashDuration;
}
