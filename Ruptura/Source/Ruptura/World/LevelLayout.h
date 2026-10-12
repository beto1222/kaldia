#pragma once

#include "CoreMinimal.h"

/**
 * Planta da Missão 01 (unidades em cm, X = sentido da rua).
 *
 *  X -1000 .. 5000   Rua de entrada (prédios dos dois lados, Y ±1300)
 *  X  5000 .. 8000   Instalação militar (muro com portão em X 5500, posto da guarda, contêineres)
 *  X  8000           Porta do bunker (requer cartão)
 *  X  8100 .. 10100  Rampa descendo até o subsolo (Z 0 -> -800)
 *  X 10100 .. 13000  Ala de segurança subterrânea (sala do terminal ao norte)
 *  X 13000           Porta blindada (aberta pelo terminal)
 *  X 13000 .. 17000  Salão do reator: o HOSPEDEIRO e o núcleo do Protocolo Zero
 */
namespace RupturaLayout
{
	constexpr float StreetStartX = -1000.f;
	constexpr float StreetEndX = 8000.f;
	constexpr float StreetHalfWidth = 1300.f;
	constexpr float BoundaryY = 3100.f;
	constexpr float CompoundFrontX = 5500.f;
	constexpr float BunkerDoorX = 8000.f;
	constexpr float RampStartX = 8100.f;
	constexpr float RampEndX = 10100.f;
	constexpr float TunnelHalfWidth = 400.f;
	constexpr float SubFloorZ = -800.f;
	constexpr float Hall1EndX = 13000.f;
	constexpr float Hall1HalfWidth = 1600.f;
	constexpr float Hall1CeilingZ = -100.f;
	constexpr float ReactorEndX = 17000.f;
	constexpr float ReactorHalfWidth = 2000.f;
	constexpr float ReactorCeilingZ = 200.f;

	inline FVector KeycardLocation() { return FVector(7100.f, 1950.f, 92.f); }
	inline FVector TerminalLocation() { return FVector(12300.f, 2480.f, SubFloorZ); }
	inline FVector CoreLocation() { return FVector(15900.f, 0.f, SubFloorZ + 30.f); }
	inline FVector BossSpawn() { return FVector(15000.f, 0.f, SubFloorZ + 200.f); }
	inline FVector ExtractionLocation() { return FVector(0.f, 0.f, 0.f); }

	/** Área em que o chefe se movimenta. */
	inline FBox2D ReactorArena() { return FBox2D(FVector2D(13400.f, -1750.f), FVector2D(16900.f, 1750.f)); }
}
