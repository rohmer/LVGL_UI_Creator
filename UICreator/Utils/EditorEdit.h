#pragma once
#include <cstddef>
namespace EditorEdit {
struct Observer {
    virtual ~Observer() = default;
    virtual void BeginEdit(const void* key, bool coalesce) = 0;
    virtual void EndEdit() noexcept = 0;
};
inline thread_local Observer* observer = nullptr;
class Scope {
    Observer* owner = nullptr;
public:
    explicit Scope(const void* key = nullptr, bool coalesce = false, bool enabled = true) {
        if(enabled && observer) {owner=observer;owner->BeginEdit(key,coalesce);}
    }
    ~Scope() {if(owner && observer==owner)owner->EndEdit();}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
