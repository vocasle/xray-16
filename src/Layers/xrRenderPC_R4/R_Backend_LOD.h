#pragma once

class R_LOD
{
public:
    R_constant* c_LOD;

public:
    explicit R_LOD(CBackend& cmd_list_in);

    void unmap() { c_LOD = 0; }
    void set_LOD(R_constant* C) { c_LOD = C; }
    void set_LOD(float LOD);

    CBackend& cmd_list;
};
