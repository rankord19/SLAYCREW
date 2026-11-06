// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Environment/SLAYCREWEnvironmentKitCatalog.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSLAYCREWEnvironmentKitCatalogTest,
	"SLAYCREW.Environment.GothicKit.CatalogAndGridSnapping",
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

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSLAYCREWEnvironmentKitPrototypeMigrationTest,
	"SLAYCREW.Environment.GothicKit.PrototypeMigrationAndLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSLAYCREWEnvironmentKitPrototypeMigrationTest::RunTest(const FString& Parameters)
{
	const FString SourceContentDir = TEXT("D:/Unreal Projects/MedievalVillageMegascansS/Content");
	if (!IFileManager::Get().DirectoryExists(*SourceContentDir))
	{
		AddInfo(FString::Printf(
			TEXT("Source sample Content directory not present at '%s'; skipping local prototype migration verification."),
			*SourceContentDir));
		return true;
	}

	const TArray<FName> RepresentativeRootPackages = {
		FName(TEXT("/Game/Meshes/Candles/SM_Candles_01")),
		FName(TEXT("/Game/Megascans/Atlases/WallRustLeakage/MI_WallRustLeakage")),
		FName(TEXT("/Game/Megascans/3D_Assets/JapaneseShrineStoneFloorA/SM_JapaneseShrineStoneFloorA")),
		FName(TEXT("/Game/Megascans/3D_Assets/DamagedCastleStairs/SM_DamagedCastleStairs"))
	};

	FSLAYCREWPackageDependencyReport Report;
	const bool bVerified = USLAYCREWEnvironmentKitCatalog::VerifyPrototypeMigrationInScopedSandbox(
		SourceContentDir,
		RepresentativeRootPackages,
		Report,
		/*bCleanUpStagedFilesOnExit=*/true);

	TestTrue(TEXT("All representative Gothic kit packages resolved and loaded in UE 5.8"), bVerified);
	TestEqual(TEXT("Zero missing package dependencies"), Report.MissingPackages.Num(), 0);
	TestTrue(TEXT("Resolved transitive package closure is non-empty"), Report.TotalFiles >= 30);
	TestEqual(TEXT("Verified all 4 root assets"), Report.VerifiedAssets.Num(), RepresentativeRootPackages.Num());

	AddInfo(FString::Printf(
		TEXT("Prototype Dependency Closure: %d packages (%lld bytes / %.2f MB)"),
		Report.TotalFiles,
		Report.TotalBytes,
		static_cast<double>(Report.TotalBytes) / (1024.0 * 1024.0)));

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

	// Verify staged binary files were cleaned up so the Git worktree stays clean
	const FString StagedCandleCheck = FPaths::Combine(
		FPaths::ProjectContentDir(),
		TEXT("Meshes/Candles/SM_Candles_01.uasset"));
	TestFalse(
		TEXT("Temporary staged .uasset files cleaned up after verification"),
		IFileManager::Get().FileExists(*StagedCandleCheck));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
