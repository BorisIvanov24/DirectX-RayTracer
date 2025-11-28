#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <comdef.h>
#include <QImage>

#define CDXC_MAKE_SMART_COM_POINTER(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))

CDXC_MAKE_SMART_COM_POINTER(IDXGIFactory4);
CDXC_MAKE_SMART_COM_POINTER(IDXGIAdapter1);
CDXC_MAKE_SMART_COM_POINTER(ID3D12Device);
CDXC_MAKE_SMART_COM_POINTER(ID3D12CommandQueue);
CDXC_MAKE_SMART_COM_POINTER(ID3D12CommandAllocator);
CDXC_MAKE_SMART_COM_POINTER(ID3D12GraphicsCommandList);
CDXC_MAKE_SMART_COM_POINTER(ID3D12Resource);
CDXC_MAKE_SMART_COM_POINTER(ID3D12DescriptorHeap);
CDXC_MAKE_SMART_COM_POINTER(ID3D12Fence);
CDXC_MAKE_SMART_COM_POINTER(IDXGISwapChain1);
CDXC_MAKE_SMART_COM_POINTER(IDXGISwapChain3);

#define RGBA_COLOR_CHANNELS_COUNT 4

class DXRTRenderer
{
public:

	DXRTRenderer();

	// Initiate the actual rendering
	void render();

	void renderFrame();

	void renderFrameWithSwapChain();

	// Create the necessary DirectX infrastructure and rendering resources
	void prepareForRendering(HWND hwnd);

	QImage getQImageForFrame();

	void stopRendering();

private:
	// Create ID3D12Device, an interface which allows access to the GPU for the purpose of Direct3D API
	void createDevice();

	// Create ID3D12CommandQueue, ID3D12CommandAllocator and ID3D12GraphicsCommandList, for preparing and passing GPU commands
	void createCommandsManagers();

	// Describe the 2D buffer, which will be used for the texture, and create its heap
	void createGPUTexture();

	// Create a descriptor for the render target, with which the texture could be accessed for the next pipeline stages
	// Create a descriptor heap for the descriptor
	void createRenderTargetView();

	// Add commands in the command list for generating a texture with constant color
	void generateConstColorTexture();

	// Create a read-back heap and a read-back buffer, based on the texture for rendering
	// Store the memory layout information for the texture
	void createReadbackBuffer();

	// Create fence, which will be used to synchronize the CPU and GPU after frame rendering
	void createFence();

	void createSwapChain(HWND hwnd);

	void createRenderTargetViewsFromSwapChain();

	void createDescriptorHeapForSwapChain();

	// Stall the CPU untill the GPU finishes with the frame rendering
	void waitForGPURenderFrame();

	void recordExecuteAndReadback();

	void writeImageToFile();

	void frameBegin();

	void frameEnd();

private:
	
	IDXGIFactory4Ptr dxgiFactory; // Grants access to the GPUs on the machine
	IDXGIAdapter1Ptr adapter;	// Represents the video card used for rendering
	ID3D12DevicePtr d3d12Device; // Allows access to the GPU for the purpose of Direct3D API

	ID3D12CommandQueuePtr commandQueue; // Holds the command lists and will be given to the GPU for execution
	ID3D12CommandAllocatorPtr commandAllocator; // Manages the GPU memory for the commands
	ID3D12GraphicsCommandListPtr commandList; // The actual commands that will be executet by the GPU

	D3D12_RESOURCE_DESC textureDesc; // Holds the texture properties
	ID3D12ResourcePtr renderTarget; // The render target used for the texture in which the GPU will write colors
	ID3D12DescriptorHeapPtr rtvHeap; // The heap which holds the render target descriptor of the texture
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle; // Handle for the descriptor of the texture, with which it could be used in the pipeline

	ID3D12ResourcePtr readbackBuffer; // The buffer which will hold the rendered image
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT renderTargetFootprint; // Memory layout information for the texture

	ID3D12FencePtr renderFramefence; // Synchronize the CPU and GPU after frame rendering
	HANDLE renderFrameEventHandle = nullptr; // The event which is fired when the GPU is ready with the rendering
	UINT64 renderFramefenceValue = 1;

	IDXGISwapChain3Ptr swapChain;
	UINT64 swapChainFrameIdx = 0;
	UINT64 rtvDescriptorSize = 0;

	static const UINT FrameCount = 2;

	ID3D12ResourcePtr renderTargets[FrameCount];
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[FrameCount];
	ID3D12DescriptorHeapPtr swapChainRTVHeap;


	float rendColor[4] = { 0.f, 1.f, 0.f, 1.f };
	int frameIdx = 1;
};

