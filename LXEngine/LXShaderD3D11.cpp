//------------------------------------------------------------------------------------------------------
//
// This is a part of Seetron Engine
//
// Copyright (c) 2018 Nicolas Arques. All rights reserved.
//
//------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "LXShaderD3D11.h"
#include "LXDirectX11.h"
#include <d3dcompiler.h>
#include "LXPrimitiveD3D11.h"
#include "LXLogger.h"
#include "LXStatistic.h"
#include "LXMemory.h" // --- Must be the last included ---

#pragma comment(lib, "d3dcompiler.lib")

#define VSSHADER_VERSION "vs_5_0"
#define HSSHADER_VERSION "hs_5_0"
#define DSSHADER_VERSION "ds_5_0"
#define GSSHADER_VERSION "gs_5_0"
#define PSSHADER_VERSION "ps_5_0"

LXShaderD3D11::LXShaderD3D11()
{
	LX_COUNTSCOPEINC(LXShaderD3D11)
}

LXShaderD3D11::~LXShaderD3D11()
{
	LX_COUNTSCOPEDEC(LXShaderD3D11)

	LX_SAFE_RELEASE(D3D11VertexShader)
	LX_SAFE_RELEASE(D3D11HullShader);
	LX_SAFE_RELEASE(D3D11DomainShader);
	LX_SAFE_RELEASE(D3D11GeometryShader);
	LX_SAFE_RELEASE(D3D11PixelShader);

	LX_SAFE_RELEASE(D3D11VertexLayout);
}

void LXShaderD3D11::AddMacro(const char * Name, const char * Definition)
{
	ShaderMacros.push_back(LXShaderMacro(Name, Definition));
}

HRESULT CompileShaderFromBuffer(char* Buffer, const char* szEntryPoint, const char* szShaderModel, ID3DBlob** ppBlobOut)
{
	uint SrcDataSize = (uint)strlen(Buffer);

	HRESULT hr = S_OK;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
	//DWORD dwShaderFlags = D3DCOMPILE_IEEE_STRICTNESS;

#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;

	// Disable optimizations to further improve shader debugging
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompile(Buffer, SrcDataSize, nullptr, nullptr, nullptr, szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);

	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}
		return hr;
	}

	if (pErrorBlob)
		pErrorBlob->Release();

	return S_OK;
}

HRESULT LXShaderD3D11::CompileShaderFromFile(wchar_t* Filename, const char* szEntryPoint, const char* szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
	//DWORD dwShaderFlags = D3DCOMPILE_IEEE_STRICTNESS;

#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;

	// Disable optimizations to further improve shader debugging
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	// Macros

	D3D_SHADER_MACRO* D3DShadersMacross = nullptr;

	if (!ShaderMacros.empty())
	{
		D3DShadersMacross = new D3D_SHADER_MACRO[ShaderMacros.size() + 1];
		for (int i = 0; i < ShaderMacros.size(); i++)
		{
			D3DShadersMacross[i].Name = ShaderMacros[i].first.c_str();
			D3DShadersMacross[i].Definition = ShaderMacros[i].second.c_str();
		}
		D3DShadersMacross[ShaderMacros.size()].Name = NULL;
		D3DShadersMacross[ShaderMacros.size()].Definition = NULL;
	}

	// Includes

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(Filename, D3DShadersMacross, D3D_COMPILE_STANDARD_FILE_INCLUDE, szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);

	if (D3DShadersMacross)
		delete [] D3DShadersMacross;

	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			LXString StrError = reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer());
			LogE(ShaderD3D11, L"Compilation error: %s: %s", Filename, StrError.GetBuffer());
		}
		else
		{
			switch (hr)
			{
				case HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND):
					LogE(ShaderD3D11, L"File not found: %s", Filename);
					break;
				default:
					LogE(ShaderD3D11, L"Unknown error");
				break;
			}
		}
		return hr;
	}
	else
	{
		LogI(ShaderD3D11, L"Compilation OK: %s", Filename);
	}

	if (pErrorBlob)
		pErrorBlob->Release();

	return S_OK;
}

bool LXShaderD3D11::CreateVertexShader(wchar_t* Filename, const D3D11_INPUT_ELEMENT_DESC* Layout, UINT NumElements)
{
	LX_SAFE_RELEASE(D3D11PixelShader);

	ID3DBlob* pVSBlob = nullptr;
	HRESULT hr = CompileShaderFromFile(Filename, "VS", VSSHADER_VERSION, &pVSBlob);
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CompilationError;
		return false;
	}
	
	ID3D11Device* d = LXDirectX11::GetCurrentDevice();
	hr = d->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &D3D11VertexShader);
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CreationError;
		pVSBlob->Release();
		return false;
	}

	//
	// Define the input layout
	//


	// Create the input layout
	hr = d->CreateInputLayout(Layout, NumElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &D3D11VertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CreationError;
		return false;
	}

	State = EShaderD3D11State::Ok;
	return true;
}

bool LXShaderD3D11::CreateHullShader(wchar_t* Filename)
{
	// Compile the pixel shader
	ID3DBlob* pHSBlob = nullptr;
	HRESULT hr = CompileShaderFromFile(Filename, "HS", HSSHADER_VERSION, &pHSBlob);
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CompilationError;
		return false;
	}

	ID3D11Device* d = LXDirectX11::GetCurrentDevice();
	hr = d->CreateHullShader(pHSBlob->GetBufferPointer(), pHSBlob->GetBufferSize(), nullptr, &D3D11HullShader);
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CreationError;
		pHSBlob->Release();
		return false;
	}

	State = EShaderD3D11State::Ok;
	return true;
}

bool LXShaderD3D11::CreateDomainShader(wchar_t* Filename)
{
	// Compile the domain shader
	ID3DBlob* pDSBlob = nullptr;
	HRESULT hr = CompileShaderFromFile(Filename, "DS", DSSHADER_VERSION, &pDSBlob);
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CompilationError;
		return false;
	}

	ID3D11Device* d = LXDirectX11::GetCurrentDevice();
	hr = d->CreateDomainShader(pDSBlob->GetBufferPointer(), pDSBlob->GetBufferSize(), nullptr, &D3D11DomainShader);
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CreationError;
		pDSBlob->Release();
		return false;
	}

	State = EShaderD3D11State::Ok;
	return true;
}

bool LXShaderD3D11::CreateGeometryShader(wchar_t* Filename)
{
	// Compile the pixel shader
	ID3DBlob* pGSBlob = nullptr;
	HRESULT hr = CompileShaderFromFile(Filename, "GS", GSSHADER_VERSION, &pGSBlob);
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CompilationError;
		return false;
	}

	ID3D11Device* d = LXDirectX11::GetCurrentDevice();
	hr = d->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), nullptr, &D3D11GeometryShader);
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CreationError;
		pGSBlob->Release();
		return false;
	}

	State = EShaderD3D11State::Ok;
	return true;
}

bool LXShaderD3D11::CreatePixelShader(wchar_t* Filename, const char* EntryPoint)
{
	// Compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
	HRESULT hr = CompileShaderFromFile(Filename, EntryPoint/*"PS"*/, PSSHADER_VERSION, &pPSBlob);
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CompilationError;
		return false;
	}

	ID3D11Device* d = LXDirectX11::GetCurrentDevice();
	hr = d->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &D3D11PixelShader);
	if (FAILED(hr))
	{
		State = EShaderD3D11State::CreationError;
		pPSBlob->Release();
		return false;
	}

	State = EShaderD3D11State::Ok;
	return true;
}