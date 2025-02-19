#pragma once
#include "xrUICore/ScrollBar/UIScrollBar.h"
#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Callbacks/UIWndCallback.h"

class CUIScrollBar;
class CUIFixedScrollBar;

class XRUICORE_API CUIScrollView : public CUIWindow, public CUIWndCallback
{
    typedef CUIWindow inherited;
    friend class CUIXmlInitBase; // for init
protected:
    enum
    {
        eVertFlip = (1 << 0),
        eNeedRecalc = (1 << 1),
        eFixedScrollBar = (1 << 2),
        eItemsSelectabe = (1 << 3),
        eInverseDir = (1 << 4) /*,eMultiSelect=(1<<5)*/
    };
    CUIScrollBar* m_VScrollBar{};
    CUIWindow* m_pad{};

    float m_rightIndent{};
    float m_leftIndent{};
    float m_upIndent{};
    float m_downIndent{};

    float m_vertInterval{};

    Flags16 m_flags{};
    shared_str m_scrollbar_profile;
    Ivector2 m_visible_rgn{ -1, -1 };

    virtual void RecalcSize();
    void UpdateScroll();
    void OnScrollV(CUIWindow*, void*);
    void SetRightIndention(float val);
    void SetLeftIndention(float val);
    void SetUpIndention(float val);
    void SetDownIndention(float val);
    void SetVertFlip(bool val) { m_flags.set(eVertFlip, val); }

public:
    CUIScrollView();
    CUIScrollView(CUIScrollBar* scroll_bar);
    virtual ~CUIScrollView();
    void InitScrollView(); // need parent to be initialized
    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData = NULL);
    virtual bool OnMouseAction(float x, float y, EUIMessages mouse_action);
    virtual void Draw();
    virtual void Update();
    void AddWindow(CUIWindow* pWnd, bool auto_delete);
    void RemoveWindow(CUIWindow* pWnd);
    void Clear();
    void ScrollToBegin();
    void ScrollToEnd();
    void ScrollToWindow(CUIWindow* pWnd, float center_y_ratio = 0.5f);
    bool GetVertFlip() const { return !!m_flags.test(eVertFlip); }
    bool Empty() const { return m_pad->GetChildWndList().empty(); }

    [[nodiscard]]
    u32 GetSize() const;

    WINDOW_LIST& Items() { return m_pad->GetChildWndList(); }
    CUIWindow* GetItem(u32 idx);
    void SetFixedScrollBar(bool b);
    float GetDesiredChildWidth() const;
    virtual void SetSelected(CUIWindow*);
    bool SelectFirst();
    CUIWindow* GetSelected();
    Fvector2 GetPadSize();
    void ForceUpdate();
    int GetMinScrollPos() const;
    int GetMaxScrollPos() const;
    int GetCurrentScrollPos() const;
    void SetScrollPos(int value);
    void SetScrollBarProfile(LPCSTR profile);
    IC bool NeedShowScrollBar() const; // no comment
    float GetHorizIndent() const; // left + right indent
    float GetVertIndent() const; // top + bottom indent
    void UpdateChildrenLenght(); // set default width for all children
    float Scroll2ViewV() const; // calculate scale for scroll position
    CUIScrollBar* ScrollBar() { return m_VScrollBar; }

    pcstr GetDebugType() override { return "CUIScrollView"; }
    void FillDebugInfo() override;

    typedef fastdelegate::FastDelegate2<CUIWindow*, CUIWindow*, bool> cmp_function;
    cmp_function m_sort_function;
};

#define ADD_TEXT_TO_VIEW3(txt, st, view)              \
    st = xr_new<CUIStatic>("Text");                   \
    st->SetFont(UI().Font().pFontLetterica16Russian); \
    st->SetText(txt);                                 \
    st->SetTextComplexMode(true);                     \
    st->SetWidth(view->GetDesiredChildWidth());       \
    st->AdjustHeightToText();                         \
    view->AddWindow(st, true)

#define ADD_TEXT_TO_VIEW2(txt, view) \
    CUIStatic* pSt;                 \
    ADD_TEXT_TO_VIEW3(txt, pSt, view)
