#pragma once

#include "xrEngine/WaveForm.h"

class IReader;
class IWriter;

class ECORE_API CConstant : public xr_resource_named
{
public:
    enum
    {
        modeProgrammable = 0,
        modeWaveForm
    };

public:
    Fcolor const_float{ 0.0f, 0.0f, 0.0f, 0.0f };
    u32 const_dword{ 0 };

    u32 dwFrame{ 0 };
    u32 dwMode{ 0 };
    WaveForm R;
    WaveForm G;
    WaveForm B;
    WaveForm A;

    void set_float(float r, float g, float b, float a)
    {
        const_float.set(r, g, b, a);
        const_dword = const_float.get();
    }

    void set_float(Fcolor& c)
    {
        const_float = c;
        const_dword = const_float.get();
    }

    void set_dword(u32 c)
    {
        const_float = c;
        const_dword = c;
    }
    void Calculate();

    BOOL Similar(CConstant& C) // comare by modes and params
    {
        if (dwMode != C.dwMode)
            return FALSE;
        if (!R.Similar(C.R))
            return FALSE;
        if (!G.Similar(C.G))
            return FALSE;
        if (!B.Similar(C.B))
            return FALSE;
        if (!A.Similar(C.A))
            return FALSE;
        return TRUE;
    }
    void Load(IReader* fs);
    void Save(IWriter* fs);
};

typedef resptr_core<CConstant, resptr_base<CConstant>> ref_constant_obsolette;
