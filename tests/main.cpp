#include "AlphaMaskTest.h"
#include "AnimationControllerTest.h"
#include "ClickAccountingTest.h"
#include "DragClassifierTest.h"
#include "FrameSchedulerTest.h"
#include "PetPackLoaderTest.h"
#include "SilhouetteTest.h"
#include "TestRunner.h"

int main() {
    nimvlets::testing::TestRunner runner;

    nimvlets::tests::RegisterDragClassifierTests(runner);
    nimvlets::tests::RegisterFrameSchedulerTests(runner);
    nimvlets::tests::RegisterSilhouetteTests(runner);
    nimvlets::tests::RegisterAlphaMaskTests(runner);
    nimvlets::tests::RegisterAnimationControllerTests(runner);
    nimvlets::tests::RegisterPetPackLoaderTests(runner);
    nimvlets::tests::RegisterClickAccountingTests(runner);

    return runner.RunAll();
}
