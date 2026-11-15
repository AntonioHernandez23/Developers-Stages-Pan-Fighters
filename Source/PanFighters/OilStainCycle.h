// Stage 3 El Sarten: gotas de aceite que caen desde lo alto sobre el sarten, dejan una
// mancha amarilla que se va oscureciendo hasta parecer aceite quemado, y el ciclo se
// repite sin fin. Puramente visual: sin colision y sin daño a los combatientes.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tickable.h"
#include "OilStainCycle.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class PANFIGHTERS_API AOilStainCycle : public AActor, public FTickableGameObject
{
	GENERATED_BODY()

public:
	AOilStainCycle();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// FTickableGameObject: permite ver la animacion en el editor sin darle Play
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return bPrevisualizarEnEditor; }
	virtual TStatId GetStatId() const override;

	/** Duracion total del ciclo en segundos (2 minutos por defecto) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	float DuracionCiclo = 120.f;

	/** Cuantas gotas caen por ciclo (cada una deja su mancha) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	int32 MaxManchas = 18;

	/** Radio del area donde caen las gotas (unidades) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	float RadioArea = 1750.f;

	/** Altura desde la que caen las gotas */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	float AlturaCaida = 1100.f;

	/** Gravedad de la caida (unidades/s2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	float Gravedad = 2200.f;

	/** Radio de la gota */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	float RadioGota = 26.f;

	/** Radio minimo y maximo de cada mancha (unidades) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	float RadioManchaMin = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	float RadioManchaMax = 170.f;

	/** Altura de la mancha sobre el piso del sarten */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	float AlturaSobrePiso = 1.5f;

	/** Color del aceite recien caido (amarillo del borde del sarten) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	FLinearColor ColorAceiteNuevo = FLinearColor(0.920f, 0.640f, 0.120f, 1.f);

	/** Color final, aceite quemado */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	FLinearColor ColorQuemado = FLinearColor(0.150f, 0.065f, 0.020f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	TObjectPtr<UMaterialInterface> MaterialMancha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	TObjectPtr<UMaterialInterface> MaterialGota;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	TObjectPtr<UStaticMesh> MallaMancha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	TObjectPtr<UStaticMesh> MallaGota;

	/** Previsualizar la animacion dentro del editor (sin darle Play) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aceite")
	bool bPrevisualizarEnEditor = false;

private:
	void CrearComponentes();
	void ReiniciarCiclo();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Manchas;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDsMancha;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Gotas;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDsGota;

	UPROPERTY(Transient)
	TArray<int32> Fase;		// 0 = libre, 1 = cayendo, 2 = mancha

	UPROPERTY(Transient)
	TArray<float> Nacimiento;

	UPROPERTY(Transient)
	TArray<float> RadioBase;

	UPROPERTY(Transient)
	TArray<FVector> Destino;

	UPROPERTY(Transient)
	TArray<float> VelocidadCaida;

	float Reloj = 0.f;
	float LogAcum = 0.f;
	int32 Siguiente = 0;
	bool bListo = false;
};
