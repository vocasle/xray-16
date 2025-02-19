#include "pch_script.h"
#include "GamePersistent.h"
#include "xrCore/FMesh.hpp"
#include "xrEngine/XR_IOConsole.h"
#include "xrMaterialSystem/GameMtlLib.h"
#include "Include/xrRender/Kinematics.h"
#include "xrEngine/profiler.h"
#include "MainMenu.h"
#include "xrUICore/Cursor/UICursor.h"
#include "game_base_space.h"
#include "Level.h"
#include "ParticlesObject.h"
#include "game_base_space.h"
#include "stalker_animation_data_storage.h"
#include "stalker_velocity_holder.h"

#include "ActorEffector.h"
#include "Actor.h"
#include "Spectator.h"

#include "xrUICore/XML/UITextureMaster.h"

#include "ai_space.h"

#include "holder_custom.h"
#include "game_cl_base.h"
#include "xrServer_Objects_ALife_Monsters.h"
#include "xrServerEntities/xrServer_Object_Base.h"
#include "ui/UIGameTutorial.h"
#include "xrEngine/GameFont.h"
#include "xrEngine/PerformanceAlert.hpp"
#include "xrEngine/xr_input.h"
#include "ui/UILoadingScreen.h"
#include "AnselManager.h"
#include "xrCore/Threading/TaskManager.hpp"

#include "xrPhysics/IPHWorld.h"

#ifndef MASTER_GOLD
#include "CustomMonster.h"
#endif // MASTER_GOLD

#ifndef _EDITOR
#include "ai_debug.h"
#endif // _EDITOR

#include "xrEngine/xr_level_controller.h"

CGamePersistent::CGamePersistent()
{
    ZoneScoped;

    m_game_params.m_e_game_type = eGameIDNoGame;

    ambient_sound_next_time.reserve(32);

    m_intro_event.bind(this, &CGamePersistent::start_logo_intro);

    if ((0 != strstr(Core.Params, "-demomode ")))
    {
        string256 fname;
        cpcstr name = strstr(Core.Params, "-demomode ") + 10;
        sscanf(name, "%s", fname);
        R_ASSERT2(fname[0], "Missing filename for 'demomode'");
        Msg("- playing in demo mode '%s'", fname);
        pDemoFile = FS.r_open(fname);
        Device.seqFrame.Add(this);
        eDemoStart = Engine.Event.Handler_Attach("GAME:demo", this);
    }

    eQuickLoad = Engine.Event.Handler_Attach("Game:QuickLoad", this);
    const Fvector3* DofValue = Console->GetFVectorPtr("r2_dof");
    SetBaseDof(*DofValue);
}

CGamePersistent::~CGamePersistent()
{
    ZoneScoped;

    FS.r_close(pDemoFile);
    Device.seqFrame.Remove(this);
    Engine.Event.Handler_Detach(eDemoStart, this);
    Engine.Event.Handler_Detach(eQuickLoad, this);
}

IGame_Level* CGamePersistent::CreateLevel()
{
    return xr_new<CLevel>();
}

void CGamePersistent::DestroyLevel(IGame_Level*& lvl)
{
    xr_delete(lvl);
}

void CGamePersistent::PreStart(LPCSTR op)
{
    inherited::PreStart(op);
}

extern void clean_game_globals();
extern void init_game_globals();

void CGamePersistent::OnAppStart()
{
    ZoneScoped;

    // load game materials
    GMLib.Load(); // XXX: not ready to be loaded in parallel. Crashes on Linux, rare crashes on Windows and bugs with water became mercury on Windows.

    // init game globals
#ifndef XR_PLATFORM_WINDOWS
    init_game_globals();
#else
    const auto& initializeGlobals = TaskScheduler->AddTask(init_game_globals);
#endif

    GEnv.UI = xr_new<UICore>();
    GEnv.UI->Focus().RegisterDebuggable();
    m_pMainMenu = xr_new<CMainMenu>();
    if (GEnv.isDedicatedServer)
        m_pLoadingScreen = xr_new<NullLoadingScreen>();
    else
        m_pLoadingScreen = xr_new<UILoadingScreen>();

    inherited::OnAppStart();

#ifdef XR_PLATFORM_WINDOWS
    ansel = xr_new<AnselManager>();
    ansel->Load();
    ansel->Init();
#endif

#ifdef XR_PLATFORM_WINDOWS
    TaskScheduler->Wait(initializeGlobals);
#endif
}

void CGamePersistent::OnAppEnd()
{
    if (m_pMainMenu->IsActive())
        m_pMainMenu->Activate(false);

    xr_delete(m_pLoadingScreen);
    xr_delete(m_pMainMenu);

    inherited::OnAppEnd();

    clean_game_globals();

    xr_delete(GEnv.UI);

    GMLib.Unload();

#ifdef XR_PLATFORM_WINDOWS
    xr_delete(ansel);
#endif
}

void CGamePersistent::Start(LPCSTR op) { inherited::Start(op); }
void CGamePersistent::Disconnect()
{
    ZoneScoped;

    // destroy ambient particles
    CParticlesObject::Destroy(ambient_particles);

    inherited::Disconnect();
    // stop all played emitters
    GEnv.Sound->stop_emitters();
    m_game_params.m_e_game_type = eGameIDNoGame;
}

void CGamePersistent::OnGameStart()
{
    inherited::OnGameStart();
    UpdateGameType();
}

LPCSTR GameTypeToString(EGameIDs gt, bool bShort)
{
    switch (gt)
    {
    case eGameIDSingle:             return "single";
    case eGameIDDeathmatch:         return (bShort) ? "dm"  : "deathmatch";
    case eGameIDTeamDeathmatch:     return (bShort) ? "tdm" : "teamdeathmatch";
    case eGameIDArtefactHunt:       return (bShort) ? "ah"  : "artefacthunt";
    case eGameIDCaptureTheArtefact: return (bShort) ? "cta" : "capturetheartefact";
    case eGameIDDominationZone:     return (bShort) ? "dz"  : "dominationzone";
    case eGameIDTeamDominationZone: return (bShort) ? "tdz" : "teamdominationzone";
    default: return "---";
    }
}

LPCSTR GameTypeToStringEx(u32 gt, bool bShort)
{
    switch (gt)
    {
    case eGameIDTeamDeathmatch_SoC: gt = eGameIDTeamDeathmatch; break;
    case eGameIDArtefactHunt_SoC: gt = eGameIDArtefactHunt; break;
    }
    return GameTypeToString(static_cast<EGameIDs>(gt), bShort);
}

void CGamePersistent::UpdateGameType()
{
    inherited::UpdateGameType();

    m_game_params.m_e_game_type = ParseStringToGameType(m_game_params.m_game_type);

    if (m_game_params.m_e_game_type == eGameIDSingle)
        g_current_keygroup = _sp;
    else
        g_current_keygroup = _mp;
}

void CGamePersistent::OnGameEnd()
{
    inherited::OnGameEnd();

    xr_delete(g_stalker_animation_data_storage);
    xr_delete(g_stalker_velocity_holder);
}

void CGamePersistent::WeathersUpdate()
{
    ZoneScoped;

    if (g_pGameLevel && !GEnv.isDedicatedServer)
    {
        CActor* actor = smart_cast<CActor*>(Level().CurrentViewEntity());
        BOOL bIndoor = TRUE;
        if (actor)
            bIndoor = g_pGamePersistent->IsActorInHideout() && (actor->renderable_ROS()->get_luminocity_hemi() < 0.05f);

        if (CEnvAmbient* env_amb = Environment().CurrentEnv.env_ambient)
        {
            CEnvAmbient::SSndChannelVec& vec = env_amb->get_snd_channels();

            auto I = vec.cbegin();
            const auto E = vec.cend();

            for (size_t idx = 0; I != E; ++I, ++idx)
            {
                CEnvAmbient::SSndChannel& ch = **I;

                if (ambient_sound_next_time[idx] == 0) // first
                {
                    ambient_sound_next_time[idx] = Device.dwTimeGlobal + ch.get_rnd_sound_first_time();
                }
                else if (Device.dwTimeGlobal > ambient_sound_next_time[idx])
                {
                    ref_sound& snd = ch.get_rnd_sound();

                    Fvector pos;
                    const float angle = ::Random.randF(PI_MUL_2);
                    pos.x = _cos(angle);
                    pos.y = 0;
                    pos.z = _sin(angle);
                    pos.normalize().mul(ch.get_rnd_sound_dist()).add(Device.vCameraPosition);
                    pos.y += 10.f;
                    snd.play_at_pos(nullptr, pos);

#ifdef DEBUG
                    if (!snd._handle() && !Engine.Sound.IsSoundEnabled())
                        continue;
#endif // DEBUG

                    VERIFY(snd._handle());
                    const u32 _length_ms = iFloor(snd.get_length_sec() * 1000.0f);
                    ambient_sound_next_time[idx] = Device.dwTimeGlobal + _length_ms + ch.get_rnd_sound_time();
                    //Msg("- Playing ambient sound channel [%s] file[%s]", ch.m_load_section.c_str(), snd._handle()->file_name());
                }
            }

            // start effect
            if ((FALSE == bIndoor) && (0 == ambient_particles) && Device.dwTimeGlobal > ambient_effect_next_time)
            {
                if (CEnvAmbient::SEffect* eff = env_amb->get_rnd_effect(); eff)
                {
                    Environment().wind_gust_factor = eff->wind_gust_factor;
                    ambient_effect_next_time = Device.dwTimeGlobal + env_amb->get_rnd_effect_time();
                    ambient_effect_stop_time = Device.dwTimeGlobal + eff->life_time;
                    ambient_effect_wind_start = Device.fTimeGlobal;
                    ambient_effect_wind_in_time = Device.fTimeGlobal + eff->wind_blast_in_time;
                    ambient_effect_wind_end = Device.fTimeGlobal + eff->life_time / 1000.f;
                    ambient_effect_wind_out_time =
                        Device.fTimeGlobal + eff->life_time / 1000.f + eff->wind_blast_out_time;
                    ambient_effect_wind_on = true;

                    ambient_particles = CParticlesObject::Create(eff->particles.c_str(), FALSE, false);
                    Fvector pos;
                    pos.add(Device.vCameraPosition, eff->offset);
                    ambient_particles->play_at_pos(pos);
                    if (eff->sound._handle())
                        eff->sound.play_at_pos(0, pos);

                    Environment().wind_blast_strength_start_value = Environment().wind_strength_factor;
                    Environment().wind_blast_strength_stop_value = eff->wind_blast_strength;

                    if (Environment().wind_blast_strength_start_value == 0.f)
                    {
                        Environment().wind_blast_start_time.set(
                            0.f, eff->wind_blast_direction.x, eff->wind_blast_direction.y, eff->wind_blast_direction.z);
                    }
                    else
                    {
                        Environment().wind_blast_start_time.set(0.f, Environment().wind_blast_direction.x,
                            Environment().wind_blast_direction.y, Environment().wind_blast_direction.z);
                    }
                    Environment().wind_blast_stop_time.set(
                        0.f, eff->wind_blast_direction.x, eff->wind_blast_direction.y, eff->wind_blast_direction.z);
                }
            }
        }
        if (Device.fTimeGlobal >= ambient_effect_wind_start && Device.fTimeGlobal <= ambient_effect_wind_in_time &&
            ambient_effect_wind_on)
        {
            const float delta = ambient_effect_wind_in_time - ambient_effect_wind_start;
            float t;
            if (delta != 0.f)
            {
                const float cur_in = Device.fTimeGlobal - ambient_effect_wind_start;
                t = cur_in / delta;
            }
            else
            {
                t = 0.f;
            }
            Environment().wind_blast_current.slerp(
                Environment().wind_blast_start_time, Environment().wind_blast_stop_time, t);

            Environment().wind_blast_direction.set(Environment().wind_blast_current.x,
                Environment().wind_blast_current.y, Environment().wind_blast_current.z);
            Environment().wind_strength_factor = Environment().wind_blast_strength_start_value +
                t * (Environment().wind_blast_strength_stop_value - Environment().wind_blast_strength_start_value);
        }

        // stop if time exceed or indoor
        if (bIndoor || Device.dwTimeGlobal >= ambient_effect_stop_time)
        {
            if (ambient_particles)
                ambient_particles->Stop();

            Environment().wind_gust_factor = 0.f;
        }

        if (Device.fTimeGlobal >= ambient_effect_wind_end && ambient_effect_wind_on)
        {
            Environment().wind_blast_strength_start_value = Environment().wind_strength_factor;
            Environment().wind_blast_strength_stop_value = 0.f;

            ambient_effect_wind_on = false;
        }

        if (Device.fTimeGlobal >= ambient_effect_wind_end && Device.fTimeGlobal <= ambient_effect_wind_out_time)
        {
            const float delta = ambient_effect_wind_out_time - ambient_effect_wind_end;
            float t;
            if (delta != 0.f)
            {
                const float cur_in = Device.fTimeGlobal - ambient_effect_wind_end;
                t = cur_in / delta;
            }
            else
            {
                t = 0.f;
            }
            Environment().wind_strength_factor = Environment().wind_blast_strength_start_value +
                t * (Environment().wind_blast_strength_stop_value - Environment().wind_blast_strength_start_value);
        }
        if (Device.fTimeGlobal > ambient_effect_wind_out_time && ambient_effect_wind_out_time != 0.f)
        {
            Environment().wind_strength_factor = 0.0;
        }

        // if particles not playing - destroy
        if (ambient_particles && !ambient_particles->IsPlaying())
            CParticlesObject::Destroy(ambient_particles);
    }
}

bool allow_intro()
{
    if ((0 != strstr(Core.Params, "-nointro")))
        return false;

    return true;
}

bool allow_game_intro()
{
    return !strstr(Core.Params, "-nogameintro");
}

void CGamePersistent::start_logo_intro()
{
    const bool notLoadingLevel = xr_strlen(m_game_params.m_game_or_spawn) == 0 && g_pGameLevel == nullptr;
    if (!allow_intro())
    {
        m_intro_event = nullptr;
        if (notLoadingLevel)
            m_pMainMenu->Activate(true);
        return;
    }

    if (Device.dwPrecacheFrame == 0)
    {
        m_intro_event = nullptr;
        if (!GEnv.isDedicatedServer && notLoadingLevel)
        {
            VERIFY(NULL == m_intro);
            m_intro = xr_new<CUISequencer>();
            m_intro->m_on_destroy_event.bind(this, &CGamePersistent::update_logo_intro);
            m_intro->Start("intro_logo");
        }
    }
}

void CGamePersistent::update_logo_intro()
{
    xr_delete(m_intro);
    m_pMainMenu->Activate(true);
}

extern int g_keypress_on_start;
void CGamePersistent::game_loaded()
{
    if (Device.dwPrecacheFrame <= 2)
    {
        m_intro_event = nullptr;
        if (g_pGameLevel && g_pGameLevel->bReady && g_keypress_on_start &&
            load_screen_renderer.NeedsUserInput() && m_game_params.m_e_game_type == eGameIDSingle)
        {
            VERIFY(NULL == m_intro);
            m_intro = xr_new<CUISequencer>();
            m_intro->m_on_destroy_event.bind(this, &CGamePersistent::update_game_loaded);
            if (!m_intro->Start("game_loaded"))
                m_intro->Destroy();
        }
    }
}

void CGamePersistent::update_game_loaded()
{
    xr_delete(m_intro);
    load_screen_renderer.Stop();
    start_game_intro();
}

void CGamePersistent::start_game_intro()
{
    if (!allow_game_intro())
    {
        return;
    }

    if (g_pGameLevel && g_pGameLevel->bReady && Device.dwPrecacheFrame <= 2)
    {
        if (0 == xr_stricmp(m_game_params.m_new_or_load, "new"))
        {
            VERIFY(NULL == m_intro);
            Log("intro_start intro_game");
            m_intro = xr_new<CUISequencer>();
            m_intro->m_on_destroy_event.bind(this, &CGamePersistent::update_game_intro);
            m_intro->Start("intro_game");
        }
    }
}

void CGamePersistent::update_game_intro()
{
    xr_delete(m_intro);
}

extern CUISequencer* g_tutorial;
extern CUISequencer* g_tutorial2;

void CGamePersistent::OnFrame()
{
    ZoneScoped;

    if (Device.dwPrecacheFrame == 5 && m_intro_event.empty())
    {
        LoadTitle();
        m_intro_event.bind(this, &CGamePersistent::game_loaded);
    }

    if (g_tutorial2)
    {
        g_tutorial2->Destroy();
        xr_delete(g_tutorial2);
    }

    if (g_tutorial && !g_tutorial->IsActive())
    {
        xr_delete(g_tutorial);
    }
    if (0 == Device.dwFrame % 200)
        CUITextureMaster::FreeCachedShaders();

#ifdef DEBUG
    ++m_frame_counter;
#endif
    if (!GEnv.isDedicatedServer)
    {
        if (!m_intro_event.empty())
            m_intro_event();
        else if (!m_intro)
        {
            if (Device.dwPrecacheFrame == 0)
                load_screen_renderer.Stop();
        }
    }
    if (!m_pMainMenu->IsActive())
        m_pMainMenu->DestroyInternal(false);

    if (!g_pGameLevel)
        return;
    if (!g_pGameLevel->bReady)
        return;

    g_pGameLevel->WorldRendered(false);

    if (Device.Paused())
    {
        if (Level().IsDemoPlay())
        {
            CSpectator* tmp_spectr = smart_cast<CSpectator*>(Level().CurrentControlEntity());
            if (tmp_spectr)
            {
                tmp_spectr->UpdateCL(); // updating spectator in pause (pause ability of demo play)
            }
        }
#ifndef MASTER_GOLD
        if (Level().CurrentViewEntity() && IsGameTypeSingle())
        {
            if (!g_actor || (g_actor->ID() != Level().CurrentViewEntity()->ID()))
            {
                CCustomMonster* custom_monster = smart_cast<CCustomMonster*>(Level().CurrentViewEntity());
                if (custom_monster) // can be spectator in multiplayer
                    custom_monster->UpdateCamera();
            }
            else
            {
                CCameraBase* C = NULL;
                if (g_actor)
                {
                    if (!Actor()->Holder())
                        C = Actor()->cam_Active();
                    else
                        C = Actor()->Holder()->Camera();

                    Actor()->Cameras().UpdateFromCamera(C);
                    Actor()->Cameras().ApplyDevice();

                    if (psActorFlags.test(AF_NO_CLIP))
                    {
#ifdef DEBUG
                        Actor()->SetDbgUpdateFrame(0);
                        Actor()->GetSchedulerData().dbg_update_shedule = 0;
#endif
                        Device.dwTimeDelta = 0;
                        Device.fTimeDelta = 0.01f;
                        Actor()->UpdateCL();
                        Actor()->shedule_Update(0);
#ifdef DEBUG
                        Actor()->SetDbgUpdateFrame(0);
                        Actor()->GetSchedulerData().dbg_update_shedule = 0;
#endif

                        CSE_Abstract* e = Level().Server->ID_to_entity(Actor()->ID());
                        VERIFY(e);
                        CSE_ALifeCreatureActor* s_actor = smart_cast<CSE_ALifeCreatureActor*>(e);
                        VERIFY(s_actor);
                        xr_vector<u16>::iterator it = s_actor->children.begin();
                        for (; it != s_actor->children.end(); ++it)
                        {
                            IGameObject* obj = Level().Objects.net_Find(*it);
                            if (obj && Engine.Sheduler.Registered(obj))
                            {
#ifdef DEBUG
                                obj->GetSchedulerData().dbg_update_shedule = 0;
                                obj->SetDbgUpdateFrame(0);
#endif
                                obj->shedule_Update(0);
                                obj->UpdateCL();
#ifdef DEBUG
                                obj->GetSchedulerData().dbg_update_shedule = 0;
                                obj->SetDbgUpdateFrame(0);
#endif
                            }
                        }
                    }
                }
            }
        }
#else // MASTER_GOLD
        if (g_actor && IsGameTypeSingle())
        {
            CCameraBase* C = NULL;
            if (!Actor()->Holder())
                C = Actor()->cam_Active();
            else
                C = Actor()->Holder()->Camera();

            Actor()->Cameras().UpdateFromCamera(C);
            Actor()->Cameras().ApplyDevice();
        }
#endif // MASTER_GOLD
    }
    inherited::OnFrame();

    if (!Device.Paused())
    {
        Engine.Sheduler.Update();
    }

    // update weathers ambient
    if (!Device.Paused())
        WeathersUpdate();

    if (0 != pDemoFile)
    {
        if (Device.dwTimeGlobal > uTime2Change)
        {
            // Change level + play demo
            if (pDemoFile->elapsed() < 3)
                pDemoFile->seek(0); // cycle

            // Read params
            string512 params;
            pDemoFile->r_string(params, sizeof(params));
            string256 o_server, o_client, o_demo;
            u32 o_time;
            sscanf(params, "%[^,],%[^,],%[^,],%d", o_server, o_client, o_demo, &o_time);

            // Start _new level + demo
            Engine.Event.Defer("KERNEL:disconnect");
            Engine.Event.Defer("KERNEL:start", size_t(xr_strdup(_Trim(o_server))), size_t(xr_strdup(_Trim(o_client))));
            Engine.Event.Defer("GAME:demo", size_t(xr_strdup(_Trim(o_demo))), u64(o_time));
            uTime2Change = 0xffffffff; // Block changer until Event received
        }
    }

#ifdef DEBUG
    if ((m_last_stats_frame + 1) < m_frame_counter)
        profiler().clear();
#endif
    UpdateDof();
}

#include "game_sv_single.h"
#include "xrServer.h"
#include "UIGameCustom.h"
#include "ui/UIMainIngameWnd.h"
#include "ui/UIPdaWnd.h"

void CGamePersistent::OnEvent(EVENT E, u64 P1, u64 P2)
{
    ZoneScoped;

    if (E == eQuickLoad)
    {
        if (Device.Paused())
            Device.Pause(FALSE, TRUE, TRUE, "eQuickLoad");

        if (CurrentGameUI())
        {
            CurrentGameUI()->HideShownDialogs();
            CurrentGameUI()->UIMainIngameWnd->reset_ui();
            CurrentGameUI()->GetPdaMenu().Reset();
        }

        if (g_tutorial)
            g_tutorial->Stop();

        if (g_tutorial2)
            g_tutorial2->Stop();

        pstr saved_name = (pstr)(P1);

        Level().remove_objects();
        game_sv_Single* game = smart_cast<game_sv_Single*>(Level().Server->GetGameState());
        R_ASSERT(game);
        game->restart_simulator(saved_name);
        xr_free(saved_name);
        return;
    }
    if (E == eDemoStart)
    {
        string256 cmd;
        pstr demo = pstr(P1);
        xr_sprintf(cmd, "demo_play %s", demo);
        Console->Execute(cmd);
        xr_free(demo);
        uTime2Change = Device.TimerAsync() + u32(P2) * 1000;
        return;
    }
    inherited::OnEvent(E, P1, P2);
}

void CGamePersistent::DumpStatistics(IGameFont& font, IPerformanceAlert* alert)
{
    IGame_Persistent::DumpStatistics(font, alert);
    if (physics_world())
        physics_world()->DumpStatistics(font, alert);

#ifdef DEBUG
#ifndef _EDITOR
    const Fvector2 prev = font.GetPosition();
    font.OutSet(400, 120);
    m_last_stats_frame = m_frame_counter;
    profiler().show_stats(font, psAI_Flags.test(aiStats));
    font.OutSet(prev.x, prev.y);
#endif
#endif
}

static BOOL bRestorePause = FALSE;
static BOOL bEntryFlag = TRUE;

void CGamePersistent::OnAppActivate()
{
    if (psDeviceFlags.test(rsAlwaysActive))
        return;

    bool bIsMP = (g_pGameLevel && Level().game && GameID() != eGameIDSingle);
    bIsMP &= !Device.Paused();

    if (!bIsMP)
        Device.Pause(FALSE, !bRestorePause, TRUE, "CGP::OnAppActivate");
    else
        Device.Pause(FALSE, TRUE, TRUE, "CGP::OnAppActivate MP");

    bEntryFlag = TRUE;
}

void CGamePersistent::OnAppDeactivate()
{
    if (!bEntryFlag || psDeviceFlags.test(rsAlwaysActive))
        return;

    bool bIsMP = (g_pGameLevel && Level().game && GameID() != eGameIDSingle);

    bRestorePause = FALSE;

    if (!bIsMP)
    {
        bRestorePause = Device.Paused();
        Device.Pause(TRUE, TRUE, TRUE, "CGP::OnAppDeactivate");
    }
    else
    {
        Device.Pause(TRUE, FALSE, TRUE, "CGP::OnAppDeactivate MP");
    }
    bEntryFlag = FALSE;
}

bool CGamePersistent::OnRenderPPUI_query()
{
    return MainMenu()->OnRenderPPUI_query();
    // enable PP or not
}

void CGamePersistent::OnRenderPPUI_main()
{
    // always
    MainMenu()->OnRenderPPUI_main();
}

void CGamePersistent::OnRenderPPUI_PP() { MainMenu()->OnRenderPPUI_PP(); }

bool CGamePersistent::CanBePaused() { return IsGameTypeSingle() || (g_pGameLevel && Level().IsDemoPlay()); }
void CGamePersistent::SetPickableEffectorDOF(bool bSet)
{
    m_bPickableDOF = bSet;
    if (!bSet)
        RestoreEffectorDOF();
}

void CGamePersistent::GetCurrentDof(Fvector3& dof) { dof = m_dof[1]; }
void CGamePersistent::SetBaseDof(const Fvector3& dof) { m_dof[0] = m_dof[1] = m_dof[2] = m_dof[3] = dof; }
void CGamePersistent::SetEffectorDOF(const Fvector& needed_dof)
{
    if (m_bPickableDOF)
        return;
    m_dof[0] = needed_dof;
    m_dof[2] = m_dof[1]; // current
}

void CGamePersistent::RestoreEffectorDOF() { SetEffectorDOF(m_dof[3]); }
#include "HUDManager.h"

//	m_dof		[4];	// 0-dest 1-current 2-from 3-original
void CGamePersistent::UpdateDof()
{
    if (m_bPickableDOF)
    {
        static float diff_far = pSettings->read_if_exists<float>("zone_pick_dof", "far", 70.0f);
        static float diff_near = pSettings->read_if_exists<float>("zone_pick_dof", "near", -70.0f);

        Fvector pick_dof;
        pick_dof.y = HUD().GetCurrentRayQuery().range;
        pick_dof.x = pick_dof.y + diff_near;
        pick_dof.z = pick_dof.y + diff_far;
        m_dof[0] = pick_dof;
        m_dof[2] = m_dof[1]; // current
    }
    if (m_dof[1].similar(m_dof[0]))
        return;

    float td = Device.fTimeDelta;
    Fvector diff;
    diff.sub(m_dof[0], m_dof[2]);
    diff.mul(td / 0.2f); // 0.2 sec
    m_dof[1].add(diff);
    (m_dof[0].x < m_dof[2].x) ? clamp(m_dof[1].x, m_dof[0].x, m_dof[2].x) : clamp(m_dof[1].x, m_dof[2].x, m_dof[0].x);
    (m_dof[0].y < m_dof[2].y) ? clamp(m_dof[1].y, m_dof[0].y, m_dof[2].y) : clamp(m_dof[1].y, m_dof[2].y, m_dof[0].y);
    (m_dof[0].z < m_dof[2].z) ? clamp(m_dof[1].z, m_dof[0].z, m_dof[2].z) : clamp(m_dof[1].z, m_dof[2].z, m_dof[0].z);
}

void CGamePersistent::OnSectorChanged(IRender_Sector::sector_id_t sector)
{
    if (CurrentGameUI())
        CurrentGameUI()->UIMainIngameWnd->OnSectorChanged(sector);
}

void CGamePersistent::OnAssetsChanged()
{
    IGame_Persistent::OnAssetsChanged();
    StringTable().rescan();
}
