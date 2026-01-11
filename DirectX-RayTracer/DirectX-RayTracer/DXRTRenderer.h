#pragma once
#include <directx/d3d12.h>
#include <dxgi1_4.h>
#include <comdef.h>
#include <QImage>
#include <dxcapi.h>
#include <memory>

#include <DirectXMath.h>

#include "CRTScene.h"

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
CDXC_MAKE_SMART_COM_POINTER(ID3D12RootSignature);
CDXC_MAKE_SMART_COM_POINTER(ID3DBlob);
CDXC_MAKE_SMART_COM_POINTER(ID3D12PipelineState);
CDXC_MAKE_SMART_COM_POINTER(IDxcBlob);
CDXC_MAKE_SMART_COM_POINTER(IDxcLibrary);
CDXC_MAKE_SMART_COM_POINTER(IDxcCompiler);
CDXC_MAKE_SMART_COM_POINTER(IDxcBlobEncoding);
CDXC_MAKE_SMART_COM_POINTER(IDxcOperationResult);
CDXC_MAKE_SMART_COM_POINTER(IDxcUtils);
CDXC_MAKE_SMART_COM_POINTER(IDxcCompiler3);
CDXC_MAKE_SMART_COM_POINTER(IDxcResult);
CDXC_MAKE_SMART_COM_POINTER(IDxcBlobEncoding);
CDXC_MAKE_SMART_COM_POINTER(IDxcBlobUtf8);
CDXC_MAKE_SMART_COM_POINTER(ID3D12Device5);
CDXC_MAKE_SMART_COM_POINTER(ID3D12StateObject);
CDXC_MAKE_SMART_COM_POINTER(ID3D12StateObjectProperties);
CDXC_MAKE_SMART_COM_POINTER(ID3D12GraphicsCommandList4);
CDXC_MAKE_SMART_COM_POINTER(ID3D12InfoQueue);

#define RGBA_COLOR_CHANNELS_COUNT 4

struct Vertex
{
	float x;
	float y;
	float z;
};

struct CameraCB
{
	DirectX::XMFLOAT3 cameraPosition;
	float pad0;
	DirectX::XMFLOAT4X4 cameraRotation;
};

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

	void prepareForRayTracing();

	QImage getQImageForFrame();

	void stopRendering();

	CRTScene& getScene();
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

	// Create the vertices that will be rendered by the pipeline for the frame
	// Use an upload heap to store the vertices on the CPU memory, the GPU will access them using the PCIe
	void createVertexBuffer();

	void createIndexBuffer();

	void createRootSignature();

	void createPipelineState();

	void createViewport();

	void createScene();

	void updateCameraCB();

	void recordExecuteAndReadback();

	void writeImageToFile();

	void rotateTriangleVertices();

	void frameBegin();

	void frameEnd();

	void createCameraBuffer();

	void createAccelerationStructure();

	void createGlobalRootSignature();

	void createRayTracingPipelineState();

	void createRayTracingShaderTexture();

	void createShaderBindingTable();

	IDxcBlobPtr compileShader(const std::wstring& fileName, const std::wstring& entryPoint,
							  const std::wstring& target);

	D3D12_STATE_SUBOBJECT createRayGenSubObject();
	D3D12_STATE_SUBOBJECT createClosestHitSubObject();
	D3D12_STATE_SUBOBJECT createMissLibSubObject();
	D3D12_STATE_SUBOBJECT createShaderConfigSubObject();
	D3D12_STATE_SUBOBJECT createPipelineConfigSubObject();
	D3D12_STATE_SUBOBJECT createGlobalRootSignatureSubObject();
	D3D12_STATE_SUBOBJECT createHitGroupSubObject();

	void createSBTUploadHeap(const UINT sbtSize);
	void createSBTDefaultHeap(const UINT sbtSize);
	void copySBTDataToUploadHeap(const UINT rayGenOffset, const UINT missOffset, const UINT hitGroupOffset,
		void* rayGenID, void* missId, void* hitGroupID);
	void copySBTDataToDefaultHeap();
	void prepareDispatchRaysDesc(const UINT sbtSize, const UINT rayGenOffset,
		const UINT missOffset, const UINT hitGroupOffset);

private:
	
	IDXGIFactory4Ptr dxgiFactory; // Grants access to the GPUs on the machine
	IDXGIAdapter1Ptr adapter;	// Represents the video card used for rendering
	ID3D12DevicePtr d3d12Device; // Allows access to the GPU for the purpose of Direct3D API
	ID3D12Device5Ptr dxrDevice;

	ID3D12CommandQueuePtr commandQueue; // Holds the command lists and will be given to the GPU for execution
	ID3D12CommandAllocatorPtr commandAllocator; // Manages the GPU memory for the commands
	ID3D12GraphicsCommandListPtr commandList; // The actual commands that will be executet by the GPU

	D3D12_RESOURCE_DESC textureDesc; // Holds the texture properties
	ID3D12ResourcePtr renderTarget; // The render target used for the texture in which the GPU will write colors
	ID3D12DescriptorHeapPtr rtvHeap; // The heap which holds the render target descriptor of the texture
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle; // Handle for the descriptor of the texture, with which it could be used in the pipeline

	ID3D12ResourcePtr readbackBuffer; // The buffer which will hold the rendered image
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT renderTargetFootprint; // Memory layout information for the texture

	ID3D12ResourcePtr raytracingOutput;
	ID3D12DescriptorHeapPtr uavHeap;
	ID3D12RootSignaturePtr globalRootSignature;

	D3D12_EXPORT_DESC rayGenExportDesc;
	D3D12_DXIL_LIBRARY_DESC rayGenLib;
	IDxcBlobPtr rayGenBlob;

	D3D12_EXPORT_DESC closestHitExportDesc;
	D3D12_DXIL_LIBRARY_DESC closestHitLib;
	IDxcBlobPtr closestHitBlob;

	D3D12_EXPORT_DESC missExportDesc;
	D3D12_DXIL_LIBRARY_DESC missLib;
	IDxcBlobPtr missBlob;

	D3D12_HIT_GROUP_DESC hitGroupDesc;

	D3D12_RAYTRACING_SHADER_CONFIG shaderConfig;
	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig;
	D3D12_GLOBAL_ROOT_SIGNATURE rootSigDesc;
	D3D12_DISPATCH_RAYS_DESC raysDesc;

	ID3D12StateObjectPtr rtStateObject;

	ID3D12ResourcePtr sbtUploadBuff;
	ID3D12ResourcePtr sbtDefaultBuff;

	ID3D12GraphicsCommandList4Ptr dxrCmdList;

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

	ID3D12ResourcePtr indexBuffer;
	ID3D12ResourcePtr vertexBuffer;
	ID3D12ResourcePtr uploadVertexBuffer;// The vertices that we want to render
	D3D12_VERTEX_BUFFER_VIEW vbView; // Vertex buffer descriptor
	ID3D12RootSignaturePtr rootSignature;
	ID3D12PipelineStatePtr state;

	D3D12_ROOT_CONSTANTS constant;
	D3D12_ROOT_PARAMETER param;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	float rendColor[4] = { 0.f, 1.f, 0.f, 1.f };
	int frameIdx = 1;
	Vertex vertices[3];

	D3D12_RESOURCE_STATES raytracingOutputState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES backBufferState = D3D12_RESOURCE_STATE_PRESENT;

	// Bottom-Level Acceleration Structure (BLAS)
	ID3D12ResourcePtr blasBuffer;      // GPU buffer storing BLAS
	ID3D12ResourcePtr blasScratch;     // Scratch buffer for building BLAS

	// Top-Level Acceleration Structure (TLAS)
	ID3D12ResourcePtr tlasBuffer;      // GPU buffer storing TLAS
	ID3D12ResourcePtr tlasScratch;     // Scratch buffer for building TLAS
	ID3D12ResourcePtr instanceBuffer;  // Instance description buffer

	// GPU pointers
	D3D12_GPU_VIRTUAL_ADDRESS blasBufferAddress;
	D3D12_GPU_VIRTUAL_ADDRESS tlasBufferAddress;

	std::unique_ptr<CRTScene> scene;
	ID3D12ResourcePtr cameraCB;

	std::vector<ID3D12ResourcePtr> vertexBuffers;
	std::vector<ID3D12ResourcePtr> indexBuffers;

	std::vector<ID3D12ResourcePtr> uploadVertexBuffers;
	std::vector<ID3D12ResourcePtr> uploadIndexBuffers;
};

