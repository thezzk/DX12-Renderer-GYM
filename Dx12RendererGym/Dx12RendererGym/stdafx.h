#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <vector>
#include "d3dx12.h"
#include "ImageUtil.h"
#include "OBJ_Loader.h"


#define SAFE_RELEASE(p) { if ( (p) ) {(p)->Release(); (p) = 0; } }
using namespace DirectX;

struct Vertex
{
	Vertex(float x, float y, float z, float u, float v) : pos(x, y, z), texCoord(u, v) {}
	XMFLOAT3 pos;
	XMFLOAT2 texCoord;
};

// Handle to the window
HWND hwnd = NULL;

// name of the windows (not the title)
LPCTSTR WindowName = L"Dx12GYM";

// title of the window
LPCTSTR WindowTitle = L"Renderer Window";

// width and height of the window
int Width = 800;
int Height = 600;

bool FullScreen = false;

bool Running = false;

//create window
bool InitializeWindow(HINSTANCE hInstance,
	int ShowWnd,
	bool fullscreen);

//main loop
void mainloop();

//callback function for windows messages
LRESULT CALLBACK WndProc(HWND hWnd,
	UINT msg,
	WPARAM wParam, 
	LPARAM lParam);

// direct3d stuff
const int frameBufferCount = 3; // Tripple buffer

IDXGIFactory4* dxgiFactory;

ID3D12Device* device;

IDXGISwapChain3* swapChain;

ID3D12CommandQueue* commandQueue;

ID3D12DescriptorHeap* rtvDescriptorHeap;

ID3D12Resource* renderTargets[frameBufferCount];

ID3D12CommandAllocator* commandAllocator[frameBufferCount];

ID3D12GraphicsCommandList* commandList;

DXGI_SAMPLE_DESC sampleDesc;

ID3D12Fence* fence[frameBufferCount];

HANDLE fenceEvent;

UINT64 fenceValue[frameBufferCount];

int frameIndex;

int rtvDescriptorSize;

D3D12_SHADER_BYTECODE vertexShaderBytecode;
D3D12_SHADER_BYTECODE pixelShaderBytecode;

ID3D12PipelineState* pipelineStateObject;

ID3D12RootSignature* rootSignature;

D3D12_VIEWPORT viewport;

D3D12_RECT scissorRect;

ID3D12Resource* vertexBuffer;
D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
int vBufferSize;

ID3D12Resource* indexBuffer;
D3D12_INDEX_BUFFER_VIEW indexBufferView;
int iBufferSize;

D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc;
ID3D12Resource* depthStencilBuffer;
ID3D12DescriptorHeap* dsDescriptorHeap;

struct ConstantBufferPerObject {
	XMFLOAT4X4 wvpMat;
};

int ConstantBufferPerObjectAlignedSize = (sizeof(ConstantBufferPerObject) + 255) & ~255;

ConstantBufferPerObject cbPerObject;

ID3D12Resource* constantBufferUploadHeaps[frameBufferCount];

UINT8* cbvGPUAddress[frameBufferCount];

XMFLOAT4X4 cameraProjMat;
XMFLOAT4X4 cameraViewMat;

XMFLOAT4 cameraPosition;
XMFLOAT4 cameraTarget;
XMFLOAT4 cameraUp;

XMFLOAT4X4 cube1WorldMat;
XMFLOAT4X4 cube1RotMat;
XMFLOAT4 cube1Position;

XMFLOAT4X4 cube2WorldMat;
XMFLOAT4X4 cube2RotMat;
XMFLOAT4 cube2PositionOffset;

int numCubeIndices;

ID3D12Resource* textureBuffer;
D3D12_RESOURCE_DESC textureDesc;

int LoadImageDataFromFile(BYTE** imageData, D3D12_RESOURCE_DESC& resourceDescription, LPCWSTR filename, int &bytesPerRow);

bool loadMesh(std::string objfileName, std::vector<Vertex>& vertexList, std::vector<DWORD>& indexList);

ID3D12DescriptorHeap* mainDescriptorHeap;
ID3D12Resource* textureBufferUploadHeap;

// functions
bool InitD3D();
bool InitD3DDevice();
bool InitCommandQueue();
bool InitCommandAllocators();
bool InitCommandList();
bool InitFenceAndEvent();
bool InitSwapChain();
bool InitDescriptorHeaps();
bool InitRootSignature();
bool InitResources();
bool InitViews();
bool InitVSPS();
bool InitPSO();

void Update();

void UpdatePipeline();

void Render();

void Cleanup();

void WaitForPreviousFrame();