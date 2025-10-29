# Архитектура плагина Asset Deduplication

```
┌─────────────────────────────────────────────────────────────────┐
│                    FDeduplicatePluginModule                     │
│                         (Main Module)                          │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ├─── UDeduplicationManager
                      │    ├─── UEqualNameDeduplication
                      │    ├─── UEqualUassetDataDeduplication
                      │    └─── [Custom Algorithms...]
                      │
                      ├─── UDuplicateResolutionManager
                      │    ├─── Asset Reference Updates
                      │    ├─── Asset Deletion
                      │    └─── Backup Management
                      │
                      └─── SDeduplicationWidget
                           ├─── Algorithm Configuration Panel
                           ├─── Results List
                           └─── Content Browser Integration

┌─────────────────────────────────────────────────────────────────┐
│                    Data Flow                                    │
└─────────────────────────────────────────────────────────────────┘

1. User Input → SDeduplicationWidget
2. Widget → UDeduplicationManager.AnalyzeAssets()
3. Manager → Each Algorithm.FindDuplicates()
4. Algorithms → Return FDuplicateGroup[]
5. Manager → Merge Groups → FDuplicateCluster[]
6. Widget → Display Results
7. User Action → UDuplicateResolutionManager.ResolveDuplicates()
8. Resolution Manager → Update References + Delete Assets

┌─────────────────────────────────────────────────────────────────┐
│                    Key Data Structures                          │
└─────────────────────────────────────────────────────────────────┘

FDuplicateGroup:
├── TArray<FAssetData> DuplicateAssets
├── float ConfidenceScore
└── FString AlgorithmName

FDuplicateCluster:
├── TArray<FAssetDuplicateInfo> DuplicateAssets
├── float ClusterScore
├── FString PrimaryAlgorithm
└── bool bRequiresUserReview

FAssetDuplicateInfo:
├── FAssetData Asset
├── float FinalScore
├── TArray<FString> ContributingAlgorithms
└── TArray<FAssetData> RelatedDuplicates

FResolutionResult:
├── bool bSuccess
├── FString ErrorMessage
├── TArray<FAssetData> KeptAssets
├── TArray<FAssetData> DeletedAssets
└── int32 ReferencesUpdated

┌─────────────────────────────────────────────────────────────────┐
│                    Algorithm Interface                           │
└─────────────────────────────────────────────────────────────────┘

UDeduplicateObject (Base Class):
├── GetAlgorithmName() → FString
├── GetAlgorithmDescription() → FString
├── FindDuplicates() → TArray<FDuplicateGroup>
├── IsEnabled() → bool
├── SetEnabled(bool)
├── GetWeight() → float
└── SetWeight(float)

┌─────────────────────────────────────────────────────────────────┐
│                    UI Components                                 │
└─────────────────────────────────────────────────────────────────┘

SDeduplicationWidget:
├── Toolbar (Analyze Button, Confidence Slider)
├── Algorithm Config Panel (Enable/Disable, Weights)
├── Results List (Cluster Items)
└── Content Browser Integration

Cluster Item:
├── Cluster Info (Count, Score)
├── Primary Algorithm
├── Review Status
└── Resolve Button

┌─────────────────────────────────────────────────────────────────┐
│                    Integration Points                            │
└─────────────────────────────────────────────────────────────────┘

Unreal Engine Integration:
├── AssetRegistry (Asset Discovery)
├── AssetTools (Asset Operations)
├── ContentBrowser (UI Integration)
├── ToolMenus (Menu Integration)
└── EditorStyle (UI Styling)

External Dependencies:
├── HAL/PlatformFilemanager (File Operations)
├── Misc/FileHelper (File I/O)
├── Framework/Notifications (User Notifications)
└── Widgets (UI Components)
```
