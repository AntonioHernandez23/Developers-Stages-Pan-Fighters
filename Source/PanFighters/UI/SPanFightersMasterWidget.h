// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "PanFightersMenuTypes.h"

class APanFightersPlayerController;
class SBorder;
class SOverlay;
class SProgressBar;
class STextBlock;
class SVerticalBox;

/**
 * Master Slate UI widget for Pan Fighters:
 * - 15-second animated loading Splash screen
 * - Main Menu (Jugar, Opciones, Salir)
 * - Options Menu (Audio, Graficos)
 * - Stage Selection Menu with image preview for each map
 * - In-Game HUD banner and 40% Blur/Dimmed Pause Menu
 */
class PANFIGHTERS_API SPanFightersMasterWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPanFightersMasterWidget)
		: _OwnerPlayerController(nullptr)
		, _SplashDuration(5.0f)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<APanFightersPlayerController>, OwnerPlayerController)
		SLATE_ARGUMENT(float, SplashDuration)
	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	/** Per-frame update */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Supports keyboard focus for hotkeys like ESC / P */
	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** Keyboard input handling for ESC and P key pause toggle */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Switches the current screen state */
	void SetScreenState(EPanFightersScreenState NewState);

	/** Notifies that a stage has been entered */
	void NotifyStageEntered(EPanFightersStage Stage, const FText& InStageName, const FText& InStageSubtitle);

	/** Triggers full screen red hit vignette reaction when player receives damage */
	void TriggerHitReaction(float DamageAmount = 25.0f);

	/** Returns the current screen state */
	EPanFightersScreenState GetScreenState() const { return CurrentState; }

	/** Returns current splash progress (0.0 to 1.0) */
	float GetSplashProgress() const { return SplashProgress; }

private:
	// Loads thumbnail images for the 3 stages
	void LoadStageImages();

	// Screen builders
	TSharedRef<SWidget> BuildSplashScreen();
	TSharedRef<SWidget> BuildMainMenu();
	TSharedRef<SWidget> BuildOptionsMenu();
	TSharedRef<SWidget> BuildCharacterSelectMenu();
	TSharedRef<SWidget> BuildStageSelectMenu();
	TSharedRef<SWidget> BuildInGameHUD();
	TSharedRef<SWidget> BuildInGameFightHUD();
	TSharedRef<SWidget> BuildInGameMenu();
	TSharedRef<SWidget> BuildMatchResultMenu(bool bVictory);

	// Helper widget builders
	TSharedRef<SWidget> BuildCharacterCard(
		EPanFightersCharacter CharacterId,
		const FText& CharacterName,
		const FText& CharacterSubtitle,
		const FText& Description,
		const FLinearColor& AccentColor,
		TSharedPtr<FSlateBrush> CharacterBrush
	);

	TSharedRef<SWidget> BuildStageCard(
		EPanFightersStage StageId,
		const FText& StageTitle,
		const FText& Subtitle,
		const FLinearColor& AccentColor,
		TSharedPtr<FSlateBrush> StageImageBrush
	);

	TSharedRef<SWidget> BuildMenuButton(
		const FText& ButtonText,
		FOnClicked OnClicked,
		const FLinearColor& AccentColor = FLinearColor(0.95f, 0.72f, 0.20f, 1.0f)
	);

	// Callbacks
	FReply OnPlayClicked();
	FReply OnOptionsClicked();
	FReply OnQuitClicked();
	FReply OnBackToMainMenuClicked();
	FReply OnCharacterClicked(EPanFightersCharacter CharacterId);
	FReply OnChangeCharacterClicked();
	FReply OnStageClicked(EPanFightersStage StageId);
	FReply OnResumeInGameClicked();
	FReply OnChangeStageFromInGameClicked();
	FReply OnRetryClicked();

	// Progress & Text helpers
	TOptional<float> GetProgressBarPercent() const;
	FText GetProgressPercentText() const;
	FText GetLoadingStatusText() const;

	// Fighting HUD & Health Bar helpers
	EVisibility GetInGameFightHUDVisibility() const;
	TOptional<float> GetPlayerHealthPercent() const;
	TOptional<float> GetPlayerGhostHealthPercent() const;
	FSlateColor GetPlayerHealthBarColor() const;
	FText GetPlayerHealthText() const;
	FText GetPlayerNameText() const;
	const FSlateBrush* GetPlayerAvatarBrush() const;

	TOptional<float> GetRivalHealthPercent() const;
	TOptional<float> GetRivalGhostHealthPercent() const;
	FSlateColor GetRivalHealthBarColor() const;
	FText GetRivalHealthText() const;
	FText GetRivalNameText() const;
	const FSlateBrush* GetRivalAvatarBrush() const;
	FSlateColor ComputeHealthColor(float Ratio) const;

	// Hit Reaction helpers
	FSlateColor GetHitReactionColor() const;

	// Owner controller
	TWeakObjectPtr<APanFightersPlayerController> OwnerPC;

	// Current State
	EPanFightersScreenState CurrentState = EPanFightersScreenState::None;

	// Splash timing (5 seconds)
	float SplashTotalDuration = 5.0f;
	float SplashElapsedTime = 0.0f;
	float SplashProgress = 0.0f;
	bool bSplashFinished = false;

	// InGame banner timing
	float BannerElapsedTime = 0.0f;
	float BannerDuration = 4.5f;
	bool bShowInGameBanner = false;
	FText ActiveStageName;
	FText ActiveStageSubtitle;

	// Settings state
	float MasterVolume = 1.0f;
	float MusicVolume = 0.8f;
	float SFXVolume = 0.9f;
	int32 GraphicsQualityLevel = 3;

	// Stage & Splash & Character Preview Image Brushes
	TSharedPtr<FSlateBrush> BrushRanchito;
	TSharedPtr<FSlateBrush> BrushCampoLlano;
	TSharedPtr<FSlateBrush> BrushSarten;
	TSharedPtr<FSlateBrush> BrushSplash;
	TSharedPtr<FSlateBrush> BrushPolloBlanco;
	TSharedPtr<FSlateBrush> BrushPolloChocolate;
	TSharedPtr<FSlateBrush> BrushPolloAmarillo;

	// In-game combat HUD & Hit reaction state
	float HitReactionAlpha = 0.0f;
	float PlayerGhostHealth = 100.0f;
	float RivalGhostHealth = 100.0f;
	float PlayerHealthBarShakeTime = 0.0f;

	// Root elements
	TSharedPtr<SOverlay> RootOverlay;
	TSharedPtr<SBorder> ScreenContentContainer;
	TSharedPtr<SBorder> InGameBannerContainer;
	TSharedPtr<SBorder> InGameHUDHelperContainer;
	TSharedPtr<SBorder> HitReactionContainer;
	TSharedPtr<SWidget> InGameFightHUDWidget;
};
