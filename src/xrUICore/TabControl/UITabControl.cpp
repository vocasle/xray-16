#include "pch.hpp"
#include "UITabControl.h"
#include "UITabButton.h"

bool operator==(const CUITabButton* btn, const shared_str& id)
{
    R_ASSERT2_CURE(btn, "The btn pointer should never be nullptr.\n"
                        "It's either because of wrong usage or UB (yay!)",
    {
        return id.empty();
    });
    return btn->m_btn_id == id;
}

CUITabControl::CUITabControl() : CUIWindow("CUITabControl")
{
}

CUITabControl::~CUITabControl() { RemoveAll(); }
void CUITabControl::SetCurrentOptValue()
{
    shared_str v = GetOptStringValue();
    CUITabButton* b = GetButtonById(v);
    if (NULL == b)
    {
#ifndef MASTER_GOLD
        Msg("! tab named [%s] doesnt exist", v.c_str());
#endif // #ifndef MASTER_GOLD
        v = m_TabsArr[0]->m_btn_id;
    }
    SetActiveTab(v);
}

void CUITabControl::SaveOptValue()
{
    CUIOptionsItem::SaveOptValue();
    SaveOptStringValue(GetActiveId().c_str());
}

void CUITabControl::UndoOptValue()
{
    SetActiveTab(m_opt_backup_value);
    CUIOptionsItem::UndoOptValue();
}

void CUITabControl::SaveBackUpOptValue()
{
    m_opt_backup_value = GetActiveId();
}

bool CUITabControl::IsChangedOptValue() const { return GetActiveId() != m_opt_backup_value; }
// добавление кнопки-закладки в список закладок контрола
bool CUITabControl::AddItem(LPCSTR pItemName, LPCSTR pTexName, Fvector2 pos, Fvector2 size)
{
    CUITabButton* pNewButton = xr_new<CUITabButton>();
    pNewButton->SetAutoDelete(true);
    pNewButton->InitButton(pos, size);
    pNewButton->InitTexture(pTexName);
    pNewButton->TextItemControl()->SetText(pItemName);
    pNewButton->TextItemControl()->SetTextColor(m_cGlobalTextColor);
    pNewButton->SetTextureColor(m_cGlobalButtonColor);

    return AddItem(pNewButton);
}

bool CUITabControl::AddItem(CUITabButton* pButton)
{
    pButton->SetAutoDelete(true);
    pButton->Show(true);
    pButton->Enable(true);
    pButton->SetButtonAsSwitch(true);

    AttachChild(pButton);
    m_TabsArr.push_back(pButton);
    R_ASSERT(pButton->m_btn_id.size());
    return true;
}

void CUITabControl::RemoveItemById(const shared_str& id)
{
    const auto it = std::find(m_TabsArr.begin(), m_TabsArr.end(), id);
    const bool tabControlItemFound = it != m_TabsArr.end();

    R_ASSERT(tabControlItemFound);
    if (tabControlItemFound)
    {
        DetachChild(*it);
        m_TabsArr.erase(it);
    }
}

void CUITabControl::RemoveItemByIndex(u32 index)
{
    R_ASSERT(m_TabsArr.size() > index);

    // Меняем значение заданного элемента, и последнего элемента.
    // Так как у нас хранятся указатели операция будет проходить быстро.
    std::swap(m_TabsArr[index], m_TabsArr.back());

    DetachChild(m_TabsArr.back());
    m_TabsArr.pop_back();
}

void CUITabControl::RemoveAll()
{
    auto it = m_TabsArr.begin();
    for (; it != m_TabsArr.end(); ++it)
    {
        DetachChild(*it);
    }
    m_TabsArr.clear();
}

void CUITabControl::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    if (TAB_CHANGED == msg)
    {
        for (u32 i = 0; i < m_TabsArr.size(); ++i)
        {
            if (m_TabsArr[i] == pWnd)
            {
                m_sPushedId = m_TabsArr[i]->m_btn_id;
                if (m_sPrevPushedId == m_sPushedId)
                    return;

                OnTabChange(m_sPushedId, m_sPrevPushedId);
                m_sPrevPushedId = m_sPushedId;
                break;
            }
        }
    }

    else if (WINDOW_FOCUS_RECEIVED == msg || WINDOW_FOCUS_LOST == msg)
    {
        for (u8 i = 0; i < m_TabsArr.size(); ++i)
        {
            if (pWnd == m_TabsArr[i])
            {
                if (msg == WINDOW_FOCUS_RECEIVED)
                    OnStaticFocusReceive(pWnd);
                else
                    OnStaticFocusLost(pWnd);
            }
        }
    }
    else
    {
        inherited::SendMessage(pWnd, msg, pData);
    }
}

void CUITabControl::OnStaticFocusReceive(CUIWindow* pWnd)
{
    GetMessageTarget()->SendMessage(this, WINDOW_FOCUS_RECEIVED, static_cast<void*>(pWnd));
}

void CUITabControl::OnStaticFocusLost(CUIWindow* pWnd)
{
    GetMessageTarget()->SendMessage(this, WINDOW_FOCUS_LOST, static_cast<void*>(pWnd));
}

void CUITabControl::OnTabChange(const shared_str& sCur, const shared_str& sPrev)
{
    CUITabButton* tb_cur = GetButtonById(sCur);
    CUITabButton* tb_prev = GetButtonById(sPrev);
    if (tb_prev)
        tb_prev->SendMessage(tb_cur, TAB_CHANGED, NULL);

    if (tb_cur)
        tb_cur->SendMessage(tb_cur, TAB_CHANGED, NULL);

    GetMessageTarget()->SendMessage(this, TAB_CHANGED, NULL);
}

int CUITabControl::GetActiveIndex() const
{
    for (int i = 0; i < (int)m_TabsArr.size(); ++i)
    {
        if (m_TabsArr[i]->m_btn_id == m_sPushedId)
            return i;
    }
    return -1;
}

void CUITabControl::SetActiveTab(const shared_str& sNewTab)
{
    if (m_sPushedId == sNewTab)
        return;

    m_sPushedId = sNewTab;
    OnTabChange(m_sPushedId, m_sPrevPushedId);

    m_sPrevPushedId = m_sPushedId;
}

void CUITabControl::SetActiveTabByIndex(u32 index)
{
    CUITabButton* newBtn = GetButtonByIndex(index);
    CUITabButton* prevBtn = GetButtonById(GetActiveId());
    if (newBtn == prevBtn)
        return;

    SetActiveTab(newBtn->m_btn_id);
}

bool CUITabControl::SetNextActiveTab(bool next, bool loop)
{
    const int idx = GetActiveIndex();
    if (next)
    {
        if (idx < (int)GetTabsCount() - 1)
            SetActiveTabByIndex(idx + 1);
        else if (loop)
            SetActiveTabByIndex(0);
        else
            return false;
    }
    else
    {
        if (idx > 0)
            SetActiveTabByIndex(idx - 1);
        else if (loop)
            SetActiveTabByIndex(GetTabsCount() - 1);
        else
            return false;
    }
    return true;
}

bool CUITabControl::OnKeyboardAction(int dik, EUIMessages keyboard_action)
{
    if (WINDOW_KEY_PRESSED == keyboard_action)
    {
        if (GetAcceleratorsMode())
        {
            switch (GetBindedAction(dik, EKeyContext::UI))
            {
            case kUI_TAB_PREV: return SetNextActiveTab(false, true);
            case kUI_TAB_NEXT: return SetNextActiveTab(true,  true);
            }
        }
        if (GetButtonsAcceleratorsMode())
        {
            for (const auto& button : m_TabsArr)
            {
                if (button->IsAccelerator(dik))
                {
                    SetActiveTab(button->m_btn_id);
                    return true;
                }
            }
        }
    }
    return false;
}

bool CUITabControl::OnControllerAction(int axis, const ControllerAxisState& state, EUIMessages controller_action)
{
    if (WINDOW_KEY_PRESSED == controller_action)
    {
        if (GetButtonsAcceleratorsMode())
        {
            for (const auto& button : m_TabsArr)
            {
                if (button->IsAccelerator(axis))
                {
                    SetActiveTab(button->m_btn_id);
                    return true;
                }
            }
        }
    }
    return false;
}

CUITabButton* CUITabControl::GetButtonById(const shared_str& id)
{
    TABS_VECTOR::const_iterator it = std::find(m_TabsArr.begin(), m_TabsArr.end(), id);
    if (it != m_TabsArr.end())
        return *it;
    return nullptr;
}

CUITabButton* CUITabControl::GetButtonByIndex(u32 index) const
{
    R_ASSERT(index < static_cast<u32>(m_TabsArr.size()));
    return m_TabsArr[index];
}

void CUITabControl::ResetTab()
{
    for (u32 i = 0; i < m_TabsArr.size(); ++i)
    {
        m_TabsArr[i]->SetButtonState(CUIButton::BUTTON_NORMAL);
    }
    m_sPushedId = "";
    m_sPrevPushedId = "";
}

void CUITabControl::Enable(bool status)
{
    for (u32 i = 0; i < m_TabsArr.size(); ++i)
        m_TabsArr[i]->Enable(status);

    //	m_sPushedId		= "";
    //	m_sPrevPushedId	= "";
    inherited::Enable(status);
}
