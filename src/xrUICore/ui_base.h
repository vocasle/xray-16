#pragma once

#include "ui_defs.h"
#include "ui_debug.h"
#include "ui_focus.h"
#include "FontManager/FontManager.h"

#include "xrEngine/pure.h"
#include "xrEngine/device.h"

#include "xrCommon/xr_vector.h"
#include "xrCommon/xr_stack.h"

class CUICursor;
class CUIGameCustom;

class XRUICORE_API UICore : public CDeviceResetNotifier, public CUIResetNotifier
{
    C2DFrustum m_2DFrustum;
    C2DFrustum m_2DFrustumPP;
    C2DFrustum m_FrustumLIT;

    bool m_bPostprocess;

    CFontManager* m_pFontManager;
    CUICursor* m_pUICursor;
    CUIFocusSystem m_focusSystem;
    CUIDebugger m_debugger;

    Fvector2 m_pp_scale_;
    Fvector2 m_scale_;
    Fvector2* m_current_scale;

public:
    xr_stack<Frect> m_Scissors;

    UICore();
    ~UICore();
    void ReadTextureInfo();
    CFontManager& Font() { return *m_pFontManager; }
    CUICursor& GetUICursor() { return *m_pUICursor; }
    auto& Focus() { return m_focusSystem; }
    auto& Debugger() { return m_debugger; }
    IC float ClientToScreenScaledX(float left) const { return left * m_current_scale->x; };
    IC float ClientToScreenScaledY(float top) const { return top * m_current_scale->y; };
    void ClientToScreenScaled(Fvector2& dest, float left, float top) const;
    void ClientToScreenScaled(Fvector2& src_and_dest) const;
    void ClientToScreenScaledWidth(float& src_and_dest) const;
    void ClientToScreenScaledHeight(float& src_and_dest) const;
    void AlignPixel(float& src_and_dest) const;

    const C2DFrustum& ScreenFrustum() const { return (m_bPostprocess) ? m_2DFrustumPP : m_2DFrustum; }
    C2DFrustum& ScreenFrustumLIT() { return m_FrustumLIT; }
    void PushScissor(const Frect& r, bool overlapped = false);
    void PopScissor();

    void pp_start();
    void pp_stop();
    void RenderFont();

    virtual void OnDeviceReset();
    void OnUIReset() override;
    static bool is_widescreen();
    static float get_current_kx();
    static shared_str get_xml_name(pcstr path, pcstr fn);

    IUIRender::ePointType m_currentPointType;
};

XRUICORE_API extern CUICursor& GetUICursor();
XRUICORE_API extern UICore& UI();
