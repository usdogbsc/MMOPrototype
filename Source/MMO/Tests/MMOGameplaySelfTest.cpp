// Copyright Epic Games, Inc. All Rights Reserved.

// Development-only end-to-end check of the gameplay loop (Milestones 1A, 1B and 2), run inside a live game world:
//   UnrealEditor.exe MMO.uproject -game -ExecCmds="mmo.selftest"
// It drives the real player/wolf objects and logs PASS/FAIL per step.
// Options: "quit" exits when finished, "shots" saves HUD screenshots to Saved/Screenshots (needs rendering).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
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
		float OtherHealthMark = 0.0f;
		int32 CounterMark = 0;
		int32 HitCount = 0;
		bool bFlag = false;
		float LastPlayerDamage = 0.0f;
		float MinTargetHit = TNumericLimits<float>::Max();
		int32 FillerAdded = 0;
		int32 ItemsBeforeDeath = 0;
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

	static int32 CountOwned(const AMMOCharacter* Player)
	{
		return Player->GetInventory()->GetTotalItemCount() + Player->GetEquipment()->GetEquippedCount();
	}

	static void Finish()
	{
		UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST COMPLETE: %d passed, %d failed -> %s"),
			State->Passes, State->Failures, State->Failures == 0 ? TEXT("SUCCESS") : TEXT("FAILURE"));

		UMMOHealthComponent::OnAnyCombatEvent.Remove(State->CombatEventHandle);
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
			Check(FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 100.0f) && FMath::IsNearlyEqual(PlayerHealth->GetMaxHealth(), 100.0f), TEXT("Player starts at 100/100 health"));
			Check(Progression->GetCurrentXP() == 0 && Progression->GetXPToNextLevel() == 100, TEXT("Player starts at 0/100 XP"));
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
				Check(Progression->GetCurrentXP() == Wolf->XPReward, TEXT("Killing the wolf awards its XP"));
				Check(Combat->StartAutoAttack() == EMMOAttackResult::TargetDead, TEXT("Cannot attack a dead target"));
				WolfHealth->ApplyDamage(10.0f, Player);
				Check(Progression->GetCurrentXP() == Wolf->XPReward, TEXT("XP is awarded exactly once"));

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
				PlacePlayerNear(WolfHome, 1500.0f);
				NextStep();
			}
			break;

		case 16: // respawn: the looted corpse sinks away and the wolf returns fresh
			if (!Wolf->IsDead())
			{
				Check(Elapsed() >= 4.0f, TEXT("Looted corpse is removed and the wolf respawns"));
				Check(WolfHealth->GetCurrentHealth() == WolfHealth->GetMaxHealth() && Wolf->IsTargetable(), TEXT("Respawned wolf has full health and is targetable"));
				Check(FVector::Dist2D(Wolf->GetActorLocation(), WolfHome) < 10.0f, TEXT("Respawned wolf is at its spawn point"));
				Check(!Wolf->GetLoot()->HasLoot(), TEXT("Respawned wolf carries no old loot"));
				Check(Combat->GetCurrentTarget() == nullptr, TEXT("Target cleared once the corpse despawned"));
				NextStep();
			}
			else if (Elapsed() > Wolf->RespawnDelay + 3.0f)
			{
				Check(false, TEXT("Wolf respawned in time"));
				NextStep();
			}
			break;

		case 17: // XP overflow + level up
			Progression->AddXP(90);
			Check(Progression->GetLevel() == 2 && Progression->GetCurrentXP() == Wolf->XPReward + 90 - 100 && Progression->GetXPToNextLevel() == 150, TEXT("Level up at 100 XP with overflow; level 2 needs 150"));
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
				Check(State->MinTargetHit >= 19.0f && State->MinTargetHit <= 24.0f, FString::Printf(TEXT("Greyfang swings hit for 19-24 at level 2 (lowest non-final hit %.0f)"), State->MinTargetHit));
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
			Check(BootsOnCorpse == 1 && Loot->GetItems().ContainsByPredicate([Boots](const FMMOItemStack& S) { return S.Item == Boots; }), TEXT("Loot that doesn't fit stays on the corpse"));
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
			NextStep();
			break;
		}

		case 26: // player death keeps every item
			PlayerHealth->ApplyDamage(10000.0f, State->Pack[0].Get());
			Check(Player->IsDead() && FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 0.0f), TEXT("Player dies at zero health (never below)"));
			Check(Combat->StartAutoAttack() == EMMOAttackResult::AttackerDead, TEXT("Dead player cannot attack"));
			State->bFlag = false;
			NextStep();
			break;

		case 27:
			if (!State->bFlag && Elapsed() > 0.6f)
			{
				State->bFlag = true;
				Shot(TEXT("6_Death"));
			}
			if (!Player->IsDead())
			{
				Check(Elapsed() >= Player->GetRespawnDelay() - 0.5f, TEXT("Player respawns after the respawn delay"));
				Check(PlayerHealth->GetCurrentHealth() == PlayerHealth->GetMaxHealth() && Player->InputEnabled(), TEXT("Respawned player has full health and control"));
				Check(Progression->GetLevel() == 2, TEXT("Level is kept through death"));
				Check(CountOwned(Player) == State->ItemsBeforeDeath && Equipment->GetEquipped(EMMOEquipmentSlot::MainHand).Item == Greyfang, TEXT("Inventory and equipment are kept through death"));
				Check(!InCombat(State->Pack[0].Get()), TEXT("Wolves disengage after the player dies"));
				Finish();
				return false;
			}
			else if (Elapsed() > Player->GetRespawnDelay() + 3.0f)
			{
				Check(false, TEXT("Player respawned in time"));
				Finish();
				return false;
			}
			break;
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

		// the lone wolf (nearest the player start) is the main test subject; the others form the pack
		TArray<AMMOCreature*> Creatures;
		for (TActorIterator<AMMOCreature> It(World); It; ++It)
		{
			Creatures.Add(*It);
		}
		Creatures.Sort([](const AMMOCreature& A, const AMMOCreature& B)
		{
			return FVector::DistSquared(A.GetActorLocation(), State->SafeOrigin) < FVector::DistSquared(B.GetActorLocation(), State->SafeOrigin);
		});
		if (Creatures.Num() > 0)
		{
			State->Wolf = Creatures[0];
		}
		for (int32 i = 1; i < Creatures.Num(); ++i)
		{
			State->Pack.Add(Creatures[i]);
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
