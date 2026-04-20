#include "menuoverlay.h"
#include "overlaymanager.h"
#include "path.h"
#include "streaming/session.h"

MenuOverlay::MenuOverlay()
    : m_Visible(false),
      m_SelectedIndex(0),
      m_Font(nullptr)
{
}

MenuOverlay::~MenuOverlay()
{
    freeFont();
}

void MenuOverlay::freeFont()
{
    if (m_Font) {
        TTF_CloseFont(m_Font);
        m_Font = nullptr;
    }

    m_FontData.clear();
}

void MenuOverlay::setVisible(bool visible)
{
    m_Visible = visible;
    if (visible) {
        m_SelectedIndex = 0;
    }
    repaint();
}

void MenuOverlay::setItems(std::vector<MenuOverlayItem> items)
{
    m_Items = std::move(items);
    // Caller is responsible for calling repaint() after setItems() + setVisible(true)
}

void MenuOverlay::navigateUp()
{
    if (m_Items.empty()) return;
    m_SelectedIndex = (m_SelectedIndex - 1 + (int)m_Items.size()) % (int)m_Items.size();
    repaint();
}

void MenuOverlay::navigateDown()
{
    if (m_Items.empty()) return;
    m_SelectedIndex = (m_SelectedIndex + 1) % (int)m_Items.size();
    repaint();
}

void MenuOverlay::confirm()
{
    if (!m_Visible || m_Items.empty()) return;
    m_Items[m_SelectedIndex].action();
}

void MenuOverlay::cancel()
{
    setVisible(false);
}

void MenuOverlay::repaint()
{
    if (!m_Visible || m_Items.empty()) {
        // Clear the overlay
        Session::get()->getOverlayManager().setOverlaySurface(Overlay::OverlayMenu, nullptr);
        return;
    }

    // Lazily open font
    if (!m_Font) {
        m_FontData = Path::readDataFile("ModeSeven.ttf");
        if (m_FontData.isEmpty()) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "MenuOverlay: font file not found");
            return;
        }
        m_Font = TTF_OpenFontRW(
            SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()),
            1, k_FontSize);
        if (!m_Font) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "MenuOverlay: TTF_OpenFont failed: %s", TTF_GetError());
            return;
        }
    }

    int numItems = (int)m_Items.size();
    // Extra row at bottom for hint text
    int hintRowH = k_ItemHeight;
    int panelH   = numItems * k_ItemHeight + k_ItemPadding * 2 + hintRowH;
    int surfW    = k_PanelWidth;
    int surfH    = panelH;

    // Create an ARGB8888 surface (required by all renderers' notifyOverlayUpdated)
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, surfW, surfH, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "MenuOverlay: SDL_CreateRGBSurfaceWithFormat failed: %s", SDL_GetError());
        return;
    }

    // Create a software renderer backed by this surface
    SDL_Renderer* ren = SDL_CreateSoftwareRenderer(surf);
    if (!ren) {
        SDL_FreeSurface(surf);
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "MenuOverlay: SDL_CreateSoftwareRenderer failed: %s", SDL_GetError());
        return;
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    // Panel background (semi-transparent black)
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
    SDL_RenderClear(ren);

    // Panel border
    SDL_Rect border = {0, 0, surfW, panelH};
    SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
    SDL_RenderDrawRect(ren, &border);

    SDL_Color colorNormal   = {220, 220, 220, 255};
    SDL_Color colorSelected = {255, 255, 100, 255};

    for (int i = 0; i < numItems; i++) {
        SDL_Rect itemRect = {
            k_ItemPadding,
            k_ItemPadding + i * k_ItemHeight,
            surfW - k_ItemPadding * 2,
            k_ItemHeight - 4
        };

        if (i == m_SelectedIndex) {
            // Selection highlight
            SDL_SetRenderDrawColor(ren, 60, 60, 180, 200);
            SDL_RenderFillRect(ren, &itemRect);
            SDL_SetRenderDrawColor(ren, 120, 120, 255, 255);
            SDL_RenderDrawRect(ren, &itemRect);
        }

        // Render label text
        SDL_Color col = (i == m_SelectedIndex) ? colorSelected : colorNormal;
        SDL_Surface* textSurf = TTF_RenderText_Blended(m_Font, m_Items[i].label.c_str(), col);
        if (textSurf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, textSurf);
            if (tex) {
                SDL_Rect dst = {
                    itemRect.x + k_ItemPadding,
                    itemRect.y + (itemRect.h - textSurf->h) / 2,
                    textSurf->w, textSurf->h
                };
                SDL_RenderCopy(ren, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(textSurf);
        }
    }

    // Hint row at the bottom
    {
        SDL_Color hintColor = {150, 150, 150, 255};
        SDL_Surface* hintSurf = TTF_RenderText_Blended(m_Font, "[A] Confirm  [B] Close", hintColor);
        if (hintSurf) {
            SDL_Texture* hintTex = SDL_CreateTextureFromSurface(ren, hintSurf);
            if (hintTex) {
                // Clamp the hint text width to the panel width minus padding
                int hintW = qMin(hintSurf->w, surfW - k_ItemPadding * 2);
                int hintX = k_ItemPadding + (surfW - k_ItemPadding * 2 - hintW) / 2;
                int hintY = k_ItemPadding + numItems * k_ItemHeight + (hintRowH - hintSurf->h) / 2;
                
                SDL_Rect src = {0, 0, hintW, hintSurf->h};
                SDL_Rect dst = {hintX, hintY, hintW, hintSurf->h};
                SDL_RenderCopy(ren, hintTex, &src, &dst);
                SDL_DestroyTexture(hintTex);
            }
            SDL_FreeSurface(hintSurf);
        }
    }

    SDL_RenderPresent(ren);
    SDL_DestroyRenderer(ren);

    // Convert to ARGB8888 if needed (SDL_CreateRGBSurfaceWithFormat should already be correct,
    // but make sure the pixel format matches what renderers expect)
    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(surf);

    // Hand off to OverlayManager — it takes ownership of the surface
    Session::get()->getOverlayManager().setOverlaySurface(Overlay::OverlayMenu, converted);
}
