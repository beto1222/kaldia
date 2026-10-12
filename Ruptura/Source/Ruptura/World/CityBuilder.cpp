#include "World/CityBuilder.h"
#include "World/LevelLayout.h"
#include "Core/RupturaAssets.h"
#include "Core/FxActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/GameplayStatics.h"

using RupturaAssets::EShape;
using namespace RupturaLayout;

namespace
{
	struct FMatDef
	{
		FLinearColor Color;
		float Roughness;
		float Metallic;
		float Emissive;
	};

	// Ordem idêntica a ACityBuilder::EMat. Tudo sujo, molhado e sem cor: a cidade está morta há dias.
	const FMatDef GMaterials[] = {
		{ FLinearColor(0.02f, 0.02f, 0.022f), 0.15f, 0.f, 0.f },     // Asphalt (molhado)
		{ FLinearColor(0.16f, 0.155f, 0.15f), 0.9f, 0.f, 0.f },      // Concrete
		{ FLinearColor(0.07f, 0.07f, 0.072f), 0.3f, 0.f, 0.f },      // WetConcrete
		{ FLinearColor(0.12f, 0.125f, 0.11f), 0.85f, 0.f, 0.f },     // MilConcrete
		{ FLinearColor(0.2f, 0.2f, 0.21f), 0.45f, 1.f, 0.f },        // Metal
		{ FLinearColor(0.035f, 0.037f, 0.04f), 0.5f, 0.9f, 0.f },    // DarkMetal
		{ FLinearColor(0.015f, 0.02f, 0.03f), 0.05f, 0.7f, 0.f },    // Glass
		{ FLinearColor(0.9f, 0.6f, 0.3f), 0.3f, 0.f, 1.2f },         // WindowLit (abajur esquecido aceso)
		{ FLinearColor(0.15f, 0.025f, 0.02f), 0.35f, 0.6f, 0.f },    // CarRed
		{ FLinearColor(0.02f, 0.04f, 0.09f), 0.35f, 0.6f, 0.f },     // CarBlue
		{ FLinearColor(0.35f, 0.35f, 0.36f), 0.35f, 0.6f, 0.f },     // CarWhite
		{ FLinearColor(0.02f, 0.017f, 0.015f), 0.95f, 0.2f, 0.f },   // Burned
		{ FLinearColor(0.012f, 0.012f, 0.012f), 0.9f, 0.f, 0.f },    // Rubber
		{ FLinearColor(0.18f, 0.15f, 0.1f), 0.95f, 0.f, 0.f },       // Sandbag
		{ FLinearColor(0.4f, 0.27f, 0.04f), 0.75f, 0.1f, 0.f },      // Hazard (desbotado)
		{ FLinearColor(0.18f, 0.06f, 0.025f), 0.7f, 0.5f, 0.f },     // ContainerRust
		{ FLinearColor(0.04f, 0.1f, 0.12f), 0.7f, 0.5f, 0.f },       // ContainerTeal
		{ FLinearColor(1.f, 0.05f, 0.03f), 0.3f, 0.f, 5.f },         // EmRed
		{ FLinearColor(0.3f, 0.6f, 0.5f), 0.3f, 0.f, 2.5f },         // EmCyan (monitores de fósforo)
		{ FLinearColor(1.f, 0.45f, 0.08f), 0.3f, 0.f, 5.f },         // EmOrange
		{ FLinearColor(0.85f, 0.95f, 0.9f), 0.3f, 0.f, 3.f },        // EmWhite (fluorescente)
		{ FLinearColor(0.008f, 0.009f, 0.011f), 0.02f, 0.f, 0.f },   // Water
		{ FLinearColor(0.1f, 0.095f, 0.09f), 0.95f, 0.f, 0.f },      // Rubble
		{ FLinearColor(0.07f, 0.004f, 0.003f), 0.2f, 0.f, 0.f },     // Blood
		{ FLinearColor(0.28f, 0.06f, 0.05f), 0.25f, 0.f, 0.f },      // Flesh
		{ FLinearColor(0.12f, 0.025f, 0.025f), 0.3f, 0.f, 0.f },     // FleshDark
		{ FLinearColor(0.42f, 0.41f, 0.38f), 0.9f, 0.f, 0.f },       // Sheet (lençol sobre os corpos)
		{ FLinearColor(0.5f, 0.46f, 0.38f), 0.6f, 0.f, 0.f },        // Bone
	};

	// Ajustes de iluminação (valores físicos aproximados; o ajuste fino depende do monitor).
	// Noite fechada e nublada: quase sem luz ambiente. A lanterna é a sua principal fonte de luz.
	constexpr float MoonLux = 0.12f;
	constexpr float SkyIntensity = 0.15f;
	constexpr float LightningLux = 25.f;
	constexpr float SodiumCandelas = 650.f;
	constexpr int32 RainCount = 600;
	// Faixa de exposição automática (EV100): o mínimo baixo impede que o "olho" se acostume com o escuro.
	constexpr float ExposureMinEV = -3.f;
	constexpr float ExposureMaxEV = 3.5f;
	constexpr float ExposureBias = -0.5f;
}

ACityBuilder::ACityBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

// ---------------------------------------------------------------------------------------------
// Primitivas
// ---------------------------------------------------------------------------------------------

UInstancedStaticMeshComponent* ACityBuilder::GetISM(EMat Mat, EShape Shape)
{
	const int32 Key = static_cast<int32>(Mat) * 8 + static_cast<int32>(Shape);
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Instanced.Find(Key))
	{
		return Found->Get();
	}

	const FMatDef& Def = GMaterials[static_cast<int32>(Mat)];
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
	ISM->SetMobility(EComponentMobility::Static);
	ISM->SetStaticMesh(RupturaAssets::Mesh(Shape));
	ISM->SetMaterial(0, RupturaAssets::MakeMaterial(this, Def.Color, Def.Roughness, Def.Metallic, Def.Emissive));
	if (Def.Emissive > 0.f || Mat == EMat::Water || Mat == EMat::Blood || Mat == EMat::FleshDark)
	{
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
		ISM->CastShadow = false;
	}
	else
	{
		ISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	ISM->RegisterComponent();
	ISM->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	AddInstanceComponent(ISM);
	Instanced.Add(Key, ISM);
	return ISM;
}

int32 ACityBuilder::Box(EMat Mat, const FVector& Center, const FVector& Size, const FRotator& Rotation)
{
	return GetISM(Mat, EShape::Cube)->AddInstance(FTransform(Rotation, Center, Size / 100.f));
}

int32 ACityBuilder::Cyl(EMat Mat, const FVector& Center, float Diameter, float Height, const FRotator& Rotation)
{
	return GetISM(Mat, EShape::Cylinder)->AddInstance(FTransform(Rotation, Center, FVector(Diameter, Diameter, Height) / 100.f));
}

int32 ACityBuilder::Blob(EMat Mat, const FVector& Center, const FVector& Size, const FRotator& Rotation)
{
	return GetISM(Mat, EShape::Sphere)->AddInstance(FTransform(Rotation, Center, Size / 100.f));
}

UPointLightComponent* ACityBuilder::AddLight(const FVector& Location, const FLinearColor& Color, float Candelas, float Radius, uint8 Mode, bool bShadows)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Mode == 4 ? 0.f : Candelas);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetCastShadows(bShadows);
	L->RegisterComponent();
	L->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	L->SetWorldLocation(Location);
	AddInstanceComponent(L);

	FCityLight Entry;
	Entry.Light = L;
	Entry.Base = Candelas;
	Entry.Mode = Mode;
	Entry.Phase = Rng.FRandRange(0.f, 100.f);
	Lights.Add(Entry);
	return L;
}

void ACityBuilder::LinkLampMesh(EMat Mat, int32 Instance)
{
	if (Lights.Num() > 0 && Instance != INDEX_NONE)
	{
		Lights.Last().LampISM = GetISM(Mat, EShape::Cube);
		Lights.Last().LampInstance = Instance;
	}
}

// ---------------------------------------------------------------------------------------------
// Construção
// ---------------------------------------------------------------------------------------------

void ACityBuilder::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	Rng.Initialize(1337);

	BuildAtmosphere();
	BuildStreet();
	BuildCompound();
	BuildTunnel();
	BuildSecurityWing();
	BuildReactor();
	BuildRain();
}

void ACityBuilder::BuildAtmosphere()
{
	Atmosphere = NewObject<USkyAtmosphereComponent>(this);
	Atmosphere->RegisterComponent();
	Atmosphere->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	AddInstanceComponent(Atmosphere);

	// Lua escondida atrás das nuvens: luz fria, fraquíssima.
	Moon = NewObject<UDirectionalLightComponent>(this);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetIntensity(MoonLux);
	Moon->SetLightColor(FLinearColor(0.5f, 0.6f, 0.85f));
	Moon->SetCastShadows(true);
	Moon->RegisterComponent();
	Moon->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	Moon->SetWorldRotation(FRotator(-35.f, -40.f, 0.f));
	AddInstanceComponent(Moon);
	MoonBase = MoonLux;

	SkyLight = NewObject<USkyLightComponent>(this);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SetIntensity(SkyIntensity);
	SkyLight->RegisterComponent();
	SkyLight->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	AddInstanceComponent(SkyLight);
	SkyBase = SkyIntensity;

	// Neblina densa e escura: engole o fim da avenida e deixa o feixe da lanterna visível no ar.
	Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetFogDensity(0.06f);
	Fog->SetFogHeightFalloff(0.1f);
	Fog->SetFogInscatteringColor(FLinearColor(0.015f, 0.018f, 0.022f));
	Fog->SetFogMaxOpacity(0.97f);
	Fog->SetVolumetricFog(true);
	Fog->SetVolumetricFogScatteringDistribution(0.6f);
	Fog->SetVolumetricFogExtinctionScale(1.8f);
	Fog->SetVolumetricFogDistance(6000.f);
	Fog->RegisterComponent();
	Fog->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	AddInstanceComponent(Fog);

	// Pós-processamento: aparência de filme de terror (granulado, frio, contrastado, olho que não se
	// acostuma com o escuro). Vinheta, aberração, saturação e granulado são modulados pela câmera de
	// Kael conforme o medo e os ferimentos.
	PostProcess = NewObject<UPostProcessComponent>(this);
	PostProcess->bUnbound = true;
	FPostProcessSettings& S = PostProcess->Settings;
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	S.bOverride_AutoExposureMinBrightness = true;
	S.AutoExposureMinBrightness = ExposureMinEV;
	S.bOverride_AutoExposureMaxBrightness = true;
	S.AutoExposureMaxBrightness = ExposureMaxEV;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = ExposureBias;
	S.bOverride_AutoExposureSpeedUp = true;
	S.AutoExposureSpeedUp = 1.2f;
	S.bOverride_AutoExposureSpeedDown = true;
	S.AutoExposureSpeedDown = 0.5f;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 0.6f;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.55f;
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.35f;
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = 0.35f;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(0.75f, 0.78f, 0.8f, 1.f);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.f);
	S.bOverride_ColorGainShadows = true;
	S.ColorGainShadows = FVector4(0.94f, 1.f, 1.03f, 1.f); // sombras puxando para o verde-azulado
	S.bOverride_WhiteTemp = true;
	S.WhiteTemp = 7800.f;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.35f;
	PostProcess->RegisterComponent();
	PostProcess->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	AddInstanceComponent(PostProcess);
}

void ACityBuilder::BuildBuilding(float X0, float X1, float Height, float Side, bool bDestroyed)
{
	const float Xc = (X0 + X1) * 0.5f;
	const float Len = X1 - X0;
	const float FaceY = Side * StreetHalfWidth;
	const float Depth = 1700.f;

	Box(EMat::Concrete, FVector(Xc, Side * (StreetHalfWidth + Depth * 0.5f), Height * 0.5f), FVector(Len, Depth, Height));

	// Térreo: vitrine escura
	Box(EMat::Glass, FVector(Xc, FaceY - Side * 4.f, 150.f), FVector(Len - 120.f, 6.f, 220.f));
	Box(EMat::Concrete, FVector(Xc, FaceY - Side * 20.f, 290.f), FVector(Len, 40.f, 30.f)); // marquise

	// Janelas
	for (float Z = 450.f; Z < Height - 220.f; Z += 340.f)
	{
		for (float X = X0 + 160.f; X < X1 - 140.f; X += 300.f)
		{
			const float Roll = Rng.FRand();
			if (Roll < 0.13f)
			{
				continue; // janela quebrada/aberta: só o concreto escuro
			}
			const EMat M = Roll < 0.135f ? EMat::WindowLit : EMat::Glass; // um ou outro abajur esquecido aceso
			Box(M, FVector(X, FaceY - Side * 4.f, Z), FVector(190.f, 6.f, 170.f));
		}
	}

	// Cornija
	Box(EMat::MilConcrete, FVector(Xc, FaceY - Side * 25.f, Height - 20.f), FVector(Len, 50.f, 40.f));

	if (bDestroyed)
	{
		// Topo desmoronado e lajes tombadas.
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector P(Rng.FRandRange(X0, X1), Side * Rng.FRandRange(StreetHalfWidth + 200.f, 2800.f), Height + Rng.FRandRange(-50.f, 150.f));
			Box(EMat::Rubble, P, FVector(Rng.FRandRange(200.f, 500.f), Rng.FRandRange(200.f, 500.f), Rng.FRandRange(60.f, 200.f)),
				FRotator(Rng.FRandRange(-25.f, 25.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-25.f, 25.f)));
		}
		// Destroços caídos na calçada (cobertura baixa)
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector P(Rng.FRandRange(X0 + 100.f, X1 - 100.f), Side * Rng.FRandRange(980.f, 1220.f), 40.f);
			Box(EMat::Rubble, P, FVector(Rng.FRandRange(80.f, 180.f), Rng.FRandRange(60.f, 140.f), Rng.FRandRange(50.f, 110.f)),
				FRotator(Rng.FRandRange(-15.f, 15.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-15.f, 15.f)));
		}
	}
}

void ACityBuilder::BuildCar(const FVector& Center, float Yaw, EMat Paint, bool bBurned)
{
	const FTransform T(FRotator(0.f, Yaw, 0.f), Center);
	const FRotator R(0.f, Yaw, 0.f);
	const EMat BodyMat = bBurned ? EMat::Burned : Paint;

	Box(BodyMat, T.TransformPosition(FVector(0.f, 0.f, 58.f)), FVector(440.f, 190.f, 76.f), R);
	Box(BodyMat, T.TransformPosition(FVector(-20.f, 0.f, 124.f)), FVector(240.f, 170.f, 58.f), R);
	if (!bBurned)
	{
		Box(EMat::Glass, T.TransformPosition(FVector(-20.f, 0.f, 126.f)), FVector(244.f, 172.f, 40.f), R);
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Local((i < 2) ? 140.f : -140.f, (i % 2 == 0) ? 92.f : -92.f, 34.f);
		Cyl(EMat::Rubber, T.TransformPosition(Local), 68.f, 26.f, FRotator(0.f, Yaw, 90.f));
	}
}

void ACityBuilder::BuildCorpse(const FVector& Location, float Yaw)
{
	// Corpo coberto por um lençol, com a poça de sangue já seca por baixo.
	const FTransform T(FRotator(0.f, Yaw, 0.f), Location);
	const FRotator R(0.f, Yaw, 0.f);
	Box(EMat::Blood, T.TransformPosition(FVector(10.f, 5.f, 0.6f)), FVector(210.f, 120.f, 1.f), R);
	Box(EMat::Sheet, T.TransformPosition(FVector(0.f, 0.f, 11.f)), FVector(175.f, 62.f, 22.f), R);
	Blob(EMat::Sheet, T.TransformPosition(FVector(88.f, 0.f, 13.f)), FVector(30.f, 28.f, 24.f), R);
	Box(EMat::Blood, T.TransformPosition(FVector(20.f, 8.f, 22.5f)), FVector(50.f, 36.f, 1.f), R);
	// Um pé descalço para fora do lençol.
	Box(EMat::Concrete, T.TransformPosition(FVector(-92.f, 12.f, 6.f)), FVector(14.f, 9.f, 10.f), R);
}

void ACityBuilder::BuildBloodTrail(const FVector& Start, const FVector& End)
{
	// Rastro de algo (alguém) sendo arrastado.
	const FVector Delta = End - Start;
	const int32 Steps = FMath::Max(1, FMath::RoundToInt(Delta.Size2D() / 120.f));
	const FRotator R(0.f, Delta.Rotation().Yaw, 0.f);
	for (int32 i = 0; i < Steps; ++i)
	{
		const FVector P = Start + Delta * ((i + 0.5f) / Steps);
		Box(EMat::Blood, FVector(P.X, P.Y + Rng.FRandRange(-6.f, 6.f), P.Z + 0.6f), FVector(Rng.FRandRange(90.f, 140.f), Rng.FRandRange(25.f, 45.f), 1.f), R);
	}
}

void ACityBuilder::BuildStreet()
{
	// Chão da rua + instalação (asfalto molhado)
	Box(EMat::Asphalt, FVector((StreetStartX + StreetEndX) * 0.5f, 0.f, -50.f), FVector(StreetEndX - StreetStartX, BoundaryY * 2.f + 200.f, 100.f));

	// Calçadas
	for (const float Side : { 1.f, -1.f })
	{
		Box(EMat::Concrete, FVector(2000.f, Side * 1100.f, 7.f), FVector(6000.f, 400.f, 14.f));
		// Faixas do meio-fio
		Box(EMat::MilConcrete, FVector(2000.f, Side * 905.f, 9.f), FVector(6000.f, 14.f, 18.f));
	}

	// Limites invisíveis por trás dos prédios + paredão de escombros no início da rua
	Box(EMat::Concrete, FVector((StreetStartX + StreetEndX) * 0.5f, BoundaryY, 1000.f), FVector(StreetEndX - StreetStartX, 200.f, 2000.f));
	Box(EMat::Concrete, FVector((StreetStartX + StreetEndX) * 0.5f, -BoundaryY, 1000.f), FVector(StreetEndX - StreetStartX, 200.f, 2000.f));
	Box(EMat::Concrete, FVector(StreetStartX - 100.f, 0.f, 600.f), FVector(200.f, BoundaryY * 2.f, 1200.f));
	for (int32 i = 0; i < 12; ++i)
	{
		const FVector P(StreetStartX + Rng.FRandRange(20.f, 350.f), Rng.FRandRange(-1300.f, 1300.f), Rng.FRandRange(30.f, 200.f));
		Box(EMat::Rubble, P, FVector(Rng.FRandRange(150.f, 400.f), Rng.FRandRange(150.f, 400.f), Rng.FRandRange(80.f, 250.f)),
			FRotator(Rng.FRandRange(-30.f, 30.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-30.f, 30.f)));
	}
	// Viaduto desabado sobre o fim da rua (silhueta)
	Box(EMat::MilConcrete, FVector(-500.f, 0.f, 900.f), FVector(500.f, 3000.f, 120.f), FRotator(0.f, 0.f, 6.f));

	// Prédios (X0, X1, altura, lado, destruído)
	struct FB { float X0, X1, H, Side; bool bD; };
	const FB Buildings[] = {
		{ -1000.f, 400.f, 2600.f, 1.f, false }, { 550.f, 1700.f, 3800.f, 1.f, true }, { 1900.f, 2900.f, 2200.f, 1.f, false },
		{ 3050.f, 4200.f, 4200.f, 1.f, false }, { 4350.f, 5000.f, 1600.f, 1.f, true },
		{ -1000.f, 700.f, 3000.f, -1.f, false }, { 850.f, 1900.f, 1800.f, -1.f, true }, { 2050.f, 3300.f, 4600.f, -1.f, false },
		{ 3450.f, 5000.f, 2400.f, -1.f, false },
	};
	for (const FB& B : Buildings)
	{
		BuildBuilding(B.X0, B.X1, B.H, B.Side, B.bD);
	}

	// Postes de luz de sódio (alguns queimados ou piscando)
	int32 LampIndex = 0;
	for (float X = -400.f; X <= 4800.f; X += 1400.f)
	{
		for (const float Side : { 1.f, -1.f })
		{
			Cyl(EMat::DarkMetal, FVector(X, Side * 1000.f, 350.f), 16.f, 700.f);
			Box(EMat::DarkMetal, FVector(X, Side * 880.f, 690.f), FVector(20.f, 260.f, 14.f));
			// Quase metade dos postes está queimada; dos que restam, muitos falham.
			const float Roll = Rng.FRand();
			const bool bBroken = Roll < 0.4f;
			const int32 Lamp = Box(bBroken ? EMat::DarkMetal : EMat::EmOrange, FVector(X, Side * 760.f, 680.f), FVector(60.f, 40.f, 10.f));
			if (!bBroken)
			{
				const uint8 Mode = Roll < 0.7f ? 2 : (Roll < 0.85f ? 1 : 0);
				AddLight(FVector(X, Side * 760.f, 650.f), FLinearColor(1.f, 0.58f, 0.25f), SodiumCandelas, 1700.f, Mode, LampIndex < 3);
				LinkLampMesh(EMat::EmOrange, Lamp);
			}
			++LampIndex;
		}
	}

	// Carros abandonados (cobertura)
	BuildCar(FVector(900.f, 300.f, 0.f), 15.f, EMat::CarRed, false);
	BuildCar(FVector(1550.f, -600.f, 0.f), -170.f, EMat::CarBlue, false);
	BuildCar(FVector(2300.f, 650.f, 0.f), 95.f, EMat::CarWhite, false);
	BuildCar(FVector(2950.f, -200.f, 0.f), 30.f, EMat::CarRed, true);
	BuildCar(FVector(3650.f, 500.f, 0.f), -10.f, EMat::CarWhite, false);
	BuildCar(FVector(4100.f, -650.f, 0.f), 160.f, EMat::CarBlue, true);
	BuildCar(FVector(4750.f, 250.f, 0.f), 75.f, EMat::CarRed, true);
	// Um carro com a bateria morrendo: faróis falhando
	AddLight(FVector(1550.f - 240.f, -560.f, 70.f), FLinearColor(0.9f, 0.88f, 0.75f), 400.f, 1300.f, 2, false);

	// Barreiras de concreto (cobertura baixa)
	const FVector Barriers[] = { FVector(1200.f, -150.f, 90.f), FVector(2600.f, 100.f, 0.f), FVector(3300.f, 700.f, 60.f), FVector(3900.f, -250.f, 120.f), FVector(4500.f, 650.f, 0.f) };
	for (const FVector& B : Barriers)
	{
		Box(EMat::Concrete, FVector(B.X, B.Y, 47.f), FVector(220.f, 60.f, 95.f), FRotator(0.f, B.Z, 0.f));
	}

	// Posto militar improvisado: sacos de areia + caminhão
	Box(EMat::Sandbag, FVector(4600.f, 900.f, 50.f), FVector(320.f, 70.f, 100.f), FRotator(0.f, 20.f, 0.f));
	Box(EMat::Sandbag, FVector(4650.f, -350.f, 50.f), FVector(300.f, 70.f, 100.f), FRotator(0.f, -10.f, 0.f));
	Box(EMat::ContainerTeal, FVector(4300.f, -850.f, 160.f), FVector(650.f, 250.f, 300.f), FRotator(0.f, 15.f, 0.f));
	Box(EMat::DarkMetal, FVector(4680.f, -750.f, 120.f), FVector(200.f, 240.f, 220.f), FRotator(0.f, 15.f, 0.f));

	// Escombros espalhados
	for (int32 i = 0; i < 40; ++i)
	{
		const FVector P(Rng.FRandRange(-600.f, 5000.f), Rng.FRandRange(-1250.f, 1250.f), 15.f);
		const float S = Rng.FRandRange(25.f, 90.f);
		Box(EMat::Rubble, P, FVector(S, S * Rng.FRandRange(0.6f, 1.4f), S * 0.6f), FRotator(Rng.FRandRange(-20.f, 20.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-20.f, 20.f)));
	}

	// Poças d'água (refletem as luzes com Lumen)
	for (int32 i = 0; i < 14; ++i)
	{
		Box(EMat::Water, FVector(Rng.FRandRange(-500.f, 7600.f), Rng.FRandRange(-800.f, 800.f), 1.f), FVector(Rng.FRandRange(250.f, 600.f), Rng.FRandRange(150.f, 380.f), 2.f),
			FRotator(0.f, Rng.FRandRange(0.f, 180.f), 0.f));
	}

	// Cadáveres cobertos, rastros de sangue e uma ambulância abandonada com a sirene piscando.
	BuildCorpse(FVector(700.f, -350.f, 0.f), 20.f);
	BuildCorpse(FVector(1900.f, 820.f, 14.f), 100.f);
	BuildCorpse(FVector(2750.f, -950.f, 14.f), -15.f);
	BuildCorpse(FVector(3550.f, 150.f, 0.f), 160.f);
	BuildCorpse(FVector(4250.f, 980.f, 14.f), 75.f);
	BuildBloodTrail(FVector(2100.f, 300.f, 0.f), FVector(1350.f, 1150.f, 14.f));
	BuildBloodTrail(FVector(4400.f, -600.f, 0.f), FVector(5150.f, -100.f, 0.f));
	BuildCar(FVector(3300.f, -820.f, 0.f), 8.f, EMat::CarWhite, false);
	Box(EMat::CarRed, FVector(3300.f, -820.f, 70.f), FVector(446.f, 196.f, 14.f), FRotator(0.f, 8.f, 0.f));
	Box(EMat::CarWhite, FVector(3260.f, -820.f, 160.f), FVector(300.f, 186.f, 120.f), FRotator(0.f, 8.f, 0.f));
	AddLight(FVector(3300.f, -820.f, 250.f), FLinearColor(1.f, 0.08f, 0.05f), 260.f, 1000.f, 2, false);
	// Barreira policial derrubada no começo da avenida.
	for (int32 i = 0; i < 3; ++i)
	{
		const float Y = -600.f + i * 600.f;
		Box(EMat::Hazard, FVector(450.f, Y, 75.f), FVector(20.f, 240.f, 22.f), FRotator(0.f, Rng.FRandRange(-20.f, 20.f), Rng.FRandRange(-10.f, 10.f)));
		Box(EMat::DarkMetal, FVector(450.f, Y - 100.f, 35.f), FVector(10.f, 10.f, 70.f));
		Box(EMat::DarkMetal, FVector(450.f, Y + 100.f, 35.f), FVector(10.f, 10.f, 70.f));
	}

	// Ponto de extração: marcador no chão + sinalizador vermelho
	Cyl(EMat::EmRed, FVector(ExtractionLocation().X, 0.f, 2.f), 60.f, 4.f);
	AddLight(ExtractionLocation() + FVector(0.f, 0.f, 40.f), FLinearColor(1.f, 0.1f, 0.05f), 500.f, 1200.f, 1, false);
}

void ACityBuilder::BuildCompound()
{
	// Muro frontal com portão arrombado
	for (const float Side : { 1.f, -1.f })
	{
		Box(EMat::MilConcrete, FVector(CompoundFrontX, Side * 1850.f, 200.f), FVector(80.f, 2500.f, 400.f));
		Box(EMat::DarkMetal, FVector(CompoundFrontX, Side * 1850.f, 420.f), FVector(20.f, 2500.f, 40.f)); // arame
		Box(EMat::MilConcrete, FVector(CompoundFrontX, Side * 650.f, 250.f), FVector(120.f, 120.f, 500.f)); // pilares
		Box(EMat::Hazard, FVector(CompoundFrontX - 62.f, Side * 650.f, 250.f), FVector(4.f, 120.f, 60.f));
		AddLight(FVector(CompoundFrontX - 100.f, Side * 650.f, 480.f), FLinearColor(1.f, 0.15f, 0.08f), 500.f, 1200.f, 3, false);
	}
	Box(EMat::Metal, FVector(5300.f, 200.f, 10.f), FVector(350.f, 480.f, 15.f), FRotator(0.f, 20.f, 0.f)); // portão caído

	// Torres de vigia
	for (const float Side : { 1.f, -1.f })
	{
		const FVector Base(5700.f, Side * 2750.f, 0.f);
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector Leg = Base + FVector((i < 2) ? -120.f : 120.f, (i % 2 == 0) ? -120.f : 120.f, 350.f);
			Cyl(EMat::DarkMetal, Leg, 20.f, 700.f);
		}
		Box(EMat::Metal, Base + FVector(0.f, 0.f, 705.f), FVector(320.f, 320.f, 20.f));
		Box(EMat::MilConcrete, Base + FVector(0.f, 0.f, 830.f), FVector(260.f, 260.f, 220.f));
		AddLight(Base + FVector(0.f, -Side * 200.f, 800.f), FLinearColor(0.85f, 0.9f, 1.f), Side > 0.f ? 1800.f : 0.f, 3200.f, 2, Side > 0.f);
	}

	// Contêineres (cobertura alta)
	Box(EMat::ContainerRust, FVector(6200.f, 1800.f, 130.f), FVector(600.f, 240.f, 260.f));
	Box(EMat::ContainerTeal, FVector(6200.f, 1800.f, 390.f), FVector(600.f, 240.f, 260.f), FRotator(0.f, 4.f, 0.f));
	Box(EMat::ContainerTeal, FVector(6400.f, -1900.f, 130.f), FVector(240.f, 600.f, 260.f));
	Box(EMat::ContainerRust, FVector(7400.f, -2300.f, 130.f), FVector(600.f, 240.f, 260.f));
	Box(EMat::ContainerRust, FVector(6900.f, 2650.f, 130.f), FVector(600.f, 240.f, 260.f));

	// Sacos de areia e caixas (cobertura baixa)
	Box(EMat::Sandbag, FVector(6300.f, -500.f, 50.f), FVector(300.f, 70.f, 100.f));
	Box(EMat::Sandbag, FVector(7000.f, 350.f, 50.f), FVector(70.f, 300.f, 100.f));
	Box(EMat::Sandbag, FVector(6700.f, -1200.f, 50.f), FVector(300.f, 70.f, 100.f), FRotator(0.f, 30.f, 0.f));
	Box(EMat::Sandbag, FVector(7500.f, 1200.f, 50.f), FVector(300.f, 70.f, 100.f), FRotator(0.f, -20.f, 0.f));
	Box(EMat::Sandbag, FVector(5900.f, 1300.f, 50.f), FVector(70.f, 300.f, 100.f));
	Box(EMat::ContainerTeal, FVector(6000.f, -400.f, 60.f), FVector(120.f, 120.f, 120.f), FRotator(0.f, 12.f, 0.f));
	Box(EMat::ContainerTeal, FVector(6020.f, -270.f, 60.f), FVector(120.f, 120.f, 120.f), FRotator(0.f, -8.f, 0.f));
	Box(EMat::ContainerRust, FVector(7200.f, -500.f, 55.f), FVector(110.f, 110.f, 110.f), FRotator(0.f, 30.f, 0.f));

	// Posto da guarda (sala com mesa e o cartão de acesso)
	Box(EMat::MilConcrete, FVector(7100.f, 2100.f, 150.f), FVector(620.f, 20.f, 300.f));
	Box(EMat::MilConcrete, FVector(6800.f, 1800.f, 150.f), FVector(20.f, 600.f, 300.f));
	Box(EMat::MilConcrete, FVector(7400.f, 1800.f, 150.f), FVector(20.f, 600.f, 300.f));
	Box(EMat::MilConcrete, FVector(6900.f, 1500.f, 150.f), FVector(200.f, 20.f, 300.f));
	Box(EMat::MilConcrete, FVector(7300.f, 1500.f, 150.f), FVector(200.f, 20.f, 300.f));
	Box(EMat::MilConcrete, FVector(7100.f, 1500.f, 265.f), FVector(200.f, 20.f, 70.f));
	Box(EMat::MilConcrete, FVector(7100.f, 1800.f, 310.f), FVector(640.f, 640.f, 20.f));
	Box(EMat::Metal, FVector(7100.f, 1950.f, 45.f), FVector(160.f, 70.f, 90.f));
	Box(EMat::DarkMetal, FVector(6900.f, 2000.f, 90.f), FVector(60.f, 160.f, 180.f)); // armário
	AddLight(FVector(7100.f, 1800.f, 270.f), FLinearColor(0.85f, 0.95f, 0.85f), 220.f, 900.f, 2, false);
	// O que aconteceu no posto da guarda ficou nas paredes.
	Box(EMat::Blood, FVector(7100.f, 2088.f, 140.f), FVector(160.f, 2.f, 120.f), FRotator(0.f, 0.f, 8.f));
	Box(EMat::Blood, FVector(7050.f, 1800.f, 0.6f), FVector(220.f, 160.f, 1.f), FRotator(0.f, 30.f, 0.f));
	BuildBloodTrail(FVector(7000.f, 1700.f, 0.f), FVector(7000.f, 1350.f, 0.f));
	BuildCorpse(FVector(6400.f, 600.f, 0.f), 45.f);
	BuildCorpse(FVector(7300.f, -900.f, 0.f), -120.f);
	Box(EMat::Blood, FVector(BunkerDoorX - 52.f, 330.f, 130.f), FVector(2.f, 110.f, 170.f));

	// Muro do fundo com a porta do bunker
	for (const float Side : { 1.f, -1.f })
	{
		Box(EMat::MilConcrete, FVector(BunkerDoorX, Side * 1675.f, 300.f), FVector(100.f, 2850.f, 600.f));
		Box(EMat::Hazard, FVector(BunkerDoorX - 55.f, Side * 225.f, 175.f), FVector(10.f, 40.f, 350.f));
		AddLight(FVector(BunkerDoorX - 120.f, Side * 450.f, 450.f), FLinearColor(1.f, 0.12f, 0.05f), 700.f, 1500.f, 3, false);
	}
	Box(EMat::MilConcrete, FVector(BunkerDoorX, 0.f, 475.f), FVector(100.f, 500.f, 250.f));
	Box(EMat::EmRed, FVector(BunkerDoorX - 55.f, 0.f, 420.f), FVector(6.f, 300.f, 40.f));

	// Holofotes do pátio
	AddLight(FVector(6300.f, 0.f, 650.f), FLinearColor(0.9f, 0.95f, 1.f), 1100.f, 2600.f, 2, true);
	AddLight(FVector(7400.f, -1200.f, 600.f), FLinearColor(0.9f, 0.95f, 1.f), 900.f, 2400.f, 1, false);
	Cyl(EMat::DarkMetal, FVector(6300.f, 0.f, 330.f), 18.f, 660.f);
}

void ACityBuilder::BuildTunnel()
{
	// Soleira da porta
	Box(EMat::WetConcrete, FVector(BunkerDoorX + 55.f, 0.f, -20.f), FVector(110.f, TunnelHalfWidth * 2.f, 40.f));

	// Rampa descendo para o subsolo
	const float Run = RampEndX - RampStartX;
	const float Drop = -SubFloorZ;
	const float Pitch = -FMath::RadiansToDegrees(FMath::Atan2(Drop, Run));
	const FRotator RampRot(Pitch, 0.f, 0.f);
	const FVector Normal = RampRot.RotateVector(FVector::UpVector);
	const FVector MidTop((RampStartX + RampEndX) * 0.5f, 0.f, SubFloorZ * 0.5f);
	const float Length = FMath::Sqrt(Run * Run + Drop * Drop) + 30.f;
	Box(EMat::WetConcrete, MidTop - Normal * 20.f, FVector(Length, TunnelHalfWidth * 2.f, 40.f), RampRot);

	// Paredes e teto do túnel
	const float TunnelXc = (BunkerDoorX + RampEndX + 100.f) * 0.5f;
	const float TunnelLen = RampEndX + 100.f - BunkerDoorX;
	for (const float Side : { 1.f, -1.f })
	{
		Box(EMat::MilConcrete, FVector(TunnelXc, Side * (TunnelHalfWidth + 50.f), -300.f), FVector(TunnelLen, 100.f, 1400.f));
		// Faixa de emergência no rodapé
		for (float X = RampStartX + 100.f; X < RampEndX; X += 400.f)
		{
			const float FloorZ = SubFloorZ * (X - RampStartX) / Run;
			Box(EMat::EmRed, FVector(X, Side * (TunnelHalfWidth - 2.f), FloorZ + 25.f), FVector(120.f, 4.f, 6.f), RampRot);
		}
	}
	Box(EMat::MilConcrete, FVector(TunnelXc, 0.f, 450.f), FVector(TunnelLen, TunnelHalfWidth * 2.f + 200.f, 100.f));
	// Marcas de mãos ensanguentadas descendo a rampa.
	for (int32 i = 0; i < 6; ++i)
	{
		const float X = RampStartX + 250.f + i * 300.f;
		const float FloorZ = SubFloorZ * (X - RampStartX) / Run;
		Box(EMat::Blood, FVector(X, -TunnelHalfWidth + 2.f, FloorZ + Rng.FRandRange(90.f, 160.f)), FVector(Rng.FRandRange(40.f, 90.f), 2.f, Rng.FRandRange(20.f, 50.f)), FRotator(0.f, 0.f, Rng.FRandRange(-20.f, 20.f)));
	}

	// Luminárias de parede
	for (float X = RampStartX + 200.f; X < RampEndX; X += 500.f)
	{
		const float FloorZ = SubFloorZ * (X - RampStartX) / Run;
		const int32 Lamp = Box(EMat::EmWhite, FVector(X, TunnelHalfWidth - 4.f, FloorZ + 260.f), FVector(70.f, 6.f, 12.f));
		const uint8 Mode = Rng.FRand() < 0.5f ? 2 : 0;
		AddLight(FVector(X, TunnelHalfWidth - 60.f, FloorZ + 250.f), FLinearColor(0.85f, 0.95f, 0.85f), 220.f, 1000.f, Mode, false);
		LinkLampMesh(EMat::EmWhite, Lamp);
	}
}

void ACityBuilder::BuildSecurityWing()
{
	const float Xc = (RampEndX + Hall1EndX) * 0.5f;
	const float Len = Hall1EndX - RampEndX;
	const float WallZ = (SubFloorZ + Hall1CeilingZ) * 0.5f;
	const float WallH = Hall1CeilingZ - SubFloorZ;

	Box(EMat::WetConcrete, FVector(Xc, 0.f, SubFloorZ - 50.f), FVector(Len, Hall1HalfWidth * 2.f, 100.f));
	Box(EMat::MilConcrete, FVector(Xc, 0.f, Hall1CeilingZ + 50.f), FVector(Len, Hall1HalfWidth * 2.f + 200.f, 100.f));

	// Parede sul
	Box(EMat::MilConcrete, FVector(Xc, -(Hall1HalfWidth + 50.f), WallZ), FVector(Len, 100.f, WallH));
	// Parede norte com passagem para a sala de controle (X 12100..12500)
	Box(EMat::MilConcrete, FVector((RampEndX + 12100.f) * 0.5f, Hall1HalfWidth + 50.f, WallZ), FVector(12100.f - RampEndX, 100.f, WallH));
	Box(EMat::MilConcrete, FVector((12500.f + Hall1EndX) * 0.5f, Hall1HalfWidth + 50.f, WallZ), FVector(Hall1EndX - 12500.f, 100.f, WallH));
	Box(EMat::MilConcrete, FVector(12300.f, Hall1HalfWidth + 50.f, -250.f), FVector(400.f, 100.f, 300.f));
	// Parede oeste (em volta da saída do túnel)
	for (const float Side : { 1.f, -1.f })
	{
		const float Mid = (TunnelHalfWidth + Hall1HalfWidth) * 0.5f;
		Box(EMat::MilConcrete, FVector(RampEndX + 50.f, Side * Mid, WallZ), FVector(100.f, Hall1HalfWidth - TunnelHalfWidth, WallH));
	}
	// Parede leste com a porta blindada (abertura Y ±250)
	for (const float Side : { 1.f, -1.f })
	{
		Box(EMat::MilConcrete, FVector(Hall1EndX, Side * (250.f + (Hall1HalfWidth - 250.f) * 0.5f), WallZ), FVector(100.f, Hall1HalfWidth - 250.f, WallH));
		Box(EMat::Hazard, FVector(Hall1EndX - 55.f, Side * 230.f, SubFloorZ + 190.f), FVector(10.f, 40.f, 380.f));
	}
	Box(EMat::MilConcrete, FVector(Hall1EndX, 0.f, -260.f), FVector(100.f, 500.f, 320.f));

	// Sala de controle (terminal)
	Box(EMat::WetConcrete, FVector(12300.f, 2150.f, SubFloorZ - 50.f), FVector(1100.f, 1000.f, 100.f));
	Box(EMat::MilConcrete, FVector(12300.f, 2150.f, Hall1CeilingZ + 50.f), FVector(1100.f, 1000.f, 100.f));
	Box(EMat::MilConcrete, FVector(11750.f, 2150.f, WallZ), FVector(100.f, 1000.f, WallH));
	Box(EMat::MilConcrete, FVector(12850.f, 2150.f, WallZ), FVector(100.f, 1000.f, WallH));
	Box(EMat::MilConcrete, FVector(12300.f, 2650.f, WallZ), FVector(1100.f, 100.f, WallH));
	for (const float X : { 11900.f, 12700.f })
	{
		Box(EMat::DarkMetal, FVector(X, 2350.f, SubFloorZ + 110.f), FVector(90.f, 220.f, 220.f));
		Box(EMat::EmCyan, FVector(X + (X < 12300.f ? 47.f : -47.f), 2350.f, SubFloorZ + 150.f), FVector(4.f, 150.f, 6.f));
	}
	AddLight(FVector(12300.f, 2150.f, -200.f), FLinearColor(0.8f, 0.92f, 0.85f), 240.f, 1100.f, 2, false);
	BuildCorpse(FVector(12550.f, 1950.f, SubFloorZ), 200.f);
	Box(EMat::Blood, FVector(12300.f, 2598.f, SubFloorZ + 180.f), FVector(220.f, 2.f, 180.f));

	// Cobertura: pilares, caixas e muretas
	for (const float X : { 10800.f, 11600.f, 12400.f })
	{
		for (const float Side : { 1.f, -1.f })
		{
			Cyl(EMat::MilConcrete, FVector(X, Side * 700.f, WallZ), 120.f, WallH);
			Box(EMat::Hazard, FVector(X, Side * 700.f, SubFloorZ + 60.f), FVector(126.f, 126.f, 30.f));
		}
	}
	Box(EMat::ContainerTeal, FVector(11200.f, 0.f, SubFloorZ + 60.f), FVector(120.f, 120.f, 120.f));
	Box(EMat::ContainerTeal, FVector(11250.f, 140.f, SubFloorZ + 60.f), FVector(120.f, 120.f, 120.f), FRotator(0.f, 15.f, 0.f));
	Box(EMat::ContainerRust, FVector(11220.f, 60.f, SubFloorZ + 180.f), FVector(110.f, 110.f, 120.f), FRotator(0.f, -10.f, 0.f));
	Box(EMat::MilConcrete, FVector(11900.f, -200.f, SubFloorZ + 55.f), FVector(300.f, 40.f, 110.f));
	Box(EMat::MilConcrete, FVector(12500.f, 600.f, SubFloorZ + 55.f), FVector(40.f, 300.f, 110.f));
	Box(EMat::MilConcrete, FVector(11000.f, -1200.f, SubFloorZ + 55.f), FVector(300.f, 40.f, 110.f));
	Box(EMat::ContainerRust, FVector(12600.f, -1200.f, SubFloorZ + 60.f), FVector(220.f, 120.f, 120.f));
	BuildCorpse(FVector(11500.f, -600.f, SubFloorZ), 30.f);
	BuildBloodTrail(FVector(11700.f, 300.f, SubFloorZ), FVector(12900.f, 150.f, SubFloorZ));
	// A carne já começou a sair por baixo da porta do reator.
	for (int32 i = 0; i < 7; ++i)
	{
		Blob(EMat::Flesh, FVector(Hall1EndX - 80.f - Rng.FRandRange(0.f, 120.f), Rng.FRandRange(-500.f, 500.f), SubFloorZ + 5.f),
			FVector(Rng.FRandRange(60.f, 140.f), Rng.FRandRange(60.f, 140.f), Rng.FRandRange(15.f, 35.f)));
	}

	// Luminárias de teto + luzes de alarme (desligadas até o núcleo cair)
	int32 Index = 0;
	for (float X = 10500.f; X <= 12700.f; X += 700.f)
	{
		for (const float Side : { 1.f, -1.f })
		{
			// Metade das lâmpadas está queimada; o resto pisca.
			if (Index % 2 == 1)
			{
				Box(EMat::DarkMetal, FVector(X, Side * 800.f, Hall1CeilingZ - 6.f), FVector(200.f, 30.f, 10.f));
				++Index;
				continue;
			}
			const int32 Lamp = Box(EMat::EmWhite, FVector(X, Side * 800.f, Hall1CeilingZ - 6.f), FVector(200.f, 30.f, 10.f));
			const uint8 Mode = (Index % 4 == 0) ? 2 : 1;
			AddLight(FVector(X, Side * 800.f, Hall1CeilingZ - 80.f), FLinearColor(0.82f, 0.95f, 0.85f), 260.f, 1300.f, Mode, Index < 3);
			LinkLampMesh(EMat::EmWhite, Lamp);
			++Index;
		}
	}
	for (const float X : { 10600.f, 12000.f })
	{
		for (const float Side : { 1.f, -1.f })
		{
			Box(EMat::EmRed, FVector(X, Side * (Hall1HalfWidth - 4.f), -250.f), FVector(40.f, 6.f, 20.f));
			AddLight(FVector(X, Side * (Hall1HalfWidth - 60.f), -250.f), FLinearColor(1.f, 0.05f, 0.02f), 500.f, 1600.f, 4, false);
		}
	}
}

void ACityBuilder::BuildReactor()
{
	const float Xc = (Hall1EndX + ReactorEndX) * 0.5f;
	const float Len = ReactorEndX - Hall1EndX;
	const float WallZ = (SubFloorZ + ReactorCeilingZ) * 0.5f;
	const float WallH = ReactorCeilingZ - SubFloorZ;

	Box(EMat::WetConcrete, FVector(Xc, 0.f, SubFloorZ - 50.f), FVector(Len, ReactorHalfWidth * 2.f, 100.f));
	Box(EMat::MilConcrete, FVector(Xc, 0.f, ReactorCeilingZ + 50.f), FVector(Len, ReactorHalfWidth * 2.f + 200.f, 100.f));
	for (const float Side : { 1.f, -1.f })
	{
		Box(EMat::MilConcrete, FVector(Xc, Side * (ReactorHalfWidth + 50.f), WallZ), FVector(Len, 100.f, WallH));
		Box(EMat::MilConcrete, FVector(Hall1EndX, Side * (Hall1HalfWidth + (ReactorHalfWidth - Hall1HalfWidth) * 0.5f), WallZ), FVector(100.f, ReactorHalfWidth - Hall1HalfWidth, WallH));
		// Tubulação e faixa de emergência
		Cyl(EMat::Metal, FVector(Xc, Side * (ReactorHalfWidth - 60.f), 0.f), 80.f, Len, FRotator(-90.f, 0.f, 0.f));
		Cyl(EMat::Metal, FVector(Xc, Side * (ReactorHalfWidth - 60.f), -150.f), 50.f, Len, FRotator(-90.f, 0.f, 0.f));
		Box(EMat::EmRed, FVector(Xc, Side * (ReactorHalfWidth - 4.f), SubFloorZ + 150.f), FVector(Len, 6.f, 10.f));
		Box(EMat::Metal, FVector(Xc, Side * (ReactorHalfWidth - 150.f), -100.f), FVector(Len, 300.f, 20.f)); // passarela decorativa
	}
	Box(EMat::MilConcrete, FVector(ReactorEndX + 50.f, 0.f, WallZ), FVector(100.f, ReactorHalfWidth * 2.f + 200.f, WallH));
	Box(EMat::MilConcrete, FVector(Hall1EndX, 0.f, (Hall1CeilingZ + ReactorCeilingZ) * 0.5f), FVector(100.f, Hall1HalfWidth * 2.f, ReactorCeilingZ - Hall1CeilingZ));

	// Plataforma do núcleo
	const FVector CoreLoc = CoreLocation();
	Cyl(EMat::Hazard, FVector(CoreLoc.X, CoreLoc.Y, SubFloorZ + 1.f), 1000.f, 2.f);
	Cyl(EMat::DarkMetal, FVector(CoreLoc.X, CoreLoc.Y, SubFloorZ + 20.f), 900.f, 40.f);

	// Pilares (cobertura contra o chefe) e muretas
	const FVector2D Pillars[] = { FVector2D(14200.f, 1200.f), FVector2D(14200.f, -1200.f), FVector2D(15200.f, 1450.f), FVector2D(15200.f, -1450.f), FVector2D(16600.f, 1150.f), FVector2D(16600.f, -1150.f) };
	for (const FVector2D& P : Pillars)
	{
		Cyl(EMat::MilConcrete, FVector(P.X, P.Y, WallZ), 220.f, WallH);
		Box(EMat::Hazard, FVector(P.X, P.Y, SubFloorZ + 80.f), FVector(160.f, 160.f, 30.f), FRotator(0.f, 45.f, 0.f));
	}
	Box(EMat::MilConcrete, FVector(13800.f, 550.f, SubFloorZ + 55.f), FVector(40.f, 300.f, 110.f));
	Box(EMat::MilConcrete, FVector(13800.f, -550.f, SubFloorZ + 55.f), FVector(40.f, 300.f, 110.f));
	Box(EMat::MilConcrete, FVector(14700.f, 300.f, SubFloorZ + 55.f), FVector(300.f, 40.f, 110.f), FRotator(0.f, 30.f, 0.f));
	Box(EMat::MilConcrete, FVector(16300.f, -400.f, SubFloorZ + 55.f), FVector(300.f, 40.f, 110.f), FRotator(0.f, -25.f, 0.f));

	// Biomassa: o salão foi tomado por carne. Montes no chão e nas paredes, tentáculos do teto, casulos.
	for (int32 i = 0; i < 26; ++i)
	{
		const float Side = (i % 2 == 0) ? 1.f : -1.f;
		const FVector P(Rng.FRandRange(13300.f, 16900.f), Side * Rng.FRandRange(ReactorHalfWidth - 350.f, ReactorHalfWidth - 40.f), SubFloorZ + Rng.FRandRange(0.f, 500.f));
		Blob(EMat::Flesh, P, FVector(Rng.FRandRange(120.f, 300.f), Rng.FRandRange(80.f, 200.f), Rng.FRandRange(80.f, 260.f)),
			FRotator(Rng.FRandRange(-20.f, 20.f), Rng.FRandRange(0.f, 360.f), 0.f));
	}
	for (int32 i = 0; i < 12; ++i)
	{
		const float A = i * 30.f;
		const FVector P = FVector(CoreLoc.X, CoreLoc.Y, SubFloorZ) + FRotator(0.f, A, 0.f).Vector() * Rng.FRandRange(520.f, 640.f);
		Blob(EMat::Flesh, P + FVector(0.f, 0.f, 10.f), FVector(Rng.FRandRange(140.f, 240.f), Rng.FRandRange(100.f, 160.f), Rng.FRandRange(30.f, 70.f)), FRotator(0.f, A, 0.f));
	}
	for (int32 i = 0; i < 14; ++i)
	{
		// Tentáculos pendurados do teto (sem colisão).
		const FVector Top(Rng.FRandRange(13400.f, 16900.f), Rng.FRandRange(-1800.f, 1800.f), ReactorCeilingZ);
		const float TentacleLen = Rng.FRandRange(250.f, 650.f);
		Cyl(EMat::FleshDark, Top - FVector(0.f, 0.f, TentacleLen * 0.5f), Rng.FRandRange(12.f, 30.f), TentacleLen, FRotator(Rng.FRandRange(-12.f, 12.f), 0.f, Rng.FRandRange(-12.f, 12.f)));
		if (i % 3 == 0)
		{
			// Casulo: algo do tamanho de uma pessoa, envolto em membrana.
			Blob(EMat::FleshDark, Top - FVector(0.f, 0.f, TentacleLen + 80.f), FVector(70.f, 65.f, 190.f));
		}
	}
	for (const FVector2D& P : Pillars)
	{
		Blob(EMat::Flesh, FVector(P.X, P.Y, SubFloorZ + 250.f), FVector(250.f, 250.f, 380.f));
	}
	BuildCorpse(FVector(14400.f, 300.f, SubFloorZ), 90.f);
	BuildCorpse(FVector(16000.f, -1300.f, SubFloorZ), -30.f);

	// Iluminação: lâmpadas de serviço morrendo e luzes vermelhas de emergência
	for (float X = 13600.f; X <= 16600.f; X += 1000.f)
	{
		AddLight(FVector(X, 0.f, ReactorCeilingZ - 100.f), FLinearColor(0.65f, 0.6f, 0.5f), 220.f, 1800.f, X < 14000.f ? 2 : 1, false);
		for (const float Side : { 1.f, -1.f })
		{
			AddLight(FVector(X, Side * (ReactorHalfWidth - 100.f), -300.f), FLinearColor(1.f, 0.05f, 0.02f), 700.f, 1800.f, 4, false);
		}
	}
}

void ACityBuilder::BuildRain()
{
	RainMesh = NewObject<UInstancedStaticMeshComponent>(this);
	RainMesh->SetMobility(EComponentMobility::Movable);
	RainMesh->SetStaticMesh(RupturaAssets::Mesh(EShape::Cube));
	RainMesh->SetMaterial(0, RupturaAssets::MakeMaterial(this, FLinearColor(0.55f, 0.6f, 0.7f), 0.1f, 0.f, 0.6f));
	RainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RainMesh->SetCanEverAffectNavigation(false);
	RainMesh->CastShadow = false;
	RainMesh->RegisterComponent();
	RainMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	AddInstanceComponent(RainMesh);

	RainDrops.SetNum(RainCount);
	RainTransforms.SetNum(RainCount);
	for (int32 i = 0; i < RainCount; ++i)
	{
		RainDrops[i] = FVector(Rng.FRandRange(-2000.f, 2000.f), Rng.FRandRange(-2000.f, 2000.f), Rng.FRandRange(0.f, 2200.f));
		RainTransforms[i] = FTransform(FRotator(-8.f, 0.f, 0.f), RainDrops[i], FVector(0.012f, 0.012f, 0.7f));
		RainMesh->AddInstance(RainTransforms[i]);
	}
}

// ---------------------------------------------------------------------------------------------
// Atualização: luzes, relâmpagos, chuva
// ---------------------------------------------------------------------------------------------

void ACityBuilder::SetAlarm(bool bOn)
{
	bAlarm = bOn;
}

void ACityBuilder::Flicker(float Duration)
{
	FlickerUntil = FMath::Max(FlickerUntil, Time + Duration);
	RupturaAssets::PlaySound2D(this, TEXT("eletrica"), 0.5f, Rng.FRandRange(0.9f, 1.1f));
}

void ACityBuilder::KillLight(FCityLight& L, bool bEffects)
{
	if (L.bDead)
	{
		return;
	}
	L.bDead = true;
	L.DieAt = -1.f;
	if (L.Light)
	{
		const FVector P = L.Light->GetComponentLocation();
		L.Light->SetIntensity(0.f);
		L.Light->SetVisibility(false);
		if (bEffects)
		{
			RupturaFx::Sparks(GetWorld(), P, FLinearColor(1.f, 0.8f, 0.5f), 8, 350.f);
			RupturaAssets::PlaySound3D(this, TEXT("luz_estouro"), P, 1.f, Rng.FRandRange(0.85f, 1.15f));
		}
	}
	// A lâmpada emissiva some e no lugar fica uma lâmpada morta.
	if (L.LampISM && L.LampInstance != INDEX_NONE)
	{
		FTransform T;
		if (L.LampISM->GetInstanceTransform(L.LampInstance, T, true))
		{
			GetISM(EMat::DarkMetal, EShape::Cube)->AddInstance(T, true);
			T.SetScale3D(FVector(0.001f));
			L.LampISM->UpdateInstanceTransform(L.LampInstance, T, true, true, false);
		}
	}
}

void ACityBuilder::BlackoutArea(const FBox& Area, const FVector& From, float StartDelay, float Interval, bool bEffects)
{
	struct FCandidate
	{
		int32 Index;
		float DistSq;
	};
	TArray<FCandidate> Found;
	for (int32 i = 0; i < Lights.Num(); ++i)
	{
		const FCityLight& L = Lights[i];
		if (!L.Light || L.bDead || L.Mode == 4 || L.DieAt >= 0.f)
		{
			continue; // as luzes de emergência sobrevivem
		}
		const FVector P = L.Light->GetComponentLocation();
		if (Area.IsInsideOrOn(P))
		{
			Found.Add({ i, static_cast<float>(FVector::DistSquared(P, From)) });
		}
	}
	Found.Sort([](const FCandidate& A, const FCandidate& B) { return A.DistSq < B.DistSq; });

	const bool bInstant = StartDelay <= 0.f && Interval <= 0.f;
	for (int32 k = 0; k < Found.Num(); ++k)
	{
		FCityLight& L = Lights[Found[k].Index];
		if (bInstant)
		{
			KillLight(L, bEffects);
		}
		else
		{
			L.DieAt = Time + StartDelay + k * Interval;
			L.bDieEffects = bEffects;
		}
	}
}

void ACityBuilder::PopLightNear(const FVector& Location, float Radius)
{
	FCityLight* Best = nullptr;
	float BestDist = Radius * Radius;
	for (FCityLight& L : Lights)
	{
		if (!L.Light || L.bDead || L.Mode == 4)
		{
			continue;
		}
		const float D = FVector::DistSquared(L.Light->GetComponentLocation(), Location);
		if (D < BestDist)
		{
			BestDist = D;
			Best = &L;
		}
	}
	if (Best)
	{
		KillLight(*Best, true);
	}
}

void ACityBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		return;
	}
	Time += DeltaSeconds;
	UpdateLights(DeltaSeconds);
	UpdateLightning(DeltaSeconds);
	UpdateRain(DeltaSeconds);
}

void ACityBuilder::UpdateLights(float DeltaSeconds)
{
	const bool bGlobalFlicker = Time < FlickerUntil;
	for (FCityLight& L : Lights)
	{
		if (!L.Light || L.bDead)
		{
			continue;
		}
		if (L.DieAt >= 0.f && Time >= L.DieAt)
		{
			KillLight(L, L.bDieEffects);
			continue;
		}
		float K = 1.f;
		switch (L.Mode)
		{
		case 1: // tremulante
			K = 0.7f + 0.3f * FMath::PerlinNoise1D(Time * 3.f + L.Phase);
			break;
		case 2: // defeituosa: apaga em rajadas e às vezes fica apagada por um tempo
			K = FMath::PerlinNoise1D(Time * 1.6f + L.Phase) > 0.2f ? (FMath::Frac(Time * 17.f + L.Phase) < 0.5f ? 0.f : 1.f) : 1.f;
			if (FMath::PerlinNoise1D(Time * 0.35f + L.Phase * 3.f) < -0.45f)
			{
				K = 0.f;
			}
			break;
		case 3: // farol pulsante
			K = 0.25f + 0.75f * FMath::Square(FMath::Max(0.f, FMath::Sin(Time * 3.f + L.Phase)));
			break;
		case 4: // emergência/alarme
			K = bAlarm ? 0.25f + 0.75f * FMath::Max(0.f, FMath::Sin(Time * 4.f + L.Phase)) : 0.f;
			break;
		default:
			break;
		}
		if (bGlobalFlicker && L.Mode != 4)
		{
			K *= FMath::PerlinNoise1D(Time * 30.f + L.Phase) > 0.f ? 1.f : 0.04f;
		}
		L.Light->SetIntensity(L.Base * K);
	}
}

void ACityBuilder::UpdateLightning(float DeltaSeconds)
{
	NextLightning -= DeltaSeconds;
	if (NextLightning <= 0.f)
	{
		NextLightning = Rng.FRandRange(9.f, 24.f);
		LightningAge = 0.f;
		ThunderAt = Time + Rng.FRandRange(0.6f, 2.2f);
	}

	LightningAge += DeltaSeconds;
	float Flash = 0.f;
	if (LightningAge < 0.08f)
	{
		Flash = 1.f;
	}
	else if (LightningAge >= 0.16f && LightningAge < 0.4f)
	{
		Flash = 0.8f * (1.f - (LightningAge - 0.16f) / 0.24f);
	}
	if (Moon)
	{
		Moon->SetIntensity(MoonBase + Flash * LightningLux);
	}
	if (SkyLight)
	{
		SkyLight->SetIntensity(SkyBase + Flash * 2.5f);
	}

	if (ThunderAt > 0.f && Time >= ThunderAt)
	{
		ThunderAt = -1.f;
		float Volume = 0.9f;
		if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector C = Cam->GetCameraLocation();
			if (C.Z < -250.f || C.X > BunkerDoorX + 200.f)
			{
				Volume = 0.25f; // abafado no subsolo
			}
		}
		RupturaAssets::PlaySound2D(this, TEXT("trovao"), Volume, Rng.FRandRange(0.85f, 1.1f));
	}
}

void ACityBuilder::UpdateRain(float DeltaSeconds)
{
	if (!RainMesh)
	{
		return;
	}
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam)
	{
		return;
	}
	const FVector C = Cam->GetCameraLocation();
	const bool bIndoors = C.Z < -250.f || C.X > BunkerDoorX + 150.f;
	RainMesh->SetVisibility(!bIndoors);
	if (bIndoors)
	{
		return;
	}

	const FVector Fall(260.f, 0.f, -2800.f);
	for (int32 i = 0; i < RainDrops.Num(); ++i)
	{
		FVector& D = RainDrops[i];
		D += Fall * DeltaSeconds;
		// Mantém as gotas numa caixa de 4000 x 4000 em volta da câmera.
		if (D.Z < C.Z - 500.f || FMath::Abs(D.X - C.X) > 2000.f || FMath::Abs(D.Y - C.Y) > 2000.f)
		{
			D = FVector(C.X + Rng.FRandRange(-2000.f, 2000.f), C.Y + Rng.FRandRange(-2000.f, 2000.f), C.Z + Rng.FRandRange(800.f, 1800.f));
		}
		RainTransforms[i].SetLocation(D);
	}
	RainMesh->BatchUpdateInstancesTransforms(0, RainTransforms, false, true, true);
}
