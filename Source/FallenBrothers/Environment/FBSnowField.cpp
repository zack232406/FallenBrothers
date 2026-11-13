// Fallen Brothers - Deformable snow (RDR2-style tracks).

#include "Environment/FBSnowField.h"

#include "Physics/FBPhysicsProp.h"
#include "Town/FBBuilding.h"
#include "Town/FBTownProps.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
	const FName NoSnowTag(TEXT("NoSnow"));
	constexpr float EdgeFade = 400.f; // the detailed snow sinks to the ground over its last 4 m

	/** Small greyscale texture made in code (no asset needed). */
	UTexture2D* MakeTexture(UObject* Outer, int32 Size, TFunctionRef<float(float, float)> Value)
	{
		UTexture2D* Tex = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		if (!Tex)
		{
			return nullptr;
		}
		Tex->SRGB = false;
		Tex->Filter = TF_Bilinear;
		Tex->AddressX = TA_Clamp;
		Tex->AddressY = TA_Clamp;

		FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
		FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				// -1..1 across the texture
				const float U = (X + 0.5f) / Size * 2.f - 1.f;
				const float V = (Y + 0.5f) / Size * 2.f - 1.f;
				const uint8 C = static_cast<uint8>(FMath::Clamp(Value(U, V), 0.f, 1.f) * 255.f);
				Pixels[Y * Size + X] = FColor(C, C, C, C);
			}
		}
		Mip.BulkData.Unlock();
		Tex->UpdateResource();
		return Tex;
	}
}

AFBSnowField::AFBSnowField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // after everyone moved this frame

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SnowMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SnowMesh"));
	SnowMesh->SetupAttachment(Root);
	SnowMesh->SetUsingAbsoluteLocation(true);
	SnowMesh->SetUsingAbsoluteRotation(true);
	SnowMesh->SetUsingAbsoluteScale(true);
	SnowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // feet sink in: the real floor is the ground below
	SnowMesh->SetCanEverAffectNavigation(false);
	SnowMesh->bUseAsyncCooking = true;
}

// ------------------------------------------------------------------ Setup

void AFBSnowField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildGrid();
	SnowMesh->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, 1.f));
	UpdateMaterial();
}

void AFBSnowField::BeginPlay()
{
	Super::BeginPlay();

	BuildGrid();
	CreateTextures();
	UpdateMaterial();

	if (bClearUnderBuildingsAndRoads)
	{
		CollectClearAreas();
		for (const FBox& Box : ClearAreas)
		{
			Stamp(Box.GetCenter(), FVector2D(Box.GetSize()), 0.f, 4.f, true);
		}
	}
	FollowPlayer();
}

void AFBSnowField::BuildGrid()
{
	const int32 Num = FMath::Clamp(FMath::CeilToInt(DetailRadius * 2.f / VertexSpacing) + 1, 8, 1000);
	const float Half = (Num - 1) * VertexSpacing * 0.5f;

	TArray<int32> Triangles;
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	UKismetProceduralMeshLibrary::CreateGridMeshWelded(Num, Num, Triangles, Vertices, UVs, VertexSpacing);

	TArray<FVector> Normals;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	Normals.Init(FVector::UpVector, Vertices.Num());
	Tangents.Init(FProcMeshTangent(1.f, 0.f, 0.f), Vertices.Num());
	Colors.SetNumUninitialized(Vertices.Num());

	// Centre the grid on the component (whatever origin the engine helper used).
	FBox Bounds(ForceInit);
	for (const FVector& V : Vertices)
	{
		Bounds += V;
	}
	const FVector Center = Bounds.GetCenter();

	for (int32 i = 0; i < Vertices.Num(); ++i)
	{
		FVector& V = Vertices[i];
		V.X -= Center.X;
		V.Y -= Center.Y;
		V.Z = 0.f;

		// Vertex colour alpha = how much snow height this point gets (0 at the edge = flush with the ground).
		const float ToEdge = Half - FMath::Max(FMath::Abs(V.X), FMath::Abs(V.Y));
		const float A = FMath::SmoothStep(0.f, EdgeFade, ToEdge);
		Colors[i] = FLinearColor(1.f, 1.f, 1.f, A);
	}

	// One unused point at the top so the bounds include the raised snow (no popping at the edges of the view).
	Vertices.Add(FVector(0.f, 0.f, SnowDepth + 10.f));
	UVs.Add(FVector2D::ZeroVector);
	Normals.Add(FVector::UpVector);
	Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
	Colors.Add(FLinearColor::White);

	SnowMesh->ClearAllMeshSections();
	SnowMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
}

void AFBSnowField::CreateTextures()
{
	TrailTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, TrailResolution, TrailResolution, RTF_R16f, FLinearColor::Black, false);
	if (TrailTarget)
	{
		TrailTarget->AddressX = TA_Clamp;
		TrailTarget->AddressY = TA_Clamp;
		UKismetRenderingLibrary::ClearRenderTarget2D(this, TrailTarget, FLinearColor::Black);
	}

	// Boot print: soft-edged oval, deepest in the middle.
	BrushTexture = MakeTexture(this, 64, [](float U, float V)
	{
		const float D = FMath::Sqrt(U * U + V * V);
		return 1.f - FMath::SmoothStep(0.55f, 1.f, D);
	});

	WhiteTexture = MakeTexture(this, 4, [](float, float) { return 1.f; });
}

void AFBSnowField::UpdateMaterial()
{
	if (!SnowMaterial)
	{
		return;
	}
	SnowMID = UMaterialInstanceDynamic::Create(SnowMaterial, this);
	SnowMesh->SetMaterial(0, SnowMID);

	const FVector2D Min = AreaMin();
	SnowMID->SetVectorParameterValue(TEXT("SnowArea"), FLinearColor(Min.X, Min.Y, AreaSize.X, AreaSize.Y));
	SnowMID->SetScalarParameterValue(TEXT("SnowDepth"), SnowDepth);
	SnowMID->SetScalarParameterValue(TEXT("SnowTexel"), 1.f / FMath::Max(1, TrailResolution));
	if (TrailTarget)
	{
		SnowMID->SetTextureParameterValue(TEXT("SnowTrail"), TrailTarget);
	}
}

void AFBSnowField::CollectClearAreas()
{
	ClearAreas.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		bool bClear = A->ActorHasTag(NoSnowTag) || A->IsA<AFBBuilding>() || A->IsA<AFBRoad>();

		// Your real (scanned) houses: big static meshes standing on the ground.
		if (!bClear && A->IsA<AStaticMeshActor>())
		{
			FVector Origin, Extent;
			A->GetActorBounds(true, Origin, Extent);
			// A house: stands on the ground, rises well above the snow, isn't the ground itself.
			const float Top = Origin.Z + Extent.Z;
			const float Bottom = Origin.Z - Extent.Z;
			bClear = Extent.X > 150.f && Extent.Y > 150.f && Extent.X < 4000.f && Extent.Y < 4000.f
				&& Top > GroundZ() + 250.f && Bottom < GroundZ() + 100.f;
		}
		if (!bClear)
		{
			continue;
		}

		FVector Origin, Extent;
		A->GetActorBounds(true, Origin, Extent);
		const FBox Box = FBox(Origin - Extent, Origin + Extent).ExpandBy(FVector(30.f, 30.f, 0.f));
		if (IsInside(Box.GetCenter()))
		{
			ClearAreas.Add(Box);
			UE_LOG(LogTemp, Log, TEXT("FBSnowField: no snow under %s"), *A->GetName());
		}
	}
}

// ------------------------------------------------------------------ Every frame

void AFBSnowField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!TrailTarget)
	{
		return;
	}
	FollowPlayer();
	UpdateWalkers();
	if (bPropsCarve)
	{
		UpdateProps();
	}
	const int32 StampsThisFrame = Pending.Num();
	Flush(DeltaSeconds);

	if (bDebug && GEngine)
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		const ACharacter* C = Cast<ACharacter>(Pawn);
		FString Info = TEXT("FBSnowField: no player");
		if (C)
		{
			const FVector Feet = C->GetActorLocation() - FVector(0.f, 0.f, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const bool bClearHere = ClearAreas.ContainsByPredicate([&Feet](const FBox& B) { return B.IsInsideXY(Feet); });
			Info = FString::Printf(TEXT("FBSnowField | inside area: %s | feet Z %.0f (snow ground Z %.0f) | on ground: %s | in a no-snow area: %s | clear areas: %d | prints this frame: %d | material: %s"),
				IsInside(Feet) ? TEXT("yes") : TEXT("NO"), Feet.Z, GroundZ(),
				C->GetCharacterMovement()->IsMovingOnGround() ? TEXT("yes") : TEXT("NO"),
				bClearHere ? TEXT("YES") : TEXT("no"), ClearAreas.Num(), StampsThisFrame,
				SnowMID ? TEXT("ok") : TEXT("MISSING"));
		}
		GEngine->AddOnScreenDebugMessage(9876, 0.f, FColor::Cyan, Info);
	}
}

void AFBSnowField::FollowPlayer()
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Center = Pawn ? Pawn->GetActorLocation() : GetActorLocation();

	// Snap to the vertex spacing so the snow doesn't "swim" while the mesh follows.
	const float X = FMath::GridSnap(Center.X, VertexSpacing);
	const float Y = FMath::GridSnap(Center.Y, VertexSpacing);
	SnowMesh->SetWorldLocation(FVector(X, Y, GroundZ() + 1.f));
}

void AFBSnowField::UpdateWalkers()
{
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* C = *It;
		UCharacterMovementComponent* Move = C->GetCharacterMovement();
		const UCapsuleComponent* Capsule = C->GetCapsuleComponent();
		if (!Move || !Capsule)
		{
			continue;
		}

		const FVector Feet = C->GetActorLocation() - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight());
		const bool bInSnow = Move->IsMovingOnGround()
			&& IsInside(Feet)
			&& FMath::Abs(Feet.Z - GroundZ()) < SnowDepth + 100.f
			&& !ClearAreas.ContainsByPredicate([&Feet](const FBox& B) { return B.IsInsideXY(Feet); });

		FWalker& W = Walkers.FindOrAdd(C);
		if (!bInSnow)
		{
			W.bValid = false;
			continue;
		}

		// Heavier walking.
		if (SpeedMultiplier < 1.f)
		{
			const float Max = Move->GetMaxSpeed() * SpeedMultiplier;
			const FVector V = Move->Velocity;
			const FVector2D H(V.X, V.Y);
			if (H.SizeSquared() > Max * Max)
			{
				const FVector2D N = H.GetSafeNormal() * Max;
				Move->Velocity = FVector(N.X, N.Y, V.Z);
			}
		}

		// Big creatures leave big prints.
		const float BodyScale = FMath::Clamp(Capsule->GetScaledCapsuleRadius() / 34.f, 0.6f, 3.f);

		if (!W.bValid || FVector::Dist2D(Feet, W.LastStep) > 600.f)
		{
			// Just arrived (or teleported): both feet down where we stand.
			const float Yaw = C->GetActorRotation().Yaw;
			const FVector Right = FRotator(0.f, Yaw, 0.f).RotateVector(FVector::RightVector);
			Stamp(Feet + Right * 11.f * BodyScale, FootprintSize * BodyScale, Yaw, 1.f);
			Stamp(Feet - Right * 11.f * BodyScale, FootprintSize * BodyScale, Yaw, 1.f);
			W.LastStep = W.LastTrench = Feet;
			W.bValid = true;
			continue;
		}

		const FVector Delta = Feet - W.LastStep;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));

		if (FVector::Dist2D(Feet, W.LastStep) >= StrideLength * BodyScale)
		{
			const FVector Dir = Delta.GetSafeNormal2D();
			const FVector Right(-Dir.Y, Dir.X, 0.f);
			const float Side = W.bLeftFoot ? -1.f : 1.f;
			Stamp(Feet + Right * Side * 11.f * BodyScale, FootprintSize * BodyScale, Yaw, 1.f);
			W.bLeftFoot = !W.bLeftFoot;
			W.LastStep = Feet;
		}

		if (bTrench && FVector::Dist2D(Feet, W.LastTrench) >= TrenchWidth * 0.35f)
		{
			Stamp(Feet, FVector2D(TrenchWidth * 1.4f, TrenchWidth) * BodyScale, Yaw, TrenchStrength);
			W.LastTrench = Feet;
		}
	}
}

void AFBSnowField::UpdateProps()
{
	for (TActorIterator<AFBPhysicsProp> It(GetWorld()); It; ++It)
	{
		AFBPhysicsProp* Prop = *It;
		const UStaticMeshComponent* Mesh = Prop->GetMesh();
		if (!Mesh || !Mesh->IsSimulatingPhysics() || !Mesh->GetStaticMesh())
		{
			continue;
		}

		const FBoxSphereBounds& B = Mesh->Bounds;
		const float Bottom = B.Origin.Z - B.BoxExtent.Z;
		const float Sink = GroundZ() + SnowDepth - Bottom; // how far it is into the snow
		if (Sink <= 0.f || !IsInside(B.Origin))
		{
			continue;
		}

		FVector& Last = PropLast.FindOrAdd(Prop, FVector(FLT_MAX));
		const FVector LocalExtent = Mesh->GetStaticMesh()->GetBounds().BoxExtent * Mesh->GetComponentScale();
		const float MoveStep = FMath::Max(4.f, FMath::Min(LocalExtent.X, LocalExtent.Y) * 0.5f);
		if (FVector::Dist2D(B.Origin, Last) < MoveStep)
		{
			continue;
		}
		Last = B.Origin;

		const float Strength = FMath::Clamp(Sink / SnowDepth, 0.3f, 1.f);
		Stamp(B.Origin, FVector2D(LocalExtent.X, LocalExtent.Y) * 2.f, Mesh->GetComponentRotation().Yaw, Strength);
	}
}

void AFBSnowField::Flush(float DeltaSeconds)
{
	// Fresh snow: once a second, fade all tracks a little toward untouched snow.
	float RefillAlpha = 0.f;
	RefillTimer += DeltaSeconds;
	if (RefillPerMinute > 0.f && RefillTimer >= 1.f)
	{
		RefillAlpha = 1.f - FMath::Pow(1.f - FMath::Min(RefillPerMinute, 0.999f), RefillTimer / 60.f);
		RefillTimer = 0.f;

		// Plowed / indoor areas stay clear.
		for (const FBox& Box : ClearAreas)
		{
			Stamp(Box.GetCenter(), FVector2D(Box.GetSize()), 0.f, 4.f, true);
		}
	}

	if (Pending.Num() == 0 && RefillAlpha <= 0.f)
	{
		return;
	}

	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, TrailTarget, Canvas, CanvasSize, Context);
	if (Canvas)
	{
		if (RefillAlpha > 0.f && WhiteTexture)
		{
			Canvas->K2_DrawTexture(WhiteTexture, FVector2D::ZeroVector, CanvasSize, FVector2D::ZeroVector, FVector2D::UnitVector,
			                       FLinearColor(0.f, 0.f, 0.f, RefillAlpha), BLEND_Translucent);
		}

		for (const FStamp& S : Pending)
		{
			UTexture2D* Tex = S.bSolid ? WhiteTexture : BrushTexture;
			if (!Tex)
			{
				continue;
			}
			Canvas->K2_DrawTexture(Tex, S.Center - S.Size * 0.5f, S.Size, FVector2D::ZeroVector, FVector2D::UnitVector,
			                       FLinearColor(S.Strength, S.Strength, S.Strength, 1.f), BLEND_Additive, S.Yaw, FVector2D(0.5f, 0.5f));
		}
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	Pending.Reset();
}

// ------------------------------------------------------------------ Helpers

void AFBSnowField::Stamp(const FVector& World, const FVector2D& SizeCm, float Yaw, float Strength, bool bSolid)
{
	const FVector2D PixelsPerCm(TrailResolution / AreaSize.X, TrailResolution / AreaSize.Y);
	FStamp S;
	S.Center = (FVector2D(World.X, World.Y) - AreaMin()) * PixelsPerCm;
	S.Size = FVector2D(FMath::Max(1.f, SizeCm.X * PixelsPerCm.X), FMath::Max(1.f, SizeCm.Y * PixelsPerCm.Y));
	S.Yaw = Yaw;
	S.Strength = Strength;
	S.bSolid = bSolid;
	Pending.Add(S);
}

bool AFBSnowField::IsInside(const FVector& World) const
{
	const FVector2D Min = AreaMin();
	return World.X > Min.X && World.Y > Min.Y && World.X < Min.X + AreaSize.X && World.Y < Min.Y + AreaSize.Y;
}

FVector2D AFBSnowField::AreaMin() const
{
	return FVector2D(GetActorLocation().X, GetActorLocation().Y) - AreaSize * 0.5f;
}
