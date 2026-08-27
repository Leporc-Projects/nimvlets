#include "productui/FocusList.h"

#include <algorithm>
#include <utility>

namespace nimvlets::productui {

namespace {
const std::string kEmpty;
}

void FocusList::SetItems(std::vector<std::string> ids) {
    const std::string previous = ids_.empty() ? std::string() : ids_[index_];
    ids_ = std::move(ids);
    if (ids_.empty()) {
        index_ = 0;
        return;
    }
    const auto it = std::find(ids_.begin(), ids_.end(), previous);
    index_ = (it != ids_.end()) ? static_cast<std::size_t>(it - ids_.begin()) : 0;
}

const std::string& FocusList::FocusedId() const {
    if (ids_.empty()) {
        return kEmpty;
    }
    return ids_[index_];
}

bool FocusList::Focus(const std::string& id) {
    const auto it = std::find(ids_.begin(), ids_.end(), id);
    if (it == ids_.end()) {
        return false;
    }
    index_ = static_cast<std::size_t>(it - ids_.begin());
    return true;
}

const std::string& FocusList::Next() {
    if (ids_.empty()) {
        return kEmpty;
    }
    index_ = (index_ + 1) % ids_.size();
    return ids_[index_];
}

const std::string& FocusList::Prev() {
    if (ids_.empty()) {
        return kEmpty;
    }
    index_ = (index_ + ids_.size() - 1) % ids_.size();
    return ids_[index_];
}

void FocusList::Clear() {
    ids_.clear();
    index_ = 0;
}

}  // namespace nimvlets::productui
