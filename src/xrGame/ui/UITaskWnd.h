#pragma once

#include "xrUICore/Windows/UIWindow.h"
#include "xrUICore/Callbacks/UIWndCallback.h"
#include "xrCore/Containers/AssociativeVector.hpp"
#include "GameTaskDefs.h"

class CUIMapWnd;
class CUIMapFilters;
class CUIStatic;
class CGameTask;
class CUIXml;
class CUITaskItem;
class CUI3tButton;
class CUIFrameLineWnd;
class CUIFrameWindow;
class UITaskListWnd;
class UIMapLegend;
class UIHint;

class CUITaskWnd final : public CUIWindow, public CUIWndCallback
{
private:
    typedef CUIWindow inherited;

    CUIStatic* m_center_background{};
    CUIStatic* m_right_bottom_background{};

    CUIMapWnd* m_pMapWnd{};
    CUITaskItem* m_pStoryLineTaskItem{};
    CUITaskItem* m_pSecondaryTaskItem{};

    CUIStatic* m_second_task_index{};
    u32 m_actual_frame{};

    CUI3tButton* m_btn_focus{};
    CUI3tButton* m_btn_focus2{};

    CUIMapFilters* m_filters;

    UITaskListWnd* m_task_wnd{};
    UIMapLegend* m_map_legend_wnd{};

public:
    UIHint* hint_wnd;

public:
    CUITaskWnd(UIHint* hint);
    ~CUITaskWnd() override;

    pcstr GetDebugType() override { return "CUITaskWnd"; }

    bool OnKeyboardAction(int dik, EUIMessages keyboard_action) override;
    bool OnControllerAction(int axis, const ControllerAxisState& state, EUIMessages controller_action) override;
    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData);
    bool Init();
    virtual void Update();
    virtual void Draw();
    void DrawHint();
    virtual void Show(bool status);

    void ReloadTaskInfo();
    void Switch_ShowMapLegend() const;
    void Show_TaskListWnd(bool status) const;

    [[nodiscard]]
    bool IsTreasuresEnabled() const;

    [[nodiscard]]
    bool IsQuestNpcsEnabled() const;

    [[nodiscard]]
    bool IsSecondaryTasksEnabled() const;

    [[nodiscard]]
    bool IsPrimaryObjectsEnabled() const;

    void TreasuresEnabled(bool enable);
    void QuestNpcsEnabled(bool enable);
    void SecondaryTasksEnabled(bool enable);
    void PrimaryObjectsEnabled(bool enable);

    bool IsUsingCursorRightNow() const override;

private:
    void TaskSetTargetMap(CGameTask* task) const;
    void TaskShowMapSpot(CGameTask* task, bool show) const;

    void OnShowTaskListWnd(CUIWindow* w, void* d) const;
    void OnTask1DbClicked(CUIWindow*, void*);
    void OnTask2DbClicked(CUIWindow*, void*);
};

class CUITaskItem final : public CUIWindow
{
private:
    typedef CUIWindow inherited;

    AssociativeVector<shared_str, CUIStatic*> m_info;
    CGameTask* m_owner{};

public:
    CUITaskItem();

    virtual void OnFocusReceive();
    virtual void OnFocusLost();
    virtual void Update();
    virtual void OnMouseScroll(float iDirection);
    virtual bool OnMouseAction(float x, float y, EUIMessages mouse_action);
    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData);

    void Init(CUIXml& uiXml, LPCSTR path);
    void InitTask(CGameTask* task);
    CGameTask* OwnerTask() const { return m_owner; }

    pcstr GetDebugType() override { return "CUITaskItem"; }

public:
    bool show_hint_can{};
    bool show_hint{};

protected:
    u32 m_hint_wt;
};
