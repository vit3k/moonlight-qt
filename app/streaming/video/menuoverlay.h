#pragma once

#include "SDL_compat.h"
#include <SDL_ttf.h>
#include <functional>
#include <string>
#include <vector>

struct MenuOverlayItem {
    std::string label;
    std::function<void()> action;
};

class MenuOverlay {
public:
    MenuOverlay();
    ~MenuOverlay();

    bool isVisible() const { return m_Visible; }
    void setVisible(bool visible);

    void setItems(std::vector<MenuOverlayItem> items);

    // Navigation — call from the controller button handler
    void navigateUp();
    void navigateDown();
    void confirm();    // A button
    void cancel();     // B button

    // Renders menu to an SDL_Surface and pushes it to OverlayManager.
    // Must be called after setItems() and whenever selection changes.
    void repaint();

private:
    void freeFont();

    bool m_Visible;
    int m_SelectedIndex;
    std::vector<MenuOverlayItem> m_Items;
    TTF_Font* m_Font;

    static constexpr int k_ItemHeight    = 56;
    static constexpr int k_ItemPadding   = 16;
    static constexpr int k_PanelWidth    = 340;
    static constexpr int k_FontSize      = 24;
};
