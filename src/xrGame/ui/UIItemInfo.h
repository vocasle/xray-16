#pragma once
#include "xrUICore/Windows/UIWindow.h"

class CInventoryItem;
class CUIStatic;
class CUIScrollView;
class CUIProgressBar;
class CUIConditionParams;
class CUIWpnParams;
class CUIArtefactParams;
class CUIFrameWindow;
class UIInvUpgPropertiesWnd;
class CUIOutfitInfo;
class CUIBoosterInfo;
class CUICellItem;

extern const char* const fieldsCaptionColor;

class CUIItemInfo final : public CUIWindow
{
private:
    typedef CUIWindow inherited;
    struct _desc_info
    {
        CGameFont* pDescFont;
        u32 uDescClr;
        bool bShowDescrText;
    };
    _desc_info m_desc_info;
    CInventoryItem* m_pInvItem;

public:
    CUIItemInfo();
    ~CUIItemInfo() override;

    pcstr GetDebugType() override { return "CUIItemInfo"; }

    CInventoryItem* CurrentItem() const { return m_pInvItem; }
    void InitItemInfo(Fvector2 pos, Fvector2 size, LPCSTR xml_name);
    bool InitItemInfo(cpcstr xml_name);
    void InitItem(CUICellItem* pCellItem, CInventoryItem* pCompareItem = nullptr,
        u32 item_price = u32(-1), pcstr trade_tip = nullptr);

    void TryAddConditionInfo(CInventoryItem& pInvItem, CInventoryItem* pCompareItem);
    void TryAddWpnInfo(CInventoryItem& pInvItem, CInventoryItem* pCompareItem);
    void TryAddArtefactInfo(CInventoryItem& pInvItem);
    void TryAddOutfitInfo(CInventoryItem& pInvItem, CInventoryItem* pCompareItem);
    void TryAddUpgradeInfo(CInventoryItem& pInvItem);
    void TryAddBoosterInfo(CInventoryItem& pInvItem);

    virtual void Draw();
    bool m_b_FitToHeight;
    u32 delay;

    CUIFrameWindow* UIBackground;
    CUIStatic* UIName;
    CUIStatic* UIWeight;
    CUIStatic* UICost;
    CUIStatic* UITradeTip;
    //	CUIStatic*			UIDesc_line;
    CUIScrollView* UIDesc;
    bool m_complex_desc;

    CUIConditionParams* UIConditionWnd;
    CUIWpnParams* UIWpnParams;
    CUIArtefactParams* UIArtefactParams;
    UIInvUpgPropertiesWnd* UIProperties;
    CUIOutfitInfo* UIOutfitInfo;
    CUIBoosterInfo* UIBoosterInfo;

    Fvector2 UIItemImageSize;
    CUIStatic* UIItemImage;
};
