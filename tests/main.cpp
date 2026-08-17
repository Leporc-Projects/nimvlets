#include "AlphaMaskTest.h"
#include "DragClassifierTest.h"
#include "FrameSchedulerTest.h"
#include "SilhouetteTest.h"
#include "TestRunner.h"

int main() {
    nimvlets::testing::TestRunner runner;

    nimvlets::tests::RegisterDragClassifierTests(runner);
    nimvlets::tests::RegisterFrameSchedulerTests(runner);
    nimvlets::tests::RegisterSilhouetteTests(runner);
    nimvlets::tests::RegisterAlphaMaskTests(runner);

    return runner.RunAll();
}
