#include "catalog/PetCatalog.h"

#include <utility>

namespace nimvlets::catalog {

PetCatalog::PetCatalog(std::vector<CatalogEntry> entries) : entries_(std::move(entries)) {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].isDefault) {
            defaultIndex_ = i;
            break;
        }
    }
}

const CatalogEntry* PetCatalog::Find(const PetIdentity& identity) const {
    for (const CatalogEntry& entry : entries_) {
        if (entry.identity == identity) {
            return &entry;
        }
    }
    return nullptr;
}

const CatalogEntry& PetCatalog::Default() const {
    return entries_[defaultIndex_];
}

}  // namespace nimvlets::catalog
