#ifndef __STDAFX_H__
#define __STDAFX_H__

#ifdef _MSC_VER
#pragma once
#endif

#include <Windows.h>
#include <iostream>
#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>
#include <memory>
#include <chrono>
#include <thread>
#include <string>



#include <d3d9.h>
#include <d3dx9.h>
#include <dwmapi.h>
#pragma comment( lib, "d3d9.lib" )
#pragma comment( lib, "d3dx9.lib" )
#pragma comment( lib, "dwmapi.lib" )

typedef D3DXVECTOR3 Vec3;
typedef D3DXVECTOR2 Vec2;

#endif