#pragma once

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>

#pragma comment (lib, "dxguid")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "d3d11")

//#define d3d11u_release(obj) if ((obj)) (obj)->lpVtbl->Release((obj))
#define d3d11u_release(obj) ((obj) ? (obj)->lpVtbl->Release((obj)) : (0))
