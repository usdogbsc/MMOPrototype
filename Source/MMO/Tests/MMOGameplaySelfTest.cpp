// Copyright Epic Games, Inc. All Rights Reserved.

// Development-only end-to-end check of the gameplay loop (Milestones 1A, 1B and 2), run inside a live game world:
//   UnrealEditor.exe MMO.uproject -game -ExecCmds="mmo.selftest"
// It drives the real player/wolf objects and logs PASS/FAIL per step.
// Options: "quit" exits when finished, "shots" saves HUD screenshots to Saved/Screenshots (needs rendering).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "MMOCharacter.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Creatures/MMOCreature.h"
#include "Creatures/MMOCreatureAIController.h"
#include "Items/MMOEquipmentComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Items/MMOLootContainerComponent.h"
#include "Items/MMOLootTable.h"
#include "UI/MMOHUD.h"
#include "UI/MMOHUDWidget.h"
#include "UI/MMOItemTooltipWidget.h"
#include "Creatures/MMODireWolf.h"
#include "Creatures/MMOGreyWolf.h"
#include "World/MMODiscoveryZone.h"
#include "World/MMOExplorationComponent.h"
#include "NPC/MMONPC.h"
#include "Save/MMOSaveGame.h"
#include "Combat/MMOCooldownComponent.h"
#include "Combat/MMOAbilityComponent.h"
#include "Professions/MMOProfessionComponent.h"
#include "Professions/MMORecipeDefinition.h"
#include "World/MMOGatherNode.h"
#include "World/MMOPortal.h"
#include "Settings/MMOSettingsSubsystem.h"
#include "UI/MMOMenuWidgets.h"
#include "World/MMOTelegraph.h"
#include "Creatures/MMORustback.h"
#include "Creatures/MMORustQueen.h"
#include "World/MMOCraftingStation.h"
#include "UI/MMOCraftingWindowWidget.h"
#include "Combat/MMOAbilityDefinition.h"
#include "Items/MMOActionBarComponent.h"
#include "UI/MMOVendorWindowWidget.h"
#include "Save/MMOSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Quests/MMOQuestDefinition.h"
#include "Quests/MMOQuestLogComponent.h"
#include "UI/MMONPCPlateWidget.h"
#include "UI/MMOQuestWidgets.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "MMO.h"

namespace MMOSelfTest
{
	struct FState
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AMMOCharacter> Player;
		TWeakObjectPtr<AMMOCreature> Wolf;
		TArray<TWeakObjectPtr<AMMOCreature>> Pack;
		int32 Step = 0;
		double StepStart = 0.0;
		int32 Failures = 0;
		int32 Passes = 0;
		bool bQuitWhenDone = false;
		bool bScreenshots = false;
		FVector SafeOrigin = FVector::ZeroVector;
		float OriginalLeashRange = 0.0f;
		double Mark = 0.0;
		float HealthMark = 0.0f;
		int32 ActionIndex = INDEX_NONE;
		TWeakObjectPtr<AMMOCreature> AbilityTarget;
		TWeakObjectPtr<AMMOGatherNode> Node;
		TWeakObjectPtr<AMMORustQueen> Queen;
		TWeakObjectPtr<AMMOTelegraph> Telegraph;
		int32 Detonations = 0;
		bool bLastDetonationHitPlayer = false;
		FString LastEmote;
		FDelegateHandle DetonationHandle;
		FDelegateHandle EmoteHandle;
		int32 SkillMark = 0;
		TWeakObjectPtr<AMMOCreature> AbilityBystander;
		float OtherMark = 0.0f;
		float OtherHealthMark = 0.0f;
		int32 CounterMark = 0;
		int32 HitCount = 0;
		bool bFlag = false;
		TWeakObjectPtr<AMMOCreature> Dire;
		int32 XPMark = 0;
		int32 LevelMark = 0;
		double DeathMark = 0.0;
		float MaxWanderDistance = 0.0f;
		bool bHasZones = false;
		float SavedLootChance = 1.0f;
		float LastPlayerDamage = 0.0f;
		float MinTargetHit = TNumericLimits<float>::Max();
		int32 FillerAdded = 0;
		int32 ItemsBeforeDeath = 0;
		int32 CurrencyMark = 0;
		TWeakObjectPtr<UUserWidget> TooltipWidget;
		FDelegateHandle CombatEventHandle;
		FTSTicker::FDelegateHandle Ticker;
	};

	static TUniquePtr<FState> State;

	static double Now() { return State->World.IsValid() ? State->World->GetTimeSeconds() : 0.0; }

	static void Check(bool bCondition, const FString& What)
	{
		if (bCondition)
		{
			++State->Passes;
			UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST PASS: %s"), *What);
		}
		else
		{
			++State->Failures;
			UE_LOG(LogMMO, Error, TEXT("MMO SELFTEST FAIL: %s"), *What);
		}
	}

	static void Shot(const TCHAR* Name)
	{
		if (State->bScreenshots)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("MMOSelfTest_%s.png"), Name), true, false);
		}
	}

	static void NextStep()
	{
		++State->Step;
		State->StepStart = Now();
	}

	static float Elapsed() { return static_cast<float>(Now() - State->StepStart); }

	/**
	 *  Places the player Distance away from Location, preferably toward the player start (known floor).
	 *  If level geometry blocks that spot, other directions around Location are tried.
	 */
	static void PlacePlayerNear(const FVector& Location, float Distance)
	{
		AMMOCharacter* Player = State->Player.Get();
		const FVector Preferred = (State->SafeOrigin - Location).GetSafeNormal2D();

		for (int32 Attempt = 0; Attempt < 12; ++Attempt)
		{
			// 0, +30, -30, +60, -60 ... degrees from the preferred direction
			const float Angle = ((Attempt + 1) / 2) * 30.0f * (Attempt % 2 == 0 ? 1.0f : -1.0f);
			const FVector Direction = Preferred.RotateAngleAxis(Angle, FVector::UpVector);
			FVector Destination = Location + Direction * Distance;
			Destination.Z = FMath::Max(Location.Z, State->SafeOrigin.Z) + 30.0f;

			if (Player->TeleportTo(Destination, (-Direction).Rotation()))
			{
				if (AController* Controller = Player->GetController())
				{
					Controller->SetControlRotation(FRotator(-15.0f, (-Direction).Rotation().Yaw, 0.0f));
				}
				UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST: placed player %.0fcm from %s (attempt %d)"), FVector::Dist2D(Player->GetActorLocation(), Location), *Location.ToCompactString(), Attempt + 1);
				return;
			}
		}

		UE_LOG(LogMMO, Warning, TEXT("MMO SELFTEST: could not place player %.0fcm from %s"), Distance, *Location.ToCompactString());
	}

	static AMMOCreatureAIController* AIOf(const AMMOCreature* Creature)
	{
		return Creature ? Cast<AMMOCreatureAIController>(Creature->GetController()) : nullptr;
	}

	static EMMOCreatureAIState StateOf(const AMMOCreature* Creature)
	{
		const AMMOCreatureAIController* AI = AIOf(Creature);
		return AI ? AI->GetAIState() : EMMOCreatureAIState::Dead;
	}

	static bool InCombat(const AMMOCreature* Creature)
	{
		const EMMOCreatureAIState S = StateOf(Creature);
		return S == EMMOCreatureAIState::Chasing || S == EMMOCreatureAIState::Attacking;
	}

	static AMMOHUD* HUDOf(const AMMOCharacter* Player)
	{
		const APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
		return PC ? Cast<AMMOHUD>(PC->GetHUD()) : nullptr;
	}

	static bool CursorShown(const AMMOCharacter* Player)
	{
		const APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
		return PC && PC->bShowMouseCursor;
	}

	static int32 FindSlotOf(const UMMOInventoryComponent* Inventory, const UMMOItemDefinition* Item)
	{
		return Inventory->GetSlots().IndexOfByPredicate([Item](const FMMOItemStack& Stack) { return !Stack.IsEmpty() && Stack.Item == Item; });
	}

	/** Total XP earned so far (all levels), so checks stay valid when discoveries also award XP */
	static int32 TotalXP(const UMMOProgressionComponent* Progression)
	{
		int32 Total = Progression->GetCurrentXP();
		for (int32 Level = 1; Level < Progression->GetLevel(); ++Level)
		{
			Total += Progression->GetXPRequiredForLevel(Level);
		}
		return Total;
	}

	static AMMODiscoveryZone* FindZone(UWorld* World, FName Id)
	{
		for (TActorIterator<AMMODiscoveryZone> It(World); It; ++It)
		{
			if (It->LocationId == Id)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static AMMONPC* FindNPC(UWorld* World, FName Id)
	{
		for (TActorIterator<AMMONPC> It(World); It; ++It)
		{
			if (It->NPCId == Id)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static bool TrackerHas(const AMMOCharacter* Player, const FString& Line)
	{
		return HUDOf(Player)->GetHUDWidget()->GetTrackerLines().Contains(Line);
	}

	static EMMONPCMarker PlateMarker(const AMMONPC* NPC)
	{
		const UWidgetComponent* Plate = NPC->FindComponentByClass<UWidgetComponent>();
		const UMMONPCPlateWidget* Widget = Plate ? Cast<UMMONPCPlateWidget>(Plate->GetUserWidgetObject()) : nullptr;
		return Widget ? Widget->GetMarker() : EMMONPCMarker::None;
	}

	static AMMOCreature* FindLivingCreature(UWorld* World, FName QuestTag)
	{
		for (TActorIterator<AMMOCreature> It(World); It; ++It)
		{
			if (It->QuestTag == QuestTag && !It->GetHealth()->IsDead())
			{
				return *It;
			}
		}
		return nullptr;
	}

	static AMMOGatherNode* FindNode(UWorld* World, FName ItemId, const AMMOGatherNode* Except = nullptr)
	{
		for (TActorIterator<AMMOGatherNode> It(World); It; ++It)
		{
			if (It->YieldItemId == ItemId && !It->IsDepleted() && *It != Except)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static AMMOCraftingStation* FindStation(UWorld* World, EMMOProfession Profession)
	{
		for (TActorIterator<AMMOCraftingStation> It(World); It; ++It)
		{
			if (It->Profession == Profession)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static AMMOPortal* FindPortal(UWorld* World, FName Id)
	{
		for (TActorIterator<AMMOPortal> It(World); It; ++It)
		{
			if (It->PortalId == Id)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static int32 CountOwned(const AMMOCharacter* Player)
	{
		return Player->GetInventory()->GetTotalItemCount() + Player->GetEquipment()->GetEquippedCount();
	}

	static void Finish()
	{
		UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST COMPLETE: %d passed, %d failed -> %s"),
			State->Passes, State->Failures, State->Failures == 0 ? TEXT("SUCCESS") : TEXT("FAILURE"));

		UMMOHealthComponent::OnAnyCombatEvent.Remove(State->CombatEventHandle);
		AMMOTelegraph::OnAnyDetonation.Remove(State->DetonationHandle);
		AMMORustQueen::OnBossEmote.Remove(State->EmoteHandle);
		if (IConsoleVariable* LootChance = IConsoleManager::Get().FindConsoleVariable(TEXT("mmo.Loot.ChanceMultiplier")))
		{
			LootChance->Set(State->SavedLootChance);
		}
		FTSTicker::GetCoreTicker().RemoveTicker(State->Ticker);
		const bool bQuit = State->bQuitWhenDone;
		State.Reset();

		if (bQuit)
		{
			FPlatformMisc::RequestExit(false, TEXT("MMOSelfTest"));
		}
	}

	static bool Tick(float DeltaTime)
	{
		if (!State)
		{
			return false;
		}

		AMMOCharacter* Player = State->Player.Get();
		AMMOCreature* Wolf = State->Wolf.Get();
		if (!State->World.IsValid() || !Player || !Wolf)
		{
			Check(false, TEXT("World, player and wolf stay valid"));
			Finish();
			return false;
		}

		UWorld* World = State->World.Get();
		if (State->Pack.Num() > 0 && State->Pack[0].IsValid() && StateOf(State->Pack[0].Get()) == EMMOCreatureAIState::Idle)
		{
			const AMMOCreature* A = State->Pack[0].Get();
			State->MaxWanderDistance = FMath::Max(State->MaxWanderDistance, static_cast<float>(FVector::Dist2D(A->GetActorLocation(), A->GetSpawnTransform().GetLocation())));
		}
		UMMOHealthComponent* PlayerHealth = Player->GetHealth();
		UMMOCombatComponent* Combat = Player->GetCombat();
		UMMOProgressionComponent* Progression = Player->GetProgression();
		UMMOInventoryComponent* Inventory = Player->GetInventory();
		UMMOEquipmentComponent* Equipment = Player->GetEquipment();
		UMMOHealthComponent* WolfHealth = Wolf->GetHealth();
		const FVector WolfHome = Wolf->GetSpawnTransform().GetLocation();

		UMMOItemDefinition* Greyfang = UMMOItemDefinition::FindById(TEXT("Greyfang"));
		UMMOItemDefinition* Boots = UMMOItemDefinition::FindById(TEXT("WornLeatherBoots"));
		UMMOItemDefinition* TrainingSword = UMMOItemDefinition::FindById(TEXT("TrainingSword"));
		UMMOItemDefinition* WolfFang = UMMOItemDefinition::FindById(TEXT("WolfFang"));

		switch (State->Step)
		{
		case 0: // wait for the runtime NavMesh, then check the starting state
		{
			UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			if (NavSys && NavSys->IsNavigationBuildInProgress() && Elapsed() < 20.0f)
			{
				break;
			}

			Check(Progression->GetLevel() == 1, TEXT("Player starts at level 1"));
			if (const UMMOSettingsSubsystem* Options = World->GetGameInstance()->GetSubsystem<UMMOSettingsSubsystem>())
			{
				HUDOf(Player)->GetHUDWidget()->UpdateGuide(Player);
				Check(!Options->Get()->bShowTutorial || HUDOf(Player)->GetHUDWidget()->GetGuideTipId() == TEXT("Talk"), TEXT("The guide's first tip points new players at the quest giver"));
				Check(!HUDOf(Player)->GetHUDWidget()->IsTitleOpen(), TEXT("Automated runs skip the title screen"));
			}
			Check(FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 100.0f) && FMath::IsNearlyEqual(PlayerHealth->GetMaxHealth(), 100.0f), TEXT("Player starts at 100/100 health"));
			Check(Progression->GetCurrentXP() == 0 && Progression->GetXPToNextLevel() == 100, TEXT("Player starts at 0/100 XP"));

			if (AMMOCreature* Dire = State->Dire.Get())
			{
				Check(Dire->GetTargetDisplayName().ToString() == TEXT("Dire Wolf"), TEXT("A Dire Wolf exists in the zone"));
				Check(Dire->GetTargetLevel() > Wolf->GetTargetLevel() && Dire->GetHealth()->GetMaxHealth() > 2.0f * Wolf->GetHealth()->GetMaxHealth()
					&& Dire->AttackDamage > Wolf->AttackDamage && Dire->XPReward > 2 * Wolf->XPReward,
					FString::Printf(TEXT("Dire Wolf is stronger: level %d, %.0f HP, %.0f damage, %d XP"), Dire->GetTargetLevel(), Dire->GetHealth()->GetMaxHealth(), Dire->AttackDamage, Dire->XPReward));
				Check(Dire->GetCapsuleComponent()->GetScaledCapsuleRadius() > Wolf->GetCapsuleComponent()->GetScaledCapsuleRadius() * 1.2f, TEXT("Dire Wolf is visibly bigger"));
				Check(Dire->LootTable.ToSoftObjectPath() != Wolf->LootTable.ToSoftObjectPath() && Dire->LootTable.LoadSynchronous() != nullptr, TEXT("Dire Wolf uses its own loot table"));
				const UNavigationPath* DirePath = NavSys ? NavSys->FindPathToLocationSynchronously(World, State->SafeOrigin, Dire->GetSpawnTransform().GetLocation()) : nullptr;
				Check(DirePath && DirePath->IsValid() && !DirePath->IsPartial(), TEXT("NavMesh reaches from the village to the Dire Wolf in the deep woods"));
			}
			if (State->bHasZones)
			{
				int32 Zones = 0;
				for (TActorIterator<AMMODiscoveryZone> It(World); It; ++It)
				{
					++Zones;
				}
				Check(Zones >= 5, FString::Printf(TEXT("Zone has %d named discoverable locations"), Zones));
			}
			Check(Wolf->GetTargetDisplayName().ToString() == TEXT("Grey Wolf"), TEXT("Creature is named Grey Wolf"));
			Check(State->Pack.Num() >= 2, TEXT("Several wolves exist (a lone wolf plus a pair)"));
			Check(StateOf(Wolf) == EMMOCreatureAIState::Idle, TEXT("Wolf starts idle"));

			const AMMOHUD* HUD = HUDOf(Player);
			Check(HUD && HUD->GetHUDWidget() && HUD->GetHUDWidget()->IsInViewport() && HUD->GetHUDWidget()->HasFrames(), TEXT("UMG HUD is in the viewport with player and target frames"));

			Check(NavSys && NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr, TEXT("NavMesh exists in the test map"));
			const UNavigationPath* Path = NavSys ? NavSys->FindPathToLocationSynchronously(World, WolfHome, State->SafeOrigin) : nullptr;
			Check(Path && Path->IsValid() && !Path->IsPartial(), TEXT("NavMesh path exists from the wolf to the player start"));

			// items: data assets and starting gear
			Check(Greyfang && Boots && TrainingSword && WolfFang && UMMOItemDefinition::FindById(TEXT("WolfPelt")) && UMMOItemDefinition::FindById(TEXT("RawWolfMeat")), TEXT("Item definitions load by id"));
			Check(Equipment->GetEquipped(EMMOEquipmentSlot::MainHand).Item == TrainingSword, TEXT("Player starts with the Training Sword equipped"));
			float Min, Max;
			Combat->GetDamageRange(Min, Max);
			Check(Min == 10.0f && Max == 14.0f, FString::Printf(TEXT("Starting damage comes from the Training Sword (%.0f-%.0f)"), Min, Max));
			Check(Inventory->GetSlots().Num() == 20 && Inventory->GetTotalItemCount() == 0, TEXT("Backpack has 20 empty slots"));

			// camera zoom limits
			const float Start = Player->GetDesiredCameraDistance();
			Player->DoZoom(-2.0f);
			Check(Player->GetDesiredCameraDistance() > Start, TEXT("Mouse wheel zooms the camera out"));
			Player->DoZoom(100.0f);
			const float ZoomMin = Player->GetDesiredCameraDistance();
			Player->DoZoom(-100.0f);
			const float ZoomMax = Player->GetDesiredCameraDistance();
			Check(ZoomMin >= 200.0f && ZoomMax <= 1500.0f && ZoomMax > ZoomMin + 400.0f, FString::Printf(TEXT("Camera zoom is clamped to a sensible range (%.0f-%.0f)"), ZoomMin, ZoomMax));
			Player->DoZoom((ZoomMax - Start) / 75.0f);

			// UI windows toggle and show the cursor
			Player->DoToggleInventory();
			Check(HUD->GetHUDWidget()->IsInventoryOpen() && CursorShown(Player), TEXT("B opens the backpack and shows the cursor"));
			Player->DoToggleCharacter();
			Check(HUD->GetHUDWidget()->IsCharacterOpen(), TEXT("C opens the character window"));
			Shot(TEXT("0_Windows"));
			NextStep();
			break;
		}

		case 1:
			if (Elapsed() > 0.3f)
			{
				const AMMOHUD* HUD = HUDOf(Player);
				Player->DoClearTarget();
				Check(!HUD->GetHUDWidget()->IsInventoryOpen() && !HUD->GetHUDWidget()->IsCharacterOpen(), TEXT("Esc closes windows"));
				Check(CursorShown(Player), TEXT("Mouse cursor stays visible and free with no windows open"));
				if (State->bHasZones)
				{
					UMMOExplorationComponent* Exploration = Player->GetExploration();
					Check(Exploration->GetCurrentZone() && Exploration->HasDiscovered(Exploration->GetCurrentZone()->LocationId), TEXT("Starting village is discovered on arrival"));
					Check(TotalXP(Progression) == 0, TEXT("The starting village grants no XP"));
				}
				State->XPMark = TotalXP(Progression);
				PlacePlayerNear(WolfHome, 600.0f);
				NextStep();
			}
			break;

		case 2: // aggro + NavMesh chase
			if (Elapsed() > 0.4f)
			{
				Check(InCombat(Wolf), TEXT("Wolf aggros when the player enters its aggro range"));
				Check(AIOf(Wolf) && AIOf(Wolf)->IsUsingNavigation(), TEXT("Wolf chases using NavMesh pathfinding"));
				Check(FMath::Abs(Player->GetCameraBoom()->TargetArmLength - Player->GetDesiredCameraDistance()) < 40.0f, TEXT("Camera eased to the requested zoom distance"));
				if (State->bHasZones)
				{
					const AMMODiscoveryZone* Zone = Player->GetExploration()->GetCurrentZone();
					Check(Zone && Player->GetExploration()->HasDiscovered(Zone->LocationId), FString::Printf(TEXT("Walking out discovers %s"), Zone ? *Zone->LocationName.ToString() : TEXT("(no zone)")));
					Check(Zone && TotalXP(Progression) == State->XPMark + Zone->DiscoveryXP, TEXT("Discovery awards its XP"));
					Check(Zone && HUDOf(Player)->GetHUDWidget()->GetZoneBannerText().StartsWith(TEXT("Discovered:")), FString::Printf(TEXT("HUD shows \"%s\""), *HUDOf(Player)->GetHUDWidget()->GetZoneBannerText()));
				}
				State->XPMark = TotalXP(Progression);
				NextStep();
			}
			break;

		case 3: // wolf bite is timed by its attack animation
			if (PlayerHealth->GetCurrentHealth() < PlayerHealth->GetMaxHealth())
			{
				Check(Now() - Wolf->GetLastAttackTime() >= 0.2, FString::Printf(TEXT("Wolf bite lands on the attack's hit frame (%.2fs after the attack starts)"), Now() - Wolf->GetLastAttackTime()));
				Check(FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), PlayerHealth->GetMaxHealth() - Wolf->AttackDamage), TEXT("Wolf bite deals its Attack Damage (no armor yet)"));
				NextStep();
			}
			else if (Elapsed() > 8.0f)
			{
				Check(false, TEXT("Wolf attacked the player within 8s"));
				NextStep();
			}
			break;

		case 4: // right-click the wolf: target + auto-attack; this kill will drop the boots and Greyfang
		{
			UMMOLootTable::ForceNextDrop(Greyfang);
			UMMOLootTable::ForceNextDrop(Boots);

			// mouse picking: a ray from the camera through the wolf finds it; a ray beside it does not
			const APlayerController* PC = Cast<APlayerController>(Player->GetController());
			const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
			const FVector ToWolf = Wolf->GetActorLocation() - CameraLocation;
			Check(AMMOCharacter::FindCreatureAlongRay(World, CameraLocation, ToWolf, 6000.0f, Player) == Wolf, TEXT("Cursor ray over the wolf picks it"));
			Check(AMMOCharacter::FindCreatureAlongRay(World, CameraLocation, ToWolf.RotateAngleAxis(25.0f, FVector::UpVector), 6000.0f, Player) != Wolf, TEXT("Cursor ray beside the wolf does not pick it"));

			Player->InteractWith(Wolf);
			Check(Combat->GetCurrentTarget() == Wolf, TEXT("Right-clicking the wolf targets it"));
			Check(Combat->IsAutoAttacking(), TEXT("Right-clicking the wolf starts auto-attack"));
			State->HealthMark = WolfHealth->GetCurrentHealth();
			NextStep();
			break;
		}

		case 5: // swing starts without dealing damage yet
			if (Combat->IsSwingPending())
			{
				State->Mark = Now();
				Check(WolfHealth->GetCurrentHealth() == State->HealthMark, TEXT("No damage at the start of the swing"));
				NextStep();
			}
			else if (Elapsed() > 2.0f)
			{
				Check(false, TEXT("Auto-attack started a swing"));
				NextStep();
			}
			break;

		case 6: // damage on the animation's hit frame
			if (WolfHealth->GetCurrentHealth() < State->HealthMark)
			{
				const double Delay = Now() - State->Mark;
				const float Dealt = State->HealthMark - WolfHealth->GetCurrentHealth();
				Check(Delay >= 0.3, FString::Printf(TEXT("Swing damage lands on the hit frame (%.2fs into the swing)"), Delay));
				Check(Combat->WasLastHitFromNotify(), TEXT("Hit timing came from the attack animation's MMO Melee Hit notify"));
				Check(Dealt >= 10.0f && Dealt <= 14.0f, FString::Printf(TEXT("Training Sword swing deals 10-14 damage (%.0f)"), Dealt));
				State->Mark = Now();
				State->HealthMark = WolfHealth->GetCurrentHealth();
				Shot(TEXT("1_AutoAttack"));
				NextStep();
			}
			else if (Elapsed() > 2.0f)
			{
				Check(false, TEXT("Swing dealt damage within 2s"));
				NextStep();
			}
			break;

		case 7: // second swing happens on its own on the swing timer
			if (WolfHealth->GetCurrentHealth() < State->HealthMark)
			{
				const double Interval = Now() - State->Mark;
				Check(FMath::Abs(Interval - Combat->BasicAttackCooldown) < 0.3, FString::Printf(TEXT("Auto-attack swings again automatically on the swing timer (%.2fs)"), Interval));
				NextStep();
			}
			else if (Elapsed() > Combat->BasicAttackCooldown + 2.0f)
			{
				Check(false, TEXT("Auto-attack swung again without input"));
				NextStep();
			}
			break;

		case 8: // step out of range (wolf held in place for the test)
			if (WolfHealth->IsDead())
			{
				// a lucky high roll already killed it: skip the range checks
				State->Step = 11;
				break;
			}
			if (!Combat->IsSwingPending())
			{
				// hold the wolf in place (AI state changes reset its speed, so disable movement instead)
				Wolf->GetCharacterMovement()->DisableMovement();
				State->CounterMark = Combat->GetOutOfRangeWarningCount();
				State->HealthMark = WolfHealth->GetCurrentHealth();
				State->bFlag = false;
				PlacePlayerNear(Wolf->GetActorLocation(), 500.0f);
				NextStep();
			}
			break;

		case 9:
			State->bFlag |= WolfHealth->GetCurrentHealth() != State->HealthMark;
			if (Elapsed() > 3.0f)
			{
				const int32 Warnings = Combat->GetOutOfRangeWarningCount() - State->CounterMark;
				Check(!State->bFlag && !Combat->IsTargetInRange(), FString::Printf(TEXT("No damage while out of range (gap %.0fcm)"), UMMOCombatComponent::GetEdgeDistance(Player, Wolf)));
				Check(Combat->IsAutoAttacking(), TEXT("Auto-attack stays active while out of range"));
				Check(Warnings >= 1 && Warnings <= 2, FString::Printf(TEXT("\"Out of Range\" feedback shown and throttled (%d in 3s)"), Warnings));
				PlacePlayerNear(Wolf->GetActorLocation(), 120.0f);
				NextStep();
			}
			break;

		case 10: // back in range: resumes automatically
			if (WolfHealth->GetCurrentHealth() < State->HealthMark)
			{
				Check(Elapsed() < Combat->BasicAttackCooldown + 1.0f, TEXT("Auto-attack resumes automatically back in range"));
				Wolf->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				NextStep();
			}
			else if (Elapsed() > Combat->BasicAttackCooldown + 2.0f)
			{
				Check(false, TEXT("Auto-attack resumed back in range"));
				Wolf->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				NextStep();
			}
			break;

		case 11: // fight to the death on auto-attack
			if (WolfHealth->IsDead())
			{
				Check(!Combat->IsAutoAttacking(), TEXT("Auto-attack stops when the target dies"));
				Check(Wolf->IsDead() && !Wolf->IsTargetable(), TEXT("Dead wolf is not targetable"));
				Check(StateOf(Wolf) == EMMOCreatureAIState::Dead && !Wolf->IsAttacking(), TEXT("Dead wolf AI and attacks stop"));
				Check(TotalXP(Progression) == State->XPMark + Wolf->XPReward, TEXT("Killing the wolf awards its XP"));
				Check(Combat->StartAutoAttack() == EMMOAttackResult::TargetDead, TEXT("Cannot attack a dead target"));
				WolfHealth->ApplyDamage(10.0f, Player);
				Check(TotalXP(Progression) == State->XPMark + Wolf->XPReward, TEXT("XP is awarded exactly once"));
				State->DeathMark = Now();

				// loot
				UMMOLootContainerComponent* Loot = Wolf->GetLoot();
				Check(Wolf->IsLootable(), TEXT("Corpse is lootable"));
				Check(Loot->GetItems().ContainsByPredicate([Greyfang](const FMMOItemStack& S) { return S.Item == Greyfang; })
					&& Loot->GetItems().ContainsByPredicate([Boots](const FMMOItemStack& S) { return S.Item == Boots; }), TEXT("Forced Greyfang and boots drops are on the corpse"));
				Check(Inventory->GetTotalItemCount() == 0, TEXT("Loot is not added to the backpack automatically"));
				State->bFlag = false;
				NextStep();
			}
			else if (Elapsed() > 25.0f)
			{
				Check(false, TEXT("Wolf killed by auto-attack within 25s"));
				NextStep();
			}
			break;

		case 12: // interact opens the loot window (screenshot it before looting)
		{
			const AMMOHUD* HUD = HUDOf(Player);
			Player->InteractWith(Wolf);
			Check(HUD->GetHUDWidget()->IsLootOpen() && HUD->GetHUDWidget()->GetOpenLoot() == Wolf->GetLoot(), TEXT("Right-clicking the corpse opens its loot window"));
			Check(CursorShown(Player), TEXT("Loot window shows the mouse cursor"));
			Shot(TEXT("2_LootWindow"));
			NextStep();
			break;
		}

		case 13: // loot one item, then the rest
		{
			if (Elapsed() < 0.3f)
			{
				break;
			}
			UMMOLootContainerComponent* Loot = Wolf->GetLoot();

			const int32 CorpseBefore = Loot->GetTotalItemCount();
			const FMMOItemStack First = Loot->GetItems()[0];
			Check(Player->LootItem(Loot, First.InstanceId) == EMMOLootResult::Success, FString::Printf(TEXT("Loot a single item (%s x%d)"), *First.Item->DisplayName.ToString(), First.Quantity));
			Check(Inventory->CountItem(First.Item) == First.Quantity && Loot->GetTotalItemCount() == CorpseBefore - First.Quantity, TEXT("Looted item moved from corpse to backpack"));
			Check(Player->LootItem(Loot, First.InstanceId) == EMMOLootResult::NotFound && Inventory->CountItem(First.Item) == First.Quantity, TEXT("Clicking the same loot again does not duplicate it"));

			const int32 OwnedBefore = Inventory->GetTotalItemCount();
			const int32 Remaining = Loot->GetTotalItemCount();
			Check(Player->LootAll(Loot) == EMMOLootResult::Success && !Loot->HasLoot(), TEXT("Loot All empties the corpse"));
			Check(Inventory->GetTotalItemCount() == OwnedBefore + Remaining, TEXT("Loot All moved every item exactly once"));
			Check(Player->LootAll(Loot) == EMMOLootResult::NotFound && Inventory->GetTotalItemCount() == OwnedBefore + Remaining, TEXT("Rapid repeated Loot All does nothing more"));
			Check(Inventory->CountItem(Greyfang) == 1 && Inventory->CountItem(Boots) == 1, TEXT("Greyfang and boots are in the backpack"));
			NextStep();
			break;
		}

		case 14:
			if (Elapsed() > 0.2f)
			{
				Check(!HUDOf(Player)->GetHUDWidget()->IsLootOpen(), TEXT("Loot window closes when the corpse is empty"));
				Check(!Wolf->IsLootable(), TEXT("Empty corpse is no longer lootable"));

				// equip boots: armor
				Check(Player->EquipInventorySlot(FindSlotOf(Inventory, Boots)) == EMMOEquipResult::Success, TEXT("Equip Worn Leather Boots"));
				Check(Equipment->GetEquipped(EMMOEquipmentSlot::Feet).Item == Boots && Inventory->CountItem(Boots) == 0, TEXT("Boots moved to the Feet slot"));
				Check(PlayerHealth->GetArmor() == 8.0f && PlayerHealth->GetDamageReduction() > 0.13f, FString::Printf(TEXT("Boots give 8 armor (%.0f%% damage reduction)"), PlayerHealth->GetDamageReduction() * 100.0f));

				// equip Greyfang: damage
				const int32 Owned = CountOwned(Player);
				Check(Player->EquipInventorySlot(FindSlotOf(Inventory, Greyfang)) == EMMOEquipResult::Success, TEXT("Equip Greyfang"));
				float Min, Max;
				Combat->GetDamageRange(Min, Max);
				Check(Equipment->GetEquipped(EMMOEquipmentSlot::MainHand).Item == Greyfang && Min == 17.0f && Max == 22.0f, FString::Printf(TEXT("Greyfang raises damage to %.0f-%.0f"), Min, Max));
				Check(Inventory->CountItem(TrainingSword) == 1 && CountOwned(Player) == Owned, TEXT("Training Sword swapped into the backpack (no item lost or duplicated)"));

				// swap back and forth
				for (int32 i = 0; i < 4; ++i)
				{
					Player->EquipInventorySlot(FindSlotOf(Inventory, i % 2 == 0 ? TrainingSword : Greyfang));
				}
				Check(Equipment->GetEquipped(EMMOEquipmentSlot::MainHand).Item == Greyfang && CountOwned(Player) == Owned, TEXT("Repeated weapon swaps conserve items"));

				// unequip / re-equip boots
				Check(Player->UnequipSlot(EMMOEquipmentSlot::Feet) == EMMOEquipResult::Success && PlayerHealth->GetArmor() == 0.0f && Inventory->CountItem(Boots) == 1, TEXT("Unequip boots removes their armor"));
				Player->EquipInventorySlot(FindSlotOf(Inventory, Boots));
				Check(PlayerHealth->GetArmor() == 8.0f, TEXT("Re-equipped boots"));

				Player->DoToggleCharacter();
				Player->DoToggleInventory();
				NextStep();
			}
			break;

		case 15: // screenshot the equipped state (plus a sample tooltip), then move on
			if (Elapsed() > 0.4f && !State->bFlag)
			{
				State->bFlag = true;
				if (State->bScreenshots)
				{
					// hover tooltips can't be triggered from here, so show one directly for the capture
					UMMOItemTooltipWidget* Tooltip = CreateWidget<UMMOItemTooltipWidget>(Cast<APlayerController>(Player->GetController()), UMMOItemTooltipWidget::StaticClass());
					const FMMOItemStack Sample = FMMOItemStack::Make(Greyfang, 1);
					const FMMOItemStack Worn = FMMOItemStack::Make(TrainingSword, 1);
					Tooltip->SetItem(Sample, &Worn);
					Tooltip->AddToViewport(50);
					Tooltip->SetPositionInViewport(FVector2D(560.0f, 120.0f));
					State->TooltipWidget = Tooltip;
				}
				Shot(TEXT("3_Equipped"));
			}
			else if (Elapsed() > 0.8f)
			{
				if (UUserWidget* Tooltip = State->TooltipWidget.Get())
				{
					Tooltip->RemoveFromParent();
				}
				HUDOf(Player)->CloseAllWindows();
				PlacePlayerNear(WolfHome, 250.0f);
				State->bFlag = false;
				NextStep();
			}
			break;

		case 16: // respawn waits while the player stands on the spawn point, then happens once they leave
			if (!State->bFlag)
			{
				if (Wolf->GetRespawnDueTime() > State->DeathMark && Now() > Wolf->GetRespawnDueTime() + 1.5)
				{
					Check(Wolf->IsDead() && Wolf->IsHidden(), TEXT("Respawn is postponed while the player stands on the spawn point"));
					State->bFlag = true;
					State->Mark = Now();
					PlacePlayerNear(WolfHome, 1500.0f);
				}
				else if (Elapsed() > Wolf->RespawnDelay + 6.0f)
				{
					Check(false, TEXT("Corpse was removed and respawn became due"));
					State->bFlag = true;
					State->Mark = Now();
					PlacePlayerNear(WolfHome, 1500.0f);
				}
				break;
			}
			if (!Wolf->IsDead())
			{
				Check(Now() - State->Mark <= 4.5, TEXT("Wolf respawns once the player moves away"));
				Check(WolfHealth->GetCurrentHealth() == WolfHealth->GetMaxHealth() && Wolf->IsTargetable(), TEXT("Respawned wolf has full health and is targetable"));
				Check(FVector::Dist2D(Wolf->GetActorLocation(), WolfHome) < 10.0f, TEXT("Respawned wolf is at its spawn point"));
				Check(!Wolf->GetLoot()->HasLoot(), TEXT("Respawned wolf carries no old loot"));
				Check(Combat->GetCurrentTarget() == nullptr, TEXT("Target cleared once the corpse despawned"));
				NextStep();
			}
			else if (Now() - State->Mark > 8.0)
			{
				Check(false, TEXT("Wolf respawned in time"));
				NextStep();
			}
			break;

		case 17: // XP overflow + level up
			Progression->AddXP(Progression->GetXPToNextLevel() - Progression->GetCurrentXP() + 30);
			Check(Progression->GetLevel() == 2 && Progression->GetCurrentXP() == 30 && Progression->GetXPToNextLevel() == 150, TEXT("Level up with XP overflow; level 2 needs 150"));
			Check(FMath::IsNearlyEqual(PlayerHealth->GetMaxHealth(), 110.0f) && FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 110.0f), TEXT("Level up raises max health and heals"));
			Check(FMath::IsNearlyEqual(Combat->BonusDamage, 2.0f), TEXT("Level up adds +2 swing damage"));
			NextStep();
			break;

		case 18:
			if (Elapsed() > 0.4f)
			{
				Shot(TEXT("4_LevelUp"));
				Combat->SetTarget(Wolf);
				Check(Combat->StartAutoAttack() == EMMOAttackResult::OutOfRange && Combat->IsAutoAttacking(), TEXT("Starting auto-attack out of range warns but stays armed"));
				Combat->StopAutoAttack();
				Check(!Combat->IsAutoAttacking(), TEXT("Auto-attack toggles off"));
				Check(StateOf(Wolf) == EMMOCreatureAIState::Idle, TEXT("Wolf ignores a player outside aggro range"));
				PlacePlayerNear(WolfHome, 700.0f);
				NextStep();
			}
			break;

		case 19: // leash
			if (InCombat(Wolf))
			{
				State->OriginalLeashRange = Wolf->LeashRange;
				Wolf->LeashRange = 200.0f;
				NextStep();
			}
			else if (Elapsed() > 3.0f)
			{
				Check(false, TEXT("Wolf re-aggroed for leash test"));
				NextStep();
			}
			break;

		case 20:
			if (StateOf(Wolf) == EMMOCreatureAIState::Returning)
			{
				Check(true, TEXT("Wolf leashes and returns when pulled too far from spawn"));
				Check(WolfHealth->ApplyDamage(5.0f, Player) == 0.0f, TEXT("Returning wolf evades damage"));
				Wolf->LeashRange = State->OriginalLeashRange;
				Combat->ClearTarget();
				PlacePlayerNear(WolfHome, 1500.0f);
				NextStep();
			}
			else if (Elapsed() > 15.0f)
			{
				Check(false, TEXT("Wolf leashed within 15s"));
				NextStep();
			}
			break;

		case 21:
			if (StateOf(Wolf) == EMMOCreatureAIState::Idle)
			{
				Check(FVector::Dist2D(Wolf->GetActorLocation(), WolfHome) < 100.0f, TEXT("Wolf paths home and resets at its spawn point"));
				Check(WolfHealth->GetCurrentHealth() == WolfHealth->GetMaxHealth() && !WolfHealth->IsInvulnerable(), TEXT("Reset wolf has full health and is attackable"));
				NextStep();
			}
			else if (Elapsed() > 15.0f)
			{
				Check(false, TEXT("Wolf got home within 15s"));
				NextStep();
			}
			break;

		case 22: // several wolves: pull the pair
		{
			AMMOCreature* A = State->Pack[0].Get();
			AMMOCreature* B = State->Pack[1].Get();
			const FVector Middle = (A->GetSpawnTransform().GetLocation() + B->GetSpawnTransform().GetLocation()) * 0.5f;
			PlacePlayerNear(Middle, 450.0f);
			State->bFlag = false;
			NextStep();
			break;
		}

		case 23:
		{
			AMMOCreature* A = State->Pack[0].Get();
			AMMOCreature* B = State->Pack[1].Get();
			if (InCombat(A) && InCombat(B))
			{
				Check(true, TEXT("Several wolves independently aggro the player"));
				Check(State->MaxWanderDistance > 60.0f, FString::Printf(TEXT("Idle wolves wander around their home (moved up to %.0fcm)"), State->MaxWanderDistance));
				Check(State->MaxWanderDistance <= A->WanderRadius + 250.0f, FString::Printf(TEXT("Wandering stays within the wolf's territory (radius %.0f)"), A->WanderRadius));
				UMMOLootTable::ForceNextDrop(Boots); // B's corpse gets a non-stackable item for the full-bag test
				Combat->SetTarget(B);
				Combat->StartAutoAttack();
				State->Mark = Now();
				State->HealthMark = B->GetHealth()->GetCurrentHealth();
				State->OtherHealthMark = A->GetHealth()->GetCurrentHealth();
				State->HitCount = 0;
				State->MinTargetHit = TNumericLimits<float>::Max();
				State->LastPlayerDamage = 0.0f;
				NextStep();
			}
			else if (Elapsed() > 5.0f)
			{
				Check(false, TEXT("Both wolves of the pair aggroed"));
				NextStep();
			}
			else if (!State->bFlag && Elapsed() > 2.5f)
			{
				// an idle wolf may have wandered to the far side of its territory: walk toward the one that hasn't noticed us
				State->bFlag = true;
				if (AMMOCreature* Unaware = !InCombat(A) ? A : (!InCombat(B) ? B : nullptr))
				{
					PlacePlayerNear(Unaware->GetActorLocation(), 350.0f);
				}
			}
			break;
		}

		case 24: // Greyfang kills the selected wolf in fewer swings while the other wolf keeps biting
		{
			AMMOCreature* A = State->Pack[0].Get();
			AMMOCreature* B = State->Pack[1].Get();
			const float BHealth = B->GetHealth()->GetCurrentHealth();
			if (BHealth < State->HealthMark)
			{
				++State->HitCount;
				if (!B->IsDead())
				{
					// the killing blow is clamped to the remaining health, so only earlier hits show the full roll
					State->MinTargetHit = FMath::Min(State->MinTargetHit, State->HealthMark - BHealth);
				}
				State->HealthMark = BHealth;
			}

			if (B->IsDead())
			{
				Check(State->HitCount <= 4, FString::Printf(TEXT("Greyfang kills a 60 HP wolf in %d swings (Training Sword needs 4-5)"), State->HitCount));
				float RangeMin, RangeMax;
				Combat->GetDamageRange(RangeMin, RangeMax);
				Check(RangeMin >= 19.0f && State->MinTargetHit >= RangeMin - 0.5f && State->MinTargetHit <= RangeMax + 0.5f, FString::Printf(TEXT("Greyfang swings hit for %.0f-%.0f (lowest non-final hit %.0f)"), RangeMin, RangeMax, State->MinTargetHit));
				Check(A->GetHealth()->GetCurrentHealth() == State->OtherHealthMark, TEXT("Other aggressive wolves are not hit by auto-attack"));
				Check(A->GetLastAttackTime() > State->Mark - 2.0 && B->GetLastAttackTime() > State->Mark - 2.0, TEXT("Both wolves attack the player"));
				Check(State->LastPlayerDamage > 0.0f && State->LastPlayerDamage < A->AttackDamage, FString::Printf(TEXT("Armor reduces wolf bites (%.2f of %.0f)"), State->LastPlayerDamage, A->AttackDamage));
				Check(B->GetLoot() != Wolf->GetLoot() && B->IsLootable(), TEXT("The new corpse has its own loot container"));
				Shot(TEXT("5_Pack"));
				NextStep();
			}
			else if (Elapsed() > 15.0f)
			{
				Check(false, TEXT("Selected pack wolf killed within 15s"));
				NextStep();
			}
			break;
		}

		case 25: // full backpack: loot stays on the corpse
		{
			AMMOCreature* B = State->Pack[1].Get();
			UMMOLootContainerComponent* Loot = B->GetLoot();

			const int32 FangsBefore = Inventory->CountItem(WolfFang);
			int32 Added = 0;
			for (int32 i = 0; i < Inventory->GetSlots().Num(); ++i)
			{
				if (Inventory->GetSlot(i).IsEmpty())
				{
					Inventory->AddStack(FMMOItemStack::Make(WolfFang, WolfFang->MaxStackSize), i);
					Added += WolfFang->MaxStackSize;
				}
			}
			Check(Inventory->GetFreeSlotCount() == 0, TEXT("Backpack filled for the full-inventory test"));

			Player->DoInteract();
			Check(HUDOf(Player)->GetHUDWidget()->GetOpenLoot() == Loot, TEXT("F (interact) opens the nearest corpse's loot window"));

			const int32 BootsOnCorpse = Loot->GetItems().FilterByPredicate([Boots](const FMMOItemStack& S) { return S.Item == Boots; }).Num();
			const EMMOLootResult Result = Player->LootAll(Loot);
			Check(Result == EMMOLootResult::InventoryFull || Result == EMMOLootResult::Partial, TEXT("Loot All with a full backpack reports Inventory Full"));
			// the forced boots (plus any that dropped naturally) can't fit a full bag, so all of them must still be on the corpse
			const int32 BootsLeft = Loot->GetItems().FilterByPredicate([Boots](const FMMOItemStack& S) { return S.Item == Boots; }).Num();
			Check(BootsOnCorpse >= 1 && BootsLeft == BootsOnCorpse, FString::Printf(TEXT("Loot that doesn't fit stays on the corpse (%d of %d boots kept)"), BootsLeft, BootsOnCorpse));
			Check(Player->UnequipSlot(EMMOEquipmentSlot::Feet) == EMMOEquipResult::InventoryFull && Equipment->GetEquipped(EMMOEquipmentSlot::Feet).Item == Boots, TEXT("Unequip is refused when the backpack is full"));

			// remove the filler and take the rest
			Inventory->RemoveItem(WolfFang, Inventory->CountItem(WolfFang) - FangsBefore);
			Check(Player->LootAll(Loot) == EMMOLootResult::Success && !Loot->HasLoot(), TEXT("With room again the remaining loot can be taken"));

			// Esc closes an open window first, then clears the target on the next press
			HUDOf(Player)->OpenLoot(Loot->HasLoot() ? Loot : nullptr);
			const bool bWindowWasOpen = HUDOf(Player)->IsAnyWindowOpen();
			Player->DoClearTarget();
			Check(!HUDOf(Player)->IsAnyWindowOpen() && (!bWindowWasOpen || Combat->GetCurrentTarget() != nullptr), TEXT("Esc closes open windows before touching the target"));
			Player->DoClearTarget();
			Check(Combat->GetCurrentTarget() == nullptr, TEXT("Esc with no windows open clears the target"));
			State->ItemsBeforeDeath = CountOwned(Player);
			State->LevelMark = Progression->GetLevel();

			// the mine: discovered once; revisiting the meadow gives nothing more
			State->bFlag = false;
			if (AMMODiscoveryZone* Mine = FindZone(World, TEXT("RustveinMine")))
			{
				State->XPMark = TotalXP(Progression);
				PlacePlayerNear(Mine->GetActorLocation(), 400.0f);
			}
			NextStep();
			break;
		}

		case 26:
			if (!State->bHasZones)
			{
				NextStep();
				break;
			}
			if (Elapsed() > 0.6f && !State->bFlag)
			{
				const AMMODiscoveryZone* Mine = FindZone(World, TEXT("RustveinMine"));
				Check(Mine && Player->GetExploration()->HasDiscovered(Mine->LocationId) && TotalXP(Progression) == State->XPMark + Mine->DiscoveryXP,
					TEXT("Reaching the abandoned mine discovers Rustvein Mine (+XP)"));
				Shot(TEXT("7_Mine"));
				State->bFlag = true;
				State->XPMark = TotalXP(Progression);
				State->LevelMark = Progression->GetLevel();
				PlacePlayerNear(WolfHome, 1500.0f);
			}
			else if (State->bFlag && Elapsed() > 1.4f)
			{
				Check(TotalXP(Progression) == State->XPMark, TEXT("Revisiting a discovered zone awards nothing"));
				State->bFlag = false;
				NextStep();
			}
			break;

		case 27: // player death keeps every item
			State->LevelMark = Progression->GetLevel();
			PlayerHealth->ApplyDamage(10000.0f, State->Pack[0].Get());
			Check(Player->IsDead() && FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 0.0f), TEXT("Player dies at zero health (never below)"));
			Check(Combat->StartAutoAttack() == EMMOAttackResult::AttackerDead, TEXT("Dead player cannot attack"));
			State->bFlag = false;
			NextStep();
			break;

		case 28:
			if (!State->bFlag && Elapsed() > 0.6f)
			{
				State->bFlag = true;
				Shot(TEXT("6_Death"));
			}
			if (!Player->IsDead())
			{
				Check(Elapsed() >= Player->GetRespawnDelay() - 0.5f, TEXT("Player respawns after the respawn delay"));
				Check(PlayerHealth->GetCurrentHealth() == PlayerHealth->GetMaxHealth() && Player->InputEnabled(), TEXT("Respawned player has full health and control"));
				Check(Progression->GetLevel() == State->LevelMark, TEXT("Level is kept through death"));
				Check(CountOwned(Player) == State->ItemsBeforeDeath && Equipment->GetEquipped(EMMOEquipmentSlot::MainHand).Item == Greyfang, TEXT("Inventory and equipment are kept through death"));
				Check(!InCombat(State->Pack[0].Get()), TEXT("Wolves disengage after the player dies"));
				NextStep();
			}
			else if (Elapsed() > Player->GetRespawnDelay() + 3.0f)
			{
				Check(false, TEXT("Player respawned in time"));
				Finish();
				return false;
			}
			break;

		case 29: // villagers: talk to Warden Hollis (Milestone 4)
		{
			AMMONPC* Hollis = FindNPC(World, TEXT("Hollis"));
			if (!Hollis)
			{
				UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST: no villagers in this map, skipping the quest checks"));
				State->Step = 35;
				State->StepStart = Now();
				break;
			}
			UMMOQuestLogComponent* QuestLog = Player->GetQuestLog();
			UMMOQuestDefinition* Wolves = UMMOQuestDefinition::FindById(TEXT("WolvesAtTheGate"));
			UMMOQuestDefinition* Eyes = UMMOQuestDefinition::FindById(TEXT("EyesOnTheWild"));
			Check(Wolves && Eyes && UMMOQuestDefinition::FindById(TEXT("PeltsForTheHearth")), TEXT("Quest definitions load by id"));
			if (!Wolves || !Eyes)
			{
				Finish();
				return false;
			}
			Check(QuestLog->GetActiveQuests().Num() == 0 && QuestLog->GetQuestState(Wolves) == EMMOQuestState::Available && QuestLog->GetQuestState(Eyes) == EMMOQuestState::Unavailable,
				TEXT("Quest chain starts with Wolves at the Gate available and its follow-up locked"));
			Check(Hollis->GetMarker(QuestLog) == EMMONPCMarker::QuestAvailable && PlateMarker(Hollis) == EMMONPCMarker::QuestAvailable, TEXT("Warden Hollis shows a gold ! over his head"));

			PlacePlayerNear(Hollis->GetActorLocation(), 900.0f);
			Check(!Player->TryInteract(Hollis) && !HUDOf(Player)->GetHUDWidget()->IsDialogueOpen(), TEXT("Talking to a villager requires being close"));

			PlacePlayerNear(Hollis->GetActorLocation(), 220.0f);
			Check(Player->FindNearestInteractable() == Hollis, TEXT("F finds the villager standing next to the player"));
			Player->DoInteract();
			UMMOHUDWidget* Widget = HUDOf(Player)->GetHUDWidget();
			Check(Widget->GetDialogueNPC() == Hollis && CursorShown(Player), TEXT("F opens a conversation with Warden Hollis"));
			const TArray<UMMOQuestDefinition*> Offered = Hollis->GetDialogueQuests(QuestLog);
			Check(Offered.Contains(Wolves) && !Offered.Contains(Eyes), TEXT("Hollis offers Wolves at the Gate (and not the locked follow-up)"));
			Widget->GetDialogueWindow()->ShowQuest(Wolves);
			Check(Widget->GetDialogueWindow()->GetShownQuest() == Wolves, TEXT("Choosing the quest shows its offer page"));
			State->bFlag = false;
			NextStep();
			break;
		}

		case 30: // accept, then look at the quest log and tracker
			if (!State->bFlag && Elapsed() > 0.6f)
			{
				// screenshot the offer page; act on a later frame so the capture shows it
				Shot(TEXT("8_QuestOffer"));
				State->bFlag = true;
			}
			else if (State->bFlag && Elapsed() > 1.0f)
			{
				State->bFlag = false;
				UMMOQuestLogComponent* QuestLog = Player->GetQuestLog();
				UMMOQuestDefinition* Wolves = UMMOQuestDefinition::FindById(TEXT("WolvesAtTheGate"));
				AMMONPC* Hollis = FindNPC(World, TEXT("Hollis"));
				Check(QuestLog->AcceptQuest(Wolves) == EMMOQuestResult::Success && QuestLog->GetQuestState(Wolves) == EMMOQuestState::Active, TEXT("Accepting adds Wolves at the Gate to the quest log"));
				HUDOf(Player)->GetHUDWidget()->GetDialogueWindow()->ShowGreeting();
				Check(Hollis->GetMarker(QuestLog) == EMMONPCMarker::QuestInProgress, TEXT("Hollis's marker turns to a grey ? while the quest is in progress"));
				Check(TrackerHas(Player, TEXT("Wolves at the Gate")) && TrackerHas(Player, TEXT("Grey Wolves slain: 0/5")), TEXT("Quest tracker shows the quest and 0/5 progress"));
				HUDOf(Player)->ToggleQuestLog();
				Check(HUDOf(Player)->GetHUDWidget()->IsQuestLogOpen(), TEXT("L opens the quest log"));
				NextStep();
			}
			break;

		case 31: // kill progress and turn-in
			if (!State->bFlag && Elapsed() > 0.6f)
			{
				Shot(TEXT("9_QuestLog"));
				State->bFlag = true;
			}
			else if (State->bFlag && Elapsed() > 1.0f)
			{
				State->bFlag = false;
				Check(PlateMarker(FindNPC(World, TEXT("Hollis"))) == EMMONPCMarker::QuestInProgress, TEXT("Nameplate marker updates to grey ?"));
				HUDOf(Player)->CloseAllWindows();
				Check(!HUDOf(Player)->IsAnyWindowOpen(), TEXT("Esc closes the quest log and conversation"));

				UMMOQuestLogComponent* QuestLog = Player->GetQuestLog();
				UMMOQuestDefinition* Wolves = UMMOQuestDefinition::FindById(TEXT("WolvesAtTheGate"));
				AMMONPC* Hollis = FindNPC(World, TEXT("Hollis"));

				AMMOCreature* Prey = FindLivingCreature(World, TEXT("GreyWolf"));
				AMMOCreature* DenWolf = FindLivingCreature(World, TEXT("DenWolf"));
				Check(Prey && DenWolf, TEXT("Meadow wolves and den wolves carry their quest tags"));
				if (Prey)
				{
					Prey->GetHealth()->ApplyDamage(100000.0f, Player);
				}
				Check(QuestLog->GetObjectiveProgress(Wolves, 0) == 1 && TrackerHas(Player, TEXT("Grey Wolves slain: 1/5")), TEXT("Killing a Grey Wolf advances the quest to 1/5"));
				if (DenWolf)
				{
					DenWolf->GetHealth()->ApplyDamage(100000.0f, Player);
				}
				Check(QuestLog->GetObjectiveProgress(Wolves, 0) == 1, TEXT("Other wolves don't count toward the Grey Wolf objective"));
				for (int32 i = 0; i < 4; ++i)
				{
					QuestLog->NotifyKill(TEXT("GreyWolf"));
				}
				Check(QuestLog->GetQuestState(Wolves) == EMMOQuestState::ReadyToTurnIn && Hollis->GetMarker(QuestLog) == EMMONPCMarker::QuestReady, TEXT("At 5/5 the quest is ready and Hollis shows a gold ?"));
				Check(TrackerHas(Player, TEXT("Ready to turn in")), TEXT("Tracker says the quest is ready to turn in"));

				Check(Player->TryInteract(Hollis) && Hollis->GetDialogueQuests(QuestLog).Contains(Wolves), TEXT("Hollis lists the finished quest"));
				HUDOf(Player)->GetHUDWidget()->GetDialogueWindow()->ShowQuest(Wolves);
				NextStep();
			}
			break;

		case 32: // screenshot the completion page, then turn in
			if (!State->bFlag && Elapsed() > 0.6f)
			{
				Shot(TEXT("10_QuestComplete"));
				State->bFlag = true;
			}
			else if (State->bFlag && Elapsed() > 1.0f)
			{
				State->bFlag = false;
				UMMOQuestLogComponent* QuestLog = Player->GetQuestLog();
				UMMOQuestDefinition* Wolves = UMMOQuestDefinition::FindById(TEXT("WolvesAtTheGate"));
				UMMOQuestDefinition* Eyes = UMMOQuestDefinition::FindById(TEXT("EyesOnTheWild"));
				AMMONPC* Hollis = FindNPC(World, TEXT("Hollis"));
				State->XPMark = TotalXP(Progression);
				State->CurrencyMark = Inventory->GetCurrency();
				Check(QuestLog->TurnInQuest(Wolves) == EMMOQuestResult::Success && QuestLog->GetQuestState(Wolves) == EMMOQuestState::Completed, TEXT("Turning in completes the quest"));
				Check(TotalXP(Progression) == State->XPMark + Wolves->RewardXP && Inventory->GetCurrency() == State->CurrencyMark + Wolves->RewardCurrency,
					FString::Printf(TEXT("Turn-in pays %d XP and %d copper"), Wolves->RewardXP, Wolves->RewardCurrency));
				Check(QuestLog->GetQuestState(Eyes) == EMMOQuestState::Available && Hollis->GetMarker(QuestLog) == EMMONPCMarker::QuestAvailable, TEXT("The follow-up quest unlocks and Hollis offers it"));
				Check(!TrackerHas(Player, TEXT("Wolves at the Gate")), TEXT("Completed quests leave the tracker"));
				NextStep();
			}
			break;

		case 33: // collect quest with Brenna
		{
			AMMONPC* Brenna = FindNPC(World, TEXT("Brenna"));
			UMMOQuestLogComponent* QuestLog = Player->GetQuestLog();
			UMMOQuestDefinition* Pelts = UMMOQuestDefinition::FindById(TEXT("PeltsForTheHearth"));
			UMMOItemDefinition* Pelt = UMMOItemDefinition::FindById(TEXT("WolfPelt"));
			UMMOItemDefinition* Gloves = UMMOItemDefinition::FindById(TEXT("StitchedWolfhideGloves"));
			Check(Brenna && Pelts && Pelt && Gloves, TEXT("Brenna, her quest and its items exist"));
			if (!Brenna || !Pelts || !Pelt || !Gloves)
			{
				Finish();
				return false;
			}

			PlacePlayerNear(Brenna->GetActorLocation(), 220.0f);
			Check(Brenna->GetMarker(QuestLog) == EMMONPCMarker::QuestAvailable && Player->TryInteract(Brenna) && HUDOf(Player)->GetHUDWidget()->GetDialogueNPC() == Brenna,
				TEXT("Brenna the innkeeper has a quest and can be talked to"));

			Inventory->RemoveItem(Pelt, Inventory->CountItem(Pelt));
			Check(QuestLog->AcceptQuest(Pelts) == EMMOQuestResult::Success && QuestLog->GetObjectiveProgress(Pelts, 0) == 0, TEXT("Accept Pelts for the Hearth (0/4)"));
			Inventory->AddItem(Pelt, 2, true);
			Check(QuestLog->GetObjectiveProgress(Pelts, 0) == 2 && TrackerHas(Player, TEXT("Wolf Pelts: 2/4")), TEXT("Picking up pelts advances the collect objective"));
			Inventory->AddItem(Pelt, 3, true);
			Check(QuestLog->GetQuestState(Pelts) == EMMOQuestState::ReadyToTurnIn && Brenna->GetMarker(QuestLog) == EMMONPCMarker::QuestReady, TEXT("With 4 pelts the quest is ready"));

			const int32 GlovesBefore = Inventory->CountItem(Gloves);
			Check(QuestLog->TurnInQuest(Pelts) == EMMOQuestResult::Success, TEXT("Turn in Pelts for the Hearth"));
			Check(Inventory->CountItem(Pelt) == 1 && Inventory->CountItem(Gloves) == GlovesBefore + 1, TEXT("Turn-in takes exactly 4 pelts and gives the gloves"));

			// equip the reward
			const int32 GloveSlot = FindSlotOf(Inventory, Gloves);
			Check(GloveSlot != INDEX_NONE && Player->EquipInventorySlot(GloveSlot) == EMMOEquipResult::Success && Equipment->GetEquipped(EMMOEquipmentSlot::Hands).Item == Gloves,
				TEXT("Reward gloves can be equipped"));

			// walking away ends the conversation
			PlacePlayerNear(Brenna->GetActorLocation(), 1500.0f);
			NextStep();
			break;
		}

		case 34:
			if (Elapsed() > 0.5f)
			{
				Check(!HUDOf(Player)->GetHUDWidget()->IsDialogueOpen(), TEXT("Walking away closes the conversation"));
				NextStep();
			}
			break;

		case 35: // consumables on the hotbar (Milestone 6)
		{
			UMMOItemDefinition* Potion = UMMOItemDefinition::FindById(TEXT("MinorHealingPotion"));
			UMMOItemDefinition* Bread = UMMOItemDefinition::FindById(TEXT("HeartyBread"));
			Check(Potion && Bread && Potion->IsUsable() && Bread->IsUsable(), TEXT("Potion and food items exist"));
			if (!Potion || !Bread)
			{
				Finish();
				return false;
			}
			UMMOActionBarComponent* Bar = Player->GetActionBar();
			Inventory->RemoveItem(Potion, Inventory->CountItem(Potion));
			Bar->ClearSlot(Bar->FindSlot(UMMOActionBarComponent::MakeItem(Potion->ItemId)));
			Player->GetCooldowns()->ClearCooldown(Potion->GetCooldownKey());

			Inventory->AddItem(Potion, 2, true);
			State->ActionIndex = Bar->FindSlot(UMMOActionBarComponent::MakeItem(Potion->ItemId));
			Check(State->ActionIndex != INDEX_NONE, FString::Printf(TEXT("New potions go onto the hotbar automatically (key %d)"), State->ActionIndex + 2));

			PlayerHealth->RestoreHealth(PlayerHealth->GetMaxHealth());
			Check(!Player->UseActionSlot(State->ActionIndex) && Inventory->CountItem(Potion) == 2, TEXT("Potions aren't wasted at full health"));

			PlayerHealth->ApplyDamage(70.0f, nullptr);
			State->HealthMark = PlayerHealth->GetCurrentHealth();
			Check(Player->UseActionSlot(State->ActionIndex), TEXT("Hotbar key uses the potion"));
			Check(FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), FMath::Min(PlayerHealth->GetMaxHealth(), State->HealthMark + Potion->HealAmount)) && Inventory->CountItem(Potion) == 1,
				FString::Printf(TEXT("Potion heals %d instantly and is used up"), FMath::RoundToInt(Potion->HealAmount)));
			Check(Player->GetCooldowns()->GetRemaining(Potion->GetCooldownKey()) > Potion->Cooldown - 2.0f, TEXT("Potion starts its cooldown"));
			PlayerHealth->ApplyDamage(30.0f, nullptr);
			Check(!Player->UseActionSlot(State->ActionIndex) && Inventory->CountItem(Potion) == 1, TEXT("A second potion is refused while on cooldown"));

			Inventory->AddItem(Bread, 2, true);
			Check(Player->UseItem(Bread) == EMMOUseItemResult::InCombat && Inventory->CountItem(Bread) == 2, TEXT("Food can't be eaten in combat"));
			NextStep();
			break;
		}

		case 36: // out of combat: eat
			if (Elapsed() > 6.6f)
			{
				UMMOItemDefinition* Bread = UMMOItemDefinition::FindById(TEXT("HeartyBread"));
				Check(!Player->IsInCombat(), TEXT("Player leaves combat after a few quiet seconds"));
				PlayerHealth->RestoreHealth(PlayerHealth->GetMaxHealth() * 0.3f);
				State->HealthMark = PlayerHealth->GetCurrentHealth();
				Check(Player->UseItem(Bread) == EMMOUseItemResult::Success && Inventory->CountItem(Bread) == 1, TEXT("Eating out of combat works"));
				Check(Player->GetFoodRemaining() > Bread->EffectDuration - 1.0f, TEXT("Food heals over time"));
				NextStep();
			}
			break;

		case 37:
			if (Elapsed() > 2.1f)
			{
				const float Regen = 2.1f * 4.0f;
				Check(PlayerHealth->GetCurrentHealth() > State->HealthMark + Regen + 4.0f, FString::Printf(TEXT("Food heals faster than resting alone (+%.0f in 2s)"), PlayerHealth->GetCurrentHealth() - State->HealthMark));
				PlayerHealth->ApplyDamage(5.0f, nullptr);
				Check(Player->GetFoodRemaining() <= 0.0f, TEXT("Taking damage stops eating"));
				Check(!Player->GetCooldowns()->IsReady(TEXT("Potion")), TEXT("Potion cooldown keeps running"));
				NextStep();
			}
			break;

		case 38: // merchants
		{
			AMMONPC* Mirelle = FindNPC(World, TEXT("Mirelle"));
			if (!Mirelle)
			{
				UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST: no merchants in this map, skipping the vendor checks"));
				State->Step = 41;
				State->StepStart = Now();
				break;
			}
			AMMONPC* Doran = FindNPC(World, TEXT("Doran"));
			AMMONPC* Brenna = FindNPC(World, TEXT("Brenna"));
			Check(Mirelle->IsVendor() && Doran && Doran->VendorStock.Num() >= 3 && Brenna && Brenna->IsVendor(), TEXT("Herbalist, blacksmith and innkeeper sell goods"));

			UMMOItemDefinition* Potion = UMMOItemDefinition::FindById(TEXT("MinorHealingPotion"));
			PlacePlayerNear(Mirelle->GetActorLocation(), 220.0f);
			Check(Player->TryInteract(Mirelle), TEXT("Talk to Mirelle the herbalist"));
			HUDOf(Player)->OpenVendor(Mirelle);
			UMMOHUDWidget* Widget = HUDOf(Player)->GetHUDWidget();
			Check(Widget->IsVendorOpen() && Widget->IsInventoryOpen() && !Widget->IsDialogueOpen() && Player->GetActiveVendor() == Mirelle,
				TEXT("Browsing goods opens the merchant window next to the backpack"));

			Inventory->SetCurrency(100);
			const int32 Potions = Inventory->CountItem(Potion);
			const int32 Price = Mirelle->VendorStock[0].GetPrice();
			Check(Player->BuyFromVendor(0) == EMMOVendorResult::Success && Inventory->GetCurrency() == 100 - Price && Inventory->CountItem(Potion) == Potions + 1,
				FString::Printf(TEXT("Buying a potion costs %s"), *MMOItems::FormatCurrency(Price)));
			Inventory->SetCurrency(Price - 1);
			Check(Player->BuyFromVendor(0) == EMMOVendorResult::NotEnoughMoney && Inventory->CountItem(Potion) == Potions + 1 && Inventory->GetCurrency() == Price - 1,
				TEXT("Can't buy without enough money"));

			UMMOItemDefinition* Pelt = UMMOItemDefinition::FindById(TEXT("WolfPelt"));
			Inventory->RemoveItem(Pelt, Inventory->CountItem(Pelt));
			Inventory->AddItem(Pelt, 4);
			State->CurrencyMark = Inventory->GetCurrency();
			Player->UseOrEquipInventorySlot(FindSlotOf(Inventory, Pelt));
			Check(Inventory->CountItem(Pelt) == 0 && Inventory->GetCurrency() == State->CurrencyMark + 4 * Pelt->SellValue,
				FString::Printf(TEXT("Right-clicking pelts sells them (+%s)"), *MMOItems::FormatCurrency(4 * Pelt->SellValue)));
			Check(Player->GetBuyback().Num() == 1 && Player->GetBuyback()[0].Item == Pelt && Player->GetBuyback()[0].Quantity == 4, TEXT("Sold pelts appear under Buyback"));
			Check(Player->BuybackItem(0) == EMMOVendorResult::Success && Inventory->CountItem(Pelt) == 4 && Inventory->GetCurrency() == State->CurrencyMark && Player->GetBuyback().Num() == 0,
				TEXT("Buyback returns the pelts for the same price"));
			Player->SellInventorySlot(FindSlotOf(Inventory, Pelt));
			Inventory->SetCurrency(500);
			State->bFlag = false;
			NextStep();
			break;
		}

		case 39: // screenshot the merchant, then walk away
			if (!State->bFlag && Elapsed() > 0.6f)
			{
				Shot(TEXT("11_Vendor"));
				State->bFlag = true;
			}
			else if (State->bFlag && Elapsed() > 1.0f)
			{
				State->bFlag = false;
				PlacePlayerNear(FindNPC(World, TEXT("Mirelle"))->GetActorLocation(), 1500.0f);
				NextStep();
			}
			break;

		case 40:
			if (Elapsed() > 0.5f)
			{
				UMMOItemDefinition* Bread = UMMOItemDefinition::FindById(TEXT("HeartyBread"));
				Check(!HUDOf(Player)->GetHUDWidget()->IsVendorOpen() && Player->GetActiveVendor() == nullptr, TEXT("Walking away closes the merchant window"));
				const int32 Money = Inventory->GetCurrency();
				PlayerHealth->RestoreHealth(PlayerHealth->GetMaxHealth());
				Player->UseOrEquipInventorySlot(FindSlotOf(Inventory, Bread));
				Check(Inventory->GetCurrency() == Money && Inventory->CountItem(Bread) == 1, TEXT("Away from merchants, right-clicking an item never sells it"));
				NextStep();
			}
			break;

		case 41: // abilities: level unlocks, hotbar, range and the global cooldown (Milestone 7)
		{
			UMMOAbilityComponent* Abilities = Player->GetAbilities();
			UMMOAbilityDefinition* Rend = UMMOAbilityDefinition::FindById(TEXT("RendingStrike"));
			UMMOAbilityDefinition* Bash = UMMOAbilityDefinition::FindById(TEXT("ShoulderBash"));
			UMMOAbilityDefinition* Cleave = UMMOAbilityDefinition::FindById(TEXT("CleavingArc"));
			Check(Abilities->GetAllAbilities().Num() == 4 && Rend && Bash && Cleave, TEXT("Four abilities are defined"));
			if (!Rend || !Bash || !Cleave)
			{
				Finish();
				return false;
			}

			const int32 Level = Progression->GetLevel();
			int32 Expected = 0;
			bool bAllOnBar = true;
			for (const UMMOAbilityDefinition* Ability : Abilities->GetAllAbilities())
			{
				if (Ability->RequiredLevel <= Level)
				{
					++Expected;
					bAllOnBar &= Player->GetActionBar()->FindSlot(UMMOActionBarComponent::MakeAbility(Ability->AbilityId)) != INDEX_NONE;
				}
			}
			Check(Abilities->GetKnown().Num() == Expected && Expected > 0, FString::Printf(TEXT("Level %d knows %d abilities"), Level, Expected));
			Check(bAllOnBar, TEXT("Abilities learned while levelling went onto the hotbar"));

			while (Progression->GetLevel() < 5)
			{
				Progression->AddXP(Progression->GetXPToNextLevel() - Progression->GetCurrentXP());
			}
			Check(Abilities->IsKnown(Cleave) && Player->GetActionBar()->FindSlot(UMMOActionBarComponent::MakeAbility(Cleave->AbilityId)) != INDEX_NONE,
				TEXT("Reaching level 5 teaches Cleaving Arc and puts it on the hotbar"));

			// two wolves to practise on: the closest idle pair, so neither is pulled past its leash
			TArray<AMMOCreature*> Idle;
			for (TActorIterator<AMMOCreature> It(World); It; ++It)
			{
				if (!It->IsDead() && It->IsTargetable() && It->IsA<AMMOGreyWolf>() && StateOf(*It) == EMMOCreatureAIState::Idle)
				{
					Idle.Add(*It);
				}
			}
			AMMOCreature* A = nullptr;
			AMMOCreature* B = nullptr;
			float BestPair = TNumericLimits<float>::Max();
			for (int32 i = 0; i < Idle.Num(); ++i)
			{
				for (int32 j = i + 1; j < Idle.Num(); ++j)
				{
					const float D = FVector::Dist2D(Idle[i]->GetActorLocation(), Idle[j]->GetActorLocation());
					if (D < BestPair)
					{
						BestPair = D;
						A = Idle[i];
						B = Idle[j];
					}
				}
			}
			Check(A && B, TEXT("Two idle wolves to test abilities on"));
			if (!A || !B)
			{
				Finish();
				return false;
			}
			State->AbilityTarget = A;
			State->AbilityBystander = B;

			PlacePlayerNear(A->GetActorLocation(), 900.0f);
			Combat->SetTarget(A);
			Check(Abilities->UseAbility(Rend) == EMMOAbilityResult::OutOfRange, TEXT("Melee abilities need the target in reach"));

			PlacePlayerNear(A->GetActorLocation(), 120.0f);
			B->SetActorLocation(A->GetActorLocation() + Player->GetActorRightVector() * 110.0f, false, nullptr, ETeleportType::TeleportPhysics);
			Player->FaceActor(A);
			State->HealthMark = A->GetHealth()->GetCurrentHealth();
			Check(Abilities->UseAbility(Rend) == EMMOAbilityResult::Success && A->GetHealth()->GetCurrentHealth() < State->HealthMark, TEXT("Rending Strike hits the target"));
			Check(A->IsBleeding(), TEXT("Rending Strike makes the target bleed"));
			float Remaining = 0.0f, Duration = 0.0f;
			Abilities->GetCooldown(Rend, Remaining, Duration);
			Check(Remaining > Rend->Cooldown - 1.0f, TEXT("Rending Strike goes on cooldown"));
			Check(Abilities->UseAbility(Bash) == EMMOAbilityResult::NotReady, TEXT("The global cooldown blocks a second ability right away"));
			Check(Combat->IsAutoAttacking(), TEXT("Attack abilities start auto-attack"));
			Combat->StopAutoAttack();
			NextStep();
			break;
		}

		case 42:
			if (Elapsed() > Player->GetAbilities()->GlobalCooldown + 0.1f)
			{
				AMMOCreature* A = State->AbilityTarget.Get();
				Combat->StopAutoAttack();
				Check(A && Player->GetAbilities()->UseAbility(UMMOAbilityDefinition::FindById(TEXT("ShoulderBash"))) == EMMOAbilityResult::Success && A->IsStunned(),
					TEXT("After the global cooldown, Shoulder Bash stuns the target"));
				Combat->StopAutoAttack();
				State->Mark = A ? A->GetLastAttackTime() : 0.0;
				State->HealthMark = A ? A->GetHealth()->GetCurrentHealth() : 0.0f;
				NextStep();
			}
			break;

		case 43:
			if (Elapsed() > 1.6f)
			{
				AMMOCreature* A = State->AbilityTarget.Get();
				AMMOCreature* B = State->AbilityBystander.Get();
				Check(A && A->GetLastAttackTime() == State->Mark && A->IsStunned(), TEXT("A stunned wolf doesn't attack"));
				Check(A && A->GetHealth()->GetCurrentHealth() < State->HealthMark, TEXT("Bleed keeps damaging the target over time"));
				const float AHealth = A ? A->GetHealth()->GetCurrentHealth() : 0.0f;
				const float BHealth = B ? B->GetHealth()->GetCurrentHealth() : 0.0f;
				Player->FaceActor(A);
				Check(Player->GetAbilities()->UseAbility(UMMOAbilityDefinition::FindById(TEXT("CleavingArc"))) == EMMOAbilityResult::Success, TEXT("Cleaving Arc swings"));
				Check(A && B && A->GetHealth()->GetCurrentHealth() < AHealth && B->GetHealth()->GetCurrentHealth() < BHealth, TEXT("Cleaving Arc hits both wolves in front"));
				for (AMMOCreature* Wolf2 : { A, B })
				{
					if (Wolf2 && !Wolf2->IsDead())
					{
						Wolf2->GetHealth()->ApplyDamage(100000.0f, Player);
					}
				}
				Combat->StopAutoAttack();
				NextStep();
			}
			break;

		case 44: // cast time: moving interrupts
			if (Elapsed() > Player->GetAbilities()->GlobalCooldown + 0.1f)
			{
				UMMOAbilityDefinition* SecondWind = UMMOAbilityDefinition::FindById(TEXT("SecondWind"));
				// stray wolves may still be biting; keep the heal measurement exact
				PlayerHealth->SetInvulnerable(true);
				PlayerHealth->RestoreHealth(PlayerHealth->GetMaxHealth() * 0.4f);
				State->HealthMark = PlayerHealth->GetCurrentHealth();
				FText CastName;
				float Progress = 0.0f;
				Check(SecondWind && Player->GetAbilities()->UseAbility(SecondWind) == EMMOAbilityResult::CastStarted && Player->GetActiveCast(CastName, Progress),
					TEXT("Second Wind starts a cast"));
				Player->SetActorLocation(Player->GetActorLocation() + Player->GetActorForwardVector() * 100.0f, false, nullptr, ETeleportType::TeleportPhysics);
				NextStep();
			}
			break;

		case 45:
			if (Elapsed() > 0.2f)
			{
				UMMOAbilityDefinition* SecondWind = UMMOAbilityDefinition::FindById(TEXT("SecondWind"));
				float Remaining = 0.0f, Duration = 0.0f;
				Player->GetAbilities()->GetCooldown(SecondWind, Remaining, Duration);
				Check(!Player->GetAbilities()->IsCasting() && FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), State->HealthMark, 1.0f) && Remaining <= 0.0f,
					TEXT("Moving interrupts the cast: no heal and no cooldown spent"));
				Check(Player->GetAbilities()->UseAbility(SecondWind) == EMMOAbilityResult::CastStarted, TEXT("Second Wind can be cast again right away"));
				HUDOf(Player)->ToggleAbilities();
				Check(HUDOf(Player)->GetHUDWidget()->IsAbilitiesOpen(), TEXT("K opens the abilities window"));
				State->bFlag = false;
				NextStep();
			}
			break;

		case 46:
			if (!State->bFlag && Elapsed() > 0.7f)
			{
				Shot(TEXT("12_Abilities"));
				State->bFlag = true;
			}
			else if (State->bFlag && Elapsed() > 1.8f)
			{
				UMMOAbilityDefinition* SecondWind = UMMOAbilityDefinition::FindById(TEXT("SecondWind"));
				const float Expected = FMath::Min(PlayerHealth->GetMaxHealth(), State->HealthMark + PlayerHealth->GetMaxHealth() * SecondWind->SelfHealFraction);
				Check(!Player->GetAbilities()->IsCasting() && FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), Expected, 5.0f),
					FString::Printf(TEXT("Standing still, Second Wind finishes and heals 30%% (%.0f -> %.0f)"), State->HealthMark, PlayerHealth->GetCurrentHealth()));
				float Remaining = 0.0f, Duration = 0.0f;
				Player->GetAbilities()->GetCooldown(SecondWind, Remaining, Duration);
				Check(Remaining > SecondWind->Cooldown - 3.0f, TEXT("Second Wind goes on its cooldown"));
				HUDOf(Player)->CloseAllWindows();
				PlayerHealth->SetInvulnerable(false);
				State->bFlag = false;
				NextStep();
			}
			break;

		case 47: // gathering (Milestone 8)
		{
			AMMOGatherNode* Vein = FindNode(World, TEXT("CopperOre"));
			if (!Vein)
			{
				UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST: no gathering nodes in this map, skipping the profession checks"));
				State->Step = 53;
				State->StepStart = Now();
				break;
			}
			UMMOProfessionComponent* Professions = Player->GetProfessions();
			Check(Professions->GetSkill(EMMOProfession::Mining) == 1 && Professions->GetSkill(EMMOProfession::Smithing) == 1, TEXT("Professions start at skill 1"));
			UMMOItemDefinition* Ore = UMMOItemDefinition::FindById(TEXT("CopperOre"));
			State->CounterMark = Inventory->CountItem(Ore);
			State->Node = Vein;

			PlacePlayerNear(Vein->GetActorLocation(), 160.0f);
			Check(Player->TryInteract(Vein) && Professions->IsBusy(), TEXT("Right-clicking a copper vein starts mining"));
			FText CastName;
			float Progress = 0.0f;
			Check(Player->GetActiveCast(CastName, Progress) && CastName.ToString().Contains(TEXT("Copper")), FString::Printf(TEXT("Cast bar shows \"%s\""), *CastName.ToString()));
			NextStep();
			break;
		}

		case 48:
			if (Elapsed() > (State->Node.IsValid() ? State->Node->GatherTime : 2.5f) + 0.3f)
			{
				UMMOProfessionComponent* Professions = Player->GetProfessions();
				UMMOItemDefinition* Ore = UMMOItemDefinition::FindById(TEXT("CopperOre"));
				AMMOGatherNode* Vein = State->Node.Get();
				Check(!Professions->IsBusy() && Inventory->CountItem(Ore) > State->CounterMark, FString::Printf(TEXT("Mining yields copper ore (+%d)"), Inventory->CountItem(Ore) - State->CounterMark));
				Check(Vein && Vein->IsDepleted() && Vein->IsHidden() && !Vein->CanInteract(Player), TEXT("A mined vein disappears until it respawns"));
				Check(Professions->GetSkill(EMMOProfession::Mining) == 2, TEXT("Mining skill rises to 2"));

				// moving interrupts gathering
				AMMOGatherNode* Other = FindNode(World, TEXT("CopperOre"), Vein);
				State->Node = Other;
				State->CounterMark = Inventory->CountItem(Ore);
				if (Other)
				{
					PlacePlayerNear(Other->GetActorLocation(), 160.0f);
					Player->TryInteract(Other);
					Player->SetActorLocation(Player->GetActorLocation() + FVector(0.0f, 0.0f, 0.0f) + Player->GetActorRightVector() * 80.0f, false, nullptr, ETeleportType::TeleportPhysics);
				}
				NextStep();
			}
			break;

		case 49:
			if (Elapsed() > 0.3f)
			{
				UMMOProfessionComponent* Professions = Player->GetProfessions();
				UMMOItemDefinition* Ore = UMMOItemDefinition::FindById(TEXT("CopperOre"));
				Check(State->Node.IsValid() && !Professions->IsBusy() && !State->Node->IsDepleted() && Inventory->CountItem(Ore) == State->CounterMark,
					TEXT("Moving interrupts mining (nothing gathered, vein stays)"));

				if (AMMOGatherNode* Duskroot = FindNode(World, TEXT("Duskroot")))
				{
					PlacePlayerNear(Duskroot->GetActorLocation(), 160.0f);
					Check(Professions->StartGather(Duskroot) == EMMOProfessionResult::SkillTooLow && !Professions->IsBusy(), TEXT("Duskroot needs Herbalism 15"));
				}

				// crafting at the forge
				AMMOCraftingStation* Forge = FindStation(World, EMMOProfession::Smithing);
				UMMOItemDefinition* Bar = UMMOItemDefinition::FindById(TEXT("CopperBar"));
				Check(Forge && Forge->Recipes.Num() >= 4 && Bar, TEXT("The forge has smithing recipes"));
				if (!Forge || !Bar)
				{
					Finish();
					return false;
				}
				PlacePlayerNear(Forge->GetActorLocation(), 180.0f);
				Check(Player->TryInteract(Forge) && HUDOf(Player)->GetHUDWidget()->IsCraftingOpen() && HUDOf(Player)->GetHUDWidget()->GetOpenStation() == Forge,
					TEXT("Using the forge opens the crafting window"));

				Inventory->RemoveItem(Ore, Inventory->CountItem(Ore));
				Inventory->RemoveItem(Bar, Inventory->CountItem(Bar));
				Inventory->AddItem(Ore, 5);
				State->SkillMark = Professions->GetSkill(EMMOProfession::Smithing);
				UMMORecipeDefinition* Smelt = UMMORecipeDefinition::FindById(TEXT("SmeltCopper"));
				Check(Smelt && Professions->StartCraft(Smelt, 1000, Forge) == EMMOProfessionResult::Started && Professions->GetCraftsRemaining() == 2,
					TEXT("Craft All queues as many bars as the ore allows (5 ore -> 2 bars)"));
				NextStep();
			}
			break;

		case 50:
		{
			UMMORecipeDefinition* Smelt = UMMORecipeDefinition::FindById(TEXT("SmeltCopper"));
			if (Elapsed() > (Smelt ? Smelt->CraftTime : 2.0f) * 2.0f + 0.4f)
			{
				UMMOProfessionComponent* Professions = Player->GetProfessions();
				UMMOItemDefinition* Ore = UMMOItemDefinition::FindById(TEXT("CopperOre"));
				UMMOItemDefinition* Bar = UMMOItemDefinition::FindById(TEXT("CopperBar"));
				Check(!Professions->IsBusy() && Inventory->CountItem(Bar) == 2 && Inventory->CountItem(Ore) == 1, TEXT("Smelting turned 4 ore into 2 bars"));
				Check(Professions->GetSkill(EMMOProfession::Smithing) == State->SkillMark + 2, TEXT("Each craft raised Smithing"));
				Check(Professions->StartCraft(UMMORecipeDefinition::FindById(TEXT("CopperforgedBlade")), 1, FindStation(World, EMMOProfession::Smithing)) == EMMOProfessionResult::SkillTooLow,
					TEXT("High-level recipes need more skill"));
				Check(Professions->StartCraft(UMMORecipeDefinition::FindById(TEXT("CopperBand")), 1, FindStation(World, EMMOProfession::Smithing)) == EMMOProfessionResult::SkillTooLow
					|| Professions->CanCraft(UMMORecipeDefinition::FindById(TEXT("CopperBand"))) == EMMOProfessionResult::MissingIngredients,
					TEXT("Recipes need their reagents"));
				Professions->SetSkill(EMMOProfession::Smithing, 5);
				Check(Professions->CanCraft(UMMORecipeDefinition::FindById(TEXT("CopperBand"))) == EMMOProfessionResult::MissingIngredients, TEXT("Copper Band needs 3 bars (only 2 carried)"));
				State->bFlag = false;
				NextStep();
			}
			break;
		}

		case 51: // screenshot the forge, then cook at the cookfire
			if (!State->bFlag && Elapsed() > 0.5f)
			{
				Shot(TEXT("13_Crafting"));
				State->bFlag = true;
			}
			else if (State->bFlag && Elapsed() > 0.9f)
			{
				State->bFlag = false;
				AMMOCraftingStation* Fire = FindStation(World, EMMOProfession::Cooking);
				UMMOItemDefinition* Meat = UMMOItemDefinition::FindById(TEXT("RawWolfMeat"));
				Check(Fire && Meat, TEXT("A cookfire exists"));
				if (!Fire || !Meat)
				{
					Finish();
					return false;
				}
				PlacePlayerNear(Fire->GetActorLocation(), 180.0f);
				Inventory->RemoveItem(Meat, Inventory->CountItem(Meat));
				Inventory->AddItem(Meat, 2);
				Check(Player->GetProfessions()->StartCraft(UMMORecipeDefinition::FindById(TEXT("RoastWolfHaunch")), 1, Fire) == EMMOProfessionResult::Started, TEXT("Start roasting wolf meat"));
				NextStep();
			}
			break;

		case 52:
			if (Elapsed() > 2.4f)
			{
				UMMOItemDefinition* Haunch = UMMOItemDefinition::FindById(TEXT("RoastedWolfHaunch"));
				UMMOItemDefinition* Meat = UMMOItemDefinition::FindById(TEXT("RawWolfMeat"));
				Check(Inventory->CountItem(Haunch) >= 1 && Inventory->CountItem(Meat) == 0 && Player->GetProfessions()->GetSkill(EMMOProfession::Cooking) == 2,
					TEXT("Cooking turns 2 raw meat into a Roasted Wolf Haunch (+1 Cooking)"));
				Check(!HUDOf(Player)->GetHUDWidget()->IsCraftingOpen(), TEXT("Walking away from the forge closed its crafting window"));
				NextStep();
			}
			break;

		case 53: // Rustvein Mine (Milestone 9): through the entrance
		{
			AMMOPortal* Entrance = FindPortal(World, TEXT("MineEntrance"));
			AMMORustQueen* Queen = nullptr;
			for (TActorIterator<AMMORustQueen> It(World); It; ++It)
			{
				Queen = *It;
			}
			if (!Entrance || !Queen)
			{
				UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST: no dungeon in this map, skipping the mine checks"));
				State->Step = 60;
				State->StepStart = Now();
				break;
			}
			State->Queen = Queen;
			PlacePlayerNear(Entrance->GetActorLocation(), 200.0f);
			Check(Player->TryInteract(Entrance) && FVector::Dist(Player->GetActorLocation(), Entrance->Destination) < 300.0f, TEXT("The boarded entrance leads into the Rustvein Mine"));
			NextStep();
			break;
		}

		case 54:
			if (Elapsed() > 1.0f)
			{
				AMMORustQueen* Queen = State->Queen.Get();
				Check(Player->GetExploration()->HasDiscovered(TEXT("RustveinDepths")), TEXT("Entering discovers the Rustvein Depths"));
				int32 Beetles = 0;
				AMMORustback* Victim = nullptr;
				for (TActorIterator<AMMORustback> It(World); It; ++It)
				{
					if (!It->IsA<AMMORustQueen>())
					{
						++Beetles;
						Victim = *It;
					}
				}
				Check(Beetles >= 6, FString::Printf(TEXT("The mine is infested (%d Rustback Skitterers)"), Beetles));
				Check(Victim && Victim->QuestTag == TEXT("Rustback") && Victim->GetTargetLevel() >= 4 && Victim->GetHealth()->GetMaxHealth() > Wolf->GetHealth()->GetMaxHealth(),
					TEXT("Rustbacks are a tougher creature family than wolves"));
				Check(Queen->bIsBoss && Queen->GetTargetLevel() == 6 && Queen->GetHealth()->GetMaxHealth() >= 1000.0f, TEXT("Grindmaw the Rust Queen is a level 6 boss"));

				PlayerHealth->RestoreHealth(PlayerHealth->GetMaxHealth());
				PlacePlayerNear(Queen->GetActorLocation(), 380.0f);
				State->Detonations = 0;
				State->LastEmote.Reset();
				NextStep();
			}
			break;

		case 55: // pulled: boss frame, then the first eruption appears under the player
		{
			AMMORustQueen* Queen = State->Queen.Get();
			PlayerHealth->RestoreHealth(PlayerHealth->GetMaxHealth());
			if (!State->bFlag && Queen && Queen->IsInCombat())
			{
				State->bFlag = true;
				UMMOHUDWidget* Widget = HUDOf(Player)->GetHUDWidget();
				Widget->UpdateBossFrame(Player, 2.0f);
				Check(Widget->GetShownBoss() == Queen, TEXT("The boss health frame appears when the fight starts"));
				Check(State->LastEmote.Contains(TEXT("Grindmaw")), FString::Printf(TEXT("Boss emote on pull: \"%s\""), *State->LastEmote));
			}
			if (State->bFlag && Queen && Queen->GetActiveTelegraph())
			{
				AMMOTelegraph* Telegraph = Queen->GetActiveTelegraph();
				State->Telegraph = Telegraph;
				Check(FVector::Dist2D(Telegraph->GetActorLocation(), Player->GetActorLocation()) < Telegraph->GetRadius(), TEXT("A Rust Eruption circle appears under the player"));
				State->bFlag = false;
				NextStep();
			}
			else if (Elapsed() > 9.0f)
			{
				Check(false, TEXT("The boss engaged and cast Rust Eruption"));
				State->Step = 60;
			}
			break;
		}

		case 56: // standing in it hurts; for the next one, step out
			PlayerHealth->RestoreHealth(PlayerHealth->GetMaxHealth());
			if (State->Detonations == 1 && !State->bFlag)
			{
				Check(State->bLastDetonationHitPlayer, TEXT("Standing in the glow when it erupts hurts"));
				State->bFlag = true;
			}
			if (State->bFlag && State->Queen.IsValid() && State->Queen->GetActiveTelegraph() && State->Queen->GetActiveTelegraph() != State->Telegraph.Get())
			{
				AMMOTelegraph* Next = State->Queen->GetActiveTelegraph();
				State->Telegraph = Next;
				PlacePlayerNear(Next->GetActorLocation(), Next->GetRadius() + 300.0f);
				State->bFlag = false;
				NextStep();
			}
			else if (Elapsed() > 14.0f)
			{
				Check(false, TEXT("Rust Eruption repeats"));
				NextStep();
			}
			break;

		case 57:
			PlayerHealth->RestoreHealth(PlayerHealth->GetMaxHealth());
			if (State->Detonations >= 2)
			{
				AMMORustQueen* Queen = State->Queen.Get();
				Check(!State->bLastDetonationHitPlayer, TEXT("Stepping out of the circle avoids the eruption"));

				// at 60% she calls her brood
				UMMOHealthComponent* QueenHealth = Queen->GetHealth();
				QueenHealth->ApplyDamage(QueenHealth->GetCurrentHealth() - QueenHealth->GetMaxHealth() * 0.55f, Player);
				NextStep();
			}
			else if (Elapsed() > 4.0f)
			{
				Check(false, TEXT("The second eruption went off"));
				NextStep();
			}
			break;

		case 58:
			PlayerHealth->RestoreHealth(PlayerHealth->GetMaxHealth());
			if (Elapsed() > 0.5f && !State->bFlag)
			{
				AMMORustQueen* Queen = State->Queen.Get();
				int32 Alive = 0;
				for (const TWeakObjectPtr<AMMOCreature>& Add : Queen->GetBrood())
				{
					Alive += Add.IsValid() && !Add->IsDead() ? 1 : 0;
				}
				Check(Alive == Queen->BroodCount && State->LastEmote.Contains(TEXT("brood")), FString::Printf(TEXT("At 60%% she calls %d Rustlings"), Alive));

				// run away: she leashes home, her brood vanishes and she heals
				State->bFlag = true;
				if (AMMOPortal* Exit = FindPortal(World, TEXT("MineExit")))
				{
					PlacePlayerNear(Exit->GetActorLocation(), 250.0f);
				}
			}
			else if (State->bFlag)
			{
				AMMORustQueen* Queen = State->Queen.Get();
				const bool bReset = StateOf(Queen) == EMMOCreatureAIState::Idle && Queen->GetHealth()->GetHealthPercent() >= 1.0f;
				if (bReset)
				{
					Check(Queen->GetBrood().Num() == 0, TEXT("Leaving the fight resets the boss: brood gone, full health"));
					State->bFlag = false;
					NextStep();
				}
				else if (Elapsed() > 25.0f)
				{
					Check(false, TEXT("The boss reset after the player left"));
					NextStep();
				}
			}
			break;

		case 59: // the real kill: enrage, death, loot, and the way out
		{
			AMMORustQueen* Queen = State->Queen.Get();
			PlayerHealth->RestoreHealth(PlayerHealth->GetMaxHealth());
			if (!State->bFlag)
			{
				State->bFlag = true;
				PlacePlayerNear(Queen->GetActorLocation(), 380.0f);
				State->Mark = Now();
				break;
			}
			if (Queen->IsInCombat() && !Queen->IsEnraged() && !Queen->IsDead())
			{
				UMMOHealthComponent* QueenHealth = Queen->GetHealth();
				QueenHealth->ApplyDamage(QueenHealth->GetCurrentHealth() - QueenHealth->GetMaxHealth() * 0.2f, Player);
				State->Mark = Now();
				break;
			}
			if (Queen->IsEnraged() && !Queen->IsDead() && Now() - State->Mark > 0.3)
			{
				Check(Queen->AttackCooldown < 2.0f && State->LastEmote.Contains(TEXT("enraged")), TEXT("Below 25% she enrages and attacks faster"));
				UMMOLootTable::ForceNextDrop(UMMOItemDefinition::FindById(TEXT("MandibleCleaver")));
				Queen->GetHealth()->ApplyDamage(1000000.0f, Player);
				const bool bLoot = Queen->GetLoot()->GetItems().ContainsByPredicate([](const FMMOItemStack& S) { return S.Item && S.Item->ItemId == TEXT("MandibleCleaver"); });
				Check(Queen->IsDead() && Queen->IsLootable() && bLoot, TEXT("Grindmaw dies and drops her loot"));
				bool bBroodGone = true;
				for (const TWeakObjectPtr<AMMOCreature>& Add : Queen->GetBrood())
				{
					bBroodGone &= !Add.IsValid() || Add->IsDead();
				}
				Check(bBroodGone, TEXT("Her brood dies with her"));

				AMMOPortal* Exit = FindPortal(World, TEXT("MineExit"));
				PlacePlayerNear(Exit->GetActorLocation(), 220.0f);
				PlayerHealth->ApplyDamage(1.0f, Queen); // a last hit from the fight
				Check(Player->IsInCombat() && !Player->TryInteract(Exit), TEXT("The exit can't be used mid-fight"));
				State->bFlag = false;
				NextStep();
			}
			else if (Now() - State->Mark > 12.0)
			{
				Check(false, TEXT("The boss fight reached the enrage phase"));
				NextStep();
			}
			break;
		}

		case 60: // out of combat: leave the mine (also the entry point for maps without a dungeon)
			if (!State->Queen.IsValid())
			{
				NextStep();
				break;
			}
			if (Elapsed() > 7.0f)
			{
				AMMOPortal* Exit = FindPortal(World, TEXT("MineExit"));
				PlacePlayerNear(Exit->GetActorLocation(), 220.0f);
				Check(!Player->IsInCombat() && Player->TryInteract(Exit) && FVector::Dist(Player->GetActorLocation(), Exit->Destination) < 300.0f, TEXT("Out of combat, the exit leads back to daylight"));
				NextStep();
			}
			break;

		case 61: // Milestone 10: game menu, settings, controls and the guide
		{
			UMMOHUDWidget* Widget = HUDOf(Player)->GetHUDWidget();
			UMMOSettingsSubsystem* Options = World->GetGameInstance()->GetSubsystem<UMMOSettingsSubsystem>();
			Check(Options && Options->Get() && Options->Get()->MouseSensitivity > 0.0f && Options->Get()->MasterVolume >= 0.0f, TEXT("Player settings are loaded"));

			HUDOf(Player)->CloseAllWindows();
			Combat->ClearTarget();
			Player->DoClearTarget();
			Check(Widget->IsGameMenuOpen(), TEXT("Esc with nothing open and no target opens the game menu"));
			HUDOf(Player)->OpenSettings();
			Check(Widget->IsSettingsOpen() && !Widget->IsGameMenuOpen(), TEXT("Settings opens from the game menu"));
			Player->DoClearTarget();
			Check(!Widget->IsSettingsOpen(), TEXT("Esc closes the settings"));
			HUDOf(Player)->OpenControls();
			Check(Widget->IsControlsOpen(), TEXT("The controls list opens from the game menu"));
			Player->DoClearTarget();

			if (Options && Options->Get()->bShowTutorial)
			{
				Widget->UpdateGuide(Player);
				const bool bZone = State->bHasZones;
				Check(!bZone || Widget->GetGuideTipId().IsNone(), FString::Printf(TEXT("After the full playthrough every guide tip is done (current: %s)"), *Widget->GetGuideTipId().ToString()));
				Check(Player->HasTutorial(TEXT("Kill")) && Player->HasTutorial(TEXT("Loot")) && Player->HasTutorial(TEXT("Backpack")) && Player->HasTutorial(TEXT("Abilities")),
					TEXT("Killing, looting, the backpack and the abilities window were all noticed by the guide"));
			}
			HUDOf(Player)->ToggleGameMenu();
			State->bFlag = false;
			NextStep();
			break;
		}

		case 62:
			if (!State->bFlag && Elapsed() > 0.4f)
			{
				Shot(TEXT("14_GameMenu"));
				State->bFlag = true;
			}
			else if (State->bFlag && Elapsed() > 0.8f)
			{
				HUDOf(Player)->CloseAllWindows();
				HUDOf(Player)->OpenSettings();
				State->bFlag = false;
				NextStep();
			}
			break;

		case 63:
			if (!State->bFlag && Elapsed() > 0.4f)
			{
				Shot(TEXT("15_Settings"));
				State->bFlag = true;
			}
			else if (State->bFlag && Elapsed() > 0.8f)
			{
				HUDOf(Player)->CloseAllWindows();
				State->bFlag = false;
				NextStep();
			}
			break;

		case 64: // save, scramble everything, load: the character comes back exactly as it was (Milestone 5)
		{
			UMMOSaveSubsystem* Saves = World->GetGameInstance()->GetSubsystem<UMMOSaveSubsystem>();
			Check(Saves && !Saves->IsPersistenceEnabled(), TEXT("Autosave is off during the self-test (real progress is never overwritten)"));
			if (!Saves)
			{
				Finish();
				return false;
			}
			const FString Slot = TEXT("MMO_SelfTest");
			Saves->DeleteSave(Slot);

			// give the character something worth saving in every category
			UMMOQuestLogComponent* QuestLog = Player->GetQuestLog();
			if (UMMOQuestDefinition* Fangs = UMMOQuestDefinition::FindById(TEXT("FangsForTheForge")))
			{
				QuestLog->AcceptQuest(Fangs);
			}
			if (UMMOQuestDefinition* Eyes = UMMOQuestDefinition::FindById(TEXT("EyesOnTheWild")))
			{
				QuestLog->AcceptQuest(Eyes);
			}
			Inventory->AddItem(WolfFang, 3);
			Progression->AddXP(7);
			PlayerHealth->ApplyDamage(15.0f, nullptr);

			const UMMOSaveGame* Before = UMMOSaveSubsystem::Capture(Player, GetTransientPackage());
			const FVector Location = Player->GetActorLocation();
			const float HealthBefore = PlayerHealth->GetCurrentHealth();
			const float ArmorBefore = PlayerHealth->GetArmor();
			const int32 Owned = CountOwned(Player);
			Check(Before->Inventory.Num() > 0 && Before->Equipment.Num() > 0 && Before->ActiveQuests.Num() + Before->CompletedQuests.Num() > 0 && (Before->Discovered.Num() > 0 || !State->bHasZones),
				TEXT("Character has items, gear, quests and discoveries to save"));
			Check(Saves->SaveCharacter(Player, Slot) && Saves->HasSave(Slot), TEXT("Character saves to a slot on disk"));

			// scramble
			Inventory->ClearInventory();
			Inventory->SetCurrency(0);
			Equipment->ClearEquipment();
			Player->GetActionBar()->RestoreSlots({});
			Player->GetProfessions()->RestoreSkills({});
			Player->RestoreTutorialFlags({});
			Progression->ResetProgression();
			QuestLog->RestoreState({}, {});
			Player->GetExploration()->RestoreDiscovered({});
			PlacePlayerNear(Location, 2500.0f);
			Check(CountOwned(Player) == 0 && Progression->GetLevel() == 1, TEXT("Character wiped before loading"));

			Check(Saves->LoadCharacter(Player, Slot), TEXT("Character loads from the slot"));
			const UMMOSaveGame* After = UMMOSaveSubsystem::Capture(Player, GetTransientPackage());
			Check(After->Level == Before->Level && After->XP == Before->XP, FString::Printf(TEXT("Level and XP restored (level %d, %d XP)"), After->Level, After->XP));
			Check(After->Currency == Before->Currency && CountOwned(Player) == Owned, TEXT("Currency and every item restored"));
			bool bSameSlots = After->Inventory.Num() == Before->Inventory.Num();
			for (int32 i = 0; bSameSlots && i < Before->Inventory.Num(); ++i)
			{
				bSameSlots = After->Inventory[i].ItemId == Before->Inventory[i].ItemId && After->Inventory[i].Quantity == Before->Inventory[i].Quantity && After->Inventory[i].Slot == Before->Inventory[i].Slot;
			}
			Check(bSameSlots, TEXT("Backpack layout restored slot for slot"));
			bool bSameGear = After->Equipment.Num() == Before->Equipment.Num();
			for (int32 i = 0; bSameGear && i < Before->Equipment.Num(); ++i)
			{
				bSameGear = After->Equipment[i].ItemId == Before->Equipment[i].ItemId && After->Equipment[i].Slot == Before->Equipment[i].Slot;
			}
			Check(bSameGear && FMath::IsNearlyEqual(PlayerHealth->GetArmor(), ArmorBefore), TEXT("Worn gear and its stats restored"));
			Check(FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), HealthBefore, 0.5f), TEXT("Current health restored"));
			bool bSameQuests = After->ActiveQuests.Num() == Before->ActiveQuests.Num() && TSet<FName>(After->CompletedQuests).Includes(TSet<FName>(Before->CompletedQuests)) && After->CompletedQuests.Num() == Before->CompletedQuests.Num();
			for (int32 i = 0; bSameQuests && i < Before->ActiveQuests.Num(); ++i)
			{
				bSameQuests = After->ActiveQuests[i].QuestId == Before->ActiveQuests[i].QuestId && After->ActiveQuests[i].Counts == Before->ActiveQuests[i].Counts;
			}
			Check(bSameQuests, TEXT("Active quests (with progress) and completed quests restored"));
			Check(TSet<FName>(After->Discovered).Num() == Before->Discovered.Num() && TSet<FName>(After->Discovered).Includes(TSet<FName>(Before->Discovered)), TEXT("Discovered places restored"));
			Check(FVector::Dist(Player->GetActorLocation(), Location) < 60.0f, TEXT("Position restored"));
			Check(After->ProfessionSkills == Before->ProfessionSkills, TEXT("Profession skills restored"));
			Check(TSet<FName>(After->TutorialFlags).Num() == Before->TutorialFlags.Num() && Before->TutorialFlags.Num() > 0, TEXT("Guide progress restored"));
			Check(After->ActionBar == Before->ActionBar && !Player->GetActionBar()->GetSlot(State->ActionIndex).IsEmpty(), TEXT("Hotbar restored"));

			Check(Saves->DeleteSave(Slot) && !Saves->HasSave(Slot), TEXT("Save slot can be deleted"));
			Finish();
			return false;
		}
		}

		return true;
	}

	static void Run(const TArray<FString>& Args, UWorld* World)
	{
		if (State)
		{
			UE_LOG(LogMMO, Warning, TEXT("MMO SELFTEST already running"));
			return;
		}

		State = MakeUnique<FState>();
		State->World = World;
		State->bQuitWhenDone = Args.Contains(TEXT("quit"));
		State->bScreenshots = Args.Contains(TEXT("shots"));
		State->Player = Cast<AMMOCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		State->SafeOrigin = State->Player.IsValid() ? State->Player->GetActorLocation() : FVector::ZeroVector;

		// never let test actions reach the player's real save
		if (UMMOSaveSubsystem* Saves = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UMMOSaveSubsystem>() : nullptr)
		{
			if (Saves->IsPersistenceEnabled())
			{
				UE_LOG(LogMMO, Warning, TEXT("MMO SELFTEST: saving is turned off for the rest of this session. Launch with -MMONoSave for a fresh level-1 character."));
			}
			Saves->DisablePersistence();
		}

		// deterministic loot: only the drops the test forces (random rolls are covered by MMO.Items automation tests)
		if (IConsoleVariable* LootChance = IConsoleManager::Get().FindConsoleVariable(TEXT("mmo.Loot.ChanceMultiplier")))
		{
			State->SavedLootChance = LootChance->GetFloat();
			LootChance->Set(0.0f);
		}

		// the Grey Wolf nearest the player start is the main test subject; the closest pair of the others is the pack
		TArray<AMMOCreature*> Creatures;
		for (TActorIterator<AMMOCreature> It(World); It; ++It)
		{
			if (It->GetClass() == AMMODireWolf::StaticClass())
			{
				State->Dire = *It;
			}
			else if (It->IsA<AMMOGreyWolf>())
			{
				Creatures.Add(*It);
			}
		}
		Creatures.Sort([](const AMMOCreature& A, const AMMOCreature& B)
		{
			return FVector::DistSquared(A.GetActorLocation(), State->SafeOrigin) < FVector::DistSquared(B.GetActorLocation(), State->SafeOrigin);
		});
		if (Creatures.Num() > 0)
		{
			State->Wolf = Creatures[0];
		}
		// prefer a pair well away from the main wolf (so they don't join that fight); fall back to any pair
		for (const float MinSeparation : { 2500.0f, 0.0f })
		{
			float BestPair = TNumericLimits<float>::Max();
			for (int32 i = 1; i < Creatures.Num(); ++i)
			{
				for (int32 j = i + 1; j < Creatures.Num(); ++j)
				{
					const float D = FVector::Dist2D(Creatures[i]->GetActorLocation(), Creatures[j]->GetActorLocation());
					if (D < BestPair && FVector::Dist2D(Creatures[i]->GetActorLocation(), Creatures[0]->GetActorLocation()) >= MinSeparation)
					{
						BestPair = D;
						State->Pack = { Creatures[i], Creatures[j] };
					}
				}
			}
			if (State->Pack.Num() == 2)
			{
				break;
			}
		}
		for (TActorIterator<AMMODiscoveryZone> It(World); It; ++It)
		{
			State->bHasZones = true;
			break;
		}

		// remember the last damage the player took (to check armor)
		TWeakObjectPtr<AMMOCharacter> WeakPlayer = State->Player;
		State->CombatEventHandle = UMMOHealthComponent::OnAnyCombatEvent.AddLambda([WeakPlayer](const UMMOHealthComponent* Component, EMMOCombatEvent Event, float Amount)
		{
			if (State && Event == EMMOCombatEvent::Damage && WeakPlayer.IsValid() && Component == WeakPlayer->GetHealth() && Amount < 1000.0f)
			{
				State->LastPlayerDamage = Amount;
			}
		});

		State->DetonationHandle = AMMOTelegraph::OnAnyDetonation.AddLambda([WeakPlayer](const AMMOTelegraph* Telegraph, const TArray<AActor*>& Hit)
		{
			if (State)
			{
				++State->Detonations;
				State->bLastDetonationHitPlayer = Hit.Contains(WeakPlayer.Get());
			}
		});
		State->EmoteHandle = AMMORustQueen::OnBossEmote.AddLambda([](const AMMOCreature* Boss, const FText& Text)
		{
			if (State)
			{
				State->LastEmote += Text.ToString() + TEXT(" | ");
			}
		});

		UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST START (player: %s, wolf: %s, pack: %d)"), *GetNameSafe(State->Player.Get()), *GetNameSafe(State->Wolf.Get()), State->Pack.Num());

		if (!State->Player.IsValid() || !State->Wolf.IsValid() || State->Pack.Num() < 2)
		{
			Check(false, TEXT("Test world has a player and at least three wolves"));
			Finish();
			return;
		}

		State->StepStart = Now();
		State->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.0f);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GMMOSelfTestCommand(
	TEXT("mmo.selftest"),
	TEXT("Runs the gameplay loop self-test in the current world. Options: 'quit' exits when finished, 'shots' saves screenshots."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOSelfTest::Run));

#endif // !UE_BUILD_SHIPPING
