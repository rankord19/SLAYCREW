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
 * isolated temporary-project prototype verification.
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
	 * Uses Unreal Engine's stateless const IAssetRegistry::LoadPackageRegistryData and binary package header
	 * verification to inspect .uasset files in SourceContentDir and compute the transitive /Game/... package
	 * dependency closure without mutating the active editor's AssetRegistry, mount points, or loaded objects.
	 * Never modifies the source project.
	 */
	static bool ResolveSourcePackageDependencies(
		const FString& SourceContentDir,
		const TArray<FName>& RootPackageNames,
		FSLAYCREWPackageDependencyReport& OutReport);

	/**
	 * Performs a dependency-aware prototype verification using an isolated temporary Unreal project sandbox:
	 * 1. Resolves and validates the transitive dependency closure via ResolveSourcePackageDependencies.
	 * 2. Performs pre-flight collision checks against both the active project's Content directory and any custom staging root;
	 *    never overwrites existing game assets and aborts prior to copying if any collision is detected.
	 * 3. Creates a standalone temporary Unreal project (GothicKitSandbox.uproject + Content/) under
	 *    Saved/Temp/GothicKitSandbox/<RunId>/ and stages the dependency-closed .uasset files there.
	 * 4. Verifies every staged package in the temporary project's Content directory via stateless binary header
	 *    and const IAssetRegistry::LoadPackageRegistryData inspection — NEVER remounting /Game/, NEVER calling
	 *    ScanFilesSynchronous, and NEVER calling LoadPackage inside the active editor process.
	 * 5. Deletes all staged sandbox files and the temporary project directory on scope exit when bCleanUpStagedFilesOnExit is true.
	 *
	 * VERIFICATION SCOPE & LIMITATIONS:
	 * - Verified in-process (zero interference with active editor): Binary FPackageFileSummary integrity, UE5 file version
	 *   compatibility, transitive /Game/ and /Engine/ dependency closure completeness, FAssetData class and tags (LODs,
	 *   Materials, ApproxSize bounds, Parent material), and standalone .uproject staging.
	 * - Requires separate process: Full UObject deserialization (PostLoad), RHI resource creation, and shader compilation
	 *   cannot be executed inside the active editor without mounting /Game/; use BuildSeparateProcessVerificationCommandLine()
	 *   with bCleanUpStagedFilesOnExit=false to run UnrealEditor-Cmd.exe against OutReport.SandboxProjectFilePath in a separate OS process.
	 */
	static bool VerifyPrototypeMigrationInScopedSandbox(
		const FString& SourceContentDir,
		const TArray<FName>& RootPackageNames,
		FSLAYCREWPackageDependencyReport& OutReport,
		bool bCleanUpStagedFilesOnExit = true,
		const FString& OptionalCustomSandboxContentDir = FString());

	/**
	 * Builds the command-line arguments to invoke a separate UnrealEditor-Cmd.exe process against the generated
	 * temporary SandboxProjectFilePath for out-of-process UObject load and shader verification.
	 */
	static FString BuildSeparateProcessVerificationCommandLine(const FString& SandboxProjectFilePath);

	/** Removes any leftover temporary directories under Saved/Temp/GothicKitSandbox from prior interrupted runs. */
	static bool CleanUpAbandonedStagingSandboxes();
};
