// Copyright Epic Games, Inc. All Rights Reserved.

#include "PanFightersCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PanFightersCameraManager.h"
#include "PanFighters.h"

APanFightersCharacter::APanFightersCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void APanFightersCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APanFightersCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APanFightersCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APanFightersCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APanFightersCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &APanFightersCharacter::LookInput);
	}
	else
	{
		UE_LOG(LogPanFighters, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void APanFightersCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void APanFightersCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void APanFightersCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void APanFightersCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void APanFightersCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void APanFightersCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void APanFightersCharacter::TriggerHitImpactFeedback(int32 ComboStep, float DamageMultiplier)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	APlayerCameraManager* BaseCamMgr = PC ? PC->PlayerCameraManager.Get() : UGameplayStatics::GetPlayerCameraManager(this, 0);
	APanFightersCameraManager* CamMgr = Cast<APanFightersCameraManager>(BaseCamMgr);
	if (!CamMgr)
	{
		return;
	}

	const bool bIsComboFinisher = (ComboStep >= 3);
	const bool bHasAjiBoost = (DamageMultiplier > 1.2f);

	float HitstopSeconds = 0.028f;
	float Dilation = 0.12f;
	float ShakeStrength = 0.35f;
	float ShakeDuration = 0.10f;

	if (bIsComboFinisher && bHasAjiBoost)
	{
		HitstopSeconds = 0.060f;
		Dilation = 0.04f;
		ShakeStrength = 1.45f;
		ShakeDuration = 0.24f;
	}
	else if (bIsComboFinisher || bHasAjiBoost)
	{
		HitstopSeconds = 0.048f;
		Dilation = 0.06f;
		ShakeStrength = 1.00f;
		ShakeDuration = 0.18f;
	}

	CamMgr->TriggerHitstop(HitstopSeconds, Dilation);
	CamMgr->TriggerImpactShake(ShakeStrength, ShakeDuration);
}
