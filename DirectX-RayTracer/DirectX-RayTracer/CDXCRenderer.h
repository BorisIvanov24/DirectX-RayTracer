#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>

class CDXCRenderer
{
public:
	// Initiate the actual rendering
	void render();

private:
	// Create the necessary DirectX infrastructure and rendering resources
	void prepareForRendering();

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

private:
	
	IDXGIFactory4* dxgiFactory = nullptr; // Grants access to the GPUs on the machine
	IDXGIAdapter1* adapter = nullptr;	// Represents the video card used for rendering
	ID3D12Device* d3d12Device = nullptr; // Allows access to the GPU for the purpose of Direct3D API

	ID3D12CommandQueue* commandQueue = nullptr; // Holds the command lists and will be given to the GPU for execution
	ID3D12CommandAllocator* commandAllocator = nullptr; // Manages the GPU memory for the commands
	ID3D12GraphicsCommandList* commandList = nullptr; // The actual commands that will be executet by the GPU

	D3D12_RESOURCE_DESC* textureDesc = nullptr; // Holds the texture properties (In the slides it is object not ptr)
	ID3D12Resource* renderTarget = nullptr; // The render target used for the texture in which the GPU will write colors
	ID3D12DescriptorHeap* rtvHeap = nullptr; // The heap which holds the render target descriptor of the texture
	D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandle = nullptr; // Handle for the descriptor of the texture, with which it could be used in the pipeline

	ID3D12Resource* readbackBuffer = nullptr; // The buffer which will hold the rendered image
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* renderTargetFootprint; // Memory layout information for the texture

	ID3D12Fence* renderFramefence = nullptr; // Synchronize the CPU and GPU after frame rendering
	HANDLE renderFrameEventHandle = nullptr; // The event which is fired when the GPU is ready with the rendering
	UINT64 renderFramefenceValue = 1;
};

