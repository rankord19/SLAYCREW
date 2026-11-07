#if WITH_EDITOR
#include "Prototype/SlayPrototype.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"

// Builds only into editor targets. No map or Blueprint assets are written.
static FAutoConsoleCommand SlayPrototypeCommand(
 TEXT("Slay.Prototype"), TEXT("Open a temporary combat arena and start PIE. Save the current map first."),
 FConsoleCommandDelegate::CreateLambda([]()
 {
  if (!GEditor || GEditor->PlayWorld) { return; }
  UWorld* Current = GEditor->GetEditorWorldContext().World();
  if (Current)
  {
   for (ULevel* Level : Current->GetLevels())
   {
    if (Level && Level->GetOutermost()->IsDirty())
    { UE_LOG(LogTemp, Warning, TEXT("Slay.Prototype: save the current level before launching.")); return; }
   }
  }
  UWorld* World = GEditor->NewMap();
  World->GetWorldSettings()->DefaultGameMode = ASlayPrototypeGameMode::StaticClass();
  FRequestPlaySessionParams Params;
  Params.DestinationSlateViewport = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetFirstActiveViewport();
  GEditor->RequestPlaySession(Params);
 }));
#endif
