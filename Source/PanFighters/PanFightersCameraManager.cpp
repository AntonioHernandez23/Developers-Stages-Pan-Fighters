// Copyright Epic Games, Inc. All Rights Reserved.

#include "PanFightersCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

APanFightersCameraManager::APanFightersCameraManager()
{
	// set the min/max pitch
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;
}

void APanFightersCameraManager::TriggerImpactShake(float Strength, float DurationSeconds)
{
	if (Strength <= 0.0f || DurationSeconds <= 0.0f)
	{
		return;
	}

	if (ShakeTimeRemaining <= 0.0f || Strength >= ShakeIntensity)
	{
		ShakeIntensity = Strength;
	}

	ShakeTotalDuration = FMath::Max(ShakeTotalDuration, DurationSeconds);
	if (ShakeTimeRemaining <= 0.0f)
	{
		ShakeTotalDuration = DurationSeconds;
		ShakeElapsed = 0.0f;
	}
	ShakeTimeRemaining = FMath::Max(ShakeTimeRemaining, DurationSeconds);
	++ShakeSeed;
}

void APanFightersCameraManager::TriggerHitstop(float RealDurationSeconds, float TimeDilation)
{
	if (RealDurationSeconds <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float ClampedDilation = FMath::Clamp(TimeDilation, 0.01f, 1.0f);
	const double NowReal = FPlatformTime::Seconds();
	const double CandidateEnd = NowReal + static_cast<double>(RealDurationSeconds);

	bHitstopActive = true;
	HitstopEndRealTime = FMath::Max(HitstopEndRealTime, CandidateEnd);
	UGameplayStatics::SetGlobalTimeDilation(World, ClampedDilation);
}

void APanFightersCameraManager::ClearHitstopIfElapsed()
{
	if (bHitstopActive && FPlatformTime::Seconds() >= HitstopEndRealTime)
	{
		RestoreTimeDilation();
	}
}

void APanFightersCameraManager::RestoreTimeDilation()
{
	if (!bHitstopActive)
	{
		return;
	}

	bHitstopActive = false;
	HitstopEndRealTime = 0.0;

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}
}

void APanFightersCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	ClearHitstopIfElapsed();

	Super::UpdateViewTarget(OutVT, DeltaTime);

	if (ShakeTimeRemaining <= 0.0f || ShakeTotalDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Use unscaled delta time so camera shake remains crisp during hitstop freeze.
	const float RealStep = FMath::Clamp(static_cast<float>(FApp::GetDeltaTime()), 0.001f, 0.05f);
	ShakeElapsed += RealStep;
	ShakeTimeRemaining = FMath::Max(0.0f, ShakeTimeRemaining - RealStep);

	const float Alpha = ShakeTimeRemaining / ShakeTotalDuration;
	const float Envelope = Alpha * Alpha;
	const float DirectionSign = (ShakeSeed % 2 == 0) ? 1.0f : -1.0f;

	const float PitchPunch = FMath::Sin(ShakeElapsed * 75.0f) * 2.2f * ShakeIntensity * Envelope;
	const float YawOsc = FMath::Cos(ShakeElapsed * 62.0f) * 1.3f * ShakeIntensity * Envelope * DirectionSign;
	const float RollPunch = FMath::Sin(ShakeElapsed * 88.0f) * 2.6f * ShakeIntensity * Envelope * DirectionSign;

	OutVT.POV.Rotation.Pitch = FMath::ClampAngle(OutVT.POV.Rotation.Pitch + PitchPunch, -89.0f, 89.0f);
	OutVT.POV.Rotation.Yaw += YawOsc;
	OutVT.POV.Rotation.Roll += RollPunch;

	const FVector LocalJolt(
		-3.5f * FMath::Abs(FMath::Cos(ShakeElapsed * 55.0f)) * ShakeIntensity * Envelope,
		FMath::Sin(ShakeElapsed * 95.0f) * 2.8f * ShakeIntensity * Envelope * DirectionSign,
		FMath::Cos(ShakeElapsed * 80.0f) * 2.4f * ShakeIntensity * Envelope
	);
	OutVT.POV.Location += OutVT.POV.Rotation.RotateVector(LocalJolt);
}

void APanFightersCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreTimeDilation();
	Super::EndPlay(EndPlayReason);
}
