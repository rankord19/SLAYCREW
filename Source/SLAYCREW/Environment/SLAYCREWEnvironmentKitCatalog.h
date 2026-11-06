// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SLAYCREWEnvironmentKitTypes.h"
#include "SLAYCREWEnvironmentKitCatalog.generated.h"

/**
 * Organized asset catalog and modular construction utility for the SLAYCREW Gothic Academy environment kit.
 * Catalogs reusable Megascans/sample assets, classifies suitability for the Gothic academy setting,
 * tracks missing Gothic architectural elements, enforces modular grid snapping (including negative scale
 * for mirrored architectural modules), and provides AssetRegistry-backed dependency resolution and
 * isolated sandbox prototype verification.
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

	/**
	 * Snaps a world or relative transform to the modular grid specification (position, vertical step, rotation, scale).
	 * Preserves the sign of negative scale axes so mirrored architectural modules remain mirrored.
	 */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|EnvironmentKit")
	static FTransform SnapTransformToModularGrid(const FTransform& InTransform, const FSLAYCREWModularSnapSpec& SnapSpec);

	/**
	 * Checks whether a transform has non-degenerate (non-zero, finite) scale on all axes.
	 * Negative scales are permitted for mirrored architectural modules.
	 */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|EnvironmentKit")
	static bool HasValidModularScale(const FTransform& InTransform, const FSLAYCREWModularSnapSpec& SnapSpec);

	/**
	 * Checks whether a transform is valid, non-degenerate, and aligned with the modular grid specification
	 * (including valid negative scale for mirrored modules) within the given tolerance.
	 */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|EnvironmentKit")
	static bool IsTransformAlignedToGrid(const FTransform& InTransform, const FSLAYCREWModularSnapSpec& SnapSpec, float Tolerance = 0.05f);

	/**
	 * Uses Unreal Engine's native IAssetRegistry::LoadPackageRegistryData and binary package tag verification
	 * to inspect .uasset files in SourceContentDir and compute the transitive /Game/... package dependency closure.
	 * Validates package readability, categorizes /Engine/ and /Script/ vs unsupported dependencies,
	 * and sets OutReport.bCompleteDependencyCoverageVerified only when full coverage is verified.
	 * Never modifies the source project.
	 */
	static bool ResolveSourcePackageDependencies(
		const FString& SourceContentDir,
		const TArray<FName>& RootPackageNames,
		FSLAYCREWPackageDependencyReport& OutReport);

	/**
	 * Performs a dependency-aware prototype verification in UE 5.8 using an isolated temporary sandbox:
	 * 1. Resolves and validates the transitive dependency closure via ResolveSourcePackageDependencies.
	 * 2. Performs pre-flight collision checks against both the active project's Content directory and any custom staging root;
	 *    never overwrites existing game assets and aborts prior to copying if any collision is detected.
	 * 3. Stages .uasset files into an isolated sandbox directory under Saved/Temp/GothicKitSandbox/<RunId>/Content
	 *    (or OptionalCustomSandboxContentDir), validating all paths, creating directories explicitly, and verifying copies.
	 * 4. Temporarily mounts the sandbox Content directory to /Game/ via FPackageName::RegisterMountPoint, scans the staged
	 *    packages with IAssetRegistry::ScanFilesSynchronous, and loads/verifies each root asset in UE 5.8.
	 * 5. Unloads staged packages, unregisters the temporary mount point, and deletes all staged sandbox files on scope exit
	 *    (including after ordinary failures).
	 *
	 * NOTE ON CRASH RECOVERY: Because staging occurs in Saved/Temp/GothicKitSandbox outside the project's Content directory,
	 * an abnormal process termination (crash / power loss / kill) will never leave untracked .uasset files in Content/.
	 * Any leftover temporary directories under Saved/Temp/GothicKitSandbox can be removed via CleanUpAbandonedStagingSandboxes()
	 * or by deleting Saved/Temp/GothicKitSandbox manually.
	 */
	static bool VerifyPrototypeMigrationInScopedSandbox(
		const FString& SourceContentDir,
		const TArray<FName>& RootPackageNames,
		FSLAYCREWPackageDependencyReport& OutReport,
		bool bCleanUpStagedFilesOnExit = true,
		const FString& OptionalCustomSandboxContentDir = FString());

	/** Removes any leftover temporary directories under Saved/Temp/GothicKitSandbox from prior interrupted runs. */
	static bool CleanUpAbandonedStagingSandboxes();
};
