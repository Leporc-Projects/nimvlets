#pragma once

#include <string>
#include <vector>

namespace nimvlets::productui {

// Anillo de foco de teclado para el Product UI: una lista ordenada de
// ids de widget enfocables, con Next()/Prev() cíclicos y un id
// "enfocado" actual. Puro, sin SDL — testeable en aislamiento (block
// brief §23/§27, "basic focus traversal/state logic where testable").
//
// Los ids son strings estables que la capa de layout genera
// ("item:bunny", "variant:male", "use", ...). Cuando la lista cambia
// (se abre/cierra el detalle, cambia la colección), SetItems conserva
// el foco sobre el MISMO id si sigue existiendo; si no, cae al primero.
class FocusList {
 public:
    void SetItems(std::vector<std::string> ids);

    const std::vector<std::string>& Items() const { return ids_; }
    bool Empty() const { return ids_.empty(); }

    // Id enfocado ahora, o "" si la lista está vacía.
    const std::string& FocusedId() const;

    // true si `id` está en la lista y quedó enfocado. Un id ausente no
    // cambia nada y devuelve false.
    bool Focus(const std::string& id);

    // Mueve el foco al siguiente/anterior de forma cíclica. No-op con
    // lista vacía. Devuelven el id recién enfocado.
    const std::string& Next();
    const std::string& Prev();

    void Clear();

 private:
    std::vector<std::string> ids_;
    std::size_t index_ = 0;
};

}  // namespace nimvlets::productui
