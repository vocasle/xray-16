#include "StdAfx.h"
#include "UIMapWnd.h"
#include "UIMap.h"
#include "UIXmlInit.h"
#include "Actor.h"
#include "map_manager.h"
#include "UIInventoryUtilities.h"
#include "map_spot.h"
#include "map_location.h"
#include "xrUICore/ScrollBar/UIFixedScrollBar.h"
#include "xrUICore/Windows/UIFrameWindow.h"
#include "xrUICore/Windows/UIFrameLineWnd.h"
#include "xrUICore/TabControl/UITabControl.h"
#include "xrUICore/Buttons/UI3tButton.h"
#include "UIMapWndActions.h"
#include "UIMapWndActionsSpace.h"
#include "xrUICore/Hint/UIHint.h"
#include "map_hint.h"
#include "xrUICore/Cursor/UICursor.h"
#include "xrUICore/PropertiesBox/UIPropertiesBox.h"
#include "xrUICore/ListBox/UIListBoxItem.h"
#include "xrEngine/xr_input.h" //remove me !!!
#include "UIHelper.h"

CUIMapWnd* g_map_wnd = NULL; // quick temporary solution -(
CUIMapWnd* GetMapWnd() { return g_map_wnd; }

CUIMapWnd::CUIMapWnd(UIHint* hint)
    : CUIWindow("CUIMapWnd"), m_ActionPlanner(nullptr)
{
    m_tgtMap = NULL;
    m_GlobalMap = NULL;
    m_view_actor = false;
    m_prev_actor_pos.set(0, 0);
    m_currentZoom = 1.0f;
    m_map_location_hint = NULL;
    m_map_move_step = 10.0f;
    /*
    #ifdef DEBUG
    //	m_dbg_text_hint			= NULL;
    //	m_dbg_info				= NULL;
    #endif // DEBUG
    */

    m_UIMainMapHeader = nullptr;
    m_scroll_mode = false;
    m_nav_timing = Device.dwTimeGlobal;
    hint_wnd = hint;
    g_map_wnd = this;
    m_cur_location = nullptr;
}

CUIMapWnd::~CUIMapWnd()
{
    delete_data(m_ActionPlanner);
    delete_data(m_GameMaps);
    delete_data(m_map_location_hint);
    /*
    #ifdef DEBUG
        delete_data( m_dbg_text_hint );
        delete_data( m_dbg_info );
    #endif // DEBUG
    */
    g_map_wnd = NULL;
}

bool CUIMapWnd::Init(cpcstr xml_name, cpcstr start_from, bool critical /*= true*/)
{
    CUIXml uiXml;
    if (!uiXml.Load(CONFIG_PATH, UI_PATH, UI_PATH_DEFAULT, xml_name, critical))
        return false;

    string512 pth;
    strconcat(sizeof(pth), pth, start_from, ":main_wnd");
    CUIXmlInit::InitWindow(uiXml, pth, 0, this);

    m_map_move_step = uiXml.ReadAttribFlt(start_from, 0, "map_move_step", 10.0f);

    strconcat(sizeof(pth), pth, start_from, ":main_map_frame");
    m_UIMainFrame = UIHelper::CreateFrameWindow(uiXml, pth, this, false);
    if (!m_UIMainFrame)
    {
        strconcat(sizeof(pth), pth, start_from, ":main_wnd:main_map_frame");
        m_UIMainFrame = UIHelper::CreateFrameWindow(uiXml, pth, this, false);
    }

    strconcat(sizeof(pth), pth, start_from, ":level_frame");
    m_UILevelFrame = UIHelper::CreateNormalWindow(uiXml, pth, this, false);
    if (!m_UILevelFrame)
    {
        strconcat(sizeof(pth), pth, start_from, ":main_wnd:main_map_frame:level_frame");
        m_UILevelFrame = UIHelper::CreateNormalWindow(uiXml, pth, m_UIMainFrame);
    }

    strconcat(sizeof(pth), pth, start_from, "main_map_header");
    m_UIMainMapHeader = UIHelper::CreateFrameLine(uiXml, pth, this, false);
    if (!m_UIMainMapHeader)
    {
        strconcat(sizeof(pth), pth, start_from, ":main_wnd:map_header_frame_line");
        m_UIMainMapHeader = UIHelper::CreateFrameLine(uiXml, pth, m_UIMainFrame, false);
    }

    m_scroll_mode = uiXml.ReadAttribInt(start_from, 0, "scroll_enable", 0) == 1;
    if (m_scroll_mode || ShadowOfChernobylMode)
    {
        float dx, dy, sx, sy;
        strconcat(sizeof(pth), pth, start_from, ":main_map_frame");
        dx = uiXml.ReadAttribFlt(pth, 0, "dx", 0.0f);
        dy = uiXml.ReadAttribFlt(pth, 0, "dy", 0.0f);
        sx = uiXml.ReadAttribFlt(pth, 0, "sx", 5.0f);
        sy = uiXml.ReadAttribFlt(pth, 0, "sy", 5.0f);

        CUIWindow* rect_parent = m_UIMainFrame; // m_UILevelFrame;
        Frect r = rect_parent->GetWndRect();

        auto tempScroll = xr_new<CUIFixedScrollBar>();
        if (tempScroll->InitScrollBar(Fvector2().set(r.left + dx, r.bottom - sy), true))
            m_UIMainScrollH = tempScroll;
        else
        {
            Msg("! Failed to init m_UIMainScrollH as FixedScrollBar, trying to initialize it as ScrollBar");
            xr_delete(tempScroll);
            m_UIMainScrollH = xr_new<CUIScrollBar>();
            m_UIMainScrollH->InitScrollBar(Fvector2().set(r.left + dx, r.bottom - sy), r.right - r.left - dx * 2 - sx, true, "pda");
        }

        m_UIMainScrollH->SetStepSize(_max(1, (int)(m_UILevelFrame->GetWidth() * 0.1f)));
        m_UIMainScrollH->SetPageSize((int)m_UILevelFrame->GetWidth()); // iFloor
        m_UIMainScrollH->SetAutoDelete(true);
        AttachChild(m_UIMainScrollH);
        Register(m_UIMainScrollH);
        AddCallback(m_UIMainScrollH, SCROLLBAR_HSCROLL, CUIWndCallback::void_function(this, &CUIMapWnd::OnScrollH));

        tempScroll = xr_new<CUIFixedScrollBar>();
        if (tempScroll->InitScrollBar(Fvector2().set(r.right - sx, r.top + dy), false))
            m_UIMainScrollV = tempScroll;
        else
        {
            Msg("! Failed to init m_UIMainScrollV as FixedScrollBar, trying to initialize it as ScrollBar");
            xr_delete(tempScroll);
            m_UIMainScrollV = xr_new<CUIScrollBar>();
            m_UIMainScrollV->InitScrollBar(Fvector2().set(r.right - sx, r.top + dy), r.bottom - r.top - dy * 2, false, "pda");
        }

        m_UIMainScrollV->SetStepSize(_max(1, (int)(m_UILevelFrame->GetHeight() * 0.1f)));
        m_UIMainScrollV->SetPageSize((int)m_UILevelFrame->GetHeight());
        m_UIMainScrollV->SetAutoDelete(true);
        AttachChild(m_UIMainScrollV);
        Register(m_UIMainScrollV);
        AddCallback(m_UIMainScrollV, SCROLLBAR_VSCROLL, CUIWndCallback::void_function(this, &CUIMapWnd::OnScrollV));
    }

    init_xml_nav(uiXml, start_from, critical);

    m_map_location_hint = xr_new<CUIMapLocationHint>();
    m_map_location_hint->SetAutoDelete(false);
    m_map_location_hint->SetCustomDraw(true);
    strconcat(sizeof(pth), pth, start_from, ":map_hint_item");
    m_map_location_hint->Init(uiXml, pth);

    // Load maps

    m_GlobalMap = xr_new<CUIGlobalMap>(this);
    m_GlobalMap->SetAutoDelete(true);
    m_GlobalMap->Initialize();

    m_UILevelFrame->AttachChild(m_GlobalMap);
    m_GlobalMap->OptimalFit(m_UILevelFrame->GetWndRect());
    m_GlobalMap->SetMinZoom(m_GlobalMap->GetCurrentZoom().x);
    m_currentZoom = m_GlobalMap->GetCurrentZoom().x;

    // initialize local maps
    xr_string sect_name;
    if (IsGameTypeSingle())
        sect_name = "level_maps_single";
    else
        sect_name = "level_maps_mp";

    if (pGameIni->section_exist(sect_name.c_str()))
    {
        CInifile::Sect& S = pGameIni->r_section(sect_name.c_str());
        auto it = S.Data.cbegin(), end = S.Data.cend();
        for (; it != end; ++it)
        {
            shared_str map_name = it->first;
            xr_strlwr(map_name);
            R_ASSERT2(m_GameMaps.end() == m_GameMaps.find(map_name), "Duplicate level name not allowed");

            CUICustomMap*& l = m_GameMaps[map_name];

            l = xr_new<CUILevelMap>(this);
            R_ASSERT2(pGameIni->section_exist(map_name), map_name.c_str());
            l->Initialize(map_name, "hud" DELIMITER "default");

            l->OptimalFit(m_UILevelFrame->GetWndRect());
        }
    }

#ifdef DEBUG
    GAME_MAPS::iterator it = m_GameMaps.begin();
    GAME_MAPS::iterator it2;
    for (; it != m_GameMaps.end(); ++it)
    {
        CUILevelMap* l = smart_cast<CUILevelMap*>(it->second);
        VERIFY(l);
        for (it2 = it; it2 != m_GameMaps.end(); ++it2)
        {
            if (it == it2)
                continue;
            CUILevelMap* l2 = smart_cast<CUILevelMap*>(it2->second);
            VERIFY(l2);
            if (l->GlobalRect().intersected(l2->GlobalRect()))
            {
                Msg(" --error-incorrect map definition global rect of map [%s] intersects with [%s]", *l->MapName(),
                    *l2->MapName());
            }
        }
        if (FALSE == l->GlobalRect().intersected(GlobalMap()->BoundRect()))
        {
            Msg(" --error-incorrect map definition map [%s] places outside global map", *l->MapName());
        }
    }
#endif

    Register(m_GlobalMap);
    m_ActionPlanner = xr_new<CMapActionPlanner>();
    m_ActionPlanner->setup(this);
    m_view_actor = true;

    m_UIPropertiesBox = xr_new<CUIPropertiesBox>();
    m_UIPropertiesBox->SetAutoDelete(true);
    m_UIPropertiesBox->InitPropertiesBox(Fvector2().set(0, 0), Fvector2().set(300, 300));
    AttachChild(m_UIPropertiesBox);
    m_UIPropertiesBox->Hide();
    m_UIPropertiesBox->SetWindowName("property_box");

    return true;
}

void CUIMapWnd::Show(bool status)
{
    inherited::Show(status);
    Activated();
    if (GlobalMap())
    {
        m_GlobalMap->DetachAll();
        m_GlobalMap->Show(false);
    }
    GAME_MAPS::iterator it = m_GameMaps.begin();
    for (; it != m_GameMaps.end(); ++it)
    {
        it->second->DetachAll();
    }

    if (status)
    {
        m_GlobalMap->Show(true);
        m_GlobalMap->WorkingArea().set(ActiveMapRect());
        GAME_MAPS::iterator it = m_GameMaps.begin();
        GAME_MAPS::iterator it_e = m_GameMaps.end();
        for (; it != it_e; ++it)
        {
            m_GlobalMap->AttachChild(it->second);
            it->second->Show(true);
            it->second->WorkingArea().set(ActiveMapRect());
        }

        if (m_view_actor)
        {
            inherited::Update(); // only maps, not action planner
            ViewActor();
            m_view_actor = false;
        }
        InventoryUtilities::SendInfoToActor("ui_pda_map_local");
    }
    HideCurHint();
}

void CUIMapWnd::Activated()
{
    Fvector v = Level().CurrentEntity()->Position();
    Fvector2 v2;
    v2.set(v.x, v.z);
    if (v2.distance_to(m_prev_actor_pos) > 3.0f)
    {
        ViewActor();
    }
}

void CUIMapWnd::AddMapToRender(CUICustomMap* m)
{
    Register(m);
    m_UILevelFrame->AttachChild(m);
    m->Show(true);
    m->WorkingArea().set(ActiveMapRect());
}

void CUIMapWnd::RemoveMapToRender(CUICustomMap* m)
{
    if (m != GlobalMap())
        m_UILevelFrame->DetachChild(smart_cast<CUIWindow*>(m));
}

void CUIMapWnd::SetTargetMap(const shared_str& name, const Fvector2& pos, bool bZoomIn)
{
    u16 idx = GetIdxByName(name);
    if (idx != u16(-1))
    {
        CUICustomMap* lm = GetMapByIdx(idx);
        SetTargetMap(lm, pos, bZoomIn);
    }
}

void CUIMapWnd::SetTargetMap(const shared_str& name, bool bZoomIn)
{
    u16 idx = GetIdxByName(name);
    if (idx != u16(-1))
    {
        CUICustomMap* lm = GetMapByIdx(idx);
        SetTargetMap(lm, bZoomIn);
    }
}

void CUIMapWnd::SetTargetMap(CUICustomMap* m, bool bZoomIn)
{
    m_tgtMap = m;
    Fvector2 pos;
    Frect r = m->BoundRect();
    r.getcenter(pos);
    SetTargetMap(m, pos, bZoomIn);
}

void CUIMapWnd::SetTargetMap(CUICustomMap* m, const Fvector2& pos, bool bZoomIn)
{
    m_tgtMap = m;

    if (m == GlobalMap())
    {
        CUIGlobalMap* gm = GlobalMap();
        SetZoom(gm->GetMinZoom());
        Frect vis_rect = ActiveMapRect();
        vis_rect.getcenter(m_tgtCenter);
        Fvector2 _p;
        gm->GetAbsolutePos(_p);
        m_tgtCenter.sub(_p);
        m_tgtCenter.div(gm->GetCurrentZoom());
    }
    else
    {
        if (bZoomIn /* && fsimilar(GlobalMap()->GetCurrentZoom(), GlobalMap()->GetMinZoom(),EPS_L )*/)
            SetZoom(GlobalMap()->GetMaxZoom());

        //		m_tgtCenter						= m->ConvertRealToLocalNoTransform(pos, m->BoundRect());
        m_tgtCenter = m->ConvertRealToLocal(pos, true);
        m_tgtCenter.add(m->GetWndPos()).div(GlobalMap()->GetCurrentZoom());
    }
    ResetActionPlanner();
}

void CUIMapWnd::MoveMap(Fvector2 const& pos_delta)
{
    GlobalMap()->MoveWndDelta(pos_delta);
    UpdateScroll();
    HideCurHint();
}

void CUIMapWnd::Draw()
{
    inherited::Draw();
    /*
    #ifdef DEBUG
        m_dbg_text_hint->Draw	();
        m_dbg_info->Draw		();
    #endif // DEBUG */

    if (m_btn_nav_parent)
        m_btn_nav_parent->Draw();
}

void CUIMapWnd::MapLocationRelcase(CMapLocation* ml)
{
    CUIWindow* owner = m_map_location_hint->GetOwner();
    if (owner)
    {
        CMapSpot* ms = smart_cast<CMapSpot*>(owner);
        if (ms && ms->MapLocation() == ml) // CUITaskItem also can be a HintOwner
            m_map_location_hint->SetOwner(NULL);
    }
}

void CUIMapWnd::DrawHint()
{
    CUIWindow* owner = m_map_location_hint->GetOwner();
    if (owner)
    {
        CMapSpot* ms = smart_cast<CMapSpot*>(owner);
        if (ms)
        {
            if (ms->MapLocation() && ms->MapLocation()->HintEnabled())
            {
                m_map_location_hint->Draw();
            }
        }
        else
        {
            m_map_location_hint->Draw();
        }
    }
}

bool CUIMapWnd::OnKeyboardAction(int dik, EUIMessages keyboard_action)
{
    switch (keyboard_action)
    {
    case WINDOW_KEY_PRESSED:
    {
        switch (GetBindedAction(dik, EKeyContext::PDA))
        {
        case kPDA_MAP_ZOOM_RESET:
            ViewGlobalMap();
            return true;

        case kPDA_MAP_SHOW_ACTOR:
            ViewActor();
            return true;

        case kPDA_MAP_SHOW_LEGEND:
            OnBtnLegend_Push(this, nullptr);
            return true;
        } // switch (dik)
        break;
    }

    case WINDOW_KEY_HOLD:
    {
        Fvector2 pos_delta{};

        switch (GetBindedAction(dik, EKeyContext::PDA))
        {
        case kPDA_MAP_ZOOM_OUT:
            // SetZoom(GetZoom()/1.5f);
            UpdateZoom(false);
            // ResetActionPlanner();
            return true;

        case kPDA_MAP_ZOOM_IN:
            // SetZoom(GetZoom()*1.5f);
            UpdateZoom(true);
            // ResetActionPlanner();
            return true;

        case kPDA_MAP_MOVE_UP:
            pos_delta.y += m_map_move_step;
            break;
        case kPDA_MAP_MOVE_DOWN:
            pos_delta.y -= m_map_move_step;
            break;
        case kPDA_MAP_MOVE_LEFT:
            pos_delta.x += m_map_move_step;
            break;
        case kPDA_MAP_MOVE_RIGHT:
            pos_delta.x -= m_map_move_step;
            break;
        }

        if (pos_delta.x || pos_delta.y)
        {
            MoveMap(pos_delta);
            return true;
        }
        break;
    }
    } // switch (keyboard_action)

    return inherited::OnKeyboardAction(dik, keyboard_action);
}

bool CUIMapWnd::OnControllerAction(int axis, const ControllerAxisState& state, EUIMessages controller_action)
{
    switch (GetBindedAction(axis, EKeyContext::PDA))
    {
    default:
        return OnKeyboardAction(axis, controller_action);

    case kPDA_MAP_MOVE:
    {
        if (controller_action != WINDOW_KEY_HOLD)
            break;
        const auto pos_delta = Fvector2{ m_map_move_step, m_map_move_step }.mul(Fvector2{ -state.x, -state.y }.normalize());
        MoveMap(pos_delta);
        return true;
    }
    } // switch (GetBindedAction(axis, EKeyContext::PDA))

    return inherited::OnControllerAction(axis, state, controller_action);
}

bool CUIMapWnd::OnMouseAction(float x, float y, EUIMessages mouse_action)
{
    if (inherited::OnMouseAction(x, y, mouse_action) /*|| m_btn_nav_parent->OnMouseAction(x,y,mouse_action)*/)
    {
        return true;
    }

    Fvector2 cursor_pos1 = GetUICursor().GetCursorPosition();

    if (GlobalMap() && !GlobalMap()->Locked() && ActiveMapRect().in(cursor_pos1))
    {
        switch (mouse_action)
        {
        case WINDOW_RBUTTON_UP:
            ActivatePropertiesBox(NULL);
            break;

        case WINDOW_MOUSE_MOVE:
            if (pInput->iGetAsyncKeyState(MOUSE_1))
            {
                GlobalMap()->MoveWndDelta(GetUICursor().GetCursorPositionDelta());
                UpdateScroll();
                HideCurHint();
                return true;
            }
            break;

        case WINDOW_MOUSE_WHEEL_DOWN:
            UpdateZoom(true);
            return true;

        case WINDOW_MOUSE_WHEEL_UP:
            UpdateZoom(false);
            return true;
        } // switch (mouse_action)
    }

    return false;
}

bool CUIMapWnd::UpdateZoom(bool b_zoom_in)
{
    float prev_zoom = GetZoom();
    float z = 0.0f;
    if (b_zoom_in)
    {
        z = GetZoom() * 1.2f;
        SetZoom(z);
    }
    else
    {
        z = GetZoom() / 1.2f;
        SetZoom(z);
    }

    if (!fsimilar(prev_zoom, GetZoom()))
    {
        //		m_tgtCenter.set( 0, 0 );// = cursor_pos;
        Frect vis_rect = ActiveMapRect();
        vis_rect.getcenter(m_tgtCenter);

        Fvector2 pos;
        CUIGlobalMap* gm = GlobalMap();
        gm->GetAbsolutePos(pos);
        m_tgtCenter.sub(pos);
        m_tgtCenter.div(gm->GetCurrentZoom());

        ResetActionPlanner();
        HideCurHint();
        return false;
    }
    return true;
}

void CUIMapWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    //	inherited::SendMessage( pWnd, msg, pData);
    CUIWndCallback::OnEvent(pWnd, msg, pData);

    if (pWnd == m_UIPropertiesBox && msg == PROPERTY_CLICKED && m_UIPropertiesBox->GetClickedItem())
    {
        luabind::functor<void> funct;
        if (GEnv.ScriptEngine->functor("pda.property_box_clicked", funct))
            funct(m_UIPropertiesBox, m_cur_location);
    }
}

void CUIMapWnd::ActivatePropertiesBox(CUIWindow* w)
{
    m_UIPropertiesBox->RemoveAll();

    CMapSpot* sp = smart_cast<CMapSpot*>(w);
    if (!sp)
        return;

    m_cur_location = sp->MapLocation();
    if (!m_cur_location)
        return;

    luabind::functor<void> funct;
    if (GEnv.ScriptEngine->functor("pda.property_box_add_properties", funct))
    {
        funct(m_UIPropertiesBox, m_cur_location->ObjectID(), (LPCSTR)m_cur_location->GetLevelName().c_str(), m_cur_location->GetHint());
    }

    // Только для меток игрока
    if (m_cur_location->IsUserDefined())
    {
        m_UIPropertiesBox->AddItem("st_pda_change_spot_hint", NULL, MAP_CHANGE_SPOT_HINT_ACT); // Изменяем название метки
        m_UIPropertiesBox->AddItem("st_pda_delete_spot", NULL, MAP_REMOVE_SPOT_ACT); // Удаляем метку
    }

    if (m_UIPropertiesBox->GetItemsCount() > 0)
    {
        m_UIPropertiesBox->AutoUpdateSize();

        Fvector2 cursor_pos;
        Frect vis_rect;

        GetAbsoluteRect(vis_rect);
        cursor_pos = GetUICursor().GetCursorPosition();
        cursor_pos.sub(vis_rect.lt);
        m_UIPropertiesBox->Show(vis_rect, cursor_pos);
    }
}

CUICustomMap* CUIMapWnd::GetMapByIdx(u16 idx)
{
    VERIFY(idx != u16(-1));
    auto it = m_GameMaps.begin();
    std::advance(it, idx);
    return it->second;
}

u16 CUIMapWnd::GetIdxByName(const shared_str& map_name)
{
    auto it = m_GameMaps.find(map_name);
    if (it == m_GameMaps.end())
    {
        Msg("~ Level Map '%s' not registered", map_name.c_str());
        return u16(-1);
    }
    return (u16)std::distance(m_GameMaps.begin(), it);
}

void CUIMapWnd::UpdateScroll()
{
    if (m_scroll_mode)
    {
        Fvector2 w_pos = GlobalMap()->GetWndPos();
        m_UIMainScrollV->SetRange(m_UIMainScrollV->GetMinRange(), iFloor(GlobalMap()->GetHeight()));
        m_UIMainScrollH->SetRange(m_UIMainScrollV->GetMinRange(), iFloor(GlobalMap()->GetWidth()));

        m_UIMainScrollV->SetScrollPos(iFloor(-w_pos.y));
        m_UIMainScrollH->SetScrollPos(iFloor(-w_pos.x));
    }
}

void CUIMapWnd::OnScrollV(CUIWindow*, void*)
{
    if (m_scroll_mode && GlobalMap())
    {
        MoveScrollV(-1.0f * float(m_UIMainScrollV->GetScrollPos()));
    }
}

void CUIMapWnd::OnScrollH(CUIWindow*, void*)
{
    if (m_scroll_mode && GlobalMap())
    {
        MoveScrollH(-1.0f * float(m_UIMainScrollH->GetScrollPos()));
    }
}

void CUIMapWnd::MoveScrollV(float dy)
{
    Fvector2 w_pos = GlobalMap()->GetWndPos();
    GlobalMap()->SetWndPos(Fvector2().set(w_pos.x, dy));
}

void CUIMapWnd::MoveScrollH(float dx)
{
    Fvector2 w_pos = GlobalMap()->GetWndPos();
    GlobalMap()->SetWndPos(Fvector2().set(dx, w_pos.y));
}

void CUIMapWnd::Update()
{
    if (m_GlobalMap)
        m_GlobalMap->WorkingArea().set(ActiveMapRect());
    inherited::Update();
    m_ActionPlanner->update();
    UpdateNav();
}

void CUIMapWnd::SetZoom(float value)
{
    m_currentZoom = value;
    clamp(m_currentZoom, GlobalMap()->GetMinZoom(), GlobalMap()->GetMaxZoom());
}

void CUIMapWnd::ViewGlobalMap()
{
    if (GlobalMap()->Locked())
        return;
    SetTargetMap(GlobalMap());
}

void CUIMapWnd::ResetActionPlanner()
{
    m_ActionPlanner->m_storage.set_property(1, false);
    m_ActionPlanner->m_storage.set_property(2, false);
    m_ActionPlanner->m_storage.set_property(3, false);
}

void CUIMapWnd::ViewZoomIn()
{
    if (GlobalMap()->Locked())
        return;
    UpdateZoom(true);
}

void CUIMapWnd::ViewZoomOut()
{
    if (GlobalMap()->Locked())
        return;
    UpdateZoom(false);
}

void CUIMapWnd::ViewActor()
{
    if (GlobalMap()->Locked())
        return;

    Fvector v = Level().CurrentEntity()->Position();
    m_prev_actor_pos.set(v.x, v.z);

    CUICustomMap* lm = NULL;
    u16 idx = GetIdxByName(Level().name());
    if (idx != u16(-1))
    {
        lm = GetMapByIdx(idx);
    }
    else
    {
        lm = GlobalMap();
    }

    SetTargetMap(lm, m_prev_actor_pos, true);
}

void CUIMapWnd::ShowHintStr(CUIWindow* parent, LPCSTR text) // map name
{
    if (m_map_location_hint->GetOwner())
        return;

    m_map_location_hint->SetInfoStr(text);
    m_map_location_hint->SetOwner(parent);
    ShowHint();
}

void CUIMapWnd::ShowHintSpot(CMapSpot* spot)
{
    CUIWindow* owner = m_map_location_hint->GetOwner();
    if (!owner)
    {
        m_map_location_hint->SetInfoMSpot(spot);
        m_map_location_hint->SetOwner(spot);
        ShowHint();
        return;
    }

    CMapSpot* prev_spot = smart_cast<CMapSpot*>(owner);
    if (prev_spot && (prev_spot->get_location_level() < spot->get_location_level()))
    {
        m_map_location_hint->SetInfoMSpot(spot);
        m_map_location_hint->SetOwner(spot);
        ShowHint();
        return;
    }
}

void CUIMapWnd::ShowHintTask(CGameTask* task, CUIWindow* owner)
{
    if (task)
    {
        m_map_location_hint->SetInfoTask(task);
        m_map_location_hint->SetOwner(owner);
        ShowHint(true);
        return;
    }
    HideCurHint();
}

void CUIMapWnd::ShowHint(bool extra)
{
    Frect vis_rect;
    if (extra)
    {
        vis_rect.set(Frect().set(0.0f, 0.0f, UI_BASE_WIDTH, UI_BASE_HEIGHT));
    }
    else
    {
        vis_rect = ActiveMapRect();
    }

    bool is_visible = fit_in_rect(m_map_location_hint, vis_rect);
    if (!is_visible)
    {
        HideCurHint();
    }
}

void CUIMapWnd::HideHint(CUIWindow* parent)
{
    if (m_map_location_hint->GetOwner() == parent)
    {
        HideCurHint();
    }
}

void CUIMapWnd::HideCurHint() { m_map_location_hint->SetOwner(NULL); }
void CUIMapWnd::Hint(const shared_str& text)
{
    /*
#ifdef DEBUG
    m_dbg_text_hint->SetTextST( *text );
#endif // DEBUG */
}

void CUIMapWnd::Reset()
{
    inherited::Reset();
    ResetActionPlanner();
}

#include "GametaskManager.h"
#include "Actor.h"
#include "map_spot.h"
#include "GameTask.h"

void CUIMapWnd::SpotSelected(CUIWindow* w)
{
    CMapSpot* sp = smart_cast<CMapSpot*>(w);
    if (!sp)
    {
        return;
    }

    CGameTask* t = Level().GameTaskManager().HasGameTask(sp->MapLocation(), true);
    if (t)
    {
        Level().GameTaskManager().SetActiveTask(t);
    }
}
