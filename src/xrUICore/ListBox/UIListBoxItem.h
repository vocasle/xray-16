#pragma once
#include "xrUICore/Windows/UIFrameLineWnd.h"

class CUIStatic;

class XRUICORE_API CUIListBoxItem : public CUIFrameLineWnd, public CUISelectable
{
    typedef CUIFrameLineWnd inherited;

public:
    CUIListBoxItem(float height);
    ~CUIListBoxItem() override;

    virtual void Draw();
    bool OnKeyboardAction(int dik, EUIMessages keyboard_action) override;
    virtual bool OnMouseDown(int mouse_btn);
    virtual void OnFocusReceive();
    void InitDefault();
    void SetTAG(u32 value);
    u32 GetTAG();

    void SetData(void* data);
    void* GetData();

    CUIStatic* AddTextField(LPCSTR txt, float width);
    CUIStatic* AddIconField(float width);

    CUIStatic* GetTextItem() const { return m_text; }
    // TextControl
    void SetText(LPCSTR txt);
    LPCSTR GetText();
    void SetTextColor(u32 color);
    u32 GetTextColor();
    void SetFont(CGameFont* F);
    CGameFont* GetFont();

    pcstr GetDebugType() override { return "CUIListBoxItem"; }

protected:
    CUIStatic* m_text;
    u32 tag;
    void* pData;
    float FieldsLength() const;
};
