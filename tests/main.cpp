#include "ActivePetResolutionTest.h"
#include "AlphaMaskTest.h"
#include "AnimationControllerTest.h"
#include "AppStateSerializerTest.h"
#include "AppStateStoreTest.h"
#include "ClickAccountingTest.h"
#include "ClickThroughPolicyTest.h"
#include "CollectionLayoutTest.h"
#include "CollectionModelTest.h"
#include "PetEntitlementTest.h"
#include "PurchasePolicyTest.h"
#include "ShopLayoutTest.h"
#include "ShopModelTest.h"
#include "DirectionTest.h"
#include "DisplayControlsTest.h"
#include "FocusListTest.h"
#include "TextLayoutTest.h"
#include "FormatTest.h"
#include "LocalizationTest.h"
#include "PetAccentTest.h"
#include "PetEditorialTest.h"
#include "PreviewArtifactTest.h"
#include "QuickMenuModelTest.h"
#include "DragClassifierTest.h"
#include "FrameSchedulerTest.h"
#include "HoverDwellTrackerTest.h"
#include "LinuxBackendPolicyTest.h"
#include "RendererPolicyTest.h"
#include "PersistenceIntegrationTest.h"
#include "PersistenceSchedulerTest.h"
#include "PetCatalogLoaderTest.h"
#include "PetIdentityTest.h"
#include "PetPackLoaderTest.h"
#include "PetSwitchingTest.h"
#include "SilhouetteTest.h"
#include "StatefulBehaviorTest.h"
#include "TestRunner.h"

int main() {
    nimvlets::testing::TestRunner runner;

    nimvlets::tests::RegisterDragClassifierTests(runner);
    nimvlets::tests::RegisterFrameSchedulerTests(runner);
    nimvlets::tests::RegisterHoverDwellTrackerTests(runner);
    nimvlets::tests::RegisterSilhouetteTests(runner);
    nimvlets::tests::RegisterAlphaMaskTests(runner);
    nimvlets::tests::RegisterAnimationControllerTests(runner);
    nimvlets::tests::RegisterPetPackLoaderTests(runner);
    nimvlets::tests::RegisterClickAccountingTests(runner);
    nimvlets::tests::RegisterAppStateSerializerTests(runner);
    nimvlets::tests::RegisterAppStateStoreTests(runner);
    nimvlets::tests::RegisterPersistenceSchedulerTests(runner);
    nimvlets::tests::RegisterPersistenceIntegrationTests(runner);
    nimvlets::tests::RegisterPetIdentityTests(runner);
    nimvlets::tests::RegisterPetCatalogLoaderTests(runner);
    nimvlets::tests::RegisterActivePetResolutionTests(runner);
    nimvlets::tests::RegisterPetEntitlementTests(runner);
    nimvlets::tests::RegisterCollectionModelTests(runner);
    nimvlets::tests::RegisterCollectionLayoutTests(runner);
    nimvlets::tests::RegisterShopModelTests(runner);
    nimvlets::tests::RegisterPurchasePolicyTests(runner);
    nimvlets::tests::RegisterShopLayoutTests(runner);
    nimvlets::tests::RegisterFocusListTests(runner);
    nimvlets::tests::RegisterTextLayoutTests(runner);
    nimvlets::tests::RegisterFormatTests(runner);
    nimvlets::tests::RegisterQuickMenuModelTests(runner);
    nimvlets::tests::RegisterPetSwitchingTests(runner);
    nimvlets::tests::RegisterLinuxBackendPolicyTests(runner);
    nimvlets::tests::RegisterRendererPolicyTests(runner);
    nimvlets::tests::RegisterClickThroughPolicyTests(runner);
    nimvlets::tests::RegisterDirectionTests(runner);
    nimvlets::tests::RegisterDisplayControlsTests(runner);
    nimvlets::tests::RegisterLocalizationTests(runner);
    nimvlets::tests::RegisterPetAccentTests(runner);
    nimvlets::tests::RegisterPetEditorialTests(runner);
    nimvlets::tests::RegisterPreviewArtifactTests(runner);
    nimvlets::tests::RegisterStatefulBehaviorTests(runner);

    return runner.RunAll();
}
