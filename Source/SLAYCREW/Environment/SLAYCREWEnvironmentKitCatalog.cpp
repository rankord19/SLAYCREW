// Copyright Epic Games, Inc. All Rights Reserved.

#include "Environment/SLAYCREWEnvironmentKitCatalog.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SLAYCREW.h"
#include "UObject/Package.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	FSLAYCREWModularSnapSpec MakeSnapSpec(
		float PosStepCm,
		float VertStepCm,
		float RotStepDeg,
		ESLAYCREWModularPivotConvention Pivot)
	{
		FSLAYCREWModularSnapSpec Spec;
		Spec.PositionGridStepCm = PosStepCm;
		Spec.VerticalGridStepCm = VertStepCm;
		Spec.RotationGridStepDegrees = RotStepDeg;
		Spec.ScaleGridStep = 0.25f;
		Spec.PivotConvention = Pivot;
		return Spec;
	}

	bool ConvertGamePackageToFilename(const FString& BaseContentDir, const FName& PackageName, FString& OutFilename)
	{
		OutFilename.Reset();
		if (BaseContentDir.IsEmpty() || PackageName.IsNone())
		{
			return false;
		}

		const FString PackageStr = PackageName.ToString();
		const FString GamePrefix = TEXT("/Game/");
		if (!PackageStr.StartsWith(GamePrefix, ESearchCase::IgnoreCase) || PackageStr.Contains(TEXT("..")))
		{
			return false;
		}

		FText Reason;
		if (!FPackageName::IsValidLongPackageName(PackageStr, /*bIncludeReadOnlyRoots=*/false, &Reason))
		{
			return false;
		}

		const FString RelativePath = PackageStr.RightChop(GamePrefix.Len()) + TEXT(".uasset");
		if (RelativePath.IsEmpty() || RelativePath == TEXT(".uasset"))
		{
			return false;
		}

		FString NormalizedBase = FPaths::ConvertRelativePathToFull(BaseContentDir);
		FPaths::NormalizeDirectoryName(NormalizedBase);

		OutFilename = FPaths::ConvertRelativePathToFull(FPaths::Combine(NormalizedBase, RelativePath));
		FPaths::NormalizeFilename(OutFilename);

		if (!FPaths::ValidatePath(OutFilename) || !FPaths::IsUnderDirectory(OutFilename, NormalizedBase))
		{
			OutFilename.Reset();
			return false;
		}

		return true;
	}

	bool HasValidUnrealPackageHeader(const FString& Filename)
	{
		TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*Filename));
		if (!FileReader || FileReader->TotalSize() < static_cast<int64>(sizeof(uint32)))
		{
			return false;
		}

		uint32 Tag = 0;
		*FileReader << Tag;
		return (!FileReader->IsError()) && (Tag == PACKAGE_FILE_TAG || Tag == PACKAGE_FILE_TAG_SWAPPED);
	}

	double SnapAxisScalePreservingMirror(double AxisScale, double ScaleStep)
	{
		const double Sign = (AxisScale < 0.0) ? -1.0 : 1.0;
		const double SnappedMagnitude = FMath::Max(ScaleStep, FMath::GridSnap(FMath::Abs(AxisScale), ScaleStep));
		return Sign * SnappedMagnitude;
	}

	bool ParseApproxSizeTag(const FString& ApproxSizeStr, FVector& OutSizeCm)
	{
		TArray<FString> Parts;
		ApproxSizeStr.ParseIntoArray(Parts, TEXT("x"), /*InCullEmpty=*/true);
		if (Parts.Num() != 3)
		{
			return false;
		}

		OutSizeCm.X = FCString::Atod(*Parts[0]);
		OutSizeCm.Y = FCString::Atod(*Parts[1]);
		OutSizeCm.Z = FCString::Atod(*Parts[2]);
		return (OutSizeCm.X > 0.0 || OutSizeCm.Y > 0.0 || OutSizeCm.Z > 0.0);
	}
}

USLAYCREWEnvironmentKitCatalog::USLAYCREWEnvironmentKitCatalog()
{
	PopulateDefaultGothicAcademyCatalog();
}

void USLAYCREWEnvironmentKitCatalog::PopulateDefaultGothicAcademyCatalog()
{
	Entries.Reset();
	MissingGothicArchitecturePieces.Reset();

	const FSLAYCREWModularSnapSpec StructuralBaySnap = MakeSnapSpec(400.0f, 400.0f, 90.0f, ESLAYCREWModularPivotConvention::BottomCorner);
	const FSLAYCREWModularSnapSpec WallModuleSnap    = MakeSnapSpec(100.0f, 100.0f, 90.0f, ESLAYCREWModularPivotConvention::BottomCorner);
	const FSLAYCREWModularSnapSpec FloorSlabSnap     = MakeSnapSpec(100.0f, 25.0f, 90.0f, ESLAYCREWModularPivotConvention::BottomCenter);
	const FSLAYCREWModularSnapSpec StairModuleSnap   = MakeSnapSpec(50.0f, 25.0f, 90.0f, ESLAYCREWModularPivotConvention::BottomCenter);
	const FSLAYCREWModularSnapSpec PropSnap          = MakeSnapSpec(10.0f, 5.0f, 15.0f, ESLAYCREWModularPivotConvention::BottomCenter);
	const FSLAYCREWModularSnapSpec DecalSnap         = MakeSnapSpec(10.0f, 10.0f, 15.0f, ESLAYCREWModularPivotConvention::Center);

	auto AddEntry = [this](
		const TCHAR* Id,
		const TCHAR* PackagePath,
		ESLAYCREWEnvironmentAssetCategory Category,
		ESLAYCREWGothicSuitability Suitability,
		float FolderMB,
		float TransitiveMB,
		const TCHAR* MasterMat,
		const TCHAR* UseCase,
		const FSLAYCREWModularSnapSpec& Snap)
	{
		FSLAYCREWEnvironmentAssetEntry Entry;
		Entry.AssetId = FName(Id);
		Entry.OriginalPackagePath = FName(PackagePath);
		Entry.Category = Category;
		Entry.Suitability = Suitability;
		Entry.ApproxFolderSizeMB = FolderMB;
		Entry.ApproxTransitiveSizeMB = TransitiveMB;
		Entry.RequiredMasterMaterial = FName(MasterMat);
		Entry.ProposedAcademyUseCase = UseCase;
		Entry.SnapSpec = Snap;
		Entries.Add(MoveTemp(Entry));
	};

	// --- Shared Master Materials ---
	AddEntry(
		TEXT("M_StandardMaster"),
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		ESLAYCREWEnvironmentAssetCategory::MasterMaterial,
		ESLAYCREWGothicSuitability::DirectReuse,
		0.04f, 0.40f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Core opaque PBR master material for Gothic props, ironwork, wood furniture, and stone details without RVT dependency."),
		PropSnap);

	AddEntry(
		TEXT("M_BlendMaster"),
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		ESLAYCREWEnvironmentAssetCategory::MasterMaterial,
		ESLAYCREWGothicSuitability::DirectReuse,
		0.04f, 9.10f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Masonry and stone assembly master material with optional height-mask and RVT ground blending."),
		WallModuleSnap);

	AddEntry(
		TEXT("M_SSSMaster"),
		TEXT("/Game/Materials/Masters/M_SSSMaster"),
		ESLAYCREWEnvironmentAssetCategory::MasterMaterial,
		ESLAYCREWGothicSuitability::DirectReuse,
		0.03f, 0.36f,
		TEXT("/Game/Materials/Masters/M_SSSMaster"),
		TEXT("Subsurface scattering master material (with SSP_Candles profile) for ceremonial wax candles."),
		PropSnap);

	AddEntry(
		TEXT("M_DecalMaster"),
		TEXT("/Game/Materials/Masters/M_DecalMaster"),
		ESLAYCREWEnvironmentAssetCategory::MasterMaterial,
		ESLAYCREWGothicSuitability::DirectReuse,
		0.14f, 0.38f,
		TEXT("/Game/Materials/Masters/M_DecalMaster"),
		TEXT("Deferred decal master material for stone water leakage, soot, ash, and combat bloodstains."),
		DecalSnap);

	// --- Architecture: Stone & Masonry (Direct Reuse for Gothic Academy) ---
	AddEntry(
		TEXT("JapaneseShrineStoneFloorA"),
		TEXT("/Game/Megascans/3D_Assets/JapaneseShrineStoneFloorA/SM_JapaneseShrineStoneFloorA"),
		ESLAYCREWEnvironmentAssetCategory::ArchitectureStone,
		ESLAYCREWGothicSuitability::DirectReuse,
		20.19f, 29.16f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Dressed ashlar stone paving slab for Gothic academy cloister walkways, ceremonial dais, and courtyard paving."),
		FloorSlabSnap);

	AddEntry(
		TEXT("JapaneseShrineStoneFloorB"),
		TEXT("/Game/Megascans/3D_Assets/JapaneseShrineStoneFloorB/SM_JapaneseShrineStoneFloorB"),
		ESLAYCREWEnvironmentAssetCategory::ArchitectureStone,
		ESLAYCREWGothicSuitability::DirectReuse,
		21.82f, 30.79f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Complementary dressed ashlar stone floor module for grand hall and courtyard borders."),
		FloorSlabSnap);

	AddEntry(
		TEXT("JapaneseShrineStoneFloorC"),
		TEXT("/Game/Megascans/3D_Assets/JapaneseShrineStoneFloorC/SM_JapaneseShrineStoneFloorC_00"),
		ESLAYCREWEnvironmentAssetCategory::ArchitectureStone,
		ESLAYCREWGothicSuitability::DirectReuse,
		23.14f, 32.11f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Stepped stone floor slab variation for courtyard terraces and sanctum thresholds."),
		FloorSlabSnap);

	AddEntry(
		TEXT("DamagedCastleStairs"),
		TEXT("/Game/Megascans/3D_Assets/DamagedCastleStairs/SM_DamagedCastleStairs"),
		ESLAYCREWEnvironmentAssetCategory::ArchitectureStone,
		ESLAYCREWGothicSuitability::DirectReuse,
		21.38f, 30.35f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Heavy carved stone staircase run for courtyard approaches, podium steps, and lower crypt transitions."),
		StairModuleSnap);

	AddEntry(
		TEXT("CastleWall"),
		TEXT("/Game/Megascans/3D_Assets/CastleWall/SM_CastleWall"),
		ESLAYCREWEnvironmentAssetCategory::ArchitectureStone,
		ESLAYCREWGothicSuitability::DirectReuse,
		68.85f, 77.82f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Fortified stone masonry curtain wall section for academy perimeter battlements and retaining walls."),
		StructuralBaySnap);

	AddEntry(
		TEXT("StoneWall"),
		TEXT("/Game/Megascans/3D_Assets/StoneWall/SM_StoneWall"),
		ESLAYCREWEnvironmentAssetCategory::ArchitectureStone,
		ESLAYCREWGothicSuitability::DirectReuse,
		78.33f, 87.30f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Low dressed/coursed stone wall for courtyard parterres, practice arena boundaries, and garden terraces."),
		WallModuleSnap);

	AddEntry(
		TEXT("MossyStoneWallB"),
		TEXT("/Game/Megascans/3D_Assets/MossyStoneWallB/SM_MossyStoneWallB"),
		ESLAYCREWEnvironmentAssetCategory::ArchitectureStone,
		ESLAYCREWGothicSuitability::DirectReuse,
		24.86f, 33.83f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Lightweight weathered stone wall module for older outer academy ruins and cliffside balustrades."),
		WallModuleSnap);

	// --- Architecture: Timber & Support (Conditional Secondary Reuse) ---
	AddEntry(
		TEXT("WoodenBeam"),
		TEXT("/Game/Megascans/3D_Assets/WoodenBeam/SM_WoodenBeam04"),
		ESLAYCREWEnvironmentAssetCategory::ArchitectureTimberSupport,
		ESLAYCREWGothicSuitability::ConditionalSecondaryReuse,
		218.33f, 218.73f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Heavy dark timber beams for high hammerbeam roof trusses, library galleries, or lower cellars (not exterior facades)."),
		WallModuleSnap);

	AddEntry(
		TEXT("WornWoodenBeamA"),
		TEXT("/Game/Megascans/3D_Assets/WornWoodenBeamA/SM_WornWoodenBeamA_00"),
		ESLAYCREWEnvironmentAssetCategory::ArchitectureTimberSupport,
		ESLAYCREWGothicSuitability::ConditionalSecondaryReuse,
		69.46f, 69.86f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Secondary structural timber beam for interior scaffolding, belfry framing, or armory racks."),
		WallModuleSnap);

	// --- Surface Materials ---
	AddEntry(
		TEXT("Cobblestone"),
		TEXT("/Game/Megascans/Surfaces/Cobblestone/MI_Cobblestone"),
		ESLAYCREWEnvironmentAssetCategory::SurfaceMaterial,
		ESLAYCREWGothicSuitability::DirectReuse,
		296.39f, 305.36f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Tiled cobblestone surface and displacement mesh for main academy quadrangle and carriageways."),
		FloorSlabSnap);

	// --- Decals & Weathering ---
	AddEntry(
		TEXT("WallRustLeakage"),
		TEXT("/Game/Megascans/Atlases/WallRustLeakage/MI_WallRustLeakage"),
		ESLAYCREWEnvironmentAssetCategory::DecalWeathering,
		ESLAYCREWGothicSuitability::DirectReuse,
		1.11f, 1.49f,
		TEXT("/Game/Materials/Masters/M_DecalMaster"),
		TEXT("Iron-oxide and water stain decal beneath wrought-iron sconces, window grilles, and gargoyle spouts."),
		DecalSnap);

	AddEntry(
		TEXT("LeakageA"),
		TEXT("/Game/Megascans/Atlases/LeakageA/MI_LeakageA"),
		ESLAYCREWEnvironmentAssetCategory::DecalWeathering,
		ESLAYCREWGothicSuitability::DirectReuse,
		0.80f, 1.18f,
		TEXT("/Game/Materials/Masters/M_DecalMaster"),
		TEXT("Damp stone seepage decal for breaking up repetitive ashlar wall modules and cornice undersides."),
		DecalSnap);

	AddEntry(
		TEXT("Ash"),
		TEXT("/Game/Megascans/Atlases/Ash/MI_Ash"),
		ESLAYCREWEnvironmentAssetCategory::DecalWeathering,
		ESLAYCREWGothicSuitability::DirectReuse,
		31.29f, 31.67f,
		TEXT("/Game/Materials/Masters/M_DecalMaster"),
		TEXT("Hearth and brazier ash decal for ceremonial fire pits and armory forges."),
		DecalSnap);

	// --- Lighting & Ceremonial Props (Direct Reuse) ---
	AddEntry(
		TEXT("Candles"),
		TEXT("/Game/Meshes/Candles/SM_Candles_01"),
		ESLAYCREWEnvironmentAssetCategory::LightingAndCeremonialProp,
		ESLAYCREWGothicSuitability::DirectReuse,
		37.30f, 12.99f,
		TEXT("/Game/Materials/Masters/M_SSSMaster"),
		TEXT("Clustered wax candles with subsurface scattering for graduation hall lecterns, altars, chandeliers, and study tables."),
		PropSnap);

	AddEntry(
		TEXT("Lantern"),
		TEXT("/Game/Meshes/Lantern/SM_ManMade_Lantern_01"),
		ESLAYCREWEnvironmentAssetCategory::LightingAndCeremonialProp,
		ESLAYCREWGothicSuitability::DirectReuse,
		77.77f, 78.17f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Wrought-iron lantern for cloister arches, stair landings, and archway brackets."),
		PropSnap);

	AddEntry(
		TEXT("Chain"),
		TEXT("/Game/Meshes/Chain/SM_Chain_01"),
		ESLAYCREWEnvironmentAssetCategory::LightingAndCeremonialProp,
		ESLAYCREWGothicSuitability::DirectReuse,
		43.46f, 43.64f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Heavy iron chains for suspending Gothic chandeliers, portcullis mechanisms, and vaulted hall braziers."),
		PropSnap);

	AddEntry(
		TEXT("Flag"),
		TEXT("/Game/Meshes/Flag/SK_FlagA"),
		ESLAYCREWEnvironmentAssetCategory::LightingAndCeremonialProp,
		ESLAYCREWGothicSuitability::DirectReuse,
		96.86f, 97.26f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Cloth-simulated hanging banner base for graduation ceremony heraldry in the Great Hall (re-texture heraldry)."),
		PropSnap);

	AddEntry(
		TEXT("GraveA"),
		TEXT("/Game/Megascans/3D_Assets/GraveA/SM_GraveA_00"),
		ESLAYCREWEnvironmentAssetCategory::LightingAndCeremonialProp,
		ESLAYCREWGothicSuitability::DirectReuse,
		84.32f, 93.29f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Carved stone memorial slab for fallen monster slayers in the academy memorial cloister or crypt."),
		PropSnap);

	// --- Academy Furniture & Combat Training Props ---
	AddEntry(
		TEXT("OldWoodenBench"),
		TEXT("/Game/Megascans/3D_Assets/OldWoodenBench/SM_OldWoodenBench"),
		ESLAYCREWEnvironmentAssetCategory::AcademyFurnitureAndProp,
		ESLAYCREWGothicSuitability::DirectReuse,
		19.77f, 20.17f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Refectory and lecture hall seating bench; can be arrayed for graduation audience seating."),
		PropSnap);

	AddEntry(
		TEXT("OldWoodenTable"),
		TEXT("/Game/Megascans/3D_Assets/OldWoodenTable/SM_OldWoodenTable"),
		ESLAYCREWEnvironmentAssetCategory::AcademyFurnitureAndProp,
		ESLAYCREWGothicSuitability::DirectReuse,
		19.31f, 19.71f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Heavy timber table for tactical monster-study halls, refectory, and library reading rooms."),
		PropSnap);

	AddEntry(
		TEXT("OldWoodenDoor"),
		TEXT("/Game/Megascans/3D_Assets/OldWoodenDoor/SM_OldWoodenDoor"),
		ESLAYCREWEnvironmentAssetCategory::AcademyFurnitureAndProp,
		ESLAYCREWGothicSuitability::DirectReuse,
		77.05f, 77.45f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Iron-bound heavy timber door leaf for dormitory, study, and armory portals."),
		PropSnap);

	AddEntry(
		TEXT("OldWoodenChest"),
		TEXT("/Game/Megascans/3D_Assets/OldWoodenChest/SM_OldWoodenChest"),
		ESLAYCREWEnvironmentAssetCategory::AcademyFurnitureAndProp,
		ESLAYCREWGothicSuitability::DirectReuse,
		19.60f, 20.00f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Iron-reinforced footlocker/reliquary chest for graduate quarters and armory storage."),
		PropSnap);

	AddEntry(
		TEXT("Sword"),
		TEXT("/Game/Megascans/3D_Assets/Sword/SM_Sword"),
		ESLAYCREWEnvironmentAssetCategory::AcademyFurnitureAndProp,
		ESLAYCREWGothicSuitability::DirectReuse,
		67.42f, 67.82f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("Armory weapon rack prop and ceremonial graduation blade display."),
		PropSnap);

	// --- Courtyard Foliage & Rock ---
	AddEntry(
		TEXT("EnglishIvy"),
		TEXT("/Game/Megascans/3D_Plants/EnglishIvy/F_EnglishIvy_00"),
		ESLAYCREWEnvironmentAssetCategory::CourtyardFoliageAndRock,
		ESLAYCREWGothicSuitability::DirectReuse,
		21.83f, 30.90f,
		TEXT("/Game/Materials/Masters/M_FoliageCustomWind"),
		TEXT("Climbing ivy cards for Gothic stone buttresses, courtyard cloister walls, and ancient towers."),
		PropSnap);

	AddEntry(
		TEXT("CreeperIvy"),
		TEXT("/Game/Megascans/3D_Plants/CreeperIvy/F_CreeperIvy_00"),
		ESLAYCREWEnvironmentAssetCategory::CourtyardFoliageAndRock,
		ESLAYCREWGothicSuitability::DirectReuse,
		23.39f, 32.46f,
		TEXT("/Game/Materials/Masters/M_FoliageCustomWind"),
		TEXT("Wall-climbing creeper foliage for exterior masonry dressing."),
		PropSnap);

	AddEntry(
		TEXT("GraniteRockAssembly"),
		TEXT("/Game/Megascans/3D_Assets/GraniteRockAssembly/SM_GraniteRockAssembly"),
		ESLAYCREWEnvironmentAssetCategory::CourtyardFoliageAndRock,
		ESLAYCREWGothicSuitability::ConditionalSecondaryReuse,
		24.69f, 33.66f,
		TEXT("/Game/Materials/Masters/M_BlendMaster"),
		TEXT("Cliffside bedrock foundation anchoring the academy promontory."),
		PropSnap);

	// --- Atmosphere & VFX ---
	AddEntry(
		TEXT("EffectsFlames"),
		TEXT("/Game/Effects/Flames/NS_Fire"),
		ESLAYCREWEnvironmentAssetCategory::AtmosphereVFX,
		ESLAYCREWGothicSuitability::DirectReuse,
		2.57f, 2.57f,
		TEXT("/Game/Effects/Flames/M_CandleFlame"),
		TEXT("Candle flame and torch fire Niagara systems for sconces, chandeliers, and ceremonial braziers."),
		PropSnap);

	AddEntry(
		TEXT("EffectsIncenseSmoke"),
		TEXT("/Game/Effects/IncenseSmoke/P_Incense_Loop"),
		ESLAYCREWEnvironmentAssetCategory::AtmosphereVFX,
		ESLAYCREWGothicSuitability::DirectReuse,
		4.02f, 4.02f,
		TEXT("/Game/Effects/IncenseSmoke/Material/M_2DRaymarched_Loop"),
		TEXT("Atmospheric incense and censer smoke for the graduation ceremony hall."),
		PropSnap);

	// --- Excluded Rural Medieval Village Assets (Recorded to prevent accidental import) ---
	AddEntry(
		TEXT("Houses_ThatchRoofs"),
		TEXT("/Game/Meshes/Houses/Thatch_Cards/ThatchDense/SM_ThatchD_Dense_01"),
		ESLAYCREWEnvironmentAssetCategory::RuralVillageExcluded,
		ESLAYCREWGothicSuitability::ExcludeRuralVillage,
		202.29f, 210.0f,
		TEXT("/Game/Materials/Masters/M_StrawMaster"),
		TEXT("EXCLUDED: Peasant thatched straw roofing conflicts with prestigious Gothic stone/slate architecture."),
		WallModuleSnap);

	AddEntry(
		TEXT("MedievalModularWall_Wattle"),
		TEXT("/Game/Megascans/3D_Assets/MedievalModularWall2x2MD/SM_MedievalModularWall2x2MD"),
		ESLAYCREWEnvironmentAssetCategory::RuralVillageExcluded,
		ESLAYCREWGothicSuitability::ExcludeRuralVillage,
		617.0f, 626.0f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("EXCLUDED: Crude rural timber/wattle cottage wall kit; reads as peasant village rather than Gothic academy."),
		WallModuleSnap);

	AddEntry(
		TEXT("Scarecrow"),
		TEXT("/Game/Meshes/Scarecrow/MI_ScarecrowBody"),
		ESLAYCREWEnvironmentAssetCategory::RuralVillageExcluded,
		ESLAYCREWGothicSuitability::ExcludeRuralVillage,
		361.76f, 362.16f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("EXCLUDED: Agrarian scarecrow prop (361.76 MB); inappropriate for Gothic academy."),
		PropSnap);

	AddEntry(
		TEXT("WindmillWings"),
		TEXT("/Game/Meshes/WindmillWings/SM_WindmillWings"),
		ESLAYCREWEnvironmentAssetCategory::RuralVillageExcluded,
		ESLAYCREWGothicSuitability::ExcludeRuralVillage,
		113.25f, 113.65f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("EXCLUDED: Rural grain windmill blades (113.25 MB)."),
		PropSnap);

	AddEntry(
		TEXT("FarmProduceAndManure"),
		TEXT("/Game/Megascans/3D_Assets/HorseManure/SM_HorseManure"),
		ESLAYCREWEnvironmentAssetCategory::RuralVillageExcluded,
		ESLAYCREWGothicSuitability::ExcludeRuralVillage,
		195.92f, 196.32f,
		TEXT("/Game/Materials/Masters/M_StandardMaster"),
		TEXT("EXCLUDED: HorseManure, MedievalButterChurn, OldScythe, Carrot, PotatoA/B — rural farm clutter."),
		PropSnap);

	// --- Missing Gothic Architecture Pieces (Not Present in MedievalVillageMegascansS) ---
	MissingGothicArchitecturePieces = {
		TEXT("Pointed Lancet & Equilateral Arches (400 cm & 800 cm bays)"),
		TEXT("Ribbed & Quadripartite Stone Vault Modules (400x400 cm & 800x800 cm ceiling bays)"),
		TEXT("Clustered Pier Columns, Engaged Pilasters, Responds & Foliate Capitals"),
		TEXT("Pointed Tracery Window Frames, Rose Windows & Stained Glass Emissive Panels"),
		TEXT("Flying Buttresses, Stepped Exterior Buttress Piers & Pinnacles/Finials"),
		TEXT("Tall Octagonal/Square Gothic Bell & Observatory Towers with Spire Roofs"),
		TEXT("Monumental Ceremonial Stone Staircases (Straight, Split-Landing & Spiral Turret Stairs) with Carved Stone Balustrades"),
		TEXT("Cloister Arcade Bays with Triforium Gallery Balconies & Gargoyle/Grotesque Cornices"),
		TEXT("Ceremonial Graduation Podium, Chancel Dais, Carved Lecterns, High-Backed Choir Stalls & Wrought-Iron Chandeliers"),
		TEXT("Dressed Ashlar Interior Wall Modules (400x400 cm, 200x400 cm) with Dado Wainscoting & Stringcourses")
	};
}

TArray<FSLAYCREWEnvironmentAssetEntry> USLAYCREWEnvironmentKitCatalog::GetEntriesByCategory(
	ESLAYCREWEnvironmentAssetCategory Category) const
{
	TArray<FSLAYCREWEnvironmentAssetEntry> Result;
	for (const FSLAYCREWEnvironmentAssetEntry& Entry : Entries)
	{
		if (Entry.Category == Category)
		{
			Result.Add(Entry);
		}
	}
	return Result;
}

TArray<FSLAYCREWEnvironmentAssetEntry> USLAYCREWEnvironmentKitCatalog::GetEntriesBySuitability(
	ESLAYCREWGothicSuitability Suitability) const
{
	TArray<FSLAYCREWEnvironmentAssetEntry> Result;
	for (const FSLAYCREWEnvironmentAssetEntry& Entry : Entries)
	{
		if (Entry.Suitability == Suitability)
		{
			Result.Add(Entry);
		}
	}
	return Result;
}

bool USLAYCREWEnvironmentKitCatalog::FindEntryById(FName AssetId, FSLAYCREWEnvironmentAssetEntry& OutEntry) const
{
	for (const FSLAYCREWEnvironmentAssetEntry& Entry : Entries)
	{
		if (Entry.AssetId == AssetId)
		{
			OutEntry = Entry;
			return true;
		}
	}
	return false;
}

FTransform USLAYCREWEnvironmentKitCatalog::SnapTransformToModularGrid(
	const FTransform& InTransform,
	const FSLAYCREWModularSnapSpec& SnapSpec)
{
	const float PosStep = FMath::Max(SnapSpec.PositionGridStepCm, 1.0f);
	const float VertStep = FMath::Max(SnapSpec.VerticalGridStepCm, 1.0f);
	const float RotStep = FMath::Max(SnapSpec.RotationGridStepDegrees, 1.0f);
	const double ScaleStep = static_cast<double>(FMath::Max(SnapSpec.ScaleGridStep, 0.01f));

	const FVector Loc = InTransform.GetLocation();
	const FVector SnappedLoc(
		FMath::GridSnap(Loc.X, PosStep),
		FMath::GridSnap(Loc.Y, PosStep),
		FMath::GridSnap(Loc.Z, VertStep));

	const FRotator Rot = InTransform.Rotator();
	const FRotator SnappedRot(
		FMath::GridSnap(Rot.Pitch, RotStep),
		FMath::GridSnap(Rot.Yaw, RotStep),
		FMath::GridSnap(Rot.Roll, RotStep));

	const FVector Scale = InTransform.GetScale3D();
	const FVector SnappedScale(
		SnapAxisScalePreservingMirror(Scale.X, ScaleStep),
		SnapAxisScalePreservingMirror(Scale.Y, ScaleStep),
		SnapAxisScalePreservingMirror(Scale.Z, ScaleStep));

	return FTransform(SnappedRot, SnappedLoc, SnappedScale);
}

bool USLAYCREWEnvironmentKitCatalog::HasValidModularScale(
	const FTransform& InTransform,
	const FSLAYCREWModularSnapSpec& SnapSpec)
{
	if (!InTransform.IsValid())
	{
		return false;
	}

	const double MinMagnitude = static_cast<double>(FMath::Max(SnapSpec.ScaleGridStep, 0.01f)) * 0.5;
	const FVector Scale = InTransform.GetScale3D();
	return FMath::Abs(Scale.X) >= MinMagnitude
		&& FMath::Abs(Scale.Y) >= MinMagnitude
		&& FMath::Abs(Scale.Z) >= MinMagnitude;
}

bool USLAYCREWEnvironmentKitCatalog::IsTransformAlignedToGrid(
	const FTransform& InTransform,
	const FSLAYCREWModularSnapSpec& SnapSpec,
	float Tolerance)
{
	if (!HasValidModularScale(InTransform, SnapSpec))
	{
		return false;
	}

	const FTransform Snapped = SnapTransformToModularGrid(InTransform, SnapSpec);
	if (!InTransform.GetLocation().Equals(Snapped.GetLocation(), Tolerance))
	{
		return false;
	}
	if (!InTransform.GetScale3D().Equals(Snapped.GetScale3D(), Tolerance))
	{
		return false;
	}
	const FRotator DeltaRot = (InTransform.Rotator() - Snapped.Rotator()).GetNormalized();
	return FMath::Abs(DeltaRot.Pitch) <= Tolerance
		&& FMath::Abs(DeltaRot.Yaw) <= Tolerance
		&& FMath::Abs(DeltaRot.Roll) <= Tolerance;
}

bool USLAYCREWEnvironmentKitCatalog::ResolveSourcePackageDependencies(
	const FString& SourceContentDir,
	const TArray<FName>& RootPackageNames,
	FSLAYCREWPackageDependencyReport& OutReport)
{
	OutReport = FSLAYCREWPackageDependencyReport();
	OutReport.RootPackages = RootPackageNames;

	if (SourceContentDir.IsEmpty() || !IFileManager::Get().DirectoryExists(*SourceContentDir))
	{
		OutReport.FailureReason = FString::Printf(
			TEXT("Source Content directory does not exist: '%s'"),
			*SourceContentDir);
		return false;
	}

	if (RootPackageNames.IsEmpty())
	{
		OutReport.FailureReason = TEXT("RootPackageNames list is empty.");
		return false;
	}

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		OutReport.FailureReason = TEXT("IAssetRegistry is unavailable.");
		return false;
	}

	TSet<FName> VisitedPackages;
	TQueue<FName> PackageQueue;

	for (const FName& RootPkg : RootPackageNames)
	{
		if (RootPkg.IsNone())
		{
			OutReport.UnsupportedDependencies.AddUnique(FName(TEXT("<None>")));
			continue;
		}

		const FString RootStr = RootPkg.ToString();
		FText ValidateReason;
		if (!RootStr.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase) ||
			RootStr.Contains(TEXT("..")) ||
			!FPackageName::IsValidLongPackageName(RootStr, /*bIncludeReadOnlyRoots=*/false, &ValidateReason))
		{
			OutReport.UnsupportedDependencies.AddUnique(RootPkg);
			continue;
		}

		if (!VisitedPackages.Contains(RootPkg))
		{
			VisitedPackages.Add(RootPkg);
			PackageQueue.Enqueue(RootPkg);
		}
	}

	FName CurrentPkg;
	while (PackageQueue.Dequeue(CurrentPkg))
	{
		FString SourceFilename;
		if (!ConvertGamePackageToFilename(SourceContentDir, CurrentPkg, SourceFilename))
		{
			OutReport.UnsupportedDependencies.AddUnique(CurrentPkg);
			continue;
		}

		const int64 FileSize = IFileManager::Get().FileSize(*SourceFilename);
		if (FileSize < 0)
		{
			OutReport.MissingPackages.AddUnique(CurrentPkg);
			continue;
		}

		if (!HasValidUnrealPackageHeader(SourceFilename))
		{
			OutReport.UnreadablePackages.AddUnique(CurrentPkg);
			continue;
		}

		IAssetRegistry::FLoadPackageRegistryData LoadData(/*bInGetDependencies=*/true);
		AssetRegistry->LoadPackageRegistryData(SourceFilename, LoadData);
		if (LoadData.Data.IsEmpty())
		{
			OutReport.UnreadablePackages.AddUnique(CurrentPkg);
			continue;
		}

		OutReport.ResolvedDependencyPackages.Add(CurrentPkg);
		OutReport.TotalFiles++;
		OutReport.TotalBytes += FileSize;

		for (const FName& DepPkg : LoadData.DataDependencies)
		{
			if (DepPkg.IsNone())
			{
				continue;
			}

			const FString DepStr = DepPkg.ToString();
			if (DepStr.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase))
			{
				FText ValidateReason;
				if (DepStr.Contains(TEXT("..")) ||
					!FPackageName::IsValidLongPackageName(DepStr, /*bIncludeReadOnlyRoots=*/false, &ValidateReason))
				{
					OutReport.UnsupportedDependencies.AddUnique(DepPkg);
					continue;
				}

				if (!VisitedPackages.Contains(DepPkg))
				{
					VisitedPackages.Add(DepPkg);
					PackageQueue.Enqueue(DepPkg);
				}
			}
			else if (DepStr.StartsWith(TEXT("/Engine/"), ESearchCase::IgnoreCase))
			{
				OutReport.EngineAndScriptDependencies.AddUnique(DepPkg);
				if (!FPackageName::DoesPackageExist(DepStr))
				{
					OutReport.MissingPackages.AddUnique(DepPkg);
				}
			}
			else if (DepStr.StartsWith(TEXT("/Script/"), ESearchCase::IgnoreCase))
			{
				OutReport.EngineAndScriptDependencies.AddUnique(DepPkg);
				if (FindObject<UPackage>(nullptr, *DepStr) == nullptr)
				{
					OutReport.UnsupportedDependencies.AddUnique(DepPkg);
				}
			}
			else
			{
				const FString MountPointRoot = FPackageName::GetPackageMountPoint(DepStr).ToString();
				if (MountPointRoot.IsEmpty() ||
					!FPackageName::MountPointExists(MountPointRoot) ||
					!FPackageName::DoesPackageExist(DepStr))
				{
					OutReport.UnsupportedDependencies.AddUnique(DepPkg);
				}
				else
				{
					OutReport.EngineAndScriptDependencies.AddUnique(DepPkg);
				}
			}
		}
	}

	OutReport.ResolvedDependencyPackages.Sort(FNameLexicalLess());
	OutReport.EngineAndScriptDependencies.Sort(FNameLexicalLess());
	OutReport.MissingPackages.Sort(FNameLexicalLess());
	OutReport.UnreadablePackages.Sort(FNameLexicalLess());
	OutReport.UnsupportedDependencies.Sort(FNameLexicalLess());

	OutReport.bCompleteDependencyCoverageVerified =
		(RootPackageNames.Num() > 0) &&
		(OutReport.TotalFiles >= RootPackageNames.Num()) &&
		OutReport.MissingPackages.IsEmpty() &&
		OutReport.UnreadablePackages.IsEmpty() &&
		OutReport.UnsupportedDependencies.IsEmpty();

	if (!OutReport.bCompleteDependencyCoverageVerified)
	{
		OutReport.FailureReason = FString::Printf(
			TEXT("Dependency verification incomplete (Resolved=%d, Missing=%d, Unreadable=%d, Unsupported=%d)."),
			OutReport.TotalFiles,
			OutReport.MissingPackages.Num(),
			OutReport.UnreadablePackages.Num(),
			OutReport.UnsupportedDependencies.Num());
		UE_LOG(LogSLAYCREW, Warning, TEXT("[GothicEnvironmentKit] %s"), *OutReport.FailureReason);
		return false;
	}

	return true;
}

FString USLAYCREWEnvironmentKitCatalog::BuildSeparateProcessVerificationCommandLine(const FString& SandboxProjectFilePath)
{
	const FString NormalizedUProject = FPaths::ConvertRelativePathToFull(SandboxProjectFilePath);
	return FString::Printf(
		TEXT("\"%s\" -run=ResavePackages -VerifyOnly -ProjectOnly -Unattended -NOPAUSE -NullRHI -NoSplash -NoSound"),
		*NormalizedUProject);
}

bool USLAYCREWEnvironmentKitCatalog::CleanUpAbandonedStagingSandboxes()
{
	const FString SandboxBaseDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Temp/GothicKitSandbox")));
	if (!IFileManager::Get().DirectoryExists(*SandboxBaseDir))
	{
		return true;
	}

	return IFileManager::Get().DeleteDirectory(*SandboxBaseDir, /*RequireExists=*/false, /*Tree=*/true);
}

bool USLAYCREWEnvironmentKitCatalog::VerifyPrototypeMigrationInScopedSandbox(
	const FString& SourceContentDir,
	const TArray<FName>& RootPackageNames,
	FSLAYCREWPackageDependencyReport& OutReport,
	bool bCleanUpStagedFilesOnExit,
	const FString& OptionalCustomSandboxContentDir)
{
	if (!ResolveSourcePackageDependencies(SourceContentDir, RootPackageNames, OutReport))
	{
		return false;
	}

	const IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		OutReport.FailureReason = TEXT("IAssetRegistry is unavailable.");
		return false;
	}

	FString NormalizedSourceDir = FPaths::ConvertRelativePathToFull(SourceContentDir);
	FPaths::NormalizeDirectoryName(NormalizedSourceDir);

	FString ActiveProjectContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	FPaths::NormalizeDirectoryName(ActiveProjectContentDir);

	const FString RunId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString DefaultSandboxRunRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Temp/GothicKitSandbox"), RunId));

	FString SandboxContentDir = OptionalCustomSandboxContentDir.IsEmpty()
		? FPaths::Combine(DefaultSandboxRunRoot, TEXT("Content"))
		: FPaths::ConvertRelativePathToFull(OptionalCustomSandboxContentDir);
	FPaths::NormalizeDirectoryName(SandboxContentDir);

	const FString SandboxProjectRoot = FPaths::GetPath(SandboxContentDir);
	const FString SandboxUProjectFile = FPaths::Combine(SandboxProjectRoot, TEXT("GothicKitSandbox.uproject"));

	OutReport.SandboxDirectoryUsed = SandboxContentDir;
	OutReport.SandboxProjectFilePath = SandboxUProjectFile;

	if (!FPaths::ValidatePath(SandboxContentDir) ||
		FPaths::IsSamePath(NormalizedSourceDir, SandboxContentDir) ||
		FPaths::IsUnderDirectory(SandboxContentDir, NormalizedSourceDir) ||
		FPaths::IsUnderDirectory(NormalizedSourceDir, SandboxContentDir) ||
		FPaths::IsSamePath(ActiveProjectContentDir, SandboxContentDir) ||
		FPaths::IsUnderDirectory(SandboxContentDir, ActiveProjectContentDir))
	{
		OutReport.FailureReason = FString::Printf(
			TEXT("Unsafe or overlapping sandbox directory '%s' relative to source '%s' or active project Content '%s'."),
			*SandboxContentDir,
			*NormalizedSourceDir,
			*ActiveProjectContentDir);
		return false;
	}

	// Pre-flight collision detection: never overwrite existing assets in ActiveProjectContentDir or SandboxContentDir.
	for (const FName& PkgName : OutReport.ResolvedDependencyPackages)
	{
		FString ActiveGameFile;
		if (ConvertGamePackageToFilename(ActiveProjectContentDir, PkgName, ActiveGameFile) &&
			IFileManager::Get().FileExists(*ActiveGameFile))
		{
			OutReport.CollidingPackages.AddUnique(PkgName);
		}

		FString SandboxFile;
		if (ConvertGamePackageToFilename(SandboxContentDir, PkgName, SandboxFile) &&
			IFileManager::Get().FileExists(*SandboxFile))
		{
			OutReport.CollidingPackages.AddUnique(PkgName);
		}
	}

	if (!OutReport.CollidingPackages.IsEmpty())
	{
		OutReport.CollidingPackages.Sort(FNameLexicalLess());
		OutReport.FailureReason = FString::Printf(
			TEXT("Aborted before staging: %d package path collision(s) detected (first: '%s'). Existing assets are never overwritten."),
			OutReport.CollidingPackages.Num(),
			*OutReport.CollidingPackages[0].ToString());
		UE_LOG(LogSLAYCREW, Warning, TEXT("[GothicEnvironmentKit] %s"), *OutReport.FailureReason);
		return false;
	}

	TArray<FString> NewlyStagedFiles;
	TArray<FString> NewlyCreatedDirs;
	bool bCreatedSandboxUProject = false;

	// Note: If the process terminates abnormally (e.g. OS kill or hardware power loss), files staged in
	// Saved/Temp/GothicKitSandbox outside ProjectContentDir can be cleaned up on next startup via
	// USLAYCREWEnvironmentKitCatalog::CleanUpAbandonedStagingSandboxes().
	ON_SCOPE_EXIT
	{
		if (bCleanUpStagedFilesOnExit)
		{
			for (const FString& StagedFile : NewlyStagedFiles)
			{
				IFileManager::Get().Delete(*StagedFile, /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
			}

			if (bCreatedSandboxUProject)
			{
				IFileManager::Get().Delete(*SandboxUProjectFile, /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
			}

			NewlyCreatedDirs.Sort([](const FString& A, const FString& B) { return A.Len() > B.Len(); });
			for (const FString& Dir : NewlyCreatedDirs)
			{
				TArray<FString> Remaining;
				IFileManager::Get().FindFilesRecursive(Remaining, *Dir, TEXT("*"), true, false);
				if (Remaining.IsEmpty())
				{
					IFileManager::Get().DeleteDirectory(*Dir, /*RequireExists=*/false, /*Tree=*/true);
				}
			}

			if (OptionalCustomSandboxContentDir.IsEmpty() && IFileManager::Get().DirectoryExists(*DefaultSandboxRunRoot))
			{
				IFileManager::Get().DeleteDirectory(*DefaultSandboxRunRoot, /*RequireExists=*/false, /*Tree=*/true);
			}
		}
	};

	// 1. Create standalone temporary Unreal project descriptor (GothicKitSandbox.uproject) and Content directory
	if (!IFileManager::Get().MakeDirectory(*SandboxContentDir, /*Tree=*/true) ||
		!IFileManager::Get().DirectoryExists(*SandboxContentDir))
	{
		OutReport.FailureReason = FString::Printf(
			TEXT("Failed to create sandbox Content directory '%s'."),
			*SandboxContentDir);
		return false;
	}
	NewlyCreatedDirs.AddUnique(SandboxContentDir);

	if (!IFileManager::Get().FileExists(*SandboxUProjectFile))
	{
		const FString MinimalUProjectJson =
			TEXT("{\n\t\"FileVersion\": 3,\n\t\"EngineAssociation\": \"5.8\",\n\t\"Category\": \"\",\n\t\"Description\": \"Isolated Gothic Environment Kit Verification Sandbox\"\n}\n");
		if (!FFileHelper::SaveStringToFile(MinimalUProjectJson, *SandboxUProjectFile))
		{
			OutReport.FailureReason = FString::Printf(
				TEXT("Failed to write temporary sandbox project descriptor '%s'."),
				*SandboxUProjectFile);
			return false;
		}
		bCreatedSandboxUProject = true;
	}

	// 2. Stage all dependency-resolved .uasset files into the temporary project's Content directory
	for (const FName& PkgName : OutReport.ResolvedDependencyPackages)
	{
		FString SrcFile;
		FString DstFile;
		if (!ConvertGamePackageToFilename(NormalizedSourceDir, PkgName, SrcFile) ||
			!ConvertGamePackageToFilename(SandboxContentDir, PkgName, DstFile))
		{
			OutReport.FailureReason = FString::Printf(
				TEXT("Invalid source or destination path for package '%s'."),
				*PkgName.ToString());
			return false;
		}

		const int64 ExpectedFileSize = IFileManager::Get().FileSize(*SrcFile);
		if (ExpectedFileSize <= 0)
		{
			OutReport.FailureReason = FString::Printf(
				TEXT("Source file missing or empty prior to copy: '%s'."),
				*SrcFile);
			return false;
		}

		if (IFileManager::Get().FileExists(*DstFile))
		{
			OutReport.CollidingPackages.AddUnique(PkgName);
			OutReport.FailureReason = FString::Printf(
				TEXT("Destination file already exists, refusing to overwrite: '%s'."),
				*DstFile);
			return false;
		}

		const FString DstDir = FPaths::GetPath(DstFile);
		FString CheckDir = DstDir;
		while (CheckDir.Len() > SandboxContentDir.Len() && !IFileManager::Get().DirectoryExists(*CheckDir))
		{
			NewlyCreatedDirs.AddUnique(CheckDir);
			CheckDir = FPaths::GetPath(CheckDir);
		}

		if (!IFileManager::Get().MakeDirectory(*DstDir, /*Tree=*/true) ||
			!IFileManager::Get().DirectoryExists(*DstDir))
		{
			OutReport.FailureReason = FString::Printf(
				TEXT("Failed to create destination directory '%s' for package '%s'."),
				*DstDir,
				*PkgName.ToString());
			return false;
		}

		const uint32 CopyResult = IFileManager::Get().Copy(*DstFile, *SrcFile, /*Replace=*/false, /*EvenIfReadOnly=*/true);
		if (CopyResult != COPY_OK)
		{
			OutReport.FailureReason = FString::Printf(
				TEXT("File copy failed (code %u) from '%s' to '%s'."),
				CopyResult,
				*SrcFile,
				*DstFile);
			return false;
		}

		NewlyStagedFiles.Add(DstFile);
		if (IFileManager::Get().FileSize(*DstFile) != ExpectedFileSize)
		{
			OutReport.FailureReason = FString::Printf(
				TEXT("Copied file size mismatch for '%s'."),
				*DstFile);
			return false;
		}
	}

	// 3. Verify each staged root package in the temporary project's Content directory using stateless,
	// const IAssetRegistry::LoadPackageRegistryData and binary header checks.
	// IMPORTANT: Never call FPackageName::RegisterMountPoint("/Game/", ...), ScanFilesSynchronous, or LoadPackage
	// inside the active editor process so active editor package resolution, loaded assets, and AssetRegistry remain untouched.
	bool bAllRootsVerified = true;
	for (const FName& RootPkg : RootPackageNames)
	{
		FSLAYCREWVerifiedAssetLoadInfo Info;
		Info.PackageName = RootPkg;

		FString StagedRootFile;
		if (!ConvertGamePackageToFilename(SandboxContentDir, RootPkg, StagedRootFile) ||
			!HasValidUnrealPackageHeader(StagedRootFile))
		{
			Info.bLoadedSuccessfully = false;
			bAllRootsVerified = false;
			OutReport.VerifiedAssets.Add(MoveTemp(Info));
			continue;
		}

		IAssetRegistry::FLoadPackageRegistryData StagedRootData(/*bInGetDependencies=*/true);
		AssetRegistry->LoadPackageRegistryData(StagedRootFile, StagedRootData);
		if (StagedRootData.Data.IsEmpty())
		{
			Info.bLoadedSuccessfully = false;
			bAllRootsVerified = false;
			OutReport.VerifiedAssets.Add(MoveTemp(Info));
			continue;
		}

		// Verify all direct /Game/... dependencies of this staged root package exist in SandboxContentDir
		bool bAllStagedDepsPresent = true;
		FName DiscoveredMaterialDependency = NAME_None;
		for (const FName& DepPkg : StagedRootData.DataDependencies)
		{
			const FString DepStr = DepPkg.ToString();
			if (DepStr.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase))
			{
				FString StagedDepFile;
				if (!ConvertGamePackageToFilename(SandboxContentDir, DepPkg, StagedDepFile) ||
					!IFileManager::Get().FileExists(*StagedDepFile) ||
					!HasValidUnrealPackageHeader(StagedDepFile))
				{
					bAllStagedDepsPresent = false;
					break;
				}

				if (DiscoveredMaterialDependency.IsNone() &&
					(DepStr.Contains(TEXT("/MI_")) || DepStr.Contains(TEXT("/M_")) || DepStr.StartsWith(TEXT("/Game/Materials/"))))
				{
					DiscoveredMaterialDependency = DepPkg;
				}
			}
		}

		if (!bAllStagedDepsPresent)
		{
			Info.bLoadedSuccessfully = false;
			bAllRootsVerified = false;
			OutReport.VerifiedAssets.Add(MoveTemp(Info));
			continue;
		}

		const FAssetData& PrimaryAssetData = StagedRootData.Data[0];
		Info.AssetClassName = PrimaryAssetData.AssetClassPath.GetAssetName().ToString();
		Info.ResolvedMaterialOrParentName = DiscoveredMaterialDependency;

		if (Info.AssetClassName == TEXT("StaticMesh"))
		{
			PrimaryAssetData.GetTagValue(FName(TEXT("LODs")), Info.NumLODs);
			PrimaryAssetData.GetTagValue(FName(TEXT("Materials")), Info.NumMaterialSlots);

			FString ApproxSizeStr;
			if (PrimaryAssetData.GetTagValue(FName(TEXT("ApproxSize")), ApproxSizeStr))
			{
				ParseApproxSizeTag(ApproxSizeStr, Info.BoundingBoxSizeCm);
			}

			Info.bLoadedSuccessfully =
				(Info.NumLODs > 0) &&
				(Info.NumMaterialSlots > 0) &&
				!Info.BoundingBoxSizeCm.IsNearlyZero() &&
				!Info.ResolvedMaterialOrParentName.IsNone();
		}
		else if (Info.AssetClassName.Contains(TEXT("Material")))
		{
			Info.NumMaterialSlots = 1;
			FString ParentTagValue;
			if (PrimaryAssetData.GetTagValue(FName(TEXT("Parent")), ParentTagValue) && !ParentTagValue.IsEmpty())
			{
				Info.ResolvedMaterialOrParentName = FName(*ParentTagValue);
			}

			Info.bLoadedSuccessfully = !Info.ResolvedMaterialOrParentName.IsNone();
		}
		else
		{
			Info.bLoadedSuccessfully = !Info.AssetClassName.IsEmpty();
		}

		if (!Info.bLoadedSuccessfully)
		{
			bAllRootsVerified = false;
		}

		OutReport.VerifiedAssets.Add(MoveTemp(Info));
	}

	if (!bAllRootsVerified && OutReport.FailureReason.IsEmpty())
	{
		OutReport.FailureReason = TEXT("One or more staged root packages failed binary header or AssetRegistry tag verification.");
	}

	return bAllRootsVerified;
}
