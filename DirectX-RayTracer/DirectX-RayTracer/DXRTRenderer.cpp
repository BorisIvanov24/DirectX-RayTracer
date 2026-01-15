#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#include <windows.h>       // Windows base headers

#include "DXRTRenderer.h"
#include <iostream>
#include <assert.h>
#include <DXGItype.h>
#include <fstream>
#include <directx/d3dx12_core.h> // Helper structs, like CD3DX12_HEAP_PROPERTIES
#include <directx/d3dx12.h>

#include "CompiledShaders/ConstColor.hlsl.h"
#include "CompiledShaders/ConstColorVS.hlsl.h"

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include "CRTMesh.h"

DXRTRenderer::DXRTRenderer()
{
#ifdef _DEBUG
	// Enable the D3D12 debug layer.
	ID3D12Debug* debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		debugController->Release();
	}
#endif // _DEBUG
}

void DXRTRenderer::render()
{
	//prepareForRendering();

	renderFrame();
	
	//cleanUp();
}

void DXRTRenderer::prepareForRendering(HWND hwnd)
{
	createDevice();
	createCommandsManagers();
	createFence();
	createSwapChain(hwnd);
	createDescriptorHeapForSwapChain();
	createRenderTargetViewsFromSwapChain();

	createScene();
	createCameraBuffer();
	createDebugCB();

	createVertexBuffers();
	createIndexBuffers();
	createAccelerationStructures();

	prepareForRayTracing();
}

void DXRTRenderer::prepareForRayTracing()
{
	createGlobalRootSignature();
	createRayTracingPipelineState();
	createRayTracingShaderTexture();
	createShaderBindingTable();
}

void DXRTRenderer::createDevice()
{
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	IDXGIAdapter1Ptr bestAdapter = nullptr;
	SIZE_T maxVRAM = 0;

	// Enumerate all adapters and select the one with the most VRAM
	for (UINT adapterIndex = 0;; adapterIndex++)
	{
		IDXGIAdapter1Ptr currentAdapter;

		if (dxgiFactory->EnumAdapters1(adapterIndex, &currentAdapter) == DXGI_ERROR_NOT_FOUND)
			break;

		DXGI_ADAPTER_DESC1 desc;
		currentAdapter->GetDesc1(&desc);

		// Skip software devices
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		// Select the adapter with the most dedicated video memory
		if (desc.DedicatedVideoMemory > maxVRAM)
		{
			maxVRAM = desc.DedicatedVideoMemory;
			bestAdapter = currentAdapter;
		}
	}

	// You must have at least one valid hardware adapter
	assert(bestAdapter);

	// Print selected GPU name
	DXGI_ADAPTER_DESC1 desc;
	bestAdapter->GetDesc1(&desc);
	std::wcout << L"Using GPU: " << desc.Description << L"\n";

	// Create the D3D12 device from the best adapter
	hr = D3D12CreateDevice(bestAdapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12Device));
	assert(SUCCEEDED(hr));

	hr = d3d12Device->QueryInterface(
		IID_PPV_ARGS(&dxrDevice)
	);

	adapter = bestAdapter; // store the chosen adapter
}

void DXRTRenderer::createCommandsManagers()
{
	const D3D12_COMMAND_LIST_TYPE commandsType = D3D12_COMMAND_LIST_TYPE_DIRECT;

	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqDesc.Type = commandsType;
	HRESULT hr = d3d12Device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr));

	hr = d3d12Device->CreateCommandAllocator(commandsType, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	hr = d3d12Device->CreateCommandList(0, commandsType, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	// IMPORTANT: newly created command lists are open for recording.
	// Close it now so the allocator isn't held busy and can be Reset() later.
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	// --- DXR interface query (CRITICAL CHECK) ---
	hr = commandList->QueryInterface(IID_PPV_ARGS(&dxrCmdList));
	if (FAILED(hr))
	{
		dxrCmdList = nullptr;

		// DXR not supported – handle this gracefully
		OutputDebugStringA(
			"ERROR: ID3D12GraphicsCommandList4 not supported. DXR unavailable.\n"
		);

		// Choose ONE of these depending on your engine design:

		// Option 1: hard fail (recommended for DXR-only renderer)
		assert(false && "DXR command list not supported");

		// Option 2: throw
		// throw std::runtime_error("DXR not supported on this system");

		// Option 3: mark DXR disabled and continue with raster path
		// dxrSupported = false;
	}
}

void DXRTRenderer::createFence()
{
	HRESULT hr = d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderFramefence));
	assert(SUCCEEDED(hr));

	renderFrameEventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(renderFrameEventHandle);

	renderFramefenceValue = 1;
}

void DXRTRenderer::createSwapChain(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = 1920;
	swapChainDesc.Height = 1080;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	IDXGISwapChain1Ptr swapChain1;

	HRESULT hr = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1
	);

	assert(SUCCEEDED(hr));

	hr = swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain));
	assert(SUCCEEDED(hr));
}

void DXRTRenderer::createRenderTargetViewsFromSwapChain()
{
	for (UINT scBuffIdx = 0; scBuffIdx < FrameCount; scBuffIdx++)
	{
		const HRESULT hr = swapChain->GetBuffer(scBuffIdx, IID_PPV_ARGS(&renderTargets[scBuffIdx]));
		assert(SUCCEEDED(hr));

		rtvHandles[scBuffIdx] = swapChainRTVHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandles[scBuffIdx].ptr += scBuffIdx * rtvDescriptorSize;
		d3d12Device->CreateRenderTargetView(renderTargets[scBuffIdx], nullptr, rtvHandles[scBuffIdx]);
	}
}

void DXRTRenderer::createDescriptorHeapForSwapChain()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = FrameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	assert(SUCCEEDED(d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&swapChainRTVHeap))));

	rtvDescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void DXRTRenderer::waitForGPURenderFrame()
{
	if (renderFramefence->GetCompletedValue() < renderFramefenceValue)
	{
		HRESULT hr = renderFramefence->SetEventOnCompletion(renderFramefenceValue, renderFrameEventHandle);
		assert(SUCCEEDED(hr));

		WaitForSingleObject(renderFrameEventHandle, INFINITE);
	}
	renderFramefenceValue++;
}

void DXRTRenderer::createScene()
{
	scene = std::make_unique<CRTScene>("Scenes/Dragon.crtscene");
}

void DXRTRenderer::updateCameraCB()
{
	CameraCB cbData = {};

	cbData.cameraPosition.x = scene->getCamera().getPosition().getX();
	cbData.cameraPosition.y = scene->getCamera().getPosition().getY();
	cbData.cameraPosition.z = scene->getCamera().getPosition().getZ();
	cbData.pad0 = 0.0f;

	const CRTMatrix& r = scene->getCamera().getRotationMatrix();

	cbData.cameraRotation = DirectX::XMFLOAT4X4(
		r.get(0, 0), r.get(0, 1), r.get(0, 2), 0.0f,
		r.get(1, 0), r.get(1, 1), r.get(1, 2), 0.0f,
		r.get(2, 0), r.get(2, 1), r.get(2, 2), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	void* mapped = nullptr;
	cameraCB->Map(0, nullptr, &mapped);
	memcpy(mapped, &cbData, sizeof(CameraCB));
	cameraCB->Unmap(0, nullptr);
}

void DXRTRenderer::createDebugCB()
{
	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto desc = CD3DX12_RESOURCE_DESC::Buffer(
		(sizeof(DebugCB) + 255) & ~255
	);

	HRESULT hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&debugCB)
	);

	assert(SUCCEEDED(hr));
}

void DXRTRenderer::updateDebugCB()
{
	DebugCB cbData = {};
	cbData.shadingMode = currentShadingMode;

	void* mapped = nullptr;
	debugCB->Map(0, nullptr, &mapped);
	memcpy(mapped, &cbData, sizeof(DebugCB));
	debugCB->Unmap(0, nullptr);
}

void DXRTRenderer::createIndexBuffers()
{
	const auto& objects = scene->getObjects();

	indexBuffers.resize(objects.size());
	uploadIndexBuffers.resize(objects.size());

	auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	for (size_t i = 0; i < objects.size(); ++i)
	{
		const auto& indices = objects[i].getIndices();
		UINT size = UINT(indices.size() * sizeof(uint32_t));

		std::cout << "objectNumber: " << i << " number of Indices: " << indices.size() << std::endl;
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);

		// ---------------------------------------------------------
		// Upload buffer
		// ---------------------------------------------------------
		d3d12Device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadIndexBuffers[i])
		);

		void* data = nullptr;
		uploadIndexBuffers[i]->Map(0, nullptr, &data);
		memcpy(data, indices.data(), size);
		uploadIndexBuffers[i]->Unmap(0, nullptr);

		// ---------------------------------------------------------
		// GPU buffer
		// ---------------------------------------------------------
		d3d12Device->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&indexBuffers[i])
		);

		// ---------------------------------------------------------
		// Copy + transition
		// ---------------------------------------------------------
		commandAllocator->Reset();
		commandList->Reset(commandAllocator, nullptr);

		commandList->CopyBufferRegion(
			indexBuffers[i], 0,
			uploadIndexBuffers[i], 0,
			size
		);

		D3D12_RESOURCE_BARRIER barrier =
			CD3DX12_RESOURCE_BARRIER::Transition(
				indexBuffers[i],
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE 
			);

		commandList->ResourceBarrier(1, &barrier);
		commandList->Close();

		ID3D12CommandList* lists[] = { commandList };
		commandQueue->ExecuteCommandLists(1, lists);
		commandQueue->Signal(renderFramefence, renderFramefenceValue);
		waitForGPURenderFrame();
	}
}


void DXRTRenderer::createVertexBuffers()
{
	const auto& objects = scene->getObjects();

	vertexBuffers.resize(objects.size());
	uploadVertexBuffers.resize(objects.size());

	auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	for (size_t i = 0; i < objects.size(); ++i)
	{
		const auto& vertices = objects[i].getVertices();
		UINT size = UINT(vertices.size() * sizeof(CRTVector));

		std::cout << "objectNumber: " << i << " number of Vertices: " << vertices.size() << std::endl;
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);

		// ---------------------------------------------------------
		// Upload buffer
		// ---------------------------------------------------------
		d3d12Device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadVertexBuffers[i])
		);

		void* data = nullptr;
		uploadVertexBuffers[i]->Map(0, nullptr, &data);
		memcpy(data, vertices.data(), size);
		uploadVertexBuffers[i]->Unmap(0, nullptr);

		// ---------------------------------------------------------
		// GPU buffer
		// ---------------------------------------------------------
		d3d12Device->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&vertexBuffers[i])
		);

		// ---------------------------------------------------------
		// Copy + transition
		// ---------------------------------------------------------
		commandAllocator->Reset();
		commandList->Reset(commandAllocator, nullptr);

		commandList->CopyBufferRegion(
			vertexBuffers[i], 0,
			uploadVertexBuffers[i], 0,
			size
		);

		D3D12_RESOURCE_BARRIER barrier =
			CD3DX12_RESOURCE_BARRIER::Transition(
				vertexBuffers[i],
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
			);

		commandList->ResourceBarrier(1, &barrier);
		commandList->Close();

		ID3D12CommandList* lists[] = { commandList };
		commandQueue->ExecuteCommandLists(1, lists);
		commandQueue->Signal(renderFramefence, renderFramefenceValue);
		waitForGPURenderFrame();
	}
}



void DXRTRenderer::frameBegin()
{
	if (isChangedShadingMode)
	{
		updateDebugCB();
		isChangedShadingMode = false;
	}

	updateCameraCB();

	assert(SUCCEEDED(commandAllocator->Reset()));
	assert(SUCCEEDED(dxrCmdList->Reset(commandAllocator, nullptr)));

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	if (backBufferState != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		barrier.Transition.pResource = renderTargets[swapChainFrameIdx];
		barrier.Transition.StateBefore = backBufferState;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		dxrCmdList->ResourceBarrier(1, &barrier);
		backBufferState = D3D12_RESOURCE_STATE_COPY_DEST;
	}

	if (raytracingOutputState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		barrier.Transition.pResource = raytracingOutput;
		barrier.Transition.StateBefore = raytracingOutputState; // now initialized to COMMON
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		dxrCmdList->ResourceBarrier(1, &barrier);
		raytracingOutputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
}

void DXRTRenderer::frameEnd()
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	barrier.Transition.pResource = raytracingOutput;
	barrier.Transition.StateBefore = raytracingOutputState;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	dxrCmdList->ResourceBarrier(1, &barrier);
	raytracingOutputState = D3D12_RESOURCE_STATE_COPY_SOURCE;

	dxrCmdList->CopyResource(renderTargets[swapChainFrameIdx], raytracingOutput);

	barrier.Transition.pResource = raytracingOutput;
	barrier.Transition.StateBefore = raytracingOutputState;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	dxrCmdList->ResourceBarrier(1, &barrier);
	raytracingOutputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	barrier.Transition.pResource = renderTargets[swapChainFrameIdx];
	barrier.Transition.StateBefore = backBufferState;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	dxrCmdList->ResourceBarrier(1, &barrier);
	backBufferState = D3D12_RESOURCE_STATE_PRESENT;

	assert(SUCCEEDED(dxrCmdList->Close()));
	ID3D12CommandList* lists[] = { dxrCmdList };
	commandQueue->ExecuteCommandLists(1, lists);
	assert(SUCCEEDED(commandQueue->Signal(renderFramefence, renderFramefenceValue)));
	assert(SUCCEEDED(swapChain->Present(0, 0)));

	waitForGPURenderFrame();

	swapChainFrameIdx = swapChain->GetCurrentBackBufferIndex();
}

void DXRTRenderer::createCameraBuffer()
{
	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(
		(sizeof(CameraCB) + 255) & ~255
	);

	HRESULT hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&cameraCB)
	);
	assert(SUCCEEDED(hr));
}

void DXRTRenderer::createAccelerationStructures()
{
	const auto& objects = scene->getObjects();
	blasList.resize(objects.size());

	// Default heap for AS + scratch buffers
	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	// ---------------------------------------------------------------------
	// Reset command list (MANDATORY)
	// ---------------------------------------------------------------------
	commandAllocator->Reset();
	dxrCmdList->Reset(commandAllocator, nullptr);

	// =====================================================================
	// Build BLASes
	// =====================================================================
	for (size_t i = 0; i < objects.size(); ++i)
	{
		// -------------------------------------------------------------
		// Transition vertex & index buffers for DXR
		// -------------------------------------------------------------
		D3D12_RESOURCE_BARRIER barriers[2];

		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
			vertexBuffers[i],
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		);

		barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
			indexBuffers[i],
			D3D12_RESOURCE_STATE_INDEX_BUFFER,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		);

		dxrCmdList->ResourceBarrier(2, barriers);

		// -------------------------------------------------------------
		// Geometry description
		// -------------------------------------------------------------
		D3D12_RAYTRACING_GEOMETRY_DESC geom{};
		geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

		geom.Triangles.VertexBuffer.StartAddress =
			vertexBuffers[i]->GetGPUVirtualAddress();
		geom.Triangles.VertexBuffer.StrideInBytes = sizeof(CRTVector);
		geom.Triangles.VertexCount =
			static_cast<UINT>(objects[i].getVertices().size());
		geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

		geom.Triangles.IndexBuffer =
			indexBuffers[i]->GetGPUVirtualAddress();
		geom.Triangles.IndexCount =
			static_cast<UINT>(objects[i].getIndices().size());
		geom.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

		// -------------------------------------------------------------
		// BLAS inputs
		// -------------------------------------------------------------
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
		inputs.Type =
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputs.DescsLayout =
			D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.NumDescs = 1;
		inputs.pGeometryDescs = &geom;
		inputs.Flags =
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		// -------------------------------------------------------------
		// Query sizes
		// -------------------------------------------------------------
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
		dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(
			&inputs, &info
		);

		// -------------------------------------------------------------
		// Allocate BLAS buffer
		// -------------------------------------------------------------
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		auto blasDesc = CD3DX12_RESOURCE_DESC::Buffer(
			info.ResultDataMaxSizeInBytes,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
		);

		dxrDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&blasDesc,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			nullptr,
			IID_PPV_ARGS(&blasList[i].buffer)
		);

		// -------------------------------------------------------------
		// Allocate scratch buffer
		// -------------------------------------------------------------
		auto scratchDesc = CD3DX12_RESOURCE_DESC::Buffer(
			info.ScratchDataSizeInBytes,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
		);

		dxrDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&scratchDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&blasList[i].scratch)
		);

		// -------------------------------------------------------------
		// Build BLAS
		// -------------------------------------------------------------
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
		build.Inputs = inputs;
		build.DestAccelerationStructureData =
			blasList[i].buffer->GetGPUVirtualAddress();
		build.ScratchAccelerationStructureData =
			blasList[i].scratch->GetGPUVirtualAddress();

		dxrCmdList->BuildRaytracingAccelerationStructure(&build, 0, nullptr);

		// UAV barrier REQUIRED after AS build
		D3D12_RESOURCE_BARRIER uavBarrier =
			CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
		dxrCmdList->ResourceBarrier(1, &uavBarrier);

		blasList[i].gpuAddress =
			blasList[i].buffer->GetGPUVirtualAddress();
	}

	// =====================================================================
	// Build TLAS
	// =====================================================================

	// -------------------------------------------------------------
	// Instance buffer (CPU-> GPU)
	// -------------------------------------------------------------
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances(blasList.size());

	for (UINT i = 0; i < instances.size(); ++i)
	{
		instances[i].AccelerationStructure =
			blasList[i].gpuAddress;
		instances[i].InstanceID = i;
		instances[i].InstanceMask = 0xFF;
		instances[i].InstanceContributionToHitGroupIndex = 0;
		instances[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

		instances[i].Transform[0][0] = 1.0f;
		instances[i].Transform[1][1] = 1.0f;
		instances[i].Transform[2][2] = 1.0f;
	}

	auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto instDesc = CD3DX12_RESOURCE_DESC::Buffer(
		sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instances.size()
	);

	dxrDevice->CreateCommittedResource(
		&uploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&instDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&instanceBuffer)
	);

	void* mapped = nullptr;
	instanceBuffer->Map(0, nullptr, &mapped);
	memcpy(mapped, instances.data(), instDesc.Width);
	instanceBuffer->Unmap(0, nullptr);

	// -------------------------------------------------------------
	// TLAS inputs
	// -------------------------------------------------------------
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{};
	tlasInputs.Type =
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasInputs.DescsLayout =
		D3D12_ELEMENTS_LAYOUT_ARRAY;
	tlasInputs.NumDescs = (UINT)instances.size();
	tlasInputs.InstanceDescs =
		instanceBuffer->GetGPUVirtualAddress();
	tlasInputs.Flags =
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	// -------------------------------------------------------------
	// Query TLAS sizes
	// -------------------------------------------------------------
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasInfo{};
	dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(
		&tlasInputs, &tlasInfo
	);

	// -------------------------------------------------------------
	// Allocate TLAS buffer
	// -------------------------------------------------------------
	auto tlasDesc = CD3DX12_RESOURCE_DESC::Buffer(
		tlasInfo.ResultDataMaxSizeInBytes,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	);

	dxrDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&tlasDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr,
		IID_PPV_ARGS(&tlasBuffer)
	);

	// -------------------------------------------------------------
	// Allocate TLAS scratch
	// -------------------------------------------------------------
	auto tlasScratchDesc = CD3DX12_RESOURCE_DESC::Buffer(
		tlasInfo.ScratchDataSizeInBytes,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	);

	dxrDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&tlasScratchDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&tlasScratch)
	);

	// -------------------------------------------------------------
	// Build TLAS
	// -------------------------------------------------------------
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuild{};
	tlasBuild.Inputs = tlasInputs;
	tlasBuild.DestAccelerationStructureData =
		tlasBuffer->GetGPUVirtualAddress();
	tlasBuild.ScratchAccelerationStructureData =
		tlasScratch->GetGPUVirtualAddress();

	dxrCmdList->BuildRaytracingAccelerationStructure(&tlasBuild, 0, nullptr);

	// Required UAV barrier
	D3D12_RESOURCE_BARRIER tlasUav =
		CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
	dxrCmdList->ResourceBarrier(1, &tlasUav);

	// ---------------------------------------------------------------------
	// Execute
	// ---------------------------------------------------------------------
	dxrCmdList->Close();
	ID3D12CommandList* lists[] = { dxrCmdList };
	commandQueue->ExecuteCommandLists(1, lists);
	commandQueue->Signal(renderFramefence, renderFramefenceValue);
	waitForGPURenderFrame();
}


void DXRTRenderer::createGlobalRootSignature()
{
	D3D12_DESCRIPTOR_RANGE ranges[2] = {};

	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].RegisterSpace = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 0;
	ranges[1].RegisterSpace = 0;
	ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParams[3] = {};

	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[0].DescriptorTable.NumDescriptorRanges = 2;
	rootParams[0].DescriptorTable.pDescriptorRanges = ranges;

	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[1].Descriptor.ShaderRegister = 0;
	rootParams[1].Descriptor.RegisterSpace = 0;

	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[2].Descriptor.ShaderRegister = 1;
	rootParams[2].Descriptor.RegisterSpace = 0;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = 3;
	rootSigDesc.pParameters = rootParams;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlobPtr sigBlob;
	ID3DBlobPtr errorBlob;

	D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&sigBlob,
		&errorBlob
	);

	d3d12Device->CreateRootSignature(
		0,
		sigBlob->GetBufferPointer(),
		sigBlob->GetBufferSize(),
		IID_PPV_ARGS(&globalRootSignature)
	);
}

void DXRTRenderer::createRayTracingPipelineState()
{
	D3D12_STATE_SUBOBJECT rayGenLibSubobject = createRayGenSubObject();
	D3D12_STATE_SUBOBJECT closestHitSubobject = createClosestHitSubObject();
	D3D12_STATE_SUBOBJECT missLibSubobject = createMissLibSubObject();
	D3D12_STATE_SUBOBJECT shaderConfigSubobject = createShaderConfigSubObject();
	D3D12_STATE_SUBOBJECT pipelineConfigSubObject = createPipelineConfigSubObject();
	D3D12_STATE_SUBOBJECT rootSigSubobject = createGlobalRootSignatureSubObject();
	D3D12_STATE_SUBOBJECT hitGroupSubobject = createHitGroupSubObject();

	std::vector<D3D12_STATE_SUBOBJECT> subobjects = {
		rayGenLibSubobject,
		closestHitSubobject,
		missLibSubobject,
		shaderConfigSubobject,
		pipelineConfigSubObject,
		rootSigSubobject,
		hitGroupSubobject
	};

	D3D12_STATE_OBJECT_DESC rtpsoDesc = {};
	rtpsoDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	rtpsoDesc.NumSubobjects = (UINT)subobjects.size();
	rtpsoDesc.pSubobjects = subobjects.data();

	HRESULT hr = dxrDevice->CreateStateObject(&rtpsoDesc, IID_PPV_ARGS(&rtStateObject));

	assert(SUCCEEDED(hr));

	if (FAILED(hr))
	{
#if defined(_DEBUG)
		ID3D12InfoQueuePtr infoQueue;
		if (SUCCEEDED(dxrDevice->QueryInterface(IID_PPV_ARGS(&infoQueue))))
		{
			UINT64 count = infoQueue->GetNumStoredMessages();
			for (UINT64 i = 0; i < count; ++i)
			{
				SIZE_T length = 0;
				infoQueue->GetMessage(i, nullptr, &length);

				std::vector<char> buffer(length);
				D3D12_MESSAGE* msg = (D3D12_MESSAGE*)buffer.data();
				infoQueue->GetMessage(i, msg, &length);

				OutputDebugStringA(msg->pDescription);
				OutputDebugStringA("\n");
			}
			infoQueue->ClearStoredMessages();
		}
#endif

		assert(false);
	}
}

void DXRTRenderer::createRayTracingShaderTexture()
{
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = 1920;
	texDesc.Height = 1080;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&raytracingOutput)
	);

	raytracingOutputState = D3D12_RESOURCE_STATE_COMMON;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 2;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	d3d12Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&uavHeap));

	UINT inc = d3d12Device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		uavHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location =
		tlasBuffer->GetGPUVirtualAddress();

	d3d12Device->CreateShaderResourceView(
		nullptr,
		&srvDesc,
		handle
	);

	handle.ptr += inc;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	d3d12Device->CreateUnorderedAccessView(
		raytracingOutput,
		nullptr,
		&uavDesc,
		handle
	);

	// CPU-only heap for ClearUAV
	D3D12_DESCRIPTOR_HEAP_DESC cpuHeapDesc = {};
	cpuHeapDesc.NumDescriptors = 1;
	cpuHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cpuHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // CPU only
	d3d12Device->CreateDescriptorHeap(&cpuHeapDesc, IID_PPV_ARGS(&clearHeap));

	// Create a UAV descriptor in this CPU-only heap
	uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3d12Device->CreateUnorderedAccessView(
		raytracingOutput,
		nullptr,
		&uavDesc,
		clearHeap->GetCPUDescriptorHandleForHeapStart()
	);
}


inline UINT alignedSize(UINT size, UINT alignBytes)
{
	return alignBytes * (size / alignBytes + (size % alignBytes ? 1 : 0));
}

void DXRTRenderer::createShaderBindingTable()
{
	ID3D12StateObjectPropertiesPtr rtStateObjectProps;
	HRESULT hr = rtStateObject->QueryInterface(IID_PPV_ARGS(&rtStateObjectProps));

	void* rayGenID = rtStateObjectProps->GetShaderIdentifier(L"rayGen");
	void* missID = rtStateObjectProps->GetShaderIdentifier(L"miss");
	void* hitGroupID = rtStateObjectProps->GetShaderIdentifier(L"HitGroup");

	const UINT shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	const UINT recordSize = alignedSize(shaderIDSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

	UINT rayGenOffset = 0;
	UINT missOffset = alignedSize(rayGenOffset + recordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	UINT hitGroupOffset = alignedSize(missOffset + recordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

	const UINT sbtSize = hitGroupOffset + recordSize;

	// Allocate GPU upload + default heap as before
	createSBTUploadHeap(sbtSize);
	createSBTDefaultHeap(sbtSize);

	// Copy shader IDs only, no descriptor handle for now
	copySBTDataToUploadHeap(rayGenOffset, missOffset, hitGroupOffset, rayGenID, missID, hitGroupID);
	copySBTDataToDefaultHeap();

	// Prepare DispatchRaysDesc with correct offsets
	prepareDispatchRaysDesc(recordSize, rayGenOffset, missOffset, hitGroupOffset);

}

IDxcBlobPtr DXRTRenderer::compileShader(
	const std::wstring& fileName,
	const std::wstring& /*entryPoint*/,
	const std::wstring& target)
{
	IDxcUtilsPtr utils;
	IDxcCompiler3Ptr compiler;

	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	if (FAILED(hr)) return nullptr;

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	if (FAILED(hr)) return nullptr;

	IDxcBlobEncodingPtr sourceBlob;
	hr = utils->LoadFile(fileName.c_str(), nullptr, &sourceBlob);
	if (FAILED(hr)) return nullptr;

	DxcBuffer source{};
	source.Ptr = sourceBlob->GetBufferPointer();
	source.Size = sourceBlob->GetBufferSize();
	source.Encoding = DXC_CP_UTF8;

	LPCWSTR args[] =
	{
		L"-T", target.c_str(),   // lib_6_5
		L"-Zi",
		L"-Qembed_debug",
		L"-Od"
	};

	IDxcResultPtr result;
	hr = compiler->Compile(&source, args, _countof(args), nullptr, IID_PPV_ARGS(&result));
	if (FAILED(hr)) return nullptr;

	// Print errors
	IDxcBlobUtf8Ptr errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
		OutputDebugStringA(errors->GetStringPointer());

	HRESULT status;
	result->GetStatus(&status);
	if (FAILED(status))
		return nullptr;

	IDxcBlobPtr blob;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr);
	return blob;
}



D3D12_STATE_SUBOBJECT DXRTRenderer::createRayGenSubObject()
{
	rayGenBlob = compileShader(L"HLSL/ray_tracing_shaders.hlsl", L"rayGen", L"lib_6_5");

	rayGenExportDesc = D3D12_EXPORT_DESC{};
	rayGenExportDesc.Name = L"rayGen";
	rayGenExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
	
	rayGenLib.DXILLibrary.pShaderBytecode = rayGenBlob->GetBufferPointer();
	rayGenLib.DXILLibrary.BytecodeLength = rayGenBlob->GetBufferSize();
	rayGenLib.NumExports = 1;
	rayGenLib.pExports = &rayGenExportDesc;
	
	D3D12_STATE_SUBOBJECT rayGenLibSubobject = {};
	rayGenLibSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	rayGenLibSubobject.pDesc = &rayGenLib;

	return rayGenLibSubobject;
}

D3D12_STATE_SUBOBJECT DXRTRenderer::createClosestHitSubObject()
{
	closestHitBlob = compileShader(L"HLSL/ray_tracing_shaders.hlsl", L"closestHit", L"lib_6_5");

	closestHitExportDesc = D3D12_EXPORT_DESC{};
	closestHitExportDesc.Name = L"closestHit";
	closestHitExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

	closestHitLib.DXILLibrary.pShaderBytecode = closestHitBlob->GetBufferPointer();
	closestHitLib.DXILLibrary.BytecodeLength = closestHitBlob->GetBufferSize();
	closestHitLib.NumExports = 1;
	closestHitLib.pExports = &closestHitExportDesc;

	D3D12_STATE_SUBOBJECT closestHitLibSubobject = {};
	closestHitLibSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	closestHitLibSubobject.pDesc = &closestHitLib;

	return closestHitLibSubobject;
}

D3D12_STATE_SUBOBJECT DXRTRenderer::createMissLibSubObject()
{
	missBlob = compileShader(L"HLSL/ray_tracing_shaders.hlsl", L"miss", L"lib_6_5");

	missExportDesc = D3D12_EXPORT_DESC{};
	missExportDesc.Name = L"miss";
	missExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

	missLib.DXILLibrary.pShaderBytecode = missBlob->GetBufferPointer();
	missLib.DXILLibrary.BytecodeLength = missBlob->GetBufferSize();
	missLib.NumExports = 1;
	missLib.pExports = &missExportDesc;

	D3D12_STATE_SUBOBJECT missLibSubobject = {};
	missLibSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	missLibSubobject.pDesc = &missLib;

	return missLibSubobject;
}

D3D12_STATE_SUBOBJECT DXRTRenderer::createShaderConfigSubObject()
{
	shaderConfig = D3D12_RAYTRACING_SHADER_CONFIG{};
	shaderConfig.MaxPayloadSizeInBytes = 4 * sizeof(float);
	shaderConfig.MaxAttributeSizeInBytes = 2 * sizeof(float);

	D3D12_STATE_SUBOBJECT shaderConfigSubobject = {};
	shaderConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
	shaderConfigSubobject.pDesc = &shaderConfig;

	return shaderConfigSubobject;
}

D3D12_STATE_SUBOBJECT DXRTRenderer::createPipelineConfigSubObject()
{
	pipelineConfig = D3D12_RAYTRACING_PIPELINE_CONFIG{};
	pipelineConfig.MaxTraceRecursionDepth = 1;

	D3D12_STATE_SUBOBJECT pipelineConfigSubobject = {};
	pipelineConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	pipelineConfigSubobject.pDesc = &pipelineConfig;

	return pipelineConfigSubobject;
}

D3D12_STATE_SUBOBJECT DXRTRenderer::createGlobalRootSignatureSubObject()
{
	rootSigDesc = D3D12_GLOBAL_ROOT_SIGNATURE{ globalRootSignature };

	D3D12_STATE_SUBOBJECT rootSigSubobject = {};
	rootSigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	rootSigSubobject.pDesc = &rootSigDesc;

	return rootSigSubobject;
}

D3D12_STATE_SUBOBJECT DXRTRenderer::createHitGroupSubObject()
{
	hitGroupDesc = D3D12_HIT_GROUP_DESC{};
	hitGroupDesc.HitGroupExport = L"HitGroup";
	hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
	hitGroupDesc.ClosestHitShaderImport = L"closestHit";

	D3D12_STATE_SUBOBJECT hitGroupSubobject = {};
	hitGroupSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	hitGroupSubobject.pDesc = &hitGroupDesc;

	return hitGroupSubobject;
}

void DXRTRenderer::createSBTUploadHeap(const UINT sbtSize)
{
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC sbtDesc = {};
	sbtDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	sbtDesc.Alignment = 0;
	sbtDesc.Width = sbtSize;
	sbtDesc.Height = 1;
	sbtDesc.DepthOrArraySize = 1;
	sbtDesc.MipLevels = 1;
	sbtDesc.Format = DXGI_FORMAT_UNKNOWN;
	sbtDesc.SampleDesc.Count = 1;
	sbtDesc.SampleDesc.Quality = 0;
	sbtDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	sbtDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&sbtDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sbtUploadBuff)
	);
}

void DXRTRenderer::createSBTDefaultHeap(const UINT sbtSize)
{
	D3D12_HEAP_PROPERTIES defaultHeapProps = {};
	defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	defaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	defaultHeapProps.CreationNodeMask = 1;
	defaultHeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC sbtDesc = {};
	sbtDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	sbtDesc.Alignment = 0;
	sbtDesc.Width = sbtSize;
	sbtDesc.Height = 1;
	sbtDesc.DepthOrArraySize = 1;
	sbtDesc.MipLevels = 1;
	sbtDesc.Format = DXGI_FORMAT_UNKNOWN;
	sbtDesc.SampleDesc.Count = 1;
	sbtDesc.SampleDesc.Quality = 0;
	sbtDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	sbtDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	d3d12Device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&sbtDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&sbtDefaultBuff)
	);
}

void DXRTRenderer::copySBTDataToUploadHeap(
	const UINT rayGenOffset, const UINT missOffset, const UINT hitGroupOffset,
	void* rayGenID, void* missID, void* hitGroupID)
{
	uint8_t* pData = nullptr;
	sbtUploadBuff->Map(0, nullptr, reinterpret_cast<void**>(&pData));

	auto writeRecord = [&](UINT offset, void* shaderID)
		{
			memcpy(pData + offset, shaderID, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

			D3D12_GPU_DESCRIPTOR_HANDLE handle =
				uavHeap->GetGPUDescriptorHandleForHeapStart();

			memcpy(
				pData + offset + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
				&handle,
				sizeof(handle)
			);
		};

	writeRecord(rayGenOffset, rayGenID);
	writeRecord(missOffset, missID);
	writeRecord(hitGroupOffset, hitGroupID);

	sbtUploadBuff->Unmap(0, nullptr);
}

void DXRTRenderer::copySBTDataToDefaultHeap()
{
	HRESULT hr = commandAllocator->Reset();
	hr = commandList->Reset(commandAllocator, nullptr);

	commandList->CopyResource(sbtDefaultBuff, sbtUploadBuff);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barrier.Transition.pResource = sbtDefaultBuff;

	commandList->ResourceBarrier(1, &barrier);

	hr = commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	
	// Signal fence AFTER executing commands
	hr = commandQueue->Signal(renderFramefence, renderFramefenceValue);
	assert(SUCCEEDED(hr));

	waitForGPURenderFrame();
}

void DXRTRenderer::prepareDispatchRaysDesc(
	const UINT recordSize,
	const UINT rayGenOffset,
	const UINT missOffset,
	const UINT hitGroupOffset)
{
	raysDesc.RayGenerationShaderRecord.StartAddress =
		sbtDefaultBuff->GetGPUVirtualAddress() + rayGenOffset;
	raysDesc.RayGenerationShaderRecord.SizeInBytes =
		recordSize;

	raysDesc.MissShaderTable.StartAddress =
		sbtDefaultBuff->GetGPUVirtualAddress() + missOffset;
	raysDesc.MissShaderTable.SizeInBytes =
		recordSize;
	raysDesc.MissShaderTable.StrideInBytes =
		recordSize;

	raysDesc.HitGroupTable.StartAddress =
		sbtDefaultBuff->GetGPUVirtualAddress() + hitGroupOffset;
	raysDesc.HitGroupTable.SizeInBytes =
		recordSize;
	raysDesc.HitGroupTable.StrideInBytes =
		recordSize;

	raysDesc.Width = 1920;
	raysDesc.Height = 1080;
	raysDesc.Depth = 1;
	raysDesc.CallableShaderTable = {};
}

void DXRTRenderer::stopRendering()
{
	//waitForGPURenderFrame(); //Bug arises
}

void DXRTRenderer::changeShadingMode(uint32_t value)
{
	currentShadingMode = value;
	isChangedShadingMode = true;
}

CRTScene& DXRTRenderer::getScene()
{
	return *scene;
}

void DXRTRenderer::renderFrame()
{
	frameBegin();

	ID3D12DescriptorHeap* heaps[] = { uavHeap };
	dxrCmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	dxrCmdList->SetComputeRootSignature(globalRootSignature);
	dxrCmdList->SetComputeRootDescriptorTable(0, uavHeap->GetGPUDescriptorHandleForHeapStart());
	dxrCmdList->SetComputeRootConstantBufferView(1, cameraCB->GetGPUVirtualAddress());
	dxrCmdList->SetComputeRootConstantBufferView(
		2,
		debugCB->GetGPUVirtualAddress()
	);
	dxrCmdList->SetPipelineState1(rtStateObject);
	FLOAT clearColor[4] = { 0.f, 0.f, 1.f, 1.f };

	UINT inc = d3d12Device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	CD3DX12_GPU_DESCRIPTOR_HANDLE uavGpuHandle(
		uavHeap->GetGPUDescriptorHandleForHeapStart(),
		1, // UAV is descriptor #1
		inc
	);

	dxrCmdList->ClearUnorderedAccessViewFloat(
		uavGpuHandle,
		clearHeap->GetCPUDescriptorHandleForHeapStart(),
		raytracingOutput,
		clearColor,
		0,
		nullptr
	);

	dxrCmdList->DispatchRays(&raysDesc);

	frameEnd();
}

