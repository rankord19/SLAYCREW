// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SLAYCREWEnvironmentKitTypes.h"
#include "SLAYCREWEnvironmentKitCatalog.generated.h"

/**
 * Organized asset catalog and modular construction utility for the SLAYCREW Gothic Academy environment kit.
 * Catalogs reusable Megascans/sample assets, classifies suitability for the Gothic academy setting,
 * tracks missing Gothic architectural elements, enforces modular grid snapping, and provides
 * AssetRegistry-backed dependency resolution and prototype verification.
 */
UCLASS(BlueprintType)
class SLAYCREW_API USLAYCREWEnvironmentKitCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	USLAYCREWEnvironmentKitCatalog();

	/** Curated asset inventory entries with original /Game/ paths, categories, sizes, and snapping specs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GothicEnvironmentKit")
	TArray<FSLAYCREWEnvironmentAssetEntry> Entries;

	/** Explicit registry of Gothic architecture elements missing from the Medieval Village sample. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GothicEnvironmentKit")
	TArray<FString> MissingGothicArchitecturePieces;

	/** Populates this catalog with the baseline Gothic Academy environment kit inventory and missing pieces list. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|EnvironmentKit")
	void PopulateDefaultGothicAcademyCatalog();

	/** Returns all catalog entries matching the given functional category. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|EnvironmentKit")
	TArray<FSLAYCREWEnvironmentAssetEntry> GetEntriesByCategory(ESLAYCREWEnvironmentAssetCategory Category) const;

	/** Returns all catalog entries matching the given Gothic academy suitability tier. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|EnvironmentKit")
	TArray<FSLAYCREWEnvironmentAssetEntry> GetEntriesBySuitability(ESLAYCREWGothicSuitability Suitability) const;

	/** Finds a specific catalog entry by its AssetId. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|EnvironmentKit")
	bool FindEntryById(FName AssetId, FSLAYCREWEnvironmentAssetEntry& OutEntry) const;

	/** Snaps a world or relative transform to the modular grid specification (position, vertical step, rotation, scale). */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|EnvironmentKit")
	static FTransform SnapTransformToModularGrid(const FTransform& InTransform, const FSLAYCREWModularSnapSpec& SnapSpec);

	/** Checks whether a transform is aligned with the modular grid specification within the given tolerance. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|EnvironmentKit")
	static bool IsTransformAlignedToGrid(const FTransform& InTransform, const FSLAYCREWModularSnapSpec& SnapSpec, float Tolerance = 0.05f);

	/**
	 * Uses Unreal Engine's native IAssetRegistry::LoadPackageRegistryData to inspect binary .uasset headers
	 * in SourceContentDir and compute the complete transitive /Game/... package dependency closure.
	 * Never modifies the source project.
	 */
	static bool ResolveSourcePackageDependencies(
		const FString& SourceContentDir,
		const TArray<FName>& RootPackageNames,
		FSLAYCREWPackageDependencyReport& OutReport);

	/**
	 * Performs a dependency-aware prototype verification in UE 5.8:
	 * 1. Resolves the exact transitive dependency closure via IAssetRegistry::LoadPackageRegistryData.
	 * 2. Stages only the missing .uasset files into the local project's Content directory preserving /Game/... paths.
	 * 3. Scans the staged packages with IAssetRegistry::ScanFilesSynchronous.
	 * 4. Loads each root UStaticMesh / UMaterialInterface in UE 5.8 and verifies LODs, bounds, and material slots.
	 * 5. Cleans up all temporarily staged .uasset files on completion if bCleanUpStagedFilesOnExit is true.
	 */
	static bool VerifyPrototypeMigrationInScopedSandbox(
		const FString& SourceContentDir,
		const TArray<FName>& RootPackageNames,
		FSLAYCREWPackageDependencyReport& OutReport,
		bool bCleanUpStagedFilesOnExit = true);
};
