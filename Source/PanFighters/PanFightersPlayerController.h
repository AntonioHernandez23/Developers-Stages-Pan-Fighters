// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/PanFightersMenuTypes.h"
#include "PanFightersPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class SPanFightersMasterWidget;
class ACameraActor;

/**
 *  Pan Fighters Player Controller
 *  Manages input mapping, startup 15-second splash screen,
 *  main menu (Jugar, Opciones, Salir), stage selection, first-person gameplay,
 *  pause system with 40% blur/dimming, and stage transitions.
 */
UCLASS(abstract, config="Game")
class PANFIGHTERS_API APanFightersPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	APanFightersPlayerController();

	/** Selects a character and transitions to stage selection */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Character")
	void SelectCharacter(EPanFightersCharacter InCharacter);

	/** Returns the currently selected character */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Character")
	EPanFightersCharacter GetSelectedCharacter() const { return SelectedCharacter; }

	/** Returns to the character selection menu */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void ReturnToCharacterSelect();

	/** Displays the match result screen (Victory or Defeat) with Retry, Main Menu, and Quit options */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Gameplay")
	void ShowMatchResult(bool bVictory);

	/** Retries the current match/stage */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Gameplay")
	void RetryMatch();

	/** Test Victory screen trigger (Debug hotkey V) */
	void TestVictoryAction();

	/** Test Defeat screen trigger (Debug hotkey B) */
	void TestDefeatAction();

	/** Test Hit Reaction screen flash trigger (Debug hotkey H) */
	void TestHitReactionAction();

	/** Test Damage to player trigger (Debug hotkey K) */
	void TestDamagePlayerAction();

	/** Test Damage to rival trigger (Debug hotkey J) */
	void TestDamageRivalAction();

	/** Returns whether the last match ended in victory */
	UFUNCTION(BlueprintPure, Category = "PanFighters|UI")
	bool IsLastMatchVictory() const { return bLastMatchVictory; }

	/** Returns current player health (0 to 100) */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Combat")
	float GetPlayerCurrentHealth() const { return CurrentPlayerHealth; }

	/** Returns current rival health (0 to 100) */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Combat")
	float GetRivalCurrentHealth() const { return CurrentRivalHealth; }

	/** Returns display name of active rival */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Combat")
	FText GetRivalCharacterName() const;

	/** Triggers screen reaction (red hit vignette and camera punch impulse) when player receives a hit */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Combat")
	void TriggerPlayerHitReaction(float DamageAmount = 25.0f);

	/** Applies damage directly to player (used for debug / testing) */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Combat")
	void ApplyDamageToPlayer(float DamageAmount);

	/** Applies damage directly to active rival (used for debug / testing) */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Combat")
	void ApplyDamageToRival(float DamageAmount);

	/** Finds the active rival rooster in the current level */
	AActor* GetActiveRival() const;

	/** Helper to read Health property from actor via reflection */
	float GetActorHealth(AActor* InActor) const;

	/** Helper to write Health property to actor via reflection */
	void SetActorHealth(AActor* InActor, float NewHealth);

	/** Per-frame player tick */
	virtual void PlayerTick(float DeltaTime) override;

	/** Switch to a specific stage in the game in first person view */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Stages")
	void SelectStage(EPanFightersStage Stage);

	/** Switch the current screen state */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void SetScreenState(EPanFightersScreenState NewState);

	/** Toggles the in-game pause menu (ESC or P) */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void ToggleInGameMenu();

	/** Resumes the game from pause */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void ResumeGame();

	/** Returns to the main menu */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void ReturnToMainMenu();

	/** Returns to the stage selection menu */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void ReturnToStageSelect();

	/** Quits the game */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void QuitGameAction();

	/** Called when master widget changes screen state */
	void OnScreenStateChanged(EPanFightersScreenState NewState);

	/** Returns current active stage */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Stages")
	EPanFightersStage GetCurrentStage() const { return CurrentStage; }

	/** Returns current screen state */
	UFUNCTION(BlueprintPure, Category = "PanFighters|UI")
	EPanFightersScreenState GetCurrentScreenState() const { return CurrentScreenState; }

	/** Startup splash loading bar duration in seconds (5 seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PanFighters|UI")
	float SplashDuration = 5.0f;

	/** Whether startup splash is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PanFighters|UI")
	bool bEnableStartupSplash = true;

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Currently selected playable character */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|Character")
	EPanFightersCharacter SelectedCharacter = EPanFightersCharacter::PolloBlanco;

	/** Whether the last match result was a victory */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|Gameplay")
	bool bLastMatchVictory = true;

	/** Current monitored player health (0 to 100) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|Combat")
	float CurrentPlayerHealth = 100.0f;

	/** Previous frame player health for hit detection */
	float PreviousPlayerHealth = 100.0f;

	/** Current monitored rival health (0 to 100) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|Combat")
	float CurrentRivalHealth = 100.0f;

	/** Timer after match decisive hit before popup appears */
	float MatchFinishTimer = 0.0f;

	/** Flag to prevent multiple win/loss triggers */
	bool bMatchResolved = false;

	/** Camera punch recoil trauma */
	float PunchTraumaPitch = 0.0f;
	float PunchTraumaRoll = 0.0f;

	/** Currently active stage */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|Stages")
	EPanFightersStage CurrentStage = EPanFightersStage::None;

	/** Currently active screen state */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|UI")
	EPanFightersScreenState CurrentScreenState = EPanFightersScreenState::None;

	/** Slate master UI instance */
	TSharedPtr<SPanFightersMasterWidget> MasterMenuWidget;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Finds the CameraActor for a given stage in the active world */
	ACameraActor* FindStageCamera(EPanFightersStage Stage);

	/** Finds the PlayerStart actor for a given stage in the active world */
	AActor* FindStagePlayerStart(EPanFightersStage Stage);
};
