#include "OilStainCycle.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Tickable.h"

namespace
{
	const FName ParamColor(TEXT("Base Color"));

	constexpr int32 FaseLibre = 0;
	constexpr int32 FaseCayendo = 1;
	constexpr int32 FaseMancha = 2;
}

AOilStainCycle::AOilStainCycle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MallaCilindro(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (MallaCilindro.Succeeded())
	{
		MallaMancha = MallaCilindro.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MallaEsfera(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (MallaEsfera.Succeeded())
	{
		MallaGota = MallaEsfera.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatMancha(TEXT("/Game/Stages/El_Sarten/Materials/MI_ManchaGrasa.MI_ManchaGrasa"));
	if (MatMancha.Succeeded())
	{
		MaterialMancha = MatMancha.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatGota(TEXT("/Game/Stages/El_Sarten/Materials/MI_AceiteDorado.MI_AceiteDorado"));
	if (MatGota.Succeeded())
	{
		MaterialGota = MatGota.Object;
	}
}

void AOilStainCycle::BeginPlay()
{
	Super::BeginPlay();
	CrearComponentes();
	ReiniciarCiclo();
	UE_LOG(LogTemp, Warning, TEXT("[Aceite] BeginPlay %s: %d gotas, ciclo %.1fs, caida %.0f"), *GetName(), Gotas.Num(), DuracionCiclo, AlturaCaida);
}

void AOilStainCycle::CrearComponentes()
{
	if (bListo)
	{
		return;
	}

	for (UStaticMeshComponent* Vieja : Manchas)
	{
		if (IsValid(Vieja))
		{
			Vieja->DestroyComponent();
		}
	}
	for (UStaticMeshComponent* Vieja : Gotas)
	{
		if (IsValid(Vieja))
		{
			Vieja->DestroyComponent();
		}
	}
	Manchas.Reset(); MIDsMancha.Reset();
	Gotas.Reset(); MIDsGota.Reset();
	Fase.Reset(); Nacimiento.Reset(); RadioBase.Reset(); Destino.Reset(); VelocidadCaida.Reset();

	const int32 Total = FMath::Max(1, MaxManchas);
	for (int32 i = 0; i < Total; ++i)
	{
		UStaticMeshComponent* Mancha = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Mancha_%02d"), i));
		Mancha->SetupAttachment(GetRootComponent());
		Mancha->RegisterComponent();
		Mancha->SetMobility(EComponentMobility::Movable);
		Mancha->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mancha->SetGenerateOverlapEvents(false);
		Mancha->SetCastShadow(false);
		Mancha->bReceivesDecals = false;
		if (MallaMancha) { Mancha->SetStaticMesh(MallaMancha); }
		UMaterialInstanceDynamic* MIDMancha = MaterialMancha ? Mancha->CreateDynamicMaterialInstance(0, MaterialMancha) : nullptr;
		if (MIDMancha)
		{
			MIDMancha->SetVectorParameterValue(ParamColor, ColorAceiteNuevo);
			MIDMancha->SetScalarParameterValue(TEXT("Roughness"), 0.12f);
		}
		Mancha->SetVisibility(false, true);

		UStaticMeshComponent* Gota = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Gota_%02d"), i));
		Gota->SetupAttachment(GetRootComponent());
		Gota->RegisterComponent();
		Gota->SetMobility(EComponentMobility::Movable);
		Gota->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Gota->SetGenerateOverlapEvents(false);
		Gota->SetCastShadow(false);
		Gota->bReceivesDecals = false;
		if (MallaGota) { Gota->SetStaticMesh(MallaGota); }
		UMaterialInstanceDynamic* MIDGota = MaterialGota ? Gota->CreateDynamicMaterialInstance(0, MaterialGota) : nullptr;
		if (MIDGota)
		{
			MIDGota->SetVectorParameterValue(ParamColor, ColorAceiteNuevo);
			MIDGota->SetScalarParameterValue(TEXT("Roughness"), 0.1f);
		}
		Gota->SetVisibility(false, true);

		const float EscalaGota = FMath::Max(0.05f, RadioGota / 50.f);
		Gota->SetRelativeScale3D(FVector(EscalaGota, EscalaGota, EscalaGota * 1.9f));

		Manchas.Add(Mancha);
		MIDsMancha.Add(MIDMancha);
		Gotas.Add(Gota);
		MIDsGota.Add(MIDGota);
		Fase.Add(FaseLibre);
		Nacimiento.Add(-1.f);
		RadioBase.Add(0.f);
		Destino.Add(FVector::ZeroVector);
		VelocidadCaida.Add(0.f);
	}
	bListo = true;
}

bool AOilStainCycle::IsTickable() const
{
	if (!bPrevisualizarEnEditor || IsTemplate())
	{
		return false;
	}
	const UWorld* Mundo = GetWorld();
	return Mundo != nullptr && !Mundo->IsGameWorld();
}

TStatId AOilStainCycle::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(AOilStainCycle, STATGROUP_Tickables);
}

void AOilStainCycle::ReiniciarCiclo()
{
	for (int32 i = 0; i < Manchas.Num(); ++i)
	{
		if (IsValid(Manchas[i])) { Manchas[i]->SetVisibility(false, true); }
		if (IsValid(Gotas[i])) { Gotas[i]->SetVisibility(false, true); }
		Fase[i] = FaseLibre;
		Nacimiento[i] = -1.f;
		VelocidadCaida[i] = 0.f;
	}
	Reloj = 0.f;
	Siguiente = 0;
}

void AOilStainCycle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bListo)
	{
		CrearComponentes();
		return;
	}
	if (Manchas.Num() == 0)
	{
		return;
	}

	Reloj += DeltaSeconds;
	if (Reloj >= DuracionCiclo)
	{
		Reloj = FMath::Fmod(Reloj, DuracionCiclo);
		ReiniciarCiclo();
	}

	const float Intervalo = DuracionCiclo / FMath::Max(1, MaxManchas);

	// lanzar nuevas gotas
	while (Siguiente < Gotas.Num() && Reloj >= Siguiente * Intervalo)
	{
		const int32 Idx = Siguiente;
		++Siguiente;
		if (!IsValid(Gotas[Idx]) || Fase[Idx] != FaseLibre)
		{
			continue;
		}

		const float Angulo = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist = FMath::Sqrt(FMath::FRand()) * RadioArea;
		const float X = FMath::Cos(Angulo) * Dist;
		const float Y = FMath::Sin(Angulo) * Dist;

		Destino[Idx] = FVector(X, Y, AlturaSobrePiso);
		VelocidadCaida[Idx] = 0.f;
		Fase[Idx] = FaseCayendo;

		Gotas[Idx]->SetRelativeLocation(FVector(X, Y, AlturaCaida), false, nullptr, ETeleportType::TeleportPhysics);
		Gotas[Idx]->SetVisibility(true, true);
		if (MIDsGota[Idx]) { MIDsGota[Idx]->SetVectorParameterValue(ParamColor, ColorAceiteNuevo); }
	}

	// caida de las gotas
	for (int32 i = 0; i < Gotas.Num(); ++i)
	{
		if (Fase[i] != FaseCayendo || !IsValid(Gotas[i]))
		{
			continue;
		}

		VelocidadCaida[i] += Gravedad * DeltaSeconds;
		FVector Pos = Gotas[i]->GetRelativeLocation();
		Pos.Z -= VelocidadCaida[i] * DeltaSeconds;

		if (Pos.Z <= AlturaSobrePiso)
		{
			// impacto: se apaga la gota y nace la mancha
			Gotas[i]->SetVisibility(false, true);
			Fase[i] = FaseMancha;
			Nacimiento[i] = Reloj;
			RadioBase[i] = FMath::FRandRange(RadioManchaMin, RadioManchaMax);

			if (IsValid(Manchas[i]))
			{
				Manchas[i]->SetRelativeLocation(FVector(Destino[i].X, Destino[i].Y, AlturaSobrePiso), false, nullptr, ETeleportType::TeleportPhysics);
				Manchas[i]->SetRelativeRotation(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
				Manchas[i]->SetVisibility(true, true);
			}
		}
		else
		{
			Gotas[i]->SetRelativeLocation(Pos, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	// color y crecimiento de las manchas
	for (int32 i = 0; i < Manchas.Num(); ++i)
	{
		if (Fase[i] != FaseMancha || !IsValid(Manchas[i]))
		{
			continue;
		}

		const float Vida = FMath::Max(1.f, DuracionCiclo - Nacimiento[i]);
		const float Edad = FMath::Clamp((Reloj - Nacimiento[i]) / Vida, 0.f, 1.f);
		const float Curva = FMath::SmoothStep(0.f, 1.f, Edad);

		if (MIDsMancha[i])
		{
			MIDsMancha[i]->SetVectorParameterValue(ParamColor, FMath::Lerp(ColorAceiteNuevo, ColorQuemado, Curva));
		}

		const float Crecimiento = FMath::Lerp(0.5f, 1.0f, FMath::Min(1.f, Edad * 10.f));
		const float Extra = FMath::Lerp(1.0f, 1.25f, Curva);
		const float EscalaXY = (RadioBase[i] * Crecimiento * Extra) / 50.f;
		Manchas[i]->SetRelativeScale3D(FVector(EscalaXY, EscalaXY, 0.02f));
	}

	// resumen periodico para depuracion
	LogAcum += DeltaSeconds;
	if (LogAcum >= 5.f)
	{
		LogAcum = 0.f;
		int32 Cayendo = 0, Manchadas = 0;
		float Ultimo = 0.f;
		for (int32 j = 0; j < Fase.Num(); ++j)
		{
			if (Fase[j] == FaseCayendo) { ++Cayendo; }
			if (Fase[j] == FaseMancha) { Manchadas++; Ultimo = Reloj - Nacimiento[j]; }
		}
		UE_LOG(LogTemp, Warning, TEXT("[Aceite] t=%.1f cayendo=%d manchas=%d ultimaHace=%.1fs"), Reloj, Cayendo, Manchadas, Ultimo);
	}
}
