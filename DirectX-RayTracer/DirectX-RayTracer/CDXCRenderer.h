#pragma once

class IDXGIFactory4;
class IDXGIAdapter1;
class ID3D12Device;
class ID3D12CommandQueue;
class ID3D12CommandAllocator;
class ID3D12GraphicsCommandList;
class ID3D12Resource;
class ID3D12DescriptorHeap;

struct D3D12_RESOURCE_DESC;
struct D3D12_CPU_DESCRIPTOR_HANDLE;


class CDXCRenderer
{
public:
	//Initiate the actual rendering
	void render();

private:
	//Create the necessary DirectX infrastructure and rendering resources
	void prepareForRendering();

	//Create ID3D12Device, an interface which allows access to the GPU for the purpose of Direct3D API
	void createDevice();

	//Create ID3D12CommandQueue, ID3D12CommandAllocator and ID3D12GraphicsCommandList, for preparing and passing GPU commands
	void createCommandsManagers();

	//Describe the 2D buffer, which will be used for the texture, and create its heap
	void createGPUTexture();

	//Create a descriptor for the render target, with which the texture could be accessed for the next pipeline stages
	//Create a descriptor heap for the descriptor
	void createRenderTargetView();

	//Add commands in the command list for generating a texture with constant color
	void generateConstColorTexture();

private:
	
	IDXGIFactory4* dxgiFactory = nullptr; // Grants access to the GPUs on the machine
	IDXGIAdapter1* adapter = nullptr;	// Represents the video card used for rendering
	ID3D12Device* d3d12Device = nullptr; // Allows access to the GPU for the purpose of Direct3D API

	ID3D12CommandQueue* commandQueue = nullptr; // Holds the command lists and will be given to the GPU for execution
	ID3D12CommandAllocator* commandAllocator = nullptr; // Manages the GPU memory for the commands
	ID3D12GraphicsCommandList* commandList = nullptr; // The actual commands that will be executet by the GPU

	D3D12_RESOURCE_DESC* textureDesc; // Holds the texture properties (In the slides it is object not ptr)
	ID3D12Resource* renderTarget; // The render target used for the texture in which the GPU will write colors
	ID3D12DescriptorHeap* rtvHeap; // The heap which holds the render target descriptor of the texture
	D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandle; // Handle for the descriptor of the texture, with which it could be used in the pipeline

};

