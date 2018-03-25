//------------------------------------------------------------------------------------------------------
//
// This is a part of Seetron Engine
//
// Copyright (c) 2018 Nicolas Arques. All rights reserved.
//
//------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "LXAnimationManager.h"
#include "LXConsoleManager.h"
#include "LXController.h"
#include "LXCore.h"
#include "LXMaterial.h"
#include "LXPrimitiveD3D11.h"
#include "LXProject.h"
#include "LXRenderClusterManager.h"
#include "LXRenderCommandList.h"
#include "LXRenderPassShadow.h"
#include "LXRenderPassDownsample.h"
#include "LXRenderPassDynamicTexture.h"
#include "LXRenderPassGBuffer.h"
#include "LXRenderPassAux.h"
#include "LXRenderPassLighting.h"
#include "LXRenderPassToneMapping.h"
#include "LXRenderer.h"
#include "LXScene.h"
#include "LXShaderManager.h"
#include "LXThread.h"
#include "LXTimeD3D11.h"
#include "LXTrackBallCameraManipulator.h"
#include "LXViewport.h"
#include "LXStatManager.h"
#include "LXRenderPassTransparency.h"
#include "LXMemory.h" // --- Must be the last included ---

//------------------------------------------------------------------------------------------------------
// Console commands and Settings
//------------------------------------------------------------------------------------------------------

// Binded on variable.
bool ShowFPS = false;
LXConsoleCommandT<bool> CCShowFPS(L"ShowFPS", &ShowFPS);

bool ShowWireframe = false;
LXConsoleCommandT<bool> CCShowWireframe(L"ShowWireframe", &ShowWireframe);

// Request for reseting the shaders
bool gResetShaders = false; 
LXConsoleCommandT<bool> CCResetShaders(L"ResetShaders", &gResetShaders);

// Request for changing DrawImmmediate parameter
bool gToogleDrawImmediate = false;
LXConsoleCommandT<bool> CCToogleDrawImmediate(L"ToggleDrawImmediate", &gToogleDrawImmediate);

// Binded on variable and set from an INI file.
//LXConsoleCommandT<bool> CSet_NoOverwriteConstantBuffer(L"Engine.ini", L"D3D111", L"NoOverwriteConstantBuffer", L"false", &NoOverwriteConstantBuffer);

// bool gShowDynamicTexture = false;
//LXConsoleCommandT<bool> CCShowDynamicTexture(L"ShowDynamicTexture", &gShowDynamicTexture);

// Retrieve the viewport resolution
LXConsoleCommandNoArg CCResolution(L"Resolution", []()
{
	LogI(Viewport, L"Viewport size: %ix%i", GetCore().GetViewport()->GetWidth(), GetCore().GetViewport()->GetHeight());
});

LXConsoleCommand1S CCShowBuffer(L"ShowBuffer", [](const LXString& inBufferName)
{
	if (inBufferName.IsEmpty())
	{
		// List the buffers
		LogI(Renderer, L"ShowBuffer [BufferName]");
		LogI(Renderer, L"Buffers:");
		LogI(Renderer, L"-Default");
		LogI(Renderer, L"-Depth");
		LogI(Renderer, L"-Color");
		LogI(Renderer, L"-Normal");
		LogI(Renderer, L"-Shadow");
	}
	else
	{
		LXString BufferName = inBufferName.ToLower();
		if (BufferName == L"depth")
			GetCore().GetRenderer()->ShowBuffer = EShowBuffer::Depth;
		else if (BufferName == L"color")
			GetCore().GetRenderer()->ShowBuffer = EShowBuffer::Color;
		else if (BufferName == L"normal")
			GetCore().GetRenderer()->ShowBuffer = EShowBuffer::Normal;
		else if (BufferName == L"shadow")
			GetCore().GetRenderer()->ShowBuffer = EShowBuffer::Shadow;
		else
			GetCore().GetRenderer()->ShowBuffer = EShowBuffer::Default;
	}
});

bool LXRenderer::gUseRenderThread = true;

//------------------------------------------------------------------------------------------------------

int LXRenderer::RenderFunc(void* pData)
{
	RenderThread = GetCurrentThreadId();
	LXRenderer* Renderer = (LXRenderer*)pData;
	Renderer->Run();
	RenderThread = 0;
	return 0;
}

LXRenderer::LXRenderer(HWND hWND):_hWND(hWND)
{
	if (gUseRenderThread)
	{
		Thread = new LXThread();
		EventBeginFrame = new LXSyncEvent(L"Begin", FALSE);
		EventEndFrame = new LXSyncEvent(L"End", TRUE);
		Thread->Run(&RenderFunc, this);
	}

	GetCore().SetRenderer(this);
	GetLogger().Register(nullptr, [this](ELogType LogType, const wchar_t* Messsage)
	{
		ConsoleBuffer.push_back(Messsage);
		if (ConsoleBuffer.size() > (CONSOLE_MAX_LINE - 1))
			ConsoleBuffer.pop_front();
	});
}

LXRenderer::~LXRenderer()
{
	GetLogger().Unregister(nullptr);
	GetCore().SetRenderer(nullptr);
	if (gUseRenderThread)
	{
		ExitRenderThread = true;
		EventBeginFrame->SetEvent();
		Thread->Wait();
		delete Thread;
		delete EventBeginFrame;
		delete EventEndFrame;
	}
	else
	{
		DeleteObjects();
	}
}

void LXRenderer::Init()
{
	//
	// Create objects
	//

	DirectX11 = new LXDirectX11(_hWND);

	RenderCommandList = new LXRenderCommandList();

	// ConstantBuffers
	CBViewProjection = new LXConstantBufferD3D11();
	LXConstantBufferData0 Foo;
	CBViewProjection->CreateConstantBuffer(&Foo, sizeof(LXConstantBufferData0));
		
	//
	// Rasterizers
	//

	// Default rasterizer
	CD3D11_RASTERIZER_DESC RasterizerDesc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
	RasterizerDesc.FrontCounterClockwise = TRUE;
	HRESULT hr = DirectX11->GetCurrentDevice()->CreateRasterizerState(&RasterizerDesc, &D3D11RasterizerState);
	DirectX11->GetCurrentDeviceContext()->RSSetState(D3D11RasterizerState);

	// TwoSided Default rasterizer
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	hr = DirectX11->GetCurrentDevice()->CreateRasterizerState(&RasterizerDesc, &D3D11RasterizerStateTwoSided);

	// Wireframe rasterizer
	ZeroMemory(&RasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	RasterizerDesc.FrontCounterClockwise = FALSE;
	RasterizerDesc.DepthBias = 0;
	RasterizerDesc.DepthBiasClamp = 0.0f;
	RasterizerDesc.SlopeScaledDepthBias = 0.0f;
	RasterizerDesc.DepthClipEnable = TRUE;
	RasterizerDesc.ScissorEnable = FALSE;
	RasterizerDesc.MultisampleEnable = FALSE;
	RasterizerDesc.AntialiasedLineEnable = FALSE;
	RasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
	hr = DirectX11->GetCurrentDevice()->CreateRasterizerState(&RasterizerDesc, &D3D11RasterizerStateWireframe);

	//
	// BlendStates
	//

	D3D11_BLEND_DESC BlendState = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
	hr = DirectX11->GetCurrentDevice()->CreateBlendState(&BlendState, &D3D11BlendStateNoBlend);
	
	BlendState.RenderTarget[0].BlendEnable = TRUE;
	BlendState.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendState.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	hr = DirectX11->GetCurrentDevice()->CreateBlendState(&BlendState, &D3D11BlendStateBlend);
	
#if LX_D2D1

	// D2D1 SolidColorBrush
	if (DirectX11->_D2D1DeviceContext)
	{
		DirectX11->_D2D1DeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &pRedBrush);
		DirectX11->_D2D1DeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pWhiteBrush);
		DirectX11->_D2D1DeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBlackBrush);
		DirectX11->_D2D1DeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGray), &pGrayBrush);
		DirectX11->_D2D1DeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), &pYellowBrush);
		DirectX11->_D2D1DeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DarkBlue), &pDarkBlue);
		DirectX11->_D2D1DeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pConsoleBackgroundBrush);
	}

#endif

	// Passes
	RenderPassShadow = new LXRenderPassShadow(this);
	RenderPassDynamicTexture = new LXRenderPassDynamicTexture(this);
	RenderPassGBuffer = new LXRenderPassGBuffer(this);
	RenderPassAux = new LXRenderPassAux(this);
	RenderPassTransparent = new LXRenderPassTransparency(this);
	RenderPassLighting = new LXRenderPassLighting(this);
	RenderPassToneMapping = new LXRenderPassToneMapping(this);
	RenderPassDownsample = new LXRenderPassDownsample(this, EDownsampleFunction::Downsample_HalfRes);

	RenderPasses.push_back(RenderPassShadow);
	RenderPasses.push_back(RenderPassDynamicTexture);
	RenderPasses.push_back(RenderPassGBuffer);
	//RenderPasses.push_back(RenderPassAux);
	RenderPasses.push_back(RenderPassLighting);
	RenderPasses.push_back(RenderPassTransparent);
	//RenderPasses.push_back(RenderPassDownsample);
	RenderPasses.push_back(RenderPassToneMapping);

	// Misc
	ShaderManager = new LXShaderManager();
	RenderClusterManager = new LXRenderClusterManager();

	// ScreenSpace Triangle
	SSTriangle = new LXPrimitiveD3D11();
	SSTriangle->CreateSSTriangle();

	// Links between objects
	RenderPassGBuffer->Viewport = Viewport;
	RenderPassGBuffer->RenderPassShadow = RenderPassShadow;
	RenderPassAux->Viewport = Viewport;
	RenderPassLighting->RenderPassGBuffer = RenderPassGBuffer;
	RenderPassLighting->RenderPassShadow = RenderPassShadow;
	RenderPassLighting->TextureShadow = RenderPassShadow->TextureDepth.get();
	RenderCommandList->Renderer = this;
	RenderCommandList->ShaderManager = ShaderManager;
}

void LXRenderer::Run()
{
	Init();
			
	// Render !
	while (!ExitRenderThread)
	{
		// Do not remove the brackets, needed to measure the wait time.
		{
			LX_PERFOSCOPE(RenderThread_WaitTime);
			EventBeginFrame->Wait();
		}

		EventBeginFrame->Reset();
		Render();
		EventEndFrame->SetEvent();
	}
	
	Empty();
	DeleteObjects();
}

void LXRenderer::DeleteObjects()
{
	CHK(IsRenderThread())
	LX_SAFE_DELETE(CBViewProjection);
	LX_SAFE_DELETE(ShaderManager);
	LX_SAFE_DELETE(RenderClusterManager);
	LX_SAFE_DELETE(SSTriangle);
	LX_SAFE_DELETE(RenderPassShadow);
	LX_SAFE_DELETE(RenderPassDynamicTexture);
	LX_SAFE_DELETE(RenderPassGBuffer);
	LX_SAFE_DELETE(RenderPassAux);
	LX_SAFE_DELETE(RenderPassTransparent);
	LX_SAFE_DELETE(RenderPassLighting);
	LX_SAFE_DELETE(RenderPassToneMapping);
	LX_SAFE_DELETE(RenderPassDownsample);
	LX_SAFE_DELETE(RenderCommandList);
	LX_SAFE_RELEASE(D3D11RasterizerState);
	LX_SAFE_RELEASE(D3D11RasterizerStateTwoSided);
	LX_SAFE_RELEASE(D3D11RasterizerStateWireframe);
	LX_SAFE_RELEASE(D3D11BlendStateNoBlend);
	LX_SAFE_RELEASE(D3D11BlendStateBlend);
	LX_SAFE_RELEASE(pRedBrush);
	LX_SAFE_RELEASE(pWhiteBrush);
	LX_SAFE_RELEASE(pYellowBrush);
	LX_SAFE_RELEASE(pBlackBrush);
	LX_SAFE_RELEASE(pGrayBrush);
	LX_SAFE_RELEASE(pDarkBlue);
	LX_SAFE_RELEASE(pConsoleBackgroundBrush);
	LX_SAFE_DELETE(DirectX11);
}

void LXRenderer::Render_MainThread()
{
	CHK(!gUseRenderThread);

	if (!_bInit)
	{
		Init();
		_bInit = true;
	}

	Render();
}

void LXRenderer::DrawScreenSpacePrimitive(LXRenderCommandList* RCL)
{
	if (SSTriangle)
		SSTriangle->Render(RCL);
}

const LXTextureD3D11* LXRenderer::GetDepthBuffer() const
{
	return RenderPassGBuffer->TextureDepth;
}

const LXTextureD3D11* LXRenderer::GetColorBuffer() const
{
	return RenderPassGBuffer->TextureColor;
}

const LXTextureD3D11* LXRenderer::GetNormalBuffer() const
{
	return RenderPassGBuffer->TextureNormal;
}

const LXTextureD3D11* LXRenderer::GetMRULBuffer() const
{
	return RenderPassGBuffer->TextureSpecular;
}

const LXTextureD3D11* LXRenderer::GetEmissiveBuffer() const
{
	return RenderPassGBuffer->TextureEmissive;
}

void LXRenderer::SetDocument(LXProject* Project)
{ 
	_NewProject = Project;
}

const LXProject* LXRenderer::GetProject() const
{
	return _Project;
}
 
LXSyncEvent* LXRenderer::GetBeginEvent() const
{
	CHK(RenderThread); 
	return EventBeginFrame;
}

LXSyncEvent* LXRenderer::GetEndEvent() const
{
	CHK(RenderThread); 
	return EventEndFrame;
}

void LXRenderer::ShowBounds(bool Show)
{
	if (bShowBounds != Show)
	{
		if (Show)
			DoShowBounds = true;
		else
			DoHideBounds = true;
	}

	bShowBounds = Show;
}

bool LXRenderer::ShowBounds() const
{
	return bShowBounds;
}

LXMaterialD3D11* LXRenderer::GetMaterialD3D11(const LXMaterial* Material)
{
	return RenderClusterManager->GetMaterial(Material);
}

void LXRenderer::ResetShaders()
{
	gResetShaders = true;
}

void LXRenderer::Render()
{
	// RenderThread time ate
	Time.Update();
	LX_COUNT("RenderThread (Full cylce): %.2f MS\n", Time.DeltaTime());
	
	//
	// Commands & Tasks
	//

	// If project changed
	if (_Project != _NewProject)
	{
		_Project = _NewProject;
		Empty();
	}

	// "Tick" the ShaderManager
	// Maybe it has things to delete/release
	ShaderManager->Run();
	
	
	if (gResetShaders)
	{
		ShaderManager->RebuildShaders();

		for (LXRenderPass* RenderPass : RenderPasses)
		{
			RenderPass->RebuildShaders();
		}
		
		gResetShaders = false;
	}

	if (gToogleDrawImmediate)
	{
		RenderCommandList->DirectMode = !RenderCommandList->DirectMode;
		gToogleDrawImmediate = false;
	}
	
	// Resize if needed
	if (Width != Viewport->GetWidth() || Height != Viewport->GetHeight())
	{
		Width = Viewport->GetWidth();
		Height = Viewport->GetHeight();

		CHK(Width > 0);
		CHK(Height > 0);

		DirectX11->Resize(Width, Height);
		//DirectX11->Resize(Viewport->GetWidth(), Viewport->GetHeight());
		
		for (LXRenderPass* RenderPass : RenderPasses)
		{
			RenderPass->Resize(Width, Height);
		}
	}
	
	// Update the RenderClusters according the World/Scene content
	UpdateStates();
	
	//GetInput();
	if (_Project)
	{
		Frame++;
		_Project->GetAnimationManager().Update(Time.DeltaTime());
	}

	Viewport->GetCameraManipulator()->Update(Time.DeltaTime());

	if (GetScene())
	{
		GetScene()->RunRT(Time.DeltaTime());
	}

	// TODO END
	//-----------------
	
	// Clear the command list
	RenderCommandList->Empty();

	bool ValidRenderPasses = true;

	for (LXRenderPass* RenderPass : RenderPasses)
	{
		if (RenderPass->IsValid())
		{
			RenderPass->Render(RenderCommandList);
			PreviousRenderPass = RenderPass;
		}
		else
		{
			ValidRenderPasses = false;
			//LogOneTimeE(LXRenderer, L"RenderPass not valid");
			#pragma  message("Enable a screen message")
			break;
		}
	}
	
	PreviousRenderPass = nullptr;

	//
	// To back buffer.
	//

	if(1)
	{
		const LXTextureD3D11* Color = RenderPassToneMapping->GetOutputTexture();
		
		RenderCommandList->BeginEvent(L"DrawToBackBuffer");
		RenderCommandList->RSSetViewports(Viewport->GetWidth(), Viewport->GetHeight());

		// Bind the back buffer 
		RenderCommandList->OMSetRenderTargets(DirectX11->_D3D11RenderTargetView); // Hack

		// Render a triangle : GBuffer TextureColor
		RenderCommandList->IASetInputLayout(ShaderManager->VSDrawToBackBuffer);
		RenderCommandList->VSSetShader(ShaderManager->VSDrawToBackBuffer);
		RenderCommandList->PSSetShader(ShaderManager->PSDrawToBackBuffer);
		
	// 		if (gShowDynamicTexture && RenderPassDynamicTexture->GetTexture())
// 			Color = RenderPassDynamicTexture->GetTexture();

		if (ShowBuffer != EShowBuffer::Default)
		{
			switch (ShowBuffer)
			{
			case EShowBuffer::Color: Color = RenderPassGBuffer->TextureColor; break;
			case EShowBuffer::Depth: Color = RenderPassGBuffer->TextureDepth; break;
			case EShowBuffer::Normal: Color = RenderPassGBuffer->TextureNormal; break;
			case EShowBuffer::Shadow: Color = RenderPassShadow->TextureDepth.get(); break;
			}
		}
		
		RenderCommandList->PSSetShaderResources(0, 1, (LXTextureD3D11*)Color);
		RenderCommandList->PSSetSamplers(0, 1, (LXTextureD3D11*)Color);
		SSTriangle->Render(RenderCommandList);
		RenderCommandList->PSSetShaderResources(0, 1, nullptr);

		RenderCommandList->EndEvent();
	}
			
	// TODO Create a thread to Execute...
	// Execute the command list

	LXTimeD3D11 TimeGPU;
	double ElapsedGPU = 0.;
	
	RenderCommandList->Execute();
	ElapsedGPU = TimeGPU.Update();
	LX_COUNT(L"GPU (Execute commands): %.2f MS", ElapsedGPU);

#if LX_D2D1	

	if (DirectX11->_D2D1Device)
	{

		// DrawCalls
		LX_COUNT(L"DrawCalls : %f", RenderCommandList->DrawCallCount);

		// Triangles
		LX_COUNT(L"Triangles : %f", RenderCommandList->TriangleCount);

		// RenderClusters
		LX_COUNT(L"RenderClusters : %f", (int)RenderClusterManager->ListRenderClusters.size());

		// VertexShaders
		LX_COUNT(L"VertexShaders : %f", (int)ShaderManager->VertexShaders.size());

		// HullShaders
		LX_COUNT(L"HullShaders : %f", (int)ShaderManager->HullShaders.size());

		// DomainShaders
		LX_COUNT(L"DomainShaders : %f", (int)ShaderManager->DomainShaders.size());

		// PixelShaders
		LX_COUNT(L"PixelShaders : %f", (int)ShaderManager->PixelShaders.size());

		// Frame
		LX_COUNT(L"Frame : %f", (double)Frame);

		wchar_t str[256] = { 0 };

		if (1)
		{
			if (DirectX11->_D3DUserDefinedAnnotation)
				DirectX11->_D3DUserDefinedAnnotation->BeginEvent(L"UI");

			DirectX11->_D2D1DeviceContext->BeginDraw();

			if (ShowFPS)
			{

				const int LineHeight = LX_CONSOLE_FONT_SIZE;

				int PositionY = 0;
				ArrayCounters& arrayCounters = GetCore().GetCounters();
				for (LXCounter* Counter : arrayCounters)
				{
					float Top = (float)PositionY;
					float Bottom = Top + (float)LineHeight;
					LXString Str = Counter->GetSentence();
					DirectX11->_D2D1DeviceContext->DrawText(Str.GetBuffer(), Str.GetLength(), DirectX11->_DWriteTextFormat, D2D1::RectF(0.0f, Top, 600, Bottom), pWhiteBrush);
					PositionY += LineHeight;
				}

				// A Line space
				PositionY += LineHeight;

				// Performances
				for (auto &Stat : GetStatManager()->GetPerfomances())
				{
					float Top = (float)PositionY;
					float Bottom = Top + (float)LineHeight;
					LXString Str = LXString::Format(L"%s : %.2f MS : %i Hit(s)\n",
						Stat.first.c_str(),
						Stat.second.Time,
						Stat.second.Hits);

					DirectX11->_D2D1DeviceContext->DrawText(Str.GetBuffer(), Str.GetLength(), DirectX11->_DWriteTextFormat, D2D1::RectF(0.0f, Top, 600, Bottom), pWhiteBrush);
					PositionY += LineHeight;
				}

				// A Line space
				PositionY += LineHeight;

				// Counters
				for (auto& Counter : GetStatManager()->GetCounters())
				{
					float Top = (float)PositionY;
					float Bottom = Top + (float)LineHeight;
					LXString Str = LXString::Format(L"%s : %i\n",
						Counter.first.c_str(),
						Counter.second);

					DirectX11->_D2D1DeviceContext->DrawText(Str.GetBuffer(), Str.GetLength(), DirectX11->_DWriteTextFormat, D2D1::RectF(0.0f, Top, 600, Bottom), pWhiteBrush);
					PositionY += LineHeight;
				}

				GetStatManager()->Reset();
			}

			// Console
			if (Viewport->ConsoleEnabled)
			{
				const float LineHeight = LX_CONSOLE_FONT_SIZE;

				// Background
				auto RectBackground = D2D1::RectF(5.0f, Viewport->GetHeight() - (CONSOLE_MAX_LINE * LineHeight) - 5.0f, Viewport->GetWidth() - 5.0f, Viewport->GetHeight() - 5.0f);
				DirectX11->_D2D1DeviceContext->FillRectangle(RectBackground, pConsoleBackgroundBrush);

				// Previous lines
				int i = 0;
				for (LXString& Message : ConsoleBuffer)
				{
					float Top = Viewport->GetHeight() - ((ConsoleBuffer.size() - i + 1) * LineHeight) - 5.0f;
					float Bottom = Top + LineHeight;
					auto Rect = D2D1::RectF(5.0f, Top, Viewport->GetWidth() - 5.0f, Bottom);
					DirectX11->_D2D1DeviceContext->DrawTextW(Message, Message.size(), DirectX11->_DWriteTextFormat, Rect, pWhiteBrush);
					i++;
				}

				// Input line
				auto RectTextLine = D2D1::RectF(5.0f, Viewport->GetHeight() - LineHeight - 5.0f, Viewport->GetWidth() - 5.0f, Viewport->GetHeight() - 5.0f);
				LXString Input = L">";
				if (Viewport->ConsoleNearestCommand.size())
					Input += Viewport->ConsoleNearestCommand[Viewport->ConsoleSuggestionSeek];
				DirectX11->_D2D1DeviceContext->DrawTextW(Input, Input.size(), DirectX11->_DWriteTextFormat, RectTextLine, pGrayBrush);
				Input = L">" + Viewport->ConsoleString + "_";
				DirectX11->_D2D1DeviceContext->DrawTextW(Input, Input.size(), DirectX11->_DWriteTextFormat, RectTextLine, pYellowBrush);
			}

			DirectX11->_D2D1DeviceContext->EndDraw();

			if (DirectX11->_D3DUserDefinedAnnotation)
				DirectX11->_D3DUserDefinedAnnotation->EndEvent();

		}
	}
#endif

	DirectX11->Present();

	
}

void LXRenderer::UpdateStates()
{
	for (LXActor* Actor : GetController()->GetActorToUpdateRenderStateSetRT())
	{
		LogI(Renderer, L"Updated actor %s", Actor->GetName().GetBuffer());
		RenderClusterManager->UpdateActor(Actor);
		Actor->ValidateRensterState();
	}
	GetController()->GetActorToUpdateRenderStateSetRT().clear();

	for (LXMaterial* Material : GetController()->GetMaterialToUpdateRenderStateSetRT())
	{
		LogI(Renderer, L"Update Material %s", Material->GetName().GetBuffer());
		RenderClusterManager->UpdateMaterial(Material);
		//RenderClusterManager->RebuildMaterial(Material);
	}
	GetController()->GetMaterialToUpdateRenderStateSetRT().clear();

	// Actor Matrix
// 	for (LXActor* Actor : GetController()->GetActorToMoveRT())
// 	{
// 		RenderClusterManager->MoveActor(Actor);
// 	}
// 	GetController()->GetActorToMoveRT().clear();

	ListRendererUpdates& RendererUpdates = GetController()->GetRendererUpdate();
	for (LXRendererUpdate* RendererUpdate : RendererUpdates)
	{
		if (const LXRendererUpdateMatrix* RenderUpdateMatrix = dynamic_cast<LXRendererUpdateMatrix*>(RendererUpdate))
		{
			RenderClusterManager->UpdateMatrix(*RenderUpdateMatrix);
		}
		else
		{
			CHK(0);
		}

		delete RendererUpdate;
	}
	RendererUpdates.clear();
	
	if (DoShowBounds)
	{
		DoShowBounds = false;
	}

	if (DoHideBounds)
	{
		DoHideBounds = false;
	}

	RenderClusterManager->Tick();

}

void LXRenderer::AddActor(LXActor* Actor)
{
	RenderClusterManager->AddActor(Actor);
}

void LXRenderer::RemoveActor(LXActor* Actor)
{
	RenderClusterManager->RemoveActor(Actor);
}

bool LXRenderer::SetShaders(LXRenderClusterJobTexture* RenderCluster)
{
	//return ShaderManager->GetShaders(RenderCluster->Primitive, RenderCluster->Material, &RenderCluster->VertexShader, nullptr, nullptr, &RenderCluster->PixelShader);
	CHK(0); // It misses the RenderPass as GetShaders first parameter.
	return false;
}

const LXWorldTransformation& LXRenderer::GetWorldTransform() const
{
	return Viewport->WorldTransformation;
}

const LXMatrix& LXRenderer::GetMatrixView() const
{
	return Viewport->WorldTransformation.GetMatrixView();
}

const LXMatrix& LXRenderer::GetMatrixProjection() const
{
	return Viewport->WorldTransformation.GetMatrixProjection();
}

void LXRenderer::Empty()
{
	if (RenderClusterManager)
		RenderClusterManager->Empty();
}