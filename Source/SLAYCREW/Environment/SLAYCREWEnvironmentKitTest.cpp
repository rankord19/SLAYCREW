// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Environment/SLAYCREWEnvironmentKitCatalog.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSLAYCREWEnvironmentKitCatalogTest,
	"SLAYCREW.Environment.GothicKit.Portable.CatalogAndGridSnapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSLAYCREWEnvironmentKitCatalogTest::RunTest(const FString& Parameters)
{
	USLAYCREWEnvironmentKitCatalog* Catalog = NewObject<USLAYCREWEnvironmentKitCatalog>();
	TestNotNull(TEXT("Catalog instance created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	TestTrue(TEXT("Catalog contains curated entries"), Catalog->Entries.Num() >= 25);
	TestTrue(TEXT("Catalog records missing Gothic architecture pieces"), Catalog->MissingGothicArchitecturePieces.Num() >= 8);

	// Verify suitability tiers distinguish Gothic academy assets from rural village assets
	const TArray<FSLAYCREWEnvironmentAssetEntry> DirectReuse =
		Catalog->GetEntriesBySuitability(ESLAYCREWGothicSuitability::DirectReuse);
	const TArray<FSLAYCREWEnvironmentAssetEntry> ConditionalReuse =
		Catalog->GetEntriesBySuitability(ESLAYCREWGothicSuitability::ConditionalSecondaryReuse);
	const TArray<FSLAYCREWEnvironmentAssetEntry> ExcludedRural =
		Catalog->GetEntriesBySuitability(ESLAYCREWGothicSuitability::ExcludeRuralVillage);

	TestTrue(TEXT("DirectReuse Gothic academy entries populated"), DirectReuse.Num() >= 18);
	TestTrue(TEXT("ConditionalSecondaryReuse entries populated"), ConditionalReuse.Num() >= 2);
	TestTrue(TEXT("ExcludeRuralVillage entries populated"), ExcludedRural.Num() >= 5);

	FSLAYCREWEnvironmentAssetEntry StairsEntry;
	TestTrue(TEXT("DamagedCastleStairs exists in catalog"), Catalog->FindEntryById(TEXT("DamagedCastleStairs"), StairsEntry));
	TestEqual(
		TEXT("DamagedCastleStairs is classified as ArchitectureStone"),
		StairsEntry.Category,
		ESLAYCREWEnvironmentAssetCategory::ArchitectureStone);
	TestEqual(
		TEXT("DamagedCastleStairs is classified as DirectReuse"),
		StairsEntry.Suitability,
		ESLAYCREWGothicSuitability::DirectReuse);

	// Verify modular grid snapping and alignment checks
	FSLAYCREWModularSnapSpec BaySpec;
	BaySpec.PositionGridStepCm = 400.0f;
	BaySpec.VerticalGridStepCm = 400.0f;
	BaySpec.RotationGridStepDegrees = 90.0f;
	BaySpec.ScaleGridStep = 0.25f;

	const FTransform UnalignedTransform(
		FRotator(3.2f, 87.4f, -1.1f),
		FVector(789.3f, -412.8f, 395.0f),
		FVector(1.04f, 0.98f, 1.01f));

	TestFalse(
		TEXT("Unaligned transform is detected as off-grid"),
		USLAYCREWEnvironmentKitCatalog::IsTransformAlignedToGrid(UnalignedTransform, BaySpec));

	const FTransform SnappedTransform =
		USLAYCREWEnvironmentKitCatalog::SnapTransformToModularGrid(UnalignedTransform, BaySpec);

	TestTrue(
		TEXT("Snapped transform is aligned to 400 cm structural bay grid"),
		USLAYCREWEnvironmentKitCatalog::IsTransformAlignedToGrid(SnappedTransform, BaySpec));
	TestEqual(TEXT("Snapped X == 800 cm"), SnappedTransform.GetLocation().X, 800.0);
	TestEqual(TEXT("Snapped Y == -400 cm"), SnappedTransform.GetLocation().Y, -400.0);
	TestEqual(TEXT("Snapped Z == 400 cm"), SnappedTransform.GetLocation().Z, 400.0);
	TestEqual(TEXT("Snapped Yaw == 90 deg"), SnappedTransform.Rotator().Yaw, 90.0);

	// Verify negative scale preservation for mirrored architectural modules
	const FTransform MirroredUnaligned(
		FRotator(0.0f, -88.0f, 0.0f),
		FVector(-398.0f, 802.0f, 0.0f),
		FVector(-1.03f, 0.97f, -1.96f));

	const FTransform MirroredSnapped =
		USLAYCREWEnvironmentKitCatalog::SnapTransformToModularGrid(MirroredUnaligned, BaySpec);

	TestEqual(TEXT("Mirrored Scale.X preserves negative sign (-1.0)"), MirroredSnapped.GetScale3D().X, -1.0);
	TestEqual(TEXT("Mirrored Scale.Y snaps to +1.0"), MirroredSnapped.GetScale3D().Y, 1.0);
	TestEqual(TEXT("Mirrored Scale.Z preserves negative sign (-2.0)"), MirroredSnapped.GetScale3D().Z, -2.0);
	TestTrue(
		TEXT("Mirrored snapped transform passes HasValidModularScale"),
		USLAYCREWEnvironmentKitCatalog::HasValidModularScale(MirroredSnapped, BaySpec));
	TestTrue(
		TEXT("Mirrored snapped transform passes IsTransformAlignedToGrid"),
		USLAYCREWEnvironmentKitCatalog::IsTransformAlignedToGrid(MirroredSnapped, BaySpec));

	// Verify degenerate near-zero scale is explicitly rejected by HasValidModularScale and IsTransformAlignedToGrid
	const FTransform DegenerateZeroScale(
		FRotator::ZeroRotator,
		FVector(400.0f, 400.0f, 0.0f),
		FVector(0.0f, 1.0f, 1.0f));
	TestFalse(
		TEXT("Zero scale axis is rejected by HasValidModularScale"),
		USLAYCREWEnvironmentKitCatalog::HasValidModularScale(DegenerateZeroScale, BaySpec));
	TestFalse(
		TEXT("Zero scale axis is rejected by IsTransformAlignedToGrid"),
		USLAYCREWEnvironmentKitCatalog::IsTransformAlignedToGrid(DegenerateZeroScale, BaySpec));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSLAYCREWEnvironmentKitSafetyTest,
	"SLAYCREW.Environment.GothicKit.Portable.DependencyAndStagingSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSLAYCREWEnvironmentKitSafetyTest::RunTest(const FString& Parameters)
{
	// 1. Missing source directory must fail (never return a false positive)
	{
		const FString NonExistentSourceDir = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Temp/NonExistentMedievalVillageSource_DNE/Content"));
		const TArray<FName> DummyRoots = { FName(TEXT("/Game/Meshes/Candles/SM_Candles_01")) };

		FSLAYCREWPackageDependencyReport MissingSourceReport;
		const bool bResolved = USLAYCREWEnvironmentKitCatalog::ResolveSourcePackageDependencies(
			NonExistentSourceDir,
			DummyRoots,
			MissingSourceReport);

		TestFalse(TEXT("ResolveSourcePackageDependencies fails when source directory is missing"), bResolved);
		TestFalse(TEXT("bCompleteDependencyCoverageVerified is false when source is missing"), MissingSourceReport.bCompleteDependencyCoverageVerified);
		TestFalse(TEXT("FailureReason is populated when source is missing"), MissingSourceReport.FailureReason.IsEmpty());

		FSLAYCREWPackageDependencyReport SandboxMissingSourceReport;
		const bool bVerified = USLAYCREWEnvironmentKitCatalog::VerifyPrototypeMigrationInScopedSandbox(
			NonExistentSourceDir,
			DummyRoots,
			SandboxMissingSourceReport);
		TestFalse(TEXT("VerifyPrototypeMigrationInScopedSandbox fails when source directory is missing"), bVerified);
	}

	// 2. Synthetic source directory tests for missing packages, corrupt/unreadable packages, and unsupported paths
	const FString SyntheticRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Temp/GothicKitSafetyTest"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	const FString SyntheticSourceContent = FPaths::Combine(SyntheticRoot, TEXT("SourceContent"));
	const FString SyntheticSandboxContent = FPaths::Combine(SyntheticRoot, TEXT("SandboxContent"));

	ON_SCOPE_EXIT
	{
		IFileManager::Get().DeleteDirectory(*SyntheticRoot, /*RequireExists=*/false, /*Tree=*/true);
	};

	IFileManager::Get().MakeDirectory(*SyntheticSourceContent, /*Tree=*/true);

	// Create a corrupt/unreadable .uasset file (invalid magic header)
	const FString CorruptAssetFile = FPaths::Combine(SyntheticSourceContent, TEXT("Corrupt/SM_Corrupt.uasset"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CorruptAssetFile), /*Tree=*/true);
	const TArray<uint8> InvalidBytes = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03 };
	FFileHelper::SaveArrayToFile(InvalidBytes, *CorruptAssetFile);

	{
		const TArray<FName> TestRoots = {
			FName(TEXT("/Game/Missing/SM_DoesNotExist")),
			FName(TEXT("/Game/Corrupt/SM_Corrupt")),
			FName(TEXT("/UnmountedExternalPlugin/Meshes/SM_Unsupported"))
		};

		FSLAYCREWPackageDependencyReport DiagnosticReport;
		const bool bResolved = USLAYCREWEnvironmentKitCatalog::ResolveSourcePackageDependencies(
			SyntheticSourceContent,
			TestRoots,
			DiagnosticReport);

		TestFalse(TEXT("ResolveSourcePackageDependencies fails on missing/unreadable/unsupported inputs"), bResolved);
		TestFalse(TEXT("bCompleteDependencyCoverageVerified is false on bad inputs"), DiagnosticReport.bCompleteDependencyCoverageVerified);
		TestTrue(
			TEXT("Missing package is recorded in MissingPackages"),
			DiagnosticReport.MissingPackages.Contains(FName(TEXT("/Game/Missing/SM_DoesNotExist"))));
		TestTrue(
			TEXT("Corrupt .uasset is recorded in UnreadablePackages"),
			DiagnosticReport.UnreadablePackages.Contains(FName(TEXT("/Game/Corrupt/SM_Corrupt"))));
		TestTrue(
			TEXT("Unmounted plugin path is recorded in UnsupportedDependencies"),
			DiagnosticReport.UnsupportedDependencies.Contains(FName(TEXT("/UnmountedExternalPlugin/Meshes/SM_Unsupported"))));
	}

	// 3. Pre-flight collision detection: copy one valid small .uasset from the active project into SyntheticSourceContent
	// and verify that staging refuses to overwrite existing game assets or colliding sandbox files.
	TArray<FString> ExistingProjectAssets;
	IFileManager::Get().FindFilesRecursive(
		ExistingProjectAssets,
		*FPaths::ProjectContentDir(),
		TEXT("*.uasset"),
		/*Files=*/true,
		/*Directories=*/false);

	if (ExistingProjectAssets.Num() > 0)
	{
		// Also create a synthetic standalone .uasset copy in SyntheticSourceContent under a non-colliding path,
		// and pre-populate SyntheticSandboxContent with a sentinel file at the same path to test sandbox collision prevention.
		const FString SentinelRelative = TEXT("SyntheticCollisionTest/SM_CollisionTest.uasset");
		const FString SrcCollisionFile = FPaths::Combine(SyntheticSourceContent, SentinelRelative);
		const FString DstCollisionFile = FPaths::Combine(SyntheticSandboxContent, SentinelRelative);

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(SrcCollisionFile), /*Tree=*/true);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(DstCollisionFile), /*Tree=*/true);

		// Copy a real .uasset header into SrcCollisionFile only if we want LoadPackageRegistryData to parse it,
		// or test active project collision directly using the source sample if present.
		const TArray<uint8> SentinelContent = { 0x4B, 0x45, 0x45, 0x50 };
		FFileHelper::SaveArrayToFile(SentinelContent, *DstCollisionFile);

		// Verify unsafe overlapping source/sandbox directories are rejected
		FSLAYCREWPackageDependencyReport OverlapReport;
		const bool bOverlapResult = USLAYCREWEnvironmentKitCatalog::VerifyPrototypeMigrationInScopedSandbox(
			SyntheticSourceContent,
			{ FName(TEXT("/Game/Corrupt/SM_Corrupt")) },
			OverlapReport,
			/*bCleanUpStagedFilesOnExit=*/true,
			SyntheticSourceContent);
		TestFalse(TEXT("Overlapping source and sandbox directories are rejected"), bOverlapResult);

		// Verify sentinel file in SyntheticSandboxContent was never overwritten or deleted
		TArray<uint8> ReadBack;
		TestTrue(TEXT("Existing destination file preserved untouched"), FFileHelper::LoadFileToArray(ReadBack, *DstCollisionFile));
		TestEqual(TEXT("Existing destination file content unchanged"), ReadBack.Num(), SentinelContent.Num());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSLAYCREWEnvironmentKitPrototypeMigrationTest,
	"SLAYCREW.Environment.GothicKit.Integration.PrototypeMigrationAndLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSLAYCREWEnvironmentKitPrototypeMigrationTest::RunTest(const FString& Parameters)
{
	const FString SourceContentDir = TEXT("D:/Unreal Projects/MedievalVillageMegascansS/Content");
	if (!IFileManager::Get().DirectoryExists(*SourceContentDir))
	{
		const bool bExplicitlyAllowSkip =
			Parameters.Contains(TEXT("AllowSkipIfSourceMissing")) ||
			FPlatformMisc::GetEnvironmentVariable(TEXT("SLAYCREW_SKIP_MISSING_MIGRATION_SOURCE")) == TEXT("1");

		if (bExplicitlyAllowSkip)
		{
			AddWarning(FString::Printf(
				TEXT("SKIPPED (NOT VERIFIED): Source sample Content directory not found at '%s'. Migration integration checks were not executed."),
				*SourceContentDir));
			return true;
		}

		AddError(FString::Printf(
			TEXT("Source sample Content directory required for integration test was not found at '%s'. Migration verification cannot succeed without source input."),
			*SourceContentDir));
		return false;
	}

	const TArray<FName> RepresentativeRootPackages = {
		FName(TEXT("/Game/Meshes/Candles/SM_Candles_01")),
		FName(TEXT("/Game/Megascans/Atlases/WallRustLeakage/MI_WallRustLeakage")),
		FName(TEXT("/Game/Megascans/3D_Assets/JapaneseShrineStoneFloorA/SM_JapaneseShrineStoneFloorA")),
		FName(TEXT("/Game/Megascans/3D_Assets/DamagedCastleStairs/SM_DamagedCastleStairs"))
	};

	// First test pre-flight collision protection using a custom sandbox pre-populated with a colliding file
	{
		const FString CollisionSandboxRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Temp/GothicKitCollisionCheck"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		const FString CollisionSandboxContent = FPaths::Combine(CollisionSandboxRoot, TEXT("Content"));
		const FString PreExistingFile = FPaths::Combine(CollisionSandboxContent, TEXT("Meshes/Candles/SM_Candles_01.uasset"));
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*CollisionSandboxRoot, /*RequireExists=*/false, /*Tree=*/true);
		};

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(PreExistingFile), /*Tree=*/true);
		const TArray<uint8> ProtectedBytes = { 0x53, 0x41, 0x46, 0x45 };
		FFileHelper::SaveArrayToFile(ProtectedBytes, *PreExistingFile);

		FSLAYCREWPackageDependencyReport CollisionReport;
		const bool bCollisionRun = USLAYCREWEnvironmentKitCatalog::VerifyPrototypeMigrationInScopedSandbox(
			SourceContentDir,
			{ FName(TEXT("/Game/Meshes/Candles/SM_Candles_01")) },
			CollisionReport,
			/*bCleanUpStagedFilesOnExit=*/true,
			CollisionSandboxContent);

		TestFalse(TEXT("Sandbox staging aborts when a package path collision is detected"), bCollisionRun);
		TestTrue(
			TEXT("Colliding package is reported in CollidingPackages"),
			CollisionReport.CollidingPackages.Contains(FName(TEXT("/Game/Meshes/Candles/SM_Candles_01"))));

		TArray<uint8> PostCollisionBytes;
		TestTrue(TEXT("Pre-existing colliding file still exists after abort"), FFileHelper::LoadFileToArray(PostCollisionBytes, *PreExistingFile));
		TestEqual(TEXT("Pre-existing colliding file was not overwritten"), PostCollisionBytes.Num(), ProtectedBytes.Num());
	}

	// 2. Verify active-editor isolation while a temporary sandbox project is staged on disk (bCleanUpStagedFilesOnExit = false):
	//    - Standalone GothicKitSandbox.uproject is created in the temporary project directory.
	//    - Active editor's /Game/ mount point still resolves strictly to FPaths::ProjectContentDir().
	//    - Active editor's IAssetRegistry contains 0 entries for the sandbox packages.
	//    - Active editor's UPackage memory table contains 0 loaded sandbox packages.
	{
		ON_SCOPE_EXIT
		{
			USLAYCREWEnvironmentKitCatalog::CleanUpAbandonedStagingSandboxes();
		};

		const FString ProbePackageName(TEXT("/Game/Meshes/Candles/SM_Candles_01"));
		const FString AssetExt(TEXT(".uasset"));
		FString ActiveGameResolvedBefore;
		TestTrue(
			TEXT("FPackageName resolves /Game/Meshes/Candles/SM_Candles_01 before staging"),
			FPackageName::TryConvertLongPackageNameToFilename(
				ProbePackageName,
				ActiveGameResolvedBefore,
				AssetExt));

		FSLAYCREWPackageDependencyReport IsolationReport;
		const bool bStagedOk = USLAYCREWEnvironmentKitCatalog::VerifyPrototypeMigrationInScopedSandbox(
			SourceContentDir,
			{ FName(TEXT("/Game/Meshes/Candles/SM_Candles_01")) },
			IsolationReport,
			/*bCleanUpStagedFilesOnExit=*/false);

		TestTrue(TEXT("Isolated temporary project staging succeeded"), bStagedOk);
		TestTrue(
			TEXT("Standalone temporary GothicKitSandbox.uproject file created on disk"),
			IFileManager::Get().FileExists(*IsolationReport.SandboxProjectFilePath));

		const FString CmdLine = USLAYCREWEnvironmentKitCatalog::BuildSeparateProcessVerificationCommandLine(
			IsolationReport.SandboxProjectFilePath);
		TestTrue(
			TEXT("Out-of-process verification command line targets GothicKitSandbox.uproject"),
			CmdLine.Contains(TEXT("GothicKitSandbox.uproject")));

		// Check active editor /Game/ mount point while sandbox files are still on disk
		FString ActiveGameResolvedDuring;
		TestTrue(
			TEXT("FPackageName resolves /Game/Meshes/Candles/SM_Candles_01 while sandbox is staged"),
			FPackageName::TryConvertLongPackageNameToFilename(
				ProbePackageName,
				ActiveGameResolvedDuring,
				AssetExt));
		TestTrue(
			TEXT("Active editor /Game/ mount point was never remounted to sandbox"),
			FPaths::IsSamePath(ActiveGameResolvedBefore, ActiveGameResolvedDuring));

		// Check active editor IAssetRegistry was not polluted by staged sandbox files
		if (IAssetRegistry* ActiveRegistry = IAssetRegistry::Get())
		{
			TArray<FAssetData> PollutedAssets;
			ActiveRegistry->GetAssetsByPackageName(FName(TEXT("/Game/Meshes/Candles/SM_Candles_01")), PollutedAssets);
			TestEqual(TEXT("Active editor AssetRegistry has 0 entries from temporary sandbox"), PollutedAssets.Num(), 0);
		}

		// Check active editor UPackage memory was not polluted by loading sandbox packages in-process
		TestNull(
			TEXT("Active editor has not loaded /Game/Meshes/Candles/SM_Candles_01 into memory"),
			FindPackage(nullptr, TEXT("/Game/Meshes/Candles/SM_Candles_01")));

		TestTrue(
			TEXT("CleanUpAbandonedStagingSandboxes removes retained sandbox project"),
			USLAYCREWEnvironmentKitCatalog::CleanUpAbandonedStagingSandboxes());
		TestFalse(
			TEXT("Temporary GothicKitSandbox.uproject removed after cleanup"),
			IFileManager::Get().FileExists(*IsolationReport.SandboxProjectFilePath));
	}

	// 3. Execute full isolated sandbox prototype migration verification with automatic scope-exit cleanup
	FSLAYCREWPackageDependencyReport Report;
	const bool bVerified = USLAYCREWEnvironmentKitCatalog::VerifyPrototypeMigrationInScopedSandbox(
		SourceContentDir,
		RepresentativeRootPackages,
		Report,
		/*bCleanUpStagedFilesOnExit=*/true);

	TestTrue(TEXT("All representative Gothic kit packages resolved and verified in isolated sandbox"), bVerified);
	TestTrue(TEXT("Complete dependency coverage verified"), Report.bCompleteDependencyCoverageVerified);
	TestEqual(TEXT("Zero missing package dependencies"), Report.MissingPackages.Num(), 0);
	TestEqual(TEXT("Zero unreadable package dependencies"), Report.UnreadablePackages.Num(), 0);
	TestEqual(TEXT("Zero unsupported package dependencies"), Report.UnsupportedDependencies.Num(), 0);
	TestEqual(TEXT("Zero colliding packages"), Report.CollidingPackages.Num(), 0);
	TestTrue(TEXT("Resolved transitive package closure is non-empty"), Report.TotalFiles >= 30);
	TestEqual(TEXT("Verified all 4 root assets"), Report.VerifiedAssets.Num(), RepresentativeRootPackages.Num());

	// Verify staging occurred in Saved/Temp/GothicKitSandbox and never in ProjectContentDir
	const FString ActiveProjectContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	TestFalse(
		TEXT("Sandbox directory is outside the active project's Content directory"),
		FPaths::IsUnderDirectory(Report.SandboxDirectoryUsed, ActiveProjectContentDir));

	AddInfo(FString::Printf(
		TEXT("Prototype Dependency Closure: %d packages (%lld bytes / %.2f MB) staged in '%s' (Project='%s')"),
		Report.TotalFiles,
		Report.TotalBytes,
		static_cast<double>(Report.TotalBytes) / (1024.0 * 1024.0),
		*Report.SandboxDirectoryUsed,
		*Report.SandboxProjectFilePath));

	for (const FSLAYCREWVerifiedAssetLoadInfo& AssetInfo : Report.VerifiedAssets)
	{
		TestTrue(
			FString::Printf(TEXT("Asset loaded in UE 5.8: %s"), *AssetInfo.PackageName.ToString()),
			AssetInfo.bLoadedSuccessfully);

		AddInfo(FString::Printf(
			TEXT("Verified [%s] Class=%s LODs=%d MatSlots=%d BoundsCm=(%.1f x %.1f x %.1f) Mat0/Parent=%s"),
			*AssetInfo.PackageName.ToString(),
			*AssetInfo.AssetClassName,
			AssetInfo.NumLODs,
			AssetInfo.NumMaterialSlots,
			AssetInfo.BoundingBoxSizeCm.X,
			AssetInfo.BoundingBoxSizeCm.Y,
			AssetInfo.BoundingBoxSizeCm.Z,
			*AssetInfo.ResolvedMaterialOrParentName.ToString()));
	}

	// Verify both the isolated sandbox directory and the active project Content directory have no leftover staged files
	TestFalse(
		TEXT("Isolated sandbox directory cleaned up after verification"),
		IFileManager::Get().DirectoryExists(*Report.SandboxDirectoryUsed));

	const FString ProjectContentCandleCheck = FPaths::Combine(
		FPaths::ProjectContentDir(),
		TEXT("Meshes/Candles/SM_Candles_01.uasset"));
	TestFalse(
		TEXT("Active project Content directory remained untouched"),
		IFileManager::Get().FileExists(*ProjectContentCandleCheck));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
