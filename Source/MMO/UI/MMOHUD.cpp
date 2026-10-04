// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOHUD.h"
#include "MMOCharacter.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Combat/MMOTargetable.h"
#include "Creatures/MMOCreature.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"

namespace MMOHUDColors
{
	static const FLinearColor Panel(0.02f, 0.02f, 0.03f, 0.65f);
	static const FLinearColor BarBack(0.08f, 0.08f, 0.08f, 0.9f);
	static const FLinearColor PlayerHealth(0.15f, 0.75f, 0.2f);
	static const FLinearColor EnemyHealth(0.8f, 0.12f, 0.1f);
	static const FLinearColor XP(0.55f, 0.25f, 0.85f);
	static const FLinearColor Hostile(1.0f, 0.3f, 0.25f);
	static const FLinearColor Gold(1.0f, 0.8f, 0.2f);
	static const FLinearColor Dead(0.55f, 0.55f, 0.55f);
	static const FLinearColor Error(1.0f, 0.25f, 0.2f);
}

void AMMOHUD::BeginPlay()
{
	Super::BeginPlay();

	CombatEventHandle = UMMOHealthComponent::OnAnyCombatEvent.AddUObject(this, &AMMOHUD::HandleAnyCombatEvent);
}

void AMMOHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UMMOHealthComponent::OnAnyCombatEvent.Remove(CombatEventHandle);
	UnbindFromCharacter();

	Super::EndPlay(EndPlayReason);
}

void AMMOHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	UIScale = Canvas->ClipY / 1080.0f;
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();

	AMMOCharacter* Character = Cast<AMMOCharacter>(GetOwningPawn());
	if (Character != BoundCharacter.Get())
	{
		BindToCharacter(Character);
	}

	DrawNameplates(Character);
	DrawFloatingTexts(DeltaSeconds);

	if (Character)
	{
		DrawPlayerFrame(Character);
		DrawTargetFrame(Character);
		DrawXPBar(Character);
		DrawActionBar(Character);
		DrawCrosshair();
	}

	DrawHelp();
	DrawMessages(Character, DeltaSeconds);
}

void AMMOHUD::BindToCharacter(AMMOCharacter* Character)
{
	UnbindFromCharacter();
	BoundCharacter = Character;

	if (!Character)
	{
		return;
	}

	Character->GetCombat()->OnCombatError.AddDynamic(this, &AMMOHUD::HandleCombatError);
	Character->GetProgression()->OnLevelUp.AddDynamic(this, &AMMOHUD::HandleLevelUp);
	Character->GetProgression()->OnXPChanged.AddDynamic(this, &AMMOHUD::HandleXPChanged);
	Character->GetHealth()->OnDamaged.AddDynamic(this, &AMMOHUD::HandlePlayerDamaged);
}

void AMMOHUD::UnbindFromCharacter()
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

void AMMOHUD::HandleAnyCombatEvent(const UMMOHealthComponent* Component, EMMOCombatEvent Event, float Amount)
{
	const AActor* EventOwner = Component ? Component->GetOwner() : nullptr;
	if (!EventOwner || EventOwner->GetWorld() != GetWorld())
	{
		return;
	}

	const bool bIsPlayer = EventOwner == GetOwningPawn();
	const IMMOTargetable* Targetable = Cast<IMMOTargetable>(EventOwner);
	const FVector Location = Targetable ? Targetable->GetNameplateLocation() : EventOwner->GetActorLocation() + FVector(0.0f, 0.0f, 110.0f);

	switch (Event)
	{
	case EMMOCombatEvent::Damage:
		AddFloatingText(Location, FString::Printf(TEXT("%d"), FMath::RoundToInt(Amount)),
			bIsPlayer ? FLinearColor(1.0f, 0.2f, 0.15f) : FLinearColor(1.0f, 0.95f, 0.4f), bIsPlayer ? 1.1f : 1.5f);
		break;

	case EMMOCombatEvent::Immune:
		AddFloatingText(Location, TEXT("Evade"), FLinearColor(0.8f, 0.8f, 0.8f), 1.1f);
		break;

	case EMMOCombatEvent::Death:
		if (!bIsPlayer)
		{
			AddFloatingText(Location + FVector(0.0f, 0.0f, 30.0f), TEXT("Killed"), MMOHUDColors::Dead, 1.0f, 1.5f);
		}
		break;

	case EMMOCombatEvent::Heal:
		break;
	}
}

void AMMOHUD::HandleCombatError(const FText& Message)
{
	ErrorMessage = Message.ToString();
	ErrorTime = 1.6f;
}

void AMMOHUD::HandleLevelUp(int32 NewLevel)
{
	LevelUpLevel = NewLevel;
	LevelUpTime = 3.5f;
}

void AMMOHUD::HandleXPChanged(int32 CurrentXP, int32 XPToNextLevel, int32 XPGained)
{
	if (XPGained > 0 && GetOwningPawn())
	{
		AddFloatingText(GetOwningPawn()->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f),
			FString::Printf(TEXT("+%d XP"), XPGained), FLinearColor(0.75f, 0.5f, 1.0f), 1.3f, 2.0f);
	}
}

void AMMOHUD::HandlePlayerDamaged(float Amount, AActor* DamageInstigator)
{
	DamageFlashTime = 0.35f;
}

void AMMOHUD::AddFloatingText(const FVector& WorldLocation, const FString& Text, const FLinearColor& Color, float Scale, float Duration)
{
	FFloatingText& Entry = FloatingTexts.AddDefaulted_GetRef();
	Entry.WorldLocation = WorldLocation;
	Entry.Text = Text;
	Entry.Color = Color;
	Entry.Scale = Scale;
	Entry.Duration = Duration;
	Entry.JitterX = FMath::FRandRange(-30.0f, 30.0f);
}

void AMMOHUD::DrawNameplates(AMMOCharacter* Character)
{
	const AActor* Target = Character ? Character->GetCombat()->GetCurrentTarget() : nullptr;
	const FVector ViewOrigin = Character ? Character->GetActorLocation() : FVector::ZeroVector;
	UFont* Font = GEngine->GetSmallFont();

	for (TActorIterator<AMMOCreature> It(GetWorld()); It; ++It)
	{
		AMMOCreature* Creature = *It;
		if (Creature->IsHidden() || FVector::Dist(ViewOrigin, Creature->GetActorLocation()) > NameplateDistance)
		{
			continue;
		}

		FVector2D Screen;
		if (!ProjectToScreen(Creature->GetNameplateLocation(), Screen))
		{
			continue;
		}

		const bool bTargeted = Creature == Target;
		const bool bDead = Creature->IsDead();
		const FLinearColor NameColor = bDead ? MMOHUDColors::Dead : (bTargeted ? MMOHUDColors::Gold : MMOHUDColors::Hostile);

		FString Name = Creature->GetTargetDisplayName().ToString();
		if (bTargeted)
		{
			Name = FString::Printf(TEXT("> %s <"), *Name);
		}

		const float S = UIScale;
		Canvas->SetDrawColor(FColor::White);
		FCanvasTextItem NameItem(FVector2D(Screen.X, Screen.Y - 14.0f * S), FText::FromString(Name), Font, NameColor);
		NameItem.Scale = FVector2D(1.1f * GetTextScale());
		NameItem.bCentreX = true;
		NameItem.bCentreY = true;
		NameItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(NameItem);

		if (!bDead)
		{
			const float W = 90.0f * S;
			const float H = 7.0f * S;
			const float X = Screen.X - W * 0.5f;
			const float Y = Screen.Y;
			DrawRect(FLinearColor::Black, X - 1.0f, Y - 1.0f, W + 2.0f, H + 2.0f);
			DrawRect(MMOHUDColors::BarBack, X, Y, W, H);
			DrawRect(MMOHUDColors::EnemyHealth, X, Y, W * Creature->GetHealth()->GetHealthPercent(), H);
		}
	}
}

void AMMOHUD::DrawFloatingTexts(float DeltaSeconds)
{
	UFont* Font = GEngine->GetLargeFont();

	for (int32 i = FloatingTexts.Num() - 1; i >= 0; --i)
	{
		FFloatingText& Entry = FloatingTexts[i];
		Entry.Age += DeltaSeconds;
		if (Entry.Age >= Entry.Duration)
		{
			FloatingTexts.RemoveAtSwap(i);
			continue;
		}

		FVector2D Screen;
		if (!ProjectToScreen(Entry.WorldLocation, Screen))
		{
			continue;
		}

		const float T = Entry.Age / Entry.Duration;
		const float Rise = T * 70.0f * UIScale;
		const float Alpha = T < 0.6f ? 1.0f : 1.0f - (T - 0.6f) / 0.4f;
		const float Pop = T < 0.1f ? 1.0f + (0.1f - T) * 4.0f : 1.0f;

		FLinearColor Color = Entry.Color;
		Color.A = Alpha;

		FCanvasTextItem Item(FVector2D(Screen.X + Entry.JitterX * UIScale, Screen.Y - 30.0f * UIScale - Rise), FText::FromString(Entry.Text), Font, Color);
		Item.Scale = FVector2D(Entry.Scale * Pop * 0.9f * GetTextScale());
		Item.bCentreX = true;
		Item.bCentreY = true;
		Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, Alpha));
		Canvas->DrawItem(Item);
	}
}

void AMMOHUD::DrawPlayerFrame(AMMOCharacter* Character)
{
	const UMMOHealthComponent* Health = Character->GetHealth();
	const UMMOProgressionComponent* Progression = Character->GetProgression();

	const float X = 24.0f, Y = 24.0f, W = 300.0f, H = 78.0f;
	DrawPanel(X, Y, W, H);

	DrawLabel(PlayerDisplayName.ToString(), X + 12.0f, Y + 8.0f, FLinearColor::White, GEngine->GetMediumFont(), 1.0f);
	DrawLabel(FString::Printf(TEXT("Lv %d"), Progression->GetLevel()), X + W - 60.0f, Y + 8.0f, MMOHUDColors::Gold, GEngine->GetMediumFont(), 1.0f);

	DrawBar(X + 12.0f, Y + 40.0f, W - 24.0f, 26.0f, Health->GetHealthPercent(), MMOHUDColors::PlayerHealth,
		FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Health->GetCurrentHealth()), FMath::RoundToInt(Health->GetMaxHealth())));
}

void AMMOHUD::DrawTargetFrame(AMMOCharacter* Character)
{
	AActor* Target = Character->GetCombat()->GetCurrentTarget();
	const IMMOTargetable* Targetable = Cast<IMMOTargetable>(Target);
	const UMMOHealthComponent* Health = Targetable ? Targetable->GetTargetHealth() : nullptr;
	if (!Targetable || !Health)
	{
		return;
	}

	const float X = 344.0f, Y = 24.0f, W = 300.0f, H = 78.0f;
	DrawPanel(X, Y, W, H);

	const bool bDead = Health->IsDead();
	DrawLabel(Targetable->GetTargetDisplayName().ToString(), X + 12.0f, Y + 8.0f, bDead ? MMOHUDColors::Dead : MMOHUDColors::Hostile, GEngine->GetMediumFont(), 1.0f);
	DrawLabel(FString::Printf(TEXT("Lv %d"), Targetable->GetTargetLevel()), X + W - 60.0f, Y + 8.0f, MMOHUDColors::Gold, GEngine->GetMediumFont(), 1.0f);

	const FString Label = bDead ? TEXT("Dead") : FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Health->GetCurrentHealth()), FMath::RoundToInt(Health->GetMaxHealth()));
	DrawBar(X + 12.0f, Y + 40.0f, W - 24.0f, 26.0f, Health->GetHealthPercent(), MMOHUDColors::EnemyHealth, Label);
}

void AMMOHUD::DrawXPBar(AMMOCharacter* Character)
{
	const UMMOProgressionComponent* Progression = Character->GetProgression();
	const float RefWidth = Canvas->ClipX / UIScale;

	const float W = 760.0f, H = 20.0f;
	const float X = (RefWidth - W) * 0.5f;
	const float Y = 1080.0f - 40.0f;

	const int32 Required = Progression->GetXPToNextLevel();
	const float Percent = Progression->IsMaxLevel() ? 1.0f : static_cast<float>(Progression->GetCurrentXP()) / FMath::Max(1, Required);
	const FString Label = Progression->IsMaxLevel()
		? FString::Printf(TEXT("Level %d (max)"), Progression->GetLevel())
		: FString::Printf(TEXT("Level %d   XP %d / %d"), Progression->GetLevel(), Progression->GetCurrentXP(), Required);

	DrawBar(X, Y, W, H, Percent, MMOHUDColors::XP, Label);
}

void AMMOHUD::DrawActionBar(AMMOCharacter* Character)
{
	const UMMOCombatComponent* Combat = Character->GetCombat();
	const float RefWidth = Canvas->ClipX / UIScale;
	const float S = UIScale;

	const float Size = 64.0f;
	const float X = (RefWidth - Size) * 0.5f;
	const float Y = 1080.0f - 120.0f;

	// red tint when the current target is out of range, like most MMO action bars
	const AActor* Target = Combat->GetCurrentTarget();
	const bool bOutOfRange = Target && UMMOCombatComponent::GetEdgeDistance(Character, Target) > Combat->BasicAttackRange;

	DrawRect(FLinearColor::Black, (X - 2.0f) * S, (Y - 2.0f) * S, (Size + 4.0f) * S, (Size + 4.0f) * S);
	DrawRect(bOutOfRange ? FLinearColor(0.45f, 0.08f, 0.06f, 0.95f) : FLinearColor(0.25f, 0.2f, 0.15f, 0.95f), X * S, Y * S, Size * S, Size * S);

	const float Cooldown = Combat->GetBasicAttackCooldownRemaining();
	if (Cooldown > 0.0f)
	{
		const float Fraction = Cooldown / FMath::Max(0.01f, Combat->BasicAttackCooldown);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f), X * S, Y * S, Size * S, Size * Fraction * S);
		DrawLabel(FString::Printf(TEXT("%.1f"), Cooldown), X + Size * 0.5f, Y + Size * 0.5f, FLinearColor::White, GEngine->GetMediumFont(), 1.0f, true, true);
	}
	else
	{
		DrawLabel(TEXT("Attack"), X + Size * 0.5f, Y + Size * 0.5f, FLinearColor::White, GEngine->GetSmallFont(), 1.0f, true, true);
	}

	DrawLabel(TEXT("1"), X + 5.0f, Y + 3.0f, MMOHUDColors::Gold, GEngine->GetSmallFont(), 1.0f);
}

void AMMOHUD::DrawMessages(AMMOCharacter* Character, float DeltaSeconds)
{
	const float RefWidth = Canvas->ClipX / UIScale;
	const float CenterX = RefWidth * 0.5f;

	// damage flash
	if (DamageFlashTime > 0.0f)
	{
		DamageFlashTime = FMath::Max(0.0f, DamageFlashTime - DeltaSeconds);
		DrawRect(FLinearColor(0.8f, 0.0f, 0.0f, 0.1f * (DamageFlashTime / 0.35f)), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	}

	// death overlay
	if (Character && Character->IsDead())
	{
		DrawRect(FLinearColor(0.15f, 0.0f, 0.0f, 0.45f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
		DrawLabel(TEXT("YOU HAVE DIED"), CenterX, 380.0f, MMOHUDColors::Error, GEngine->GetLargeFont(), 2.0f, true, true);
		DrawLabel(FString::Printf(TEXT("Respawning in %d..."), FMath::CeilToInt(Character->GetRespawnTimeRemaining())),
			CenterX, 440.0f, FLinearColor::White, GEngine->GetMediumFont(), 1.2f, true, true);
	}

	// level up banner with a gold screen flash
	if (LevelUpTime > 0.0f)
	{
		const float Elapsed = 3.5f - LevelUpTime;
		LevelUpTime = FMath::Max(0.0f, LevelUpTime - DeltaSeconds);

		if (Elapsed < 0.6f)
		{
			DrawRect(FLinearColor(1.0f, 0.8f, 0.2f, 0.25f * (1.0f - Elapsed / 0.6f)), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
		}

		const float Alpha = LevelUpTime < 0.8f ? LevelUpTime / 0.8f : 1.0f;
		const float Pop = Elapsed < 0.25f ? 1.0f + (0.25f - Elapsed) * 2.4f : 1.0f;

		FLinearColor Gold = MMOHUDColors::Gold;
		Gold.A = Alpha;
		FLinearColor White = FLinearColor::White;
		White.A = Alpha;

		DrawLabel(TEXT("LEVEL UP!"), CenterX, 260.0f, Gold, GEngine->GetLargeFont(), 2.2f * Pop, true, true);
		DrawLabel(FString::Printf(TEXT("You have reached level %d"), LevelUpLevel), CenterX, 315.0f, White, GEngine->GetMediumFont(), 1.3f, true, true);
	}

	// error text
	if (ErrorTime > 0.0f)
	{
		ErrorTime = FMath::Max(0.0f, ErrorTime - DeltaSeconds);
		FLinearColor Color = MMOHUDColors::Error;
		Color.A = FMath::Min(1.0f, ErrorTime / 0.4f);
		DrawLabel(ErrorMessage, CenterX, 200.0f, Color, GEngine->GetMediumFont(), 1.2f, true, true);
	}
}

void AMMOHUD::DrawCrosshair()
{
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float Size = 4.0f * UIScale;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), CX - Size * 0.5f - 1.0f, CY - Size * 0.5f - 1.0f, Size + 2.0f, Size + 2.0f);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), CX - Size * 0.5f, CY - Size * 0.5f, Size, Size);
}

void AMMOHUD::DrawHelp()
{
	DrawLabel(TEXT("LMB: target under crosshair    Tab: next target    1: Basic Attack"),
		24.0f, 112.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), GEngine->GetSmallFont(), 1.0f);
}

void AMMOHUD::DrawPanel(float X, float Y, float W, float H)
{
	const float S = UIScale;
	DrawRect(MMOHUDColors::Panel, X * S, Y * S, W * S, H * S);
}

void AMMOHUD::DrawBar(float X, float Y, float W, float H, float Percent, const FLinearColor& FillColor, const FString& Label)
{
	const float S = UIScale;
	DrawRect(FLinearColor::Black, (X - 2.0f) * S, (Y - 2.0f) * S, (W + 4.0f) * S, (H + 4.0f) * S);
	DrawRect(MMOHUDColors::BarBack, X * S, Y * S, W * S, H * S);
	DrawRect(FillColor, X * S, Y * S, W * FMath::Clamp(Percent, 0.0f, 1.0f) * S, H * S);

	if (!Label.IsEmpty())
	{
		DrawLabel(Label, X + W * 0.5f, Y + H * 0.5f, FLinearColor::White, GEngine->GetSmallFont(), 1.0f, true, true);
	}
}

void AMMOHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCenterX, bool bCenterY)
{
	FCanvasTextItem Item(FVector2D(X * UIScale, Y * UIScale), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale * GetTextScale());
	Item.bCentreX = bCenterX;
	Item.bCentreY = bCenterY;
	Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, Color.A));
	Canvas->DrawItem(Item);
}

float AMMOHUD::GetTextScale() const
{
	return FMath::Max(UIScale, 0.85f) * 1.15f;
}

bool AMMOHUD::ProjectToScreen(const FVector& WorldLocation, FVector2D& OutScreen) const
{
	if (!PlayerOwner || !PlayerOwner->PlayerCameraManager || !Canvas)
	{
		return false;
	}

	const FVector CameraLocation = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
	if (FVector::DotProduct(WorldLocation - CameraLocation, CameraForward) <= 0.0f)
	{
		return false;
	}

	const FVector Screen = Canvas->Project(WorldLocation);
	OutScreen = FVector2D(Screen.X, Screen.Y);
	return true;
}
