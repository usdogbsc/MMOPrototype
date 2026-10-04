// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOHUDWidget.h"
#include "UI/MMOHotbarSlotWidget.h"
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

	// the HUD never takes mouse input away from the game
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMMOHUDWidget::BuildDefaultLayout()
{
	using namespace MMOUI;
	using namespace MMOHUDLayout;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
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

	UTextBlock* HelpText = MakeText(WidgetTree, TEXT("LMB / Tab: target    Esc: clear target    1: auto attack    Wheel: zoom"), 11, Colors::TextDim);
	Place(Root, HelpText, FAnchors(0.0f, 0.0f), FVector2D::ZeroVector, FVector2D(26.0f, 122.0f));

	// hotbar, bottom-center
	const TSubclassOf<UMMOHotbarSlotWidget> SlotClass = HotbarSlotClass ? HotbarSlotClass : TSubclassOf<UMMOHotbarSlotWidget>(UMMOHotbarSlotWidget::StaticClass());
	AttackSlot = WidgetTree->ConstructWidget<UMMOHotbarSlotWidget>(SlotClass, TEXT("AttackSlot"));
	Place(Root, AttackSlot, FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -28.0f));

	// crosshair dot
	UImage* Crosshair = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Crosshair"));
	Crosshair->SetBrush(RoundedBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.75f), 3.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), 1.0f));
	UCanvasPanelSlot* CrosshairSlot = Place(Root, Crosshair, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	CrosshairSlot->SetAutoSize(false);
	CrosshairSlot->SetSize(FVector2D(6.0f, 6.0f));

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

	AMMOCharacter* Character = Cast<AMMOCharacter>(GetOwningPlayerPawn());
	if (Character != BoundCharacter.Get())
	{
		BindToCharacter(Character);
	}

	UpdateFrames(Character);
	UpdateFloatingTexts(InDeltaTime);
	UpdateBanners(Character, InDeltaTime);
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
}

void UMMOHUDWidget::UnbindFromCharacter()
{
	if (AMMOCharacter* Character = BoundCharacter.Get())
	{
		Character->GetCombat()->OnCombatError.RemoveAll(this);
		Character->GetProgression()->OnLevelUp.RemoveAll(this);
		Character->GetProgression()->OnXPChanged.RemoveAll(this);
		Character->GetHealth()->OnDamaged.RemoveAll(this);
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
