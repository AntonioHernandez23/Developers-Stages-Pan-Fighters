// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SPanFightersMasterWidget.h"
#include "PanFightersPlayerController.h"
#include "PanFighters.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Kismet/KismetSystemLibrary.h"

void SPanFightersMasterWidget::Construct(const FArguments& InArgs)
{
	OwnerPC = InArgs._OwnerPlayerController;
	SplashTotalDuration = FMath::Max(1.0f, InArgs._SplashDuration);
	SplashElapsedTime = 0.0f;
	SplashProgress = 0.0f;
	bSplashFinished = false;

	// Load preview images for each stage
	LoadStageImages();

	ChildSlot
	[
		SAssignNew(RootOverlay, SOverlay)

		// Main dynamic screen container (Splash, MainMenu, Options, StageSelect, InGameMenu)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(ScreenContentContainer, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.04f, 0.035f, 0.03f, 1.0f))
			.Padding(0.0f)
		]

		// In-Game Top Cinematic Banner (fades out after entering stage)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(0.0f, 40.0f, 0.0f, 0.0f)
		[
			SAssignNew(InGameBannerContainer, SBorder)
			.Visibility(EVisibility::Collapsed)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.05f, 0.04f, 0.03f, 0.92f))
			.Padding(FMargin(32.0f, 14.0f))
		]

		// In-Game Bottom-Right Helper Badge ([ESC] / [P] Menu)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(0.0f, 0.0f, 30.0f, 25.0f)
		[
			SAssignNew(InGameHUDHelperContainer, SBorder)
			.Visibility(EVisibility::Collapsed)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.08f, 0.06f, 0.04f, 0.85f))
			.Padding(FMargin(18.0f, 8.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("⚙ [ESC] o [P]  Pausar Juego")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.80f, 0.40f, 0.95f)))
			]
		]

		// In-Game Fighting Combat HUD (Health bars, Avatars, VS emblem, Controls)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(InGameFightHUDWidget, SOverlay)
			.Visibility(this, &SPanFightersMasterWidget::GetInGameFightHUDVisibility)

			// Top Health Bars & VS Emblem
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(0.0f, 20.0f, 0.0f, 0.0f)
			[
				BuildInGameFightHUD()
			]

			// Bottom Combat Controls Helper Pill
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(0.0f, 0.0f, 0.0f, 18.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.06f, 0.05f, 0.04f, 0.85f))
				.Padding(FMargin(20.0f, 7.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("🥊 [CLIC IZQ] Golpear / Jab   •   [CLIC DER] Gancho   •   [ESPACIO] Saltar   •   [ESC / P] Pausa")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.86f, 0.76f, 0.95f)))
				]
			]
		]

		// Red Damage / Hit Reaction Flash Overlay (Topmost visual layer)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(HitReactionContainer, SBorder)
			.Visibility(EVisibility::HitTestInvisible)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(this, &SPanFightersMasterWidget::GetHitReactionColor)
			.Padding(0.0f)
		]
	];

	// Start at Splash screen
	SetScreenState(EPanFightersScreenState::Splash);
}

void SPanFightersMasterWidget::LoadStageImages()
{
	const FString ContentDir = FPaths::ProjectContentDir() / TEXT("UI/Stages/");

	auto LoadImageBrush = [](const FString& Path, const FVector2D& Size) -> TSharedPtr<FSlateBrush>
	{
		FString ResolvedPath = Path;
		if (!FPaths::FileExists(ResolvedPath))
		{
			ResolvedPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/UI/Stages/"), FPaths::GetCleanFilename(Path));
		}
		if (!FPaths::FileExists(ResolvedPath))
		{
			ResolvedPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/UI/Splash/"), FPaths::GetCleanFilename(Path));
		}
		if (!FPaths::FileExists(ResolvedPath))
		{
			ResolvedPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/UI/"), FPaths::GetCleanFilename(Path));
		}

		if (FPaths::FileExists(ResolvedPath))
		{
			if (UTexture2D* LoadedTex = FImageUtils::ImportFileAsTexture2D(ResolvedPath))
			{
				LoadedTex->AddToRoot(); // Prevent garbage collection
				return MakeShareable(new FSlateImageBrush(LoadedTex, Size));
			}
		}
		return nullptr;
	};

	BrushRanchito = LoadImageBrush(ContentDir / TEXT("Img_ElRanchito.jpg"), FVector2D(280.0f, 135.0f));
	BrushCampoLlano = LoadImageBrush(ContentDir / TEXT("Img_ElCampoLlano.jpg"), FVector2D(280.0f, 135.0f));
	BrushSarten = LoadImageBrush(ContentDir / TEXT("Img_ElSarten.jpg"), FVector2D(280.0f, 135.0f));
	BrushSplash = LoadImageBrush(FPaths::ProjectContentDir() / TEXT("UI/Splash/Img_Splash.jpg"), FVector2D(1024.0f, 572.0f));

	const FString UIContentDir = FPaths::ProjectContentDir() / TEXT("UI/");
	BrushPolloBlanco = LoadImageBrush(UIContentDir / TEXT("Img_PolloBlanco.jpg"), FVector2D(240.0f, 260.0f));
	BrushPolloChocolate = LoadImageBrush(UIContentDir / TEXT("Img_PolloChocolate.jpg"), FVector2D(240.0f, 260.0f));
	BrushPolloAmarillo = LoadImageBrush(UIContentDir / TEXT("Img_PolloAmarillo.jpg"), FVector2D(240.0f, 260.0f));
}

void SPanFightersMasterWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Splash 15-second loading bar progression
	if (CurrentState == EPanFightersScreenState::Splash && !bSplashFinished)
	{
		SplashElapsedTime += InDeltaTime;
		SplashProgress = FMath::Clamp(SplashElapsedTime / SplashTotalDuration, 0.0f, 1.0f);

		if (SplashProgress >= 1.0f)
		{
			bSplashFinished = true;
			// Transition to Main Menu when 15 seconds finish
			SetScreenState(EPanFightersScreenState::MainMenu);
		}
	}

	// InGame cinematic banner fade timer
	if (bShowInGameBanner)
	{
		BannerElapsedTime += InDeltaTime;
		if (BannerElapsedTime >= BannerDuration)
		{
			bShowInGameBanner = false;
			if (InGameBannerContainer.IsValid())
			{
				InGameBannerContainer->SetVisibility(EVisibility::Collapsed);
			}
		}
	}

	// Decay hit reaction screen flash
	if (HitReactionAlpha > 0.0f)
	{
		HitReactionAlpha = FMath::Max(0.0f, HitReactionAlpha - InDeltaTime * 2.4f);
	}

	if (PlayerHealthBarShakeTime > 0.0f)
	{
		PlayerHealthBarShakeTime = FMath::Max(0.0f, PlayerHealthBarShakeTime - InDeltaTime);
	}

	// Smoothly interpolate ghost damage bars towards target health
	const float TargetPlayerHP = OwnerPC.IsValid() ? OwnerPC->GetPlayerCurrentHealth() : 100.0f;
	const float TargetRivalHP = OwnerPC.IsValid() ? OwnerPC->GetRivalCurrentHealth() : 100.0f;

	if (PlayerGhostHealth > TargetPlayerHP)
	{
		PlayerGhostHealth = FMath::FInterpTo(PlayerGhostHealth, TargetPlayerHP, InDeltaTime, 3.5f);
	}
	else
	{
		PlayerGhostHealth = TargetPlayerHP;
	}

	if (RivalGhostHealth > TargetRivalHP)
	{
		RivalGhostHealth = FMath::FInterpTo(RivalGhostHealth, TargetRivalHP, InDeltaTime, 3.5f);
	}
	else
	{
		RivalGhostHealth = TargetRivalHP;
	}
}

FReply SPanFightersMasterWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::P || Key == EKeys::M)
	{
		if (CurrentState == EPanFightersScreenState::InGame || CurrentState == EPanFightersScreenState::InGameMenu)
		{
			if (OwnerPC.IsValid())
			{
				OwnerPC->ToggleInGameMenu();
				return FReply::Handled();
			}
		}
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SPanFightersMasterWidget::SetScreenState(EPanFightersScreenState NewState)
{
	CurrentState = NewState;

	if (OwnerPC.IsValid())
	{
		OwnerPC->OnScreenStateChanged(NewState);
	}

	if (!ScreenContentContainer.IsValid())
	{
		return;
	}

	// In-game helper badge visibility
	if (InGameHUDHelperContainer.IsValid())
	{
		InGameHUDHelperContainer->SetVisibility(NewState == EPanFightersScreenState::InGame ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}

	switch (NewState)
	{
	case EPanFightersScreenState::Splash:
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor(0.04f, 0.035f, 0.03f, 1.0f));
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildSplashScreen());
		break;

	case EPanFightersScreenState::MainMenu:
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor(0.05f, 0.04f, 0.035f, 0.96f));
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildMainMenu());
		break;

	case EPanFightersScreenState::OptionsMenu:
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor(0.05f, 0.04f, 0.035f, 0.96f));
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildOptionsMenu());
		break;

	case EPanFightersScreenState::CharacterSelect:
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor(0.05f, 0.04f, 0.035f, 0.96f));
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildCharacterSelectMenu());
		break;

	case EPanFightersScreenState::StageSelect:
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor(0.05f, 0.04f, 0.035f, 0.96f));
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildStageSelectMenu());
		break;

	case EPanFightersScreenState::InGame:
		// When viewing the stage in-game, collapse the main menu overlay so the 3D first-person scene is fully visible!
		ScreenContentContainer->SetVisibility(EVisibility::Collapsed);
		PlayerGhostHealth = 100.0f;
		RivalGhostHealth = 100.0f;
		HitReactionAlpha = 0.0f;
		PlayerHealthBarShakeTime = 0.0f;
		break;

	case EPanFightersScreenState::InGameMenu:
		// Translucent container for the 40% blur/dimmed pause menu
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor::Transparent);
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildInGameMenu());
		break;

	case EPanFightersScreenState::MatchResult:
		// Translucent container for the 40% blur/dimmed victory/defeat result menu
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor::Transparent);
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		{
			const bool bVictory = OwnerPC.IsValid() ? OwnerPC->IsLastMatchVictory() : true;
			ScreenContentContainer->SetContent(BuildMatchResultMenu(bVictory));
		}
		break;

	default:
		break;
	}
}

void SPanFightersMasterWidget::NotifyStageEntered(EPanFightersStage Stage, const FText& InStageName, const FText& InStageSubtitle)
{
	ActiveStageName = InStageName;
	ActiveStageSubtitle = InStageSubtitle;
	BannerElapsedTime = 0.0f;
	bShowInGameBanner = true;

	if (InGameBannerContainer.IsValid())
	{
		InGameBannerContainer->SetVisibility(EVisibility::HitTestInvisible);
		InGameBannerContainer->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::Format(FText::FromString(TEXT("✦  ESCENARIO: {0}  ✦")), ActiveStageName))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.35f, 1.0f)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(ActiveStageSubtitle)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.85f, 0.80f, 0.90f)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Vista en Primera Persona • Explora con WASD y Ratón")))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.65f, 0.60f, 0.85f)))
			]
		);
	}
}

// --------------------------------------------------------------------------
// SPLASH SCREEN BUILDER (5 Seconds loading bar con Artwork de Pan Fighters)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildSplashScreen()
{
	return SNew(SOverlay)

	// Full Cover Background Splash Image
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		BrushSplash.IsValid()
		? SNew(SImage)
			.Image(BrushSplash.Get())
		: SNew(SImage)
			.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.04f, 0.035f, 0.03f, 1.0f)))
	]

	// Dark gradient overlay at the bottom for crystal-clear loading bar legibility
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Bottom)
	[
		SNew(SBox)
		.HeightOverride(160.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f))
			.Padding(0.0f)
		]
	]

	// Bottom Loading Section (Barra de carga de 5 segundos)
	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(0.0f, 0.0f, 0.0f, 40.0f)
	[
		SNew(SVerticalBox)

		// Dynamic Loading Message
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(STextBlock)
			.Text(this, &SPanFightersMasterWidget::GetLoadingStatusText)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.88f, 1.0f)))
		]

		// Progress Bar Frame Container (760px wide, 24px high)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(760.0f)
			.HeightOverride(24.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.10f, 0.08f, 0.06f, 1.0f))
				.Padding(FMargin(2.5f))
				[
					SNew(SProgressBar)
					.Percent(this, &SPanFightersMasterWidget::GetProgressBarPercent)
					.FillColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.65f, 0.15f, 1.0f)))
					.BorderPadding(FVector2D::ZeroVector)
				]
			]
		]

		// Percentage Readout
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SPanFightersMasterWidget::GetProgressPercentText)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.40f, 1.0f)))
		]
	];
}

// --------------------------------------------------------------------------
// MAIN MENU BUILDER (Jugar, Opciones, Salir)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildMainMenu()
{
	return SNew(SOverlay)

	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)

		// Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 22.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("PAN FIGHTERS")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 46))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.35f, 1.0f)))
		]

		// OPTION 1: JUGAR
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			BuildMenuButton(
				FText::FromString(TEXT("JUGAR")),
				FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnPlayClicked),
				FLinearColor(0.95f, 0.68f, 0.15f, 1.0f)
			)
		]

		// OPTION 2: OPCIONES
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			BuildMenuButton(
				FText::FromString(TEXT("OPCIONES")),
				FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnOptionsClicked),
				FLinearColor(0.80f, 0.52f, 0.22f, 1.0f)
			)
		]

		// OPTION 3: SALIR
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 20.0f)
		[
			BuildMenuButton(
				FText::FromString(TEXT("SALIR")),
				FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnQuitClicked),
				FLinearColor(0.70f, 0.25f, 0.20f, 1.0f)
			)
		]

		// Footer
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 15.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Pan Fighters © 2026 • Desarrollado con Unreal Engine 5")))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.50f, 0.45f, 0.75f)))
		]
	];
}

// --------------------------------------------------------------------------
// OPTIONS MENU BUILDER
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildOptionsMenu()
{
	return SNew(SOverlay)

	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)

		// Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("OPCIONES Y AJUSTES")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 34))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.35f, 1.0f)))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Personaliza tu experiencia de juego")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.80f, 0.75f, 0.9f)))
		]

		// Divider
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 12.0f, 0.0f, 24.0f)
		[
			SNew(SBox)
			.WidthOverride(480.0f)
			.HeightOverride(2.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.85f, 0.60f, 0.15f, 0.8f))
			]
		]

		// Settings Box Frame
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(520.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.08f, 0.07f, 0.06f, 0.95f))
				.Padding(FMargin(28.0f, 22.0f))
				[
					SNew(SVerticalBox)

					// Row 1: Volumen Maestro
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 16.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(0.45f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Volumen Maestro:")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.90f, 0.85f, 1.0f)))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(0.55f)
						.VAlign(VAlign_Center)
						[
							SNew(SSlider)
							.Value(MasterVolume)
							.OnValueChanged_Lambda([this](float Val) { MasterVolume = Val; })
						]
					]

					// Row 2: Volumen de Música
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 16.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(0.45f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Volumen Música:")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.90f, 0.85f, 1.0f)))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(0.55f)
						.VAlign(VAlign_Center)
						[
							SNew(SSlider)
							.Value(MusicVolume)
							.OnValueChanged_Lambda([this](float Val) { MusicVolume = Val; })
						]
					]

					// Row 3: Volumen de Efectos SFX
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 16.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(0.45f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Efectos Sonoros (SFX):")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.90f, 0.85f, 1.0f)))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(0.55f)
						.VAlign(VAlign_Center)
						[
							SNew(SSlider)
							.Value(SFXVolume)
							.OnValueChanged_Lambda([this](float Val) { SFXVolume = Val; })
						]
					]

					// Row 4: Calidad Gráfica
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 10.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(0.45f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Calidad Gráfica:")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.90f, 0.85f, 1.0f)))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(0.55f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Épica (Optimizada UE5)")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.75f, 0.25f, 1.0f)))
						]
					]
				]
			]
		]

		// Back Button
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 25.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(320.0f)
			.HeightOverride(48.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.60f, 0.18f, 1.0f)))
				.OnClicked(this, &SPanFightersMasterWidget::OnBackToMainMenuClicked)
				.ContentPadding(FMargin(16.0f, 8.0f))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("« VOLVER AL MENÚ PRINCIPAL")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// CHARACTER SELECT MENU BUILDER (Catálogo de 3 Gallos Boxeadores)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildCharacterSelectMenu()
{
	return SNew(SOverlay)

	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)

		// Main Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 20.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("SELECCIÓN DE PERSONAJE")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 36))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.35f, 1.0f)))
		]

		// 3 Character Cards in Horizontal Box
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)

			// ---------------- GALLO 1: POLLO BLANCO (NUESTRO PERSONAJE) ----------------
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12.0f, 0.0f)
			[
				BuildCharacterCard(
					EPanFightersCharacter::PolloBlanco,
					FText::FromString(TEXT("Pollo Blanco")),
					FText::FromString(TEXT("El Campeón Blanco")),
					FText::FromString(TEXT("Nuestro Personaje • Equilibrado y Veloz")),
					FLinearColor(0.95f, 0.75f, 0.25f, 1.0f),
					BrushPolloBlanco
				)
			]

			// ---------------- GALLO 2: POLLO CHOCOLATE ----------------
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12.0f, 0.0f)
			[
				BuildCharacterCard(
					EPanFightersCharacter::PolloChocolate,
					FText::FromString(TEXT("Pollo Chocolate")),
					FText::FromString(TEXT("Furia Campesina")),
					FText::FromString(TEXT("Poder Contundente • Gran Resistencia")),
					FLinearColor(0.85f, 0.48f, 0.22f, 1.0f),
					BrushPolloChocolate
				)
			]

			// ---------------- GALLO 3: POLLO AMARILLO ----------------
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12.0f, 0.0f)
			[
				BuildCharacterCard(
					EPanFightersCharacter::PolloAmarillo,
					FText::FromString(TEXT("Pollo Amarillo")),
					FText::FromString(TEXT("El Rayo Dorado")),
					FText::FromString(TEXT("Velocidad Relámpago • Pegada Rápida")),
					FLinearColor(1.0f, 0.82f, 0.15f, 1.0f),
					BrushPolloAmarillo
				)
			]
		]

		// Back to Main Menu Button
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 22.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(300.0f)
			.HeightOverride(42.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.55f, 0.20f, 1.0f)))
				.OnClicked(this, &SPanFightersMasterWidget::OnBackToMainMenuClicked)
				.ContentPadding(FMargin(16.0f, 6.0f))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("« VOLVER AL MENÚ PRINCIPAL")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// HELPER: BUILD CHARACTER CARD
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildCharacterCard(
	EPanFightersCharacter CharacterId,
	const FText& CharacterName,
	const FText& CharacterSubtitle,
	const FText& Description,
	const FLinearColor& AccentColor,
	TSharedPtr<FSlateBrush> CharacterBrush)
{
	return SNew(SBox)
	.WidthOverride(290.0f)
	.HeightOverride(430.0f)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.09f, 0.08f, 0.07f, 0.96f))
		.Padding(FMargin(14.0f, 14.0f))
		[
			SNew(SVerticalBox)

			// Character Name
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(CharacterName)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.90f, 1.0f)))
			]

			// Subtitle
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(CharacterSubtitle)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(AccentColor))
			]

			// Image Container (260x250)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(260.0f)
				.HeightOverride(250.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.04f, 0.035f, 0.03f, 1.0f))
					.Padding(FMargin(2.0f))
					[
						CharacterBrush.IsValid()
						? SNew(SImage)
							.Image(CharacterBrush.Get())
						: SNew(SImage)
							.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.10f, 0.08f, 1.0f)))
					]
				]
			]

			// Description
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 8.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(Description)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.80f, 0.75f, 0.90f)))
			]

			// Select Button
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SBox)
				.HeightOverride(38.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
					.ButtonColorAndOpacity(FSlateColor(AccentColor))
					.OnClicked(this, &SPanFightersMasterWidget::OnCharacterClicked, CharacterId)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("SELECCIONAR")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
					]
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// STAGE SELECT MENU BUILDER (Con imágenes reales de cada mapa)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildStageSelectMenu()
{
	return SNew(SOverlay)

	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)

		// Main Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 22.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("SELECCIÓN DE ESCENARIO")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 36))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.35f, 1.0f)))
		]

		// 3 Scenario Cards in Horizontal Box
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)

			// ---------------- ESCENARIO 1: EL RANCHITO ----------------
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12.0f, 0.0f)
			[
				BuildStageCard(
					EPanFightersStage::Stage1_ElRanchito,
					FText::FromString(TEXT("El Ranchito")),
					FText::FromString(TEXT("El Hogar del Gallo")),
					FLinearColor(0.95f, 0.70f, 0.15f, 1.0f),
					BrushRanchito
				)
			]

			// ---------------- ESCENARIO 2: EL CAMPO LLANO ----------------
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12.0f, 0.0f)
			[
				BuildStageCard(
					EPanFightersStage::Stage2_ElCampoLlano,
					FText::FromString(TEXT("El Campo Llano")),
					FText::FromString(TEXT("La Sabana Campesina")),
					FLinearColor(0.38f, 0.75f, 0.32f, 1.0f),
					BrushCampoLlano
				)
			]

			// ---------------- ESCENARIO 3: EL SARTÉN ----------------
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12.0f, 0.0f)
			[
				BuildStageCard(
					EPanFightersStage::Stage3_ElSarten,
					FText::FromString(TEXT("El Sartén")),
					FText::FromString(TEXT("El Fuego Culinario")),
					FLinearColor(0.92f, 0.38f, 0.14f, 1.0f),
					BrushSarten
				)
			]
		]

		// Navigation Buttons (Cambiar Personaje y Volver al Menú Principal)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 22.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(260.0f)
				.HeightOverride(42.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
					.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.65f, 0.30f, 1.0f)))
					.OnClicked(this, &SPanFightersMasterWidget::OnChangeCharacterClicked)
					.ContentPadding(FMargin(16.0f, 6.0f))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("« CAMBIAR PERSONAJE")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(260.0f)
				.HeightOverride(42.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
					.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.55f, 0.20f, 1.0f)))
					.OnClicked(this, &SPanFightersMasterWidget::OnBackToMainMenuClicked)
					.ContentPadding(FMargin(16.0f, 6.0f))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("« VOLVER AL MENÚ PRINCIPAL")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
					]
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// PAUSE MENU BUILDER (Difumina un 40% el fondo para resaltar el menú)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildInGameMenu()
{
	return SNew(SOverlay)

	// 1. Fondo difuminado + 40% oscurecimiento (SBackgroundBlur + Border a 0.40 alpha)
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SBackgroundBlur)
		.BlurStrength(22.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.40f)) // Difumina y oscurece un 40% el fondo
			.Padding(0.0f)
		]
	]

	// 2. Diálogo central del Menú de Pausa
	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(440.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.09f, 0.08f, 0.07f, 0.98f))
			.Padding(FMargin(32.0f, 26.0f))
			[
				SNew(SVerticalBox)

				// Header badge
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 18.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("✦   J U E G O   E N   P A U S A   ✦")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.40f, 1.0f)))
				]

				// BUTTON 1: CONTINUAR JUEGO (Reanudar)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.68f, 0.15f, 1.0f)))
						.OnClicked(this, &SPanFightersMasterWidget::OnResumeInGameClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("CONTINUAR JUEGO")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]

				// BUTTON 2: CAMBIAR DE ESCENARIO
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.70f, 0.30f, 1.0f)))
						.OnClicked(this, &SPanFightersMasterWidget::OnChangeStageFromInGameClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("SELECCIONAR OTRO ESCENARIO")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]

				// BUTTON 3: MENÚ DE INICIO
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.52f, 0.22f, 1.0f)))
						.OnClicked(this, &SPanFightersMasterWidget::OnBackToMainMenuClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("MENÚ DE INICIO")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]

				// BUTTON 4: SALIR DEL JUEGO
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.22f, 0.18f, 1.0f)))
						.OnClicked(this, &SPanFightersMasterWidget::OnQuitClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("SALIR DEL JUEGO")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// MATCH RESULT MENU BUILDER (Victoria / Derrota con 3 Opciones)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildMatchResultMenu(bool bVictory)
{
	const FText TitleText = bVictory ? FText::FromString(TEXT("✦   ¡ V I C T O R I A !   ✦")) : FText::FromString(TEXT("✦   D E R R O T A   ✦"));
	const FLinearColor TitleColor = bVictory ? FLinearColor(1.0f, 0.84f, 0.15f, 1.0f) : FLinearColor(0.95f, 0.25f, 0.20f, 1.0f);
	const FText SubtitleText = bVictory ? FText::FromString(TEXT("¡Has triunfado en el ruedo campesino!")) : FText::FromString(TEXT("¡Has caído en combate! Levántate y vuelve a luchar."));
	const FLinearColor RetryBtnColor = bVictory ? FLinearColor(0.95f, 0.68f, 0.15f, 1.0f) : FLinearColor(0.85f, 0.40f, 0.20f, 1.0f);

	return SNew(SOverlay)

	// 1. Fondo difuminado + 40% oscurecimiento (SBackgroundBlur + Border a 0.40 alpha)
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SBackgroundBlur)
		.BlurStrength(22.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.40f)) // Difumina y oscurece un 40% el fondo
			.Padding(0.0f)
		]
	]

	// 2. Diálogo central del Menú de Victoria / Derrota
	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(460.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.09f, 0.08f, 0.07f, 0.98f))
			.Padding(FMargin(32.0f, 28.0f))
			[
				SNew(SVerticalBox)

				// Header badge (Victoria o Derrota)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock)
					.Text(TitleText)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
					.ColorAndOpacity(FSlateColor(TitleColor))
				]

				// Subtitle
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 20.0f)
				[
					SNew(STextBlock)
					.Text(SubtitleText)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.85f, 0.75f, 0.90f)))
				]

				// OPTION 1: REINTENTAR
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(RetryBtnColor))
						.OnClicked(this, &SPanFightersMasterWidget::OnRetryClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("REINTENTAR")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]

				// OPTION 2: MENÚ PRINCIPAL
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.52f, 0.22f, 1.0f)))
						.OnClicked(this, &SPanFightersMasterWidget::OnBackToMainMenuClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("MENÚ PRINCIPAL")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]

				// OPTION 3: SALIR
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.22f, 0.18f, 1.0f)))
						.OnClicked(this, &SPanFightersMasterWidget::OnQuitClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("SALIR")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// HELPER: BUILD STAGE CARD (Con imagen real del mapa)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildStageCard(
	EPanFightersStage StageId,
	const FText& StageTitle,
	const FText& Subtitle,
	const FLinearColor& AccentColor,
	TSharedPtr<FSlateBrush> StageImageBrush)
{
	return SNew(SBox)
	.WidthOverride(300.0f)
	.HeightOverride(310.0f)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.09f, 0.08f, 0.07f, 0.96f))
		.Padding(FMargin(14.0f, 14.0f))
		[
			SNew(SVerticalBox)

			// Stage Title
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(StageTitle)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.90f, 1.0f)))
			]

			// Subtitle
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(Subtitle)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(AccentColor))
			]

			// Image Preview Container (270x135)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SNew(SBox)
				.WidthOverride(270.0f)
				.HeightOverride(135.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.12f, 0.10f, 0.08f, 1.0f))
					.Padding(FMargin(1.5f))
					[
						StageImageBrush.IsValid()
						? SNew(SImage).Image(StageImageBrush.Get())
						: SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
					]
				]
			]

			// Action Button: Entrar a Jugar en Primera Persona
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SBox)
				.HeightOverride(40.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
					.ButtonColorAndOpacity(FSlateColor(AccentColor))
					.OnClicked_Lambda([this, StageId]() { return OnStageClicked(StageId); })
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("ENTRAR A JUGAR")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
					]
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// HELPER: BUILD MENU BUTTON
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildMenuButton(
	const FText& ButtonText,
	FOnClicked OnClicked,
	const FLinearColor& AccentColor)
{
	return SNew(SBox)
	.WidthOverride(320.0f)
	.HeightOverride(48.0f)
	[
		SNew(SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
		.ButtonColorAndOpacity(FSlateColor(AccentColor))
		.OnClicked(OnClicked)
		.ContentPadding(FMargin(16.0f, 8.0f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(ButtonText)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			.ColorAndOpacity(FSlateColor(FLinearColor::White))
		]
	];
}

// --------------------------------------------------------------------------
// CALLBACKS
// --------------------------------------------------------------------------
FReply SPanFightersMasterWidget::OnPlayClicked()
{
	SetScreenState(EPanFightersScreenState::CharacterSelect);
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnCharacterClicked(EPanFightersCharacter CharacterId)
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->SelectCharacter(CharacterId);
	}
	else
	{
		SetScreenState(EPanFightersScreenState::StageSelect);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnChangeCharacterClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->ReturnToCharacterSelect();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::CharacterSelect);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnRetryClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->RetryMatch();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::InGame);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnOptionsClicked()
{
	SetScreenState(EPanFightersScreenState::OptionsMenu);
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnQuitClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->QuitGameAction();
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnBackToMainMenuClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->ReturnToMainMenu();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::MainMenu);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnStageClicked(EPanFightersStage StageId)
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->SelectStage(StageId);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnResumeInGameClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->ResumeGame();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::InGame);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnChangeStageFromInGameClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->ReturnToStageSelect();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::StageSelect);
	}
	return FReply::Handled();
}

// --------------------------------------------------------------------------
// PROGRESS & TEXT HELPERS
// --------------------------------------------------------------------------
TOptional<float> SPanFightersMasterWidget::GetProgressBarPercent() const
{
	return SplashProgress;
}

FText SPanFightersMasterWidget::GetProgressPercentText() const
{
	const int32 Percent = FMath::Clamp(FMath::RoundToInt(SplashProgress * 100.0f), 0, 100);
	return FText::Format(FText::FromString(TEXT("Cargando: {0}%")), FText::AsNumber(Percent));
}

FText SPanFightersMasterWidget::GetLoadingStatusText() const
{
	if (SplashProgress < 0.25f)
	{
		return FText::FromString(TEXT("Iniciando motor Pan Fighters..."));
	}
	else if (SplashProgress < 0.55f)
	{
		return FText::FromString(TEXT("Cargando escenarios campesinos..."));
	}
	else if (SplashProgress < 0.85f)
	{
		return FText::FromString(TEXT("Configurando texturas e iluminación..."));
	}
	else if (SplashProgress < 0.99f)
	{
		return FText::FromString(TEXT("Sincronizando controles..."));
	}
	else
	{
		return FText::FromString(TEXT("¡Listo para jugar!"));
	}
}

// --------------------------------------------------------------------------
// IN-GAME COMBAT HUD BUILDER (Health Bars, Avatars, VS Emblem)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildInGameHUD()
{
	return BuildInGameFightHUD();
}

TSharedRef<SWidget> SPanFightersMasterWidget::BuildInGameFightHUD()
{
	return SNew(SBox)
	.WidthOverride(1060.0f)
	.HeightOverride(70.0f)
	[
		SNew(SHorizontalBox)

		// ---------------- PLAYER FIGHTER (LEFT SIDE) ----------------
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)

			// Player Avatar Frame
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(56.0f)
				.HeightOverride(56.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.95f, 0.75f, 0.15f, 1.0f))
					.Padding(FMargin(2.5f))
					[
						SNew(SImage)
						.Image(this, &SPanFightersMasterWidget::GetPlayerAvatarBrush)
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpacer)
				.Size(FVector2D(10.0f, 1.0f))
			]

			// Name & Health Bar Column
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				// Player Name
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(this, &SPanFightersMasterWidget::GetPlayerNameText)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.88f, 0.35f, 1.0f)))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f))
				]

				// Player Health Bar Frame
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(24.0f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.08f, 0.07f, 0.06f, 0.98f))
						.Padding(FMargin(2.0f))
						[
							SNew(SOverlay)

							// Background track
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.BorderBackgroundColor(FLinearColor(0.12f, 0.07f, 0.07f, 0.95f))
								.Padding(0.0f)
							]

							// Ghost damage bar (amber)
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SProgressBar)
								.Percent(this, &SPanFightersMasterWidget::GetPlayerGhostHealthPercent)
								.FillColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.50f, 0.15f, 0.85f)))
								.BorderPadding(FVector2D::ZeroVector)
							]

							// Active health bar (green -> amber -> red)
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SProgressBar)
								.Percent(this, &SPanFightersMasterWidget::GetPlayerHealthPercent)
								.FillColorAndOpacity(this, &SPanFightersMasterWidget::GetPlayerHealthBarColor)
								.BorderPadding(FVector2D::ZeroVector)
							]

							// HP Text readout
							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SPanFightersMasterWidget::GetPlayerHealthText)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.98f, 0.95f, 1.0f)))
								.ShadowOffset(FVector2D(1.0f, 1.0f))
								.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f))
							]
						]
					]
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SSpacer)
			.Size(FVector2D(16.0f, 1.0f))
		]

		// ---------------- CENTER VS BADGE ----------------
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(74.0f)
			.HeightOverride(56.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.08f, 0.07f, 0.06f, 0.96f))
				.Padding(FMargin(4.0f, 3.0f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("VS")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
						.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.15f, 1.0f)))
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("COMBATE")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.65f, 0.25f, 0.95f)))
					]
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SSpacer)
			.Size(FVector2D(16.0f, 1.0f))
		]

		// ---------------- RIVAL / NPC FIGHTER (RIGHT SIDE) ----------------
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)

			// Name & Health Bar Column
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				// Rival Name
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(this, &SPanFightersMasterWidget::GetRivalNameText)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.48f, 0.25f, 1.0f)))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f))
				]

				// Rival Health Bar Frame
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(24.0f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.08f, 0.07f, 0.06f, 0.98f))
						.Padding(FMargin(2.0f))
						[
							SNew(SOverlay)

							// Background track
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.BorderBackgroundColor(FLinearColor(0.12f, 0.07f, 0.07f, 0.95f))
								.Padding(0.0f)
							]

							// Ghost damage bar (amber)
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SProgressBar)
								.Percent(this, &SPanFightersMasterWidget::GetRivalGhostHealthPercent)
								.FillColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.50f, 0.15f, 0.85f)))
								.BorderPadding(FVector2D::ZeroVector)
							]

							// Active health bar (green -> amber -> red)
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SProgressBar)
								.Percent(this, &SPanFightersMasterWidget::GetRivalHealthPercent)
								.FillColorAndOpacity(this, &SPanFightersMasterWidget::GetRivalHealthBarColor)
								.BorderPadding(FVector2D::ZeroVector)
							]

							// HP Text readout
							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SPanFightersMasterWidget::GetRivalHealthText)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.98f, 0.95f, 1.0f)))
								.ShadowOffset(FVector2D(1.0f, 1.0f))
								.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f))
							]
						]
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpacer)
				.Size(FVector2D(10.0f, 1.0f))
			]

			// Rival Avatar Frame
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(56.0f)
				.HeightOverride(56.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.85f, 0.35f, 0.20f, 1.0f))
					.Padding(FMargin(2.5f))
					[
						SNew(SImage)
						.Image(this, &SPanFightersMasterWidget::GetRivalAvatarBrush)
					]
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// COMBAT HUD & HIT REACTION HELPERS
// --------------------------------------------------------------------------
EVisibility SPanFightersMasterWidget::GetInGameFightHUDVisibility() const
{
	return (CurrentState == EPanFightersScreenState::InGame) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

TOptional<float> SPanFightersMasterWidget::GetPlayerHealthPercent() const
{
	if (OwnerPC.IsValid())
	{
		return FMath::Clamp(OwnerPC->GetPlayerCurrentHealth() / 100.0f, 0.0f, 1.0f);
	}
	return 1.0f;
}

TOptional<float> SPanFightersMasterWidget::GetPlayerGhostHealthPercent() const
{
	return FMath::Clamp(PlayerGhostHealth / 100.0f, 0.0f, 1.0f);
}

FSlateColor SPanFightersMasterWidget::GetPlayerHealthBarColor() const
{
	const float Ratio = GetPlayerHealthPercent().Get(1.0f);
	return ComputeHealthColor(Ratio);
}

FText SPanFightersMasterWidget::GetPlayerHealthText() const
{
	const int32 HP = FMath::Clamp(FMath::RoundToInt(OwnerPC.IsValid() ? OwnerPC->GetPlayerCurrentHealth() : 100.0f), 0, 100);
	return FText::Format(FText::FromString(TEXT("{0} / 100 HP")), FText::AsNumber(HP));
}

FText SPanFightersMasterWidget::GetPlayerNameText() const
{
	if (OwnerPC.IsValid())
	{
		switch (OwnerPC->GetSelectedCharacter())
		{
		case EPanFightersCharacter::PolloBlanco:
			return FText::FromString(TEXT("POLLO BLANCO"));
		case EPanFightersCharacter::PolloChocolate:
			return FText::FromString(TEXT("POLLO CHOCOLATE"));
		case EPanFightersCharacter::PolloAmarillo:
			return FText::FromString(TEXT("POLLO AMARILLO"));
		default:
			break;
		}
	}
	return FText::FromString(TEXT("POLLO BLANCO"));
}

const FSlateBrush* SPanFightersMasterWidget::GetPlayerAvatarBrush() const
{
	if (OwnerPC.IsValid())
	{
		switch (OwnerPC->GetSelectedCharacter())
		{
		case EPanFightersCharacter::PolloBlanco:
			return BrushPolloBlanco.IsValid() ? BrushPolloBlanco.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
		case EPanFightersCharacter::PolloChocolate:
			return BrushPolloChocolate.IsValid() ? BrushPolloChocolate.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
		case EPanFightersCharacter::PolloAmarillo:
			return BrushPolloAmarillo.IsValid() ? BrushPolloAmarillo.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
		default:
			break;
		}
	}
	return BrushPolloBlanco.IsValid() ? BrushPolloBlanco.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
}

TOptional<float> SPanFightersMasterWidget::GetRivalHealthPercent() const
{
	if (OwnerPC.IsValid())
	{
		return FMath::Clamp(OwnerPC->GetRivalCurrentHealth() / 100.0f, 0.0f, 1.0f);
	}
	return 1.0f;
}

TOptional<float> SPanFightersMasterWidget::GetRivalGhostHealthPercent() const
{
	return FMath::Clamp(RivalGhostHealth / 100.0f, 0.0f, 1.0f);
}

FSlateColor SPanFightersMasterWidget::GetRivalHealthBarColor() const
{
	const float Ratio = GetRivalHealthPercent().Get(1.0f);
	return ComputeHealthColor(Ratio);
}

FText SPanFightersMasterWidget::GetRivalHealthText() const
{
	const int32 HP = FMath::Clamp(FMath::RoundToInt(OwnerPC.IsValid() ? OwnerPC->GetRivalCurrentHealth() : 100.0f), 0, 100);
	return FText::Format(FText::FromString(TEXT("{0} / 100 HP")), FText::AsNumber(HP));
}

FText SPanFightersMasterWidget::GetRivalNameText() const
{
	if (OwnerPC.IsValid())
	{
		return OwnerPC->GetRivalCharacterName();
	}
	return FText::FromString(TEXT("GALLO RIVAL"));
}

const FSlateBrush* SPanFightersMasterWidget::GetRivalAvatarBrush() const
{
	return BrushPolloChocolate.IsValid() ? BrushPolloChocolate.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
}

FSlateColor SPanFightersMasterWidget::ComputeHealthColor(float Ratio) const
{
	if (Ratio > 0.50f)
	{
		return FSlateColor(FLinearColor(0.18f, 0.82f, 0.28f, 1.0f));
	}
	else if (Ratio > 0.25f)
	{
		return FSlateColor(FLinearColor(0.95f, 0.65f, 0.15f, 1.0f));
	}
	else
	{
		return FSlateColor(FLinearColor(0.92f, 0.18f, 0.15f, 1.0f));
	}
}

FSlateColor SPanFightersMasterWidget::GetHitReactionColor() const
{
	return FSlateColor(FLinearColor(0.85f, 0.04f, 0.04f, HitReactionAlpha));
}

void SPanFightersMasterWidget::TriggerHitReaction(float DamageAmount)
{
	HitReactionAlpha = FMath::Clamp(0.55f + (DamageAmount / 100.0f) * 0.40f, 0.45f, 0.95f);
	PlayerHealthBarShakeTime = 0.25f;
}
