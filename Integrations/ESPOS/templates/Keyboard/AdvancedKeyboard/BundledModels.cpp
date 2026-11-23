#include "espos_keyboard/BundledModels.h"
#include "lv_advanced_keyboard_prediction.h"
#include <cstring>
#include <memory>
#include <new>

namespace espos::keyboard::bundled {
namespace detail {
const Model* GeneratedModels() noexcept;
std::size_t GeneratedCount() noexcept;
}
namespace {
struct PredictorDeleter {
    void operator()(lv_advanced_keyboard_predictor_t* predictor) const {
        lv_advanced_keyboard_predictor_destroy(predictor);
    }
};
using Predictor = std::unique_ptr<lv_advanced_keyboard_predictor_t, PredictorDeleter>;
struct Attachment {
    lv_obj_t* root = nullptr;
    Attachment* next = nullptr;
    std::uint64_t instance = 0;
    bool deleting = false;
    const Model* model = nullptr;
    Predictor predictor;
};
Attachment* attachments = nullptr;
std::uint64_t nextInstance = 0;
void Unlink(Attachment* attachment) {
    for (auto** item = &attachments; *item; item = &(*item)->next)
        if (*item == attachment) { *item = attachment->next; break; }
    attachment->root = nullptr;
}
void Deleting(lv_event_t* event) {
    // Mark unavailable before the core's normal DELETE handler frees its data.
    // Keep predictor bytes/workspace alive until that core cleanup has finished.
    static_cast<Attachment*>(lv_event_get_user_data(event))->deleting = true;
}
void Deleted(lv_event_t* event) {
    auto* attachment = static_cast<Attachment*>(lv_event_get_user_data(event));
    Unlink(attachment);
    delete attachment;
}
Attachment* Attached(lv_obj_t* root, bool includeDeleting = false) {
    for (auto* item = attachments; item; item = item->next)
        if (item->root == root && (includeDeleting || !item->deleting)) return item;
    return nullptr;
}
Attachment* Alive(lv_obj_t* root, std::uint64_t instance) {
    auto* result = Attached(root);
    return result && result->instance == instance ? result : nullptr;
}
bool Keyboard(lv_obj_t* root) {
    const auto* owner = Attached(root, true);
    return root && !(owner && owner->deleting) && lv_obj_is_in_widget_tree(root) && lv_advanced_keyboard_is_instance(root);
}
bool Supported(const char* language) {
    if (!language) return false;
    for (std::size_t index = 0; index < lv_advanced_keyboard_language_count(); ++index)
        if (!std::strcmp(language, lv_advanced_keyboard_language_id(index))) return true;
    return false;
}
Attachment* CreateAttachment(lv_obj_t* root) {
    auto owner = std::unique_ptr<Attachment>(new(std::nothrow) Attachment);
    if (!owner) return nullptr;
    owner->root = root;
    owner->instance = ++nextInstance;
    if (!lv_obj_add_event_cb(root, Deleted, LV_EVENT_DELETE, owner.get())) return nullptr;
    const auto deleting = static_cast<lv_event_code_t>(LV_EVENT_DELETE | LV_EVENT_PREPROCESS);
    if (!lv_obj_add_event_cb(root, Deleting, deleting, owner.get())) {
        lv_obj_remove_event_cb_with_user_data(root, Deleted, owner.get());
        return nullptr;
    }
    owner->next = attachments;
    attachments = owner.get();
    return owner.release();
}
}

std::size_t Count() noexcept { return detail::GeneratedCount(); }
const Model* At(std::size_t index) noexcept {
    return index < Count() ? detail::GeneratedModels() + index : nullptr;
}
const Model* Find(const char* language, std::uint32_t sizeKiB) noexcept {
    if (!language) return nullptr;
    for (std::size_t index = 0; index < Count(); ++index) {
        const auto* model = At(index);
        if (model->sizeKiB == sizeKiB && !std::strcmp(model->language, language)) return model;
    }
    return nullptr;
}
const Model* Selected(lv_obj_t* root) noexcept {
    const auto* owner = Attached(root);
    return owner && Keyboard(root) && owner->predictor &&
        lv_advanced_keyboard_get_predictor(root) == owner->predictor.get() ? owner->model : nullptr;
}
bool Apply(lv_obj_t* root, const char* language, std::uint32_t sizeKiB) {
    const auto* model = Find(language, sizeKiB);
    if (!model || !Keyboard(root) || !Supported(language)) return false;
    auto* owner = Attached(root);
    const bool reuse = owner && owner->model == model && owner->predictor &&
        lv_advanced_keyboard_get_predictor(root) == owner->predictor.get();
    Predictor next;
    if (!reuse) {
        next.reset(lv_advanced_keyboard_predictor_create_borrowed(
            model->trie, model->modelBytes, model->index, model->indexBytes));
        if (!next) return false;
    }
    if (!owner) owner = CreateAttachment(root);
    if (!owner) return false;
    const auto instance = owner->instance;
    if (std::strcmp(lv_advanced_keyboard_get_language(root), language) &&
        !lv_advanced_keyboard_set_language(root, language)) return false;
    if (!(owner = Alive(root, instance))) return false;
    lv_advanced_keyboard_set_suggestion_lookup(root, nullptr, nullptr);
    if (!(owner = Alive(root, instance))) return false;
    if (!reuse) {
        lv_advanced_keyboard_set_predictor(root, next.get());
        if (!(owner = Alive(root, instance))) return false;
        owner->predictor = std::move(next);
        owner->model = model;
    }
    return true;
}
bool Clear(lv_obj_t* root) {
    if (!Keyboard(root)) return false;
    auto* owner = Attached(root);
    const auto instance = owner ? owner->instance : 0;
    lv_advanced_keyboard_set_suggestion_lookup(root, nullptr, nullptr);
    if (instance && !Alive(root, instance)) return false;
    if (!Keyboard(root)) return false;
    lv_advanced_keyboard_set_predictor(root, nullptr);
    if (instance) {
        owner = Alive(root, instance);
        if (!owner) return false;
        owner->model = nullptr;
        owner->predictor.reset();
    }
    return true;
}
}
