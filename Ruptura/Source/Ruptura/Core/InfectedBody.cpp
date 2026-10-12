#include "Core/InfectedBody.h"
#include "Core/RupturaAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

using RupturaAssets::EShape;

FInfectedLook InfectedBody::RandomLook(FRandomStream& Rng, bool bMutated)
{
	// Pele acinzentada/amarelada de cadáver, com variação por indivíduo.
	static const FLinearColor Skins[] = {
		FLinearColor(0.30f, 0.27f, 0.24f), FLinearColor(0.24f, 0.22f, 0.2f), FLinearColor(0.33f, 0.3f, 0.25f),
		FLinearColor(0.16f, 0.13f, 0.11f), FLinearColor(0.27f, 0.25f, 0.25f),
	};
	// Roupas comuns da cidade evacuada + fardas da instalação.
	static const FLinearColor Shirts[] = {
		FLinearColor(0.06f, 0.065f, 0.07f), FLinearColor(0.2f, 0.19f, 0.17f), FLinearColor(0.07f, 0.08f, 0.055f),
		FLinearColor(0.12f, 0.03f, 0.03f), FLinearColor(0.05f, 0.07f, 0.11f), FLinearColor(0.25f, 0.25f, 0.24f),
	};
	static const FLinearColor Pants[] = {
		FLinearColor(0.035f, 0.035f, 0.04f), FLinearColor(0.04f, 0.05f, 0.07f), FLinearColor(0.06f, 0.065f, 0.045f),
	};

	FInfectedLook Look;
	Look.Skin = Skins[Rng.RandRange(0, (int32)UE_ARRAY_COUNT(Skins) - 1)];
	Look.Shirt = Shirts[Rng.RandRange(0, (int32)UE_ARRAY_COUNT(Shirts) - 1)];
	Look.Pants = Pants[Rng.RandRange(0, (int32)UE_ARRAY_COUNT(Pants) - 1)];
	Look.EyeShine = Rng.FRandRange(1.2f, 2.6f);
	Look.bMutated = bMutated;
	if (bMutated)
	{
		Look.Skin = FLinearColor(0.2f, 0.11f, 0.09f);
		Look.Shirt = FLinearColor(0.08f, 0.02f, 0.02f);
		Look.EyeShine = 3.5f;
	}
	return Look;
}

void InfectedBody::Build(AActor* Owner, USceneComponent* Root, const FInfectedLook& Look, float Scale, FInfectedBody& Out)
{
	const float S = Scale;
	Out.Root = Root;
	Out.Scale = Scale;
	Out.Seed = FMath::FRandRange(0.f, 1000.f);
	Out.Limp = FMath::FRandRange(0.15f, 0.85f);
	Out.HeadTilt = FMath::FRandRange(-22.f, 22.f);
	Out.NextTwitch = FMath::FRandRange(0.f, 2.f);

	UMaterialInterface* Skin = RupturaAssets::MakeMaterial(Owner, Look.Skin, 0.55f, 0.f, 0.f);
	UMaterialInterface* Shirt = RupturaAssets::MakeMaterial(Owner, Look.Shirt, 0.92f, 0.f, 0.f);
	UMaterialInterface* PantsM = RupturaAssets::MakeMaterial(Owner, Look.Pants, 0.9f, 0.f, 0.f);
	UMaterialInterface* Blood = RupturaAssets::MakeMaterial(Owner, FLinearColor(0.09f, 0.004f, 0.003f), 0.15f, 0.f, 0.f);
	UMaterialInterface* Dried = RupturaAssets::MakeMaterial(Owner, FLinearColor(0.045f, 0.012f, 0.008f), 0.8f, 0.f, 0.f);
	UMaterialInterface* Hair = RupturaAssets::MakeMaterial(Owner, FLinearColor(0.018f, 0.016f, 0.014f), 0.7f, 0.f, 0.f);
	UMaterialInterface* Mouth = RupturaAssets::MakeMaterial(Owner, FLinearColor(0.02f, 0.004f, 0.004f), 0.3f, 0.f, 0.f);
	UMaterialInterface* Eye = RupturaAssets::MakeMaterial(Owner, FLinearColor(0.85f, 0.8f, 0.55f), 0.05f, 0.f, Look.EyeShine);
	UMaterialInterface* Flesh = RupturaAssets::MakeMaterial(Owner, FLinearColor(0.28f, 0.05f, 0.04f), 0.25f, 0.f, 0.f);

	// Pernas: quadril -> joelho -> pé
	for (const float Side : { 1.f, -1.f })
	{
		USceneComponent* Hip = RupturaAssets::AddPivot(Owner, Root, FVector(0.f, 11.f * Side, 90.f) * S);
		RupturaAssets::AddPart(Owner, Hip, EShape::Cube, FVector(0.f, 0.f, -22.f) * S, FVector(16.f, 15.f, 46.f) * S, PantsM);
		USceneComponent* Knee = RupturaAssets::AddPivot(Owner, Hip, FVector(0.f, 0.f, -45.f) * S);
		RupturaAssets::AddPart(Owner, Knee, EShape::Cube, FVector(0.f, 0.f, -21.f) * S, FVector(13.f, 12.f, 42.f) * S, PantsM);
		RupturaAssets::AddPart(Owner, Knee, EShape::Cube, FVector(5.f, 0.f, -43.f) * S, FVector(25.f, 11.f, 8.f) * S, Dried);
		if (Side > 0.f)
		{
			Out.HipL = Hip;
			Out.KneeL = Knee;
			// Calça rasgada com a canela exposta.
			RupturaAssets::AddPart(Owner, Knee, EShape::Cube, FVector(6.8f, 0.f, -28.f) * S, FVector(1.5f, 9.f, 16.f) * S, Skin);
		}
		else
		{
			Out.HipR = Hip;
			Out.KneeR = Knee;
		}
	}

	// Pelve e tronco (o tronco curva a partir do pivô do peito)
	Out.Pelvis = RupturaAssets::AddPivot(Owner, Root, FVector(0.f, 0.f, 92.f) * S);
	RupturaAssets::AddPart(Owner, Out.Pelvis, EShape::Cube, FVector(0.f, 0.f, 2.f) * S, FVector(24.f, 32.f, 18.f) * S, PantsM);
	Out.Chest = RupturaAssets::AddPivot(Owner, Out.Pelvis, FVector(0.f, 0.f, 10.f) * S);
	RupturaAssets::AddPart(Owner, Out.Chest, EShape::Cube, FVector(0.f, 0.f, 25.f) * S, FVector(22.f, 33.f, 48.f) * S, Shirt);
	RupturaAssets::AddPart(Owner, Out.Chest, EShape::Cube, FVector(11.5f, -7.f, 24.f) * S, FVector(2.f, 12.f, 20.f) * S, Blood);   // ferida aberta
	RupturaAssets::AddPart(Owner, Out.Chest, EShape::Cube, FVector(11.2f, 6.f, 8.f) * S, FVector(1.5f, 14.f, 12.f) * S, Dried);    // mancha seca
	RupturaAssets::AddPart(Owner, Out.Chest, EShape::Cube, FVector(5.f, 12.f, 41.f) * S, FVector(12.f, 9.f, 8.f) * S, Skin);       // camisa rasgada no ombro
	if (Look.bMutated)
	{
		// Massas de tecido crescendo pelas costas e ombro.
		RupturaAssets::AddPart(Owner, Out.Chest, EShape::Sphere, FVector(-11.f, -8.f, 36.f) * S, FVector(22.f, 24.f, 20.f) * S, Flesh);
		RupturaAssets::AddPart(Owner, Out.Chest, EShape::Sphere, FVector(-9.f, 11.f, 22.f) * S, FVector(16.f, 18.f, 22.f) * S, Flesh);
		RupturaAssets::AddPart(Owner, Out.Chest, EShape::Sphere, FVector(2.f, -18.f, 46.f) * S, FVector(18.f, 16.f, 16.f) * S, Flesh);
	}

	// Cabeça
	Out.Head = RupturaAssets::AddPivot(Owner, Out.Chest, FVector(2.f, 0.f, 50.f) * S);
	RupturaAssets::AddPart(Owner, Out.Head, EShape::Cylinder, FVector(0.f, 0.f, 3.f) * S, FVector(10.f, 10.f, 10.f) * S, Skin);
	RupturaAssets::AddPart(Owner, Out.Head, EShape::Sphere, FVector(2.f, 0.f, 15.f) * S, FVector(21.f, 18.f, 25.f) * S, Skin);
	RupturaAssets::AddPart(Owner, Out.Head, EShape::Sphere, FVector(-2.f, 0.f, 20.f) * S, FVector(19.f, 17.5f, 16.f) * S, Hair);
	RupturaAssets::AddPart(Owner, Out.Head, EShape::Cube, FVector(8.f, 0.f, 5.f) * S, FVector(9.f, 12.f, 6.f) * S, Mouth);        // mandíbula aberta
	RupturaAssets::AddPart(Owner, Out.Head, EShape::Cube, FVector(10.5f, 0.f, 2.5f) * S, FVector(2.f, 11.f, 7.f) * S, Blood);      // sangue no queixo
	for (const float Side : { 1.f, -1.f })
	{
		RupturaAssets::AddPart(Owner, Out.Head, EShape::Sphere, FVector(10.5f, 3.8f * Side, 16.f) * S, FVector(2.4f, 3.2f, 2.2f) * S, Eye);
	}

	// Braços: ombro -> cotovelo -> mão
	for (const float Side : { 1.f, -1.f })
	{
		USceneComponent* Shoulder = RupturaAssets::AddPivot(Owner, Out.Chest, FVector(0.f, 20.f * Side, 44.f) * S);
		RupturaAssets::AddPart(Owner, Shoulder, EShape::Cube, FVector(0.f, 0.f, -15.f) * S, FVector(10.f, 10.f, 30.f) * S, Shirt);
		USceneComponent* Elbow = RupturaAssets::AddPivot(Owner, Shoulder, FVector(0.f, 0.f, -30.f) * S);
		RupturaAssets::AddPart(Owner, Elbow, EShape::Cube, FVector(0.f, 0.f, -13.f) * S, FVector(8.f, 8.f, 27.f) * S, Skin);
		RupturaAssets::AddPart(Owner, Elbow, EShape::Cube, FVector(1.f, 0.f, -30.f) * S, FVector(9.f, 5.f, 12.f) * S, Side > 0.f ? Dried : Blood);
		if (Side > 0.f)
		{
			Out.ShoulderL = Shoulder;
			Out.ElbowL = Elbow;
		}
		else
		{
			Out.ShoulderR = Shoulder;
			Out.ElbowR = Elbow;
		}
	}

	Animate(Out, FInfectedPose(), 0.f);
}

void InfectedBody::Animate(FInfectedBody& Body, const FInfectedPose& Pose, float DeltaSeconds)
{
	if (!Body.HipL || !Body.HipR || !Body.Chest || !Body.Head || !Body.ShoulderL || !Body.ShoulderR)
	{
		return;
	}
	Body.Clock += DeltaSeconds;

	const bool bChase = Pose.bChasing;
	const float Stride = FMath::Clamp(Pose.Speed / (bChase ? 450.f : 160.f), 0.f, 1.2f);
	const float Cadence = bChase ? (5.5f + Pose.Speed * 0.008f) : (2.2f + Pose.Speed * 0.01f);
	Body.Phase += DeltaSeconds * Cadence * FMath::Min(1.f, Stride * 3.f);
	const float Sn = FMath::Sin(Body.Phase);
	const float Cs = FMath::Cos(Body.Phase);

	// Pernas: a esquerda arrasta (amplitude menor, joelho quase reto, levemente aberta).
	const float SwingAmp = bChase ? 42.f : 24.f;
	const float KneeAmp = bChase ? 60.f : 26.f;
	Body.HipR->SetRelativeRotation(FRotator(Sn * SwingAmp * Stride, 0.f, 0.f));
	Body.HipL->SetRelativeRotation(FRotator(-Sn * SwingAmp * Stride * (1.f - 0.55f * Body.Limp), 0.f, -3.f * Body.Limp));
	if (Body.KneeR && Body.KneeL)
	{
		Body.KneeR->SetRelativeRotation(FRotator(-FMath::Max(0.f, Cs) * KneeAmp * Stride, 0.f, 0.f));
		Body.KneeL->SetRelativeRotation(FRotator(-FMath::Max(0.f, -Cs) * KneeAmp * Stride * (1.f - 0.7f * Body.Limp), 0.f, 0.f));
	}

	// Pelve: balanço pesado para o lado que manca.
	if (Body.Pelvis)
	{
		Body.Pelvis->SetRelativeRotation(FRotator(0.f, Sn * 4.f * Stride, (Sn * 5.f + Body.Limp * 4.f) * FMath::Min(1.f, Stride + 0.2f)));
		Body.Pelvis->SetRelativeLocation(FVector(0.f, 0.f, 92.f - 3.f * Stride * FMath::Abs(Sn)) * Body.Scale);
	}

	// Tronco curvado; respiração ofegante quando parado; recua ao levar tiro.
	const float Breath = FMath::Sin(Body.Clock * 0.9f + Body.Seed) * 4.f;
	float Lean = bChase ? (28.f + 6.f * Stride) : (14.f + Breath);
	Lean -= Pose.Flinch * 28.f;
	float ChestYaw = 0.f;

	// Espasmos bruscos da cabeça.
	if (Body.Clock >= Body.NextTwitch)
	{
		Body.NextTwitch = Body.Clock + (bChase ? FMath::FRandRange(0.15f, 0.9f) : FMath::FRandRange(0.4f, 2.6f));
		const float K = bChase ? 0.6f : 1.f;
		Body.TwitchTarget = FMath::FRand() < 0.35f
			? FRotator::ZeroRotator
			: FRotator(FMath::FRandRange(-18.f, 22.f) * K, FMath::FRandRange(-35.f, 35.f) * K, FMath::FRandRange(-30.f, 30.f) * K);
	}
	Body.TwitchCurrent = FMath::RInterpTo(Body.TwitchCurrent, Body.TwitchTarget, DeltaSeconds, 22.f);
	const float Jitter = FMath::PerlinNoise1D(Body.Clock * 9.f + Body.Seed) * 3.f;

	// Braços: pendurados ao vagar, estendidos para agarrar ao perseguir.
	float PitchR, PitchL, ElbowR, ElbowL;
	if (bChase)
	{
		PitchR = 76.f + FMath::Sin(Body.Phase + 1.2f) * 12.f;
		PitchL = 72.f + FMath::Sin(Body.Phase + 2.6f) * 12.f;
		ElbowR = ElbowL = 14.f;
	}
	else
	{
		PitchR = 6.f + Sn * 10.f * Stride;
		PitchL = 4.f - Sn * 6.f * Stride;
		ElbowR = 14.f;
		ElbowL = 5.f;
	}

	// Golpe: levanta o braço direito, desce rasgando, recupera.
	if (Pose.Attack >= 0.f)
	{
		const float P = Pose.Attack;
		PitchL = FMath::Max(PitchL, 70.f);
		if (P < 0.45f)
		{
			const float A = FMath::SmoothStep(0.f, 1.f, P / 0.45f);
			PitchR = FMath::Lerp(PitchR, 165.f, A);
			ElbowR = FMath::Lerp(ElbowR, 40.f, A);
			ChestYaw = 15.f * A;
			Lean -= 8.f * A;
		}
		else if (P < 0.65f)
		{
			const float A = (P - 0.45f) / 0.2f;
			PitchR = FMath::Lerp(165.f, 15.f, A);
			ElbowR = FMath::Lerp(40.f, 0.f, A);
			ChestYaw = FMath::Lerp(15.f, -22.f, A);
			Lean += 14.f * A;
		}
		else
		{
			const float A = FMath::SmoothStep(0.f, 1.f, (P - 0.65f) / 0.35f);
			PitchR = FMath::Lerp(15.f, PitchR, A);
			ElbowR = FMath::Lerp(0.f, ElbowR, A);
			ChestYaw = FMath::Lerp(-22.f, 0.f, A);
			Lean += 14.f * (1.f - A);
		}
	}

	Body.Chest->SetRelativeRotation(FRotator(-Lean, ChestYaw, FMath::Sin(Body.Phase * 0.5f + Body.Seed) * 4.f));
	// A cabeça compensa parte da curvatura para continuar olhando para frente.
	Body.Head->SetRelativeRotation(FRotator(
		Lean * 0.75f + Body.TwitchCurrent.Pitch + Jitter,
		Body.TwitchCurrent.Yaw,
		Body.HeadTilt + Body.TwitchCurrent.Roll + Jitter));

	Body.ShoulderR->SetRelativeRotation(FRotator(PitchR, 0.f, -8.f));
	Body.ShoulderL->SetRelativeRotation(FRotator(PitchL, 0.f, 8.f));
	if (Body.ElbowR && Body.ElbowL)
	{
		Body.ElbowR->SetRelativeRotation(FRotator(ElbowR, 0.f, 0.f));
		Body.ElbowL->SetRelativeRotation(FRotator(ElbowL, 0.f, 0.f));
	}
}
