// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SLAYCREWEnvironmentKitTypes.generated.h"

/**
 * Functional category of an environment kit asset for SLAYCREW level construction.
 */
UENUM(BlueprintType)
enum class ESLAYCREWEnvironmentAssetCategory : uint8
{
	MasterMaterial              UMETA(DisplayName = "Master Material"),
	MaterialFunction            UMETA(DisplayName = "Material Function"),
	ArchitectureStone           UMETA(DisplayName = "Architecture - Stone & Masonry"),
	ArchitectureTimberSupport   UMETA(DisplayName = "Architecture - Timber & Support"),
	SurfaceMaterial             UMETA(DisplayName = "Surface Material & Trim"),
	DecalWeathering             UMETA(DisplayName = "Decal & Weathering Atlas"),
	LightingAndCeremonialProp   UMETA(DisplayName = "Lighting & Ceremonial Prop"),
	AcademyFurnitureAndProp     UMETA(DisplayName = "Academy Furniture & Prop"),
	CourtyardFoliageAndRock     UMETA(DisplayName = "Courtyard Foliage & Rock"),
	AtmosphereVFX               UMETA(DisplayName = "Atmosphere & VFX"),
	RuralVillageExcluded        UMETA(DisplayName = "Rural Village (Excluded)")
};

/**
 * Suitability tier for the Gothic Academy opening location (proper name TBD).
 */
UENUM(BlueprintType)
enum class ESLAYCREWGothicSuitability : uint8
{
	/** Directly appropriate for Gothic academy halls, courtyards, cloisters, or ceremonial spaces. */
	DirectReuse                 UMETA(DisplayName = "Direct Reuse (Gothic Academy)"),

	/** Usable only as secondary/concealed structural framing, cellars, smithy training wing, or exterior cliffs. */
	ConditionalSecondaryReuse   UMETA(DisplayName = "Conditional / Secondary Reuse"),

	/** Rural peasant/agrarian village asset unsuitable for a prestigious Gothic academy. */
	ExcludeRuralVillage         UMETA(DisplayName = "Exclude (Rural Medieval Village)")
};

/**
 * Pivot placement convention for modular environment kit pieces.
 */
UENUM(BlueprintType)
enum class ESLAYCREWModularPivotConvention : uint8
{
	BottomCorner                UMETA(DisplayName = "Bottom Corner (Walls / Structural Bays)"),
	BottomCenter                UMETA(DisplayName = "Bottom Center (Columns / Props / Stairs)"),
	Center                      UMETA(DisplayName = "Center (Decals / Freeform Rocks)")
};

/**
 * Grid and rotation snapping specification for modular level construction.
 */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSLAYCREWModularSnapSpec
{
	GENERATED_BODY()

	/** Translation grid step in centimeters (e.g., 400 cm bay, 100 cm wall, 50 cm trim, 10 cm prop). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ModularGrid", meta = (ClampMin = "1.0"))
	float PositionGridStepCm = 100.0f;

	/** Vertical (Z) grid step in centimeters (e.g., 400 cm story height, 50 cm step riser). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ModularGrid", meta = (ClampMin = "1.0"))
	float VerticalGridStepCm = 50.0f;

	/** Rotation snap increment in degrees (typically 90, 45, or 15 degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ModularGrid", meta = (ClampMin = "1.0"))
	float RotationGridStepDegrees = 90.0f;

	/** Scale snap increment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ModularGrid", meta = (ClampMin = "0.01"))
	float ScaleGridStep = 0.25f;

	/** Expected pivot convention for predictable snapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ModularGrid")
	ESLAYCREWModularPivotConvention PivotConvention = ESLAYCREWModularPivotConvention::BottomCenter;
};

/**
 * Catalog entry describing a source asset, its category, Gothic suitability, size, and modular snapping rules.
 */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSLAYCREWEnvironmentAssetEntry
{
	GENERATED_BODY()

	/** Short identifier for the catalog entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentKit")
	FName AssetId;

	/** Original package folder or primary package path in /Game/... format (preserved on migration). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentKit")
	FName OriginalPackagePath;

	/** Functional category of the asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentKit")
	ESLAYCREWEnvironmentAssetCategory Category = ESLAYCREWEnvironmentAssetCategory::ArchitectureStone;

	/** Suitability classification for the Gothic academy setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentKit")
	ESLAYCREWGothicSuitability Suitability = ESLAYCREWGothicSuitability::DirectReuse;

	/** Approximate disk footprint of the asset folder in megabytes (excluding shared master materials). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentKit")
	float ApproxFolderSizeMB = 0.0f;

	/** Approximate transitive dependency closure size in megabytes (including shared master materials/functions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentKit")
	float ApproxTransitiveSizeMB = 0.0f;

	/** Primary master material package path required by this asset (/Game/Materials/Masters/...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentKit")
	FName RequiredMasterMaterial;

	/** Proposed use case in the Gothic academy (or exclusion rationale if ExcludeRuralVillage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentKit")
	FString ProposedAcademyUseCase;

	/** Modular snapping rules for placing this asset in level assembly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnvironmentKit")
	FSLAYCREWModularSnapSpec SnapSpec;
};

/**
 * Verified runtime inspection details for a single loaded prototype asset in UE 5.8.
 */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSLAYCREWVerifiedAssetLoadInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Verification")
	FName PackageName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Verification")
	FString AssetClassName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Verification")
	bool bLoadedSuccessfully = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Verification")
	int32 NumLODs = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Verification")
	int32 NumMaterialSlots = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Verification")
	FVector BoundingBoxSizeCm = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Verification")
	FName ResolvedMaterialOrParentName;
};

/**
 * Result of resolving and verifying a set of source packages via Unreal Engine's AssetRegistry.
 */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSLAYCREWPackageDependencyReport
{
	GENERATED_BODY()

	/** Root packages requested for inspection/migration. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	TArray<FName> RootPackages;

	/** Complete transitive closure of /Game/... packages verified via IAssetRegistry::LoadPackageRegistryData. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	TArray<FName> ResolvedDependencyPackages;

	/** Built-in /Engine/... and /Script/... dependencies referenced by the package closure. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	TArray<FName> EngineAndScriptDependencies;

	/** Any /Game/... or /Engine/... package dependencies that could not be found on disk. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	TArray<FName> MissingPackages;

	/** Packages that exist on disk but failed binary header or AssetRegistry metadata reading. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	TArray<FName> UnreadablePackages;

	/** Unsupported non-/Game/ or unmounted plugin/script dependencies that prevent self-contained migration. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	TArray<FName> UnsupportedDependencies;

	/** Packages that collided with existing project assets during pre-flight staging validation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	TArray<FName> CollidingPackages;

	/** True only when all root packages and transitive dependencies were readable, supported, and verified present. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	bool bCompleteDependencyCoverageVerified = false;

	/** Sandbox Content directory used during isolated prototype verification. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	FString SandboxDirectoryUsed;

	/** Standalone temporary .uproject file created for isolated out-of-editor verification. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	FString SandboxProjectFilePath;

	/** Human-readable diagnostic summary when dependency resolution or sandbox staging fails. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	FString FailureReason;

	/** Total number of readable .uasset files in the resolved /Game/... dependency closure. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	int32 TotalFiles = 0;

	/** Total size in bytes of all readable .uasset files in the resolved /Game/... dependency closure. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	int64 TotalBytes = 0;

	/** Per-asset load verification results when tested in UE 5.8. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DependencyReport")
	TArray<FSLAYCREWVerifiedAssetLoadInfo> VerifiedAssets;
};
