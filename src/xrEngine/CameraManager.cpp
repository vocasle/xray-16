// CameraManager.cpp: implementation of the CCameraManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "IGame_Level.h"
#include "IGame_Persistent.h"

#include "Environment.h"
#include "CameraBase.h"
#include "CameraManager.h"
#include "Effector.h"
#include "EffectorPP.h"

#include "GameFont.h"
#include "Render.h"

float psCamInert = 0.f;
float psCamSlideInert = 0.25f;

SPPInfo pp_identity;
SPPInfo pp_zero;

CCameraManager::CCameraManager(bool bApplyOnUpdate)
{
#ifdef DEBUG
    dbg_upd_frame = 0;
#endif

    m_bAutoApply = bApplyOnUpdate;

    pp_identity.blur = 0;
    pp_identity.gray = 0;
    pp_identity.duality.h = 0;
    pp_identity.duality.v = 0;
    pp_identity.noise.intensity = 0;
    pp_identity.noise.grain = 1.0f;
    pp_identity.noise.fps = 30;
    pp_identity.color_base.set(.5f, .5f, .5f);
    pp_identity.color_gray.set(.333f, .333f, .333f);
    pp_identity.color_add.set(0, 0, 0);

    pp_zero.blur = pp_zero.gray = pp_zero.duality.h = pp_zero.duality.v = 0.0f;
    pp_zero.noise.intensity = 0;
    pp_zero.noise.grain = 0.0f;
    pp_zero.noise.fps = 0.0f;
    pp_zero.color_base.set(0, 0, 0);
    pp_zero.color_gray.set(0, 0, 0);
    pp_zero.color_add.set(0, 0, 0);

    pp_affected = pp_identity;
}

CCameraManager::~CCameraManager()
{
    for (auto it = m_EffectorsCam.begin(); it != m_EffectorsCam.end(); ++it)
        xr_delete(*it);

    for (auto it = m_EffectorsPP.begin(); it != m_EffectorsPP.end(); ++it)
        xr_delete(*it);
}

CEffectorCam* CCameraManager::GetCamEffector(ECamEffectorType type)
{
    for (auto it = m_EffectorsCam.begin(); it != m_EffectorsCam.end(); ++it)
        if ((*it)->eType == type)
        {
            return *it;
        }
    return 0;
}

CEffectorCam* CCameraManager::AddCamEffector(CEffectorCam* ef)
{
    m_EffectorsCam_added_deffered.push_back(ef);
    return m_EffectorsCam_added_deffered.back();
}

void CCameraManager::UpdateDeffered()
{
    for (auto& effector : m_EffectorsCam_added_deffered)
    {
        RemoveCamEffector(effector->eType);

        if (effector->AbsolutePositioning())
            m_EffectorsCam.push_front(effector);
        else
            m_EffectorsCam.push_back(effector);
    }

    m_EffectorsCam_added_deffered.clear();
}

void CCameraManager::RemoveCamEffector(ECamEffectorType type)
{
    for (auto it = m_EffectorsCam.begin(); it != m_EffectorsCam.end(); ++it)
        if ((*it)->eType == type)
        {
            OnEffectorReleased(*it);
            m_EffectorsCam.erase(it);
            return;
        }
}

CEffectorPP* CCameraManager::GetPPEffector(EEffectorPPType type)
{
    for (auto& effector : m_EffectorsPP)
        if (effector->Type() == type)
            return effector;
    return nullptr;
}

ECamEffectorType CCameraManager::RequestCamEffectorId()
{
    ECamEffectorType index = (ECamEffectorType)effCustomEffectorStartID;
    for (; GetCamEffector(index); index = (ECamEffectorType)(index + 1))
    {
        ;
    }
    return index;
}

EEffectorPPType CCameraManager::RequestPPEffectorId()
{
    EEffectorPPType index = (EEffectorPPType)effCustomEffectorStartID;
    for (; GetPPEffector(index); index = (EEffectorPPType)(index + 1))
    {
        ;
    }
    return index;
}

CEffectorPP* CCameraManager::AddPPEffector(CEffectorPP* ef)
{
    RemovePPEffector(ef->Type());
    m_EffectorsPP.push_back(ef);
    return m_EffectorsPP.back();
}

void CCameraManager::RemovePPEffector(EEffectorPPType type)
{
    for (auto it = m_EffectorsPP.begin(); it != m_EffectorsPP.end(); ++it)
        if ((*it)->Type() == type)
        {
            if ((*it)->FreeOnRemove())
            {
                OnEffectorReleased(*it);
                // xr_delete (*it);
            }
            m_EffectorsPP.erase(it);
            return;
        }
}

void CCameraManager::OnEffectorReleased(SBaseEffector* e)
{
    if (!e->m_on_b_remove_callback.empty())
        e->m_on_b_remove_callback();

    xr_delete(e);
}

void CCameraManager::UpdateFromCamera(const CCameraBase* C)
{
    Update(C->vPosition, C->vDirection, C->vNormal, C->f_fov, C->f_aspect,
        g_pGamePersistent->Environment().CurrentEnv.far_plane, C->m_Flags.flags);
}

void CCameraManager::Update(const Fvector& P, const Fvector& D, const Fvector& N, float fFOV_Dest, float fASPECT_Dest,
    float fFAR_Dest, u32 flags)
{
    ZoneScoped;
#ifdef DEBUG
    if (!Device.Paused())
    {
        VERIFY(dbg_upd_frame != Device.dwFrame); // already updated !!!
        dbg_upd_frame = Device.dwFrame;
    }
#endif // DEBUG
    // camera
    if (flags & CCameraBase::flPositionRigid)
        m_cam_info.p.set(P);
    else
        m_cam_info.p.inertion(P, psCamInert);
    if (flags & CCameraBase::flDirectionRigid)
    {
        m_cam_info.d.set(D);
        m_cam_info.n.set(N);
    }
    else
    {
        m_cam_info.d.inertion(D, psCamInert);
        m_cam_info.n.inertion(N, psCamInert);
    }

    // Normalize
    m_cam_info.d.normalize();
    m_cam_info.n.normalize();
    m_cam_info.r.crossproduct(m_cam_info.n, m_cam_info.d);
    m_cam_info.n.crossproduct(m_cam_info.d, m_cam_info.r);

    float aspect = Device.fHeight_2 / Device.fWidth_2;
    float src = 10 * Device.fTimeDelta;
    clamp(src, 0.f, 1.f);
    float dst = 1 - src;
    m_cam_info.fFov = m_cam_info.fFov * dst + fFOV_Dest * src;
    m_cam_info.fNear = VIEWPORT_NEAR;
    m_cam_info.fFar = m_cam_info.fFar * dst + fFAR_Dest * src;
    m_cam_info.fAspect = m_cam_info.fAspect * dst + (fASPECT_Dest * aspect) * src;
    m_cam_info.dont_apply = false;

    UpdateCamEffectors();

    UpdatePPEffectors();

    if (!m_cam_info.dont_apply && m_bAutoApply)
        ApplyDevice();

    UpdateDeffered();
}

bool CCameraManager::ProcessCameraEffector(CEffectorCam* eff)
{
    // Do NOT delete effector here! It's unsafe because:
    // 1. Leads to failed iterators in UpdateCamEffectors
    // 2. Child classes with overrided ProcessCameraEffector would be surprised if eff becames invalid pointer
    // The best way - return 'false' when the effector should be deleted, and delete it in ProcessCameraEffector

    bool res = false;
    if (eff->Valid() && eff->ProcessCam(m_cam_info))
    {
        res = true;
    }
    else if (eff->AllowProcessingIfInvalid())
    {
        eff->ProcessIfInvalid(m_cam_info);
    }
    return res;
}

void CCameraManager::UpdateCamEffectors()
{
    if (m_EffectorsCam.empty())
        return;

    auto r_it = m_EffectorsCam.rbegin();
    while (r_it != m_EffectorsCam.rend())
    {
        if (ProcessCameraEffector(*r_it))
            ++r_it;
        else
        {
            // Dereferencing reverse iterator returns previous element of the list, r_it.base() returns current element
            // So, we should use base()-1 iterator to delete just processed element. 'Previous' element would be
            // automatically changed after deletion, so r_it would dereferencing to another value, no need to change it
            OnEffectorReleased(*r_it);
            auto r_to_del = r_it.base();
            m_EffectorsCam.erase(--r_to_del);
        }
    }

    m_cam_info.d.normalize();
    m_cam_info.n.normalize();
    m_cam_info.r.crossproduct(m_cam_info.n, m_cam_info.d);
    m_cam_info.n.crossproduct(m_cam_info.d, m_cam_info.r);
}

void CCameraManager::UpdatePPEffectors()
{
    pp_affected.validate("before applying pp");

    int _count = 0;
    if (m_EffectorsPP.size())
    {
        bool b = false;
        pp_affected = pp_identity;
        for (int i = m_EffectorsPP.size() - 1; i >= 0; --i)
        {
            CEffectorPP* eff = m_EffectorsPP[i];
            SPPInfo l_PPInf = pp_zero;
            if (eff->Valid() && eff->Process(l_PPInf))
            {
                ++_count;
                if (!b)
                {
                    pp_affected.add(l_PPInf);
                    pp_affected.sub(pp_identity);
                    pp_affected.validate("in cycle");
                }
                if (!eff->bOverlap)
                {
                    b = true;
                    pp_affected = l_PPInf;
                }
            }
            else
                RemovePPEffector(eff->Type());
        }
        if (0 == _count)
            pp_affected = pp_identity;
        else
            pp_affected.normalize();
    }
    else
    {
        pp_affected = pp_identity;
    }

    if (!positive(pp_affected.noise.grain))
        pp_affected.noise.grain = pp_identity.noise.grain;

    pp_affected.validate("after applying pp");
}

void CCameraManager::ApplyDevice()
{
    ZoneScoped;
    // Device params
    Device.mView.build_camera_dir(m_cam_info.p, m_cam_info.d, m_cam_info.n);

    Device.vCameraPosition.set(m_cam_info.p);
    Device.vCameraDirection.set(m_cam_info.d);
    Device.vCameraTop.set(m_cam_info.n);
    Device.vCameraRight.set(m_cam_info.r);

    // projection
    Device.fFOV = m_cam_info.fFov;
    Device.fASPECT = m_cam_info.fAspect;
    Device.mProject.build_projection(deg2rad(m_cam_info.fFov), m_cam_info.fAspect, m_cam_info.fNear, m_cam_info.fFar);

    // Apply offset required for Nvidia Ansel
    Device.mProject._31 = -m_cam_info.offsetX;
    Device.mProject._32 = -m_cam_info.offsetY;

    if (g_pGamePersistent && g_pGamePersistent->m_pMainMenu->IsActive())
        ResetPP();
    else
    {
        pp_affected.validate("apply device");
        // postprocess
        clamp(pp_affected.noise.grain, EPS_L, 1000.0f);
        GEnv.Render->SetPostProcessParams(pp_affected);
    }
}

void CCameraManager::ResetPP()
{
    SPPInfo params = pp_identity;
    params.cm_influence = 0.0f;
    params.cm_interpolate = 1.0f;
    params.cm_tex1 = "";
    params.cm_tex2 = "";
    GEnv.Render->SetPostProcessParams(params);
}

void CCameraManager::Dump()
{
    Fmatrix mInvCamera;
    mInvCamera.invert(Device.mView);

    const Fvector right{ mInvCamera._11, mInvCamera._12, mInvCamera._13 };
    const Fvector normal{ mInvCamera._21, mInvCamera._22, mInvCamera._23 };
    const Fvector direction{ mInvCamera._31, mInvCamera._32, mInvCamera._33 };
    const Fvector position{ mInvCamera._41, mInvCamera._42, mInvCamera._43 };

    Log("CCameraManager::Dump::vPosition = ", position);
    Log("CCameraManager::Dump::vDirection = ", direction);
    Log("CCameraManager::Dump::vNormal = ", normal);
    Log("CCameraManager::Dump::vRight = ", right);
}
