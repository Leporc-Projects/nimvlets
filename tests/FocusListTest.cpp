#include "FocusListTest.h"

#include "productui/FocusList.h"

#include <string>
#include <vector>

using nimvlets::productui::FocusList;

namespace nimvlets::tests {

namespace {

bool TestEmptyListHasNoFocus() {
    FocusList f;
    NIMVLETS_CHECK(f.Empty());
    NIMVLETS_CHECK(f.FocusedId().empty());
    NIMVLETS_CHECK(f.Next().empty());  // no-op sin crashear
    NIMVLETS_CHECK(f.Prev().empty());
    return true;
}

bool TestNextPrevAreCyclic() {
    FocusList f;
    f.SetItems({"a", "b", "c"});
    NIMVLETS_CHECK(f.FocusedId() == "a");
    NIMVLETS_CHECK(f.Next() == "b");
    NIMVLETS_CHECK(f.Next() == "c");
    NIMVLETS_CHECK(f.Next() == "a");  // vuelve al principio
    NIMVLETS_CHECK(f.Prev() == "c");  // y hacia atrás también
    NIMVLETS_CHECK(f.Prev() == "b");
    return true;
}

bool TestFocusByIdIgnoresUnknown() {
    FocusList f;
    f.SetItems({"a", "b", "c"});
    NIMVLETS_CHECK(f.Focus("c"));
    NIMVLETS_CHECK(f.FocusedId() == "c");
    NIMVLETS_CHECK(!f.Focus("zzz"));      // ausente -> false
    NIMVLETS_CHECK(f.FocusedId() == "c");  // y no cambia nada
    return true;
}

// Al cambiar la lista (se abre/cierra el detalle), el foco se queda
// sobre el MISMO id si sigue existiendo.
bool TestSetItemsKeepsFocusOnSameId() {
    FocusList f;
    f.SetItems({"item:bunny", "item:nidir", "item:frin"});
    f.Focus("item:frin");
    // Se abre el detalle de Frin -> aparecen widgets nuevos, pero
    // "item:frin" sigue ahí.
    f.SetItems({"item:bunny", "item:nidir", "item:frin", "variant:male", "variant:female", "use"});
    NIMVLETS_CHECK(f.FocusedId() == "item:frin");
    return true;
}

// Si el id enfocado desaparece, cae al primero (no a un índice fuera de
// rango).
bool TestSetItemsFallsBackWhenFocusedIdGone() {
    FocusList f;
    f.SetItems({"item:bunny", "item:frin", "use"});
    f.Focus("use");
    f.SetItems({"item:bunny", "item:frin"});  // se cerró el detalle
    NIMVLETS_CHECK(f.FocusedId() == "item:bunny");
    return true;
}

bool TestClearResets() {
    FocusList f;
    f.SetItems({"a", "b"});
    f.Next();
    f.Clear();
    NIMVLETS_CHECK(f.Empty());
    NIMVLETS_CHECK(f.FocusedId().empty());
    return true;
}

}  // namespace

void RegisterFocusListTests(testing::TestRunner& runner) {
    runner.Add("FocusList/EmptyListHasNoFocus", TestEmptyListHasNoFocus);
    runner.Add("FocusList/NextPrevAreCyclic", TestNextPrevAreCyclic);
    runner.Add("FocusList/FocusByIdIgnoresUnknown", TestFocusByIdIgnoresUnknown);
    runner.Add("FocusList/SetItemsKeepsFocusOnSameId", TestSetItemsKeepsFocusOnSameId);
    runner.Add("FocusList/SetItemsFallsBackWhenFocusedIdGone", TestSetItemsFallsBackWhenFocusedIdGone);
    runner.Add("FocusList/ClearResets", TestClearResets);
}

}  // namespace nimvlets::tests
