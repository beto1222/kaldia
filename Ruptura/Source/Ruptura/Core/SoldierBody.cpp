#include "Core/SoldierBody.h"
#include "Core/RupturaAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

using RupturaAssets::EShape;

void SoldierBody::Build(AActor* Owner, USceneComponent* Root, const FSoldierColors& Colors, float Scale, FSoldierBody& Out)
{
	const float S = Scale;
	Out.Root = Root;
	Out.Scale = Scale;

	UMaterialInterface* ArmorM = RupturaAssets::MakeMaterial(Owner, Colors.Armor, 0.45f, 0.35f, 0.f);
	UMaterialInterface* SuitM = RupturaAssets::MakeMaterial(Owner, Colors.Suit, 0.85f, 0.f, 0.f);
	UMaterialInterface* VisorM = RupturaAssets::MakeMaterial(Owner, Colors.Visor, 0.1f, 0.f, Colors.VisorGlow);
	UMaterialInterface* DarkM = RupturaAssets::MakeMaterial(Owner, FLinearColor(0.02f, 0.02f, 0.022f), 0.35f, 0.7f, 0.f);

	// Pernas (pivô no quadril para a passada)
	Out.HipL = RupturaAssets::AddPivot(Owner, Root, FVector(0.f, 12.f, 92.f) * S);
	RupturaAssets::AddPart(Owner, Out.HipL, EShape::Cube, FVector(0.f, 0.f, -44.f) * S, FVector(17.f, 17.f, 88.f) * S, SuitM);
	RupturaAssets::AddPart(Owner, Out.HipL, EShape::Cube, FVector(5.f, 0.f, -84.f) * S, FVector(30.f, 18.f, 12.f) * S, DarkM);
	RupturaAssets::AddPart(Owner, Out.HipL, EShape::Cube, FVector(4.f, 0.f, -40.f) * S, FVector(12.f, 19.f, 16.f) * S, ArmorM); // joelheira

	Out.HipR = RupturaAssets::AddPivot(Owner, Root, FVector(0.f, -12.f, 92.f) * S);
	RupturaAssets::AddPart(Owner, Out.HipR, EShape::Cube, FVector(0.f, 0.f, -44.f) * S, FVector(17.f, 17.f, 88.f) * S, SuitM);
	RupturaAssets::AddPart(Owner, Out.HipR, EShape::Cube, FVector(5.f, 0.f, -84.f) * S, FVector(30.f, 18.f, 12.f) * S, DarkM);
	RupturaAssets::AddPart(Owner, Out.HipR, EShape::Cube, FVector(4.f, 0.f, -40.f) * S, FVector(12.f, 19.f, 16.f) * S, ArmorM);

	// Tronco, colete e mochila
	RupturaAssets::AddPart(Owner, Root, EShape::Cube, FVector(0.f, 0.f, 118.f) * S, FVector(32.f, 30.f, 58.f) * S, SuitM);
	RupturaAssets::AddPart(Owner, Root, EShape::Cube, FVector(2.f, 0.f, 128.f) * S, FVector(40.f, 38.f, 40.f) * S, ArmorM);
	RupturaAssets::AddPart(Owner, Root, EShape::Cube, FVector(-24.f, 0.f, 126.f) * S, FVector(16.f, 30.f, 44.f) * S, DarkM);
	RupturaAssets::AddPart(Owner, Root, EShape::Cube, FVector(14.f, 10.f, 116.f) * S, FVector(8.f, 10.f, 14.f) * S, DarkM); // bolsas

	// Cabeça, capacete e visor
	RupturaAssets::AddPart(Owner, Root, EShape::Sphere, FVector(0.f, 0.f, 162.f) * S, FVector(24.f, 24.f, 26.f) * S, SuitM);
	RupturaAssets::AddPart(Owner, Root, EShape::Cube, FVector(0.f, 0.f, 171.f) * S, FVector(28.f, 28.f, 12.f) * S, ArmorM);
	Out.Visor = RupturaAssets::AddPart(Owner, Root, EShape::Cube, FVector(12.f, 0.f, 162.f) * S, FVector(5.f, 20.f, 6.f) * S, VisorM);

	// Braços (pivô no ombro)
	Out.ShoulderR = RupturaAssets::AddPivot(Owner, Root, FVector(0.f, -25.f, 144.f) * S);
	RupturaAssets::AddPart(Owner, Out.ShoulderR, EShape::Cube, FVector(0.f, 0.f, -24.f) * S, FVector(12.f, 12.f, 50.f) * S, SuitM);
	RupturaAssets::AddPart(Owner, Out.ShoulderR, EShape::Cube, FVector(0.f, 0.f, -4.f) * S, FVector(16.f, 16.f, 14.f) * S, ArmorM);
	Out.ShoulderL = RupturaAssets::AddPivot(Owner, Root, FVector(0.f, 25.f, 144.f) * S);
	RupturaAssets::AddPart(Owner, Out.ShoulderL, EShape::Cube, FVector(0.f, 0.f, -24.f) * S, FVector(12.f, 12.f, 50.f) * S, SuitM);
	RupturaAssets::AddPart(Owner, Out.ShoulderL, EShape::Cube, FVector(0.f, 0.f, -4.f) * S, FVector(16.f, 16.f, 14.f) * S, ArmorM);

	// Arma
	Out.GunRestLocation = FVector(16.f, -8.f, 126.f) * S;
	Out.GunPivot = RupturaAssets::AddPivot(Owner, Root, Out.GunRestLocation);
	Out.Gun = RupturaAssets::AddPart(Owner, Out.GunPivot, EShape::Cube, FVector(28.f, 0.f, 0.f) * S, FVector(62.f, 7.f, 12.f) * S, DarkM);
	RupturaAssets::AddPart(Owner, Out.GunPivot, EShape::Cube, FVector(14.f, 0.f, -10.f) * S, FVector(8.f, 6.f, 16.f) * S, DarkM); // empunhadura
	Out.Muzzle = RupturaAssets::AddPivot(Owner, Out.GunPivot, FVector(60.f, 0.f, 2.f) * S);

	Animate(Out, 0.f, 0.f, false);
}

void SoldierBody::SetGunLength(FSoldierBody& Body, float LengthCm)
{
	if (!Body.Gun || !Body.Muzzle)
	{
		return;
	}
	const float S = Body.Scale;
	Body.Gun->SetRelativeScale3D(FVector(LengthCm, 7.f, 12.f) * S / 100.f);
	Body.Gun->SetRelativeLocation(FVector(LengthCm * 0.5f - 3.f, 0.f, 0.f) * S);
	Body.Muzzle->SetRelativeLocation(FVector(LengthCm - 2.f, 0.f, 2.f) * S);
}

void SoldierBody::Animate(FSoldierBody& Body, float Speed, float DeltaSeconds, bool bAiming)
{
	if (!Body.HipL || !Body.HipR)
	{
		return;
	}

	const float Stride = FMath::Clamp(Speed / 450.f, 0.f, 1.25f);
	Body.Phase += DeltaSeconds * (3.f + Speed * 0.014f);
	const float Swing = FMath::Sin(Body.Phase) * 34.f * Stride;

	Body.HipL->SetRelativeRotation(FRotator(Swing, 0.f, 0.f));
	Body.HipR->SetRelativeRotation(FRotator(-Swing, 0.f, 0.f));

	// Braços: pitch +90 aponta o braço para frente.
	const float ArmPitch = bAiming ? 82.f : 55.f;
	const float ArmSway = bAiming ? 0.f : FMath::Sin(Body.Phase) * 6.f * Stride;
	Body.ShoulderR->SetRelativeRotation(FRotator(ArmPitch + ArmSway, 0.f, -8.f));
	Body.ShoulderL->SetRelativeRotation(FRotator(ArmPitch - ArmSway, -25.f, 10.f));

	// Coice da arma
	Body.Kick = FMath::FInterpTo(Body.Kick, 0.f, DeltaSeconds, 14.f);
	const float Bob = FMath::Abs(FMath::Sin(Body.Phase)) * 2.f * Stride;
	Body.GunPivot->SetRelativeLocation(Body.GunRestLocation + FVector(-Body.Kick * 6.f, 0.f, Bob + (bAiming ? 6.f : 0.f)) * Body.Scale);
	Body.GunPivot->SetRelativeRotation(FRotator(Body.Kick * 7.f + (bAiming ? 0.f : -12.f), 0.f, 0.f));
}
