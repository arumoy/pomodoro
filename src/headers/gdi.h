// gdi.h - Small RAII wrappers for GDI handles and shared enums.
#ifndef TOMATO_GDI_H
#define TOMATO_GDI_H

#include <windows.h>

// Timer phase.
enum class Phase { Work, Rest };

// ---------------------------------------------------------------------------
// GdiObject<T> - owns a GDI handle (HFONT, HBRUSH, ...) and deletes it in the
// destructor. Move-only, so ownership is unambiguous. Converts implicitly to
// the handle type for use with Win32 APIs.
// ---------------------------------------------------------------------------
template <typename T>
class GdiObject {
public:
    GdiObject() = default;
    explicit GdiObject(T handle) : handle_(handle) {}

    ~GdiObject() { reset(); }

    // Move-only.
    GdiObject(const GdiObject&) = delete;
    GdiObject& operator=(const GdiObject&) = delete;

    GdiObject(GdiObject&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    GdiObject& operator=(GdiObject&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    // Replace the owned handle (deleting any previous one).
    void reset(T handle = nullptr) {
        if (handle_) {
            DeleteObject(handle_);
        }
        handle_ = handle;
    }

    T get() const { return handle_; }
    operator T() const { return handle_; }        // implicit -> Win32 call sites
    explicit operator bool() const { return handle_ != nullptr; }

private:
    T handle_ = nullptr;
};

using GdiFont  = GdiObject<HFONT>;
using GdiBrush = GdiObject<HBRUSH>;

#endif // TOMATO_GDI_H
