#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#include <windows.h>       // Windows base headers

#include "DXRTRenderer.h"
#include <iostream>
#include <assert.h>
#include <DXGItype.h>
#include <fstream>
#include <directx/d3dx12_core.h> // Helper structs, like CD3DX12_HEAP_PROPERTIES

#include "CompiledShaders/ConstColor.hlsl.h"
#include "CompiledShaders/ConstColorVS.hlsl.h"

#include <wrl.h>
using Microsoft::WRL::ComPtr;

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
	createViewport();

	prepareForRayTracing();
}

void DXRTRenderer::prepareForRayTracing()
{
	createGlobalRootSignature();
	createRayTracingPipelineState();
	createRayTracingShaderTexture();
	createShaderBindingTable();
}

QImage DXRTRenderer::getQImageForFrame()
{
	void* renderTargetData = nullptr;
	HRESULT hr = readbackBuffer->Map(0, nullptr, &renderTargetData);
	assert(SUCCEEDED(hr));

	// Create a QImage with the same dimensions
	QImage image(textureDesc.Width, textureDesc.Height, QImage::Format_RGBA8888);

	// Copy data row by row, because GPU row pitch may be larger than width * 4
	for (UINT row = 0; row < textureDesc.Height; row++)
	{
		UINT rowPitch = renderTargetFootprint.Footprint.RowPitch;
		uint8_t* srcRow = reinterpret_cast<uint8_t*>(renderTargetData) + row * rowPitch;
		uint8_t* dstRow = image.scanLine(row);

		// Copy only width * 4 bytes (RGBA) per row
		memcpy(dstRow, srcRow, textureDesc.Width * RGBA_COLOR_CHANNELS_COUNT);
	}

	readbackBuffer->Unmap(0, nullptr);

	// Optional: if you need RGB only, convert:
	// image = image.convertToFormat(QImage::Format_RGB888);

	return image;
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


//void DXRTRenderer::createDevice()
//{
//	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
//	assert(SUCCEEDED(hr));
//
//	for (UINT adapterIndex = 0; dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; adapterIndex++)
//	{
//		DXGI_ADAPTER_DESC1 desc;
//		adapter->GetDesc1(&desc);
//
//		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12Device))))
//		{
//			std::wcout << "Using GPU: " << desc.Description << "\n";
//			break;
//		}
//	}
//	assert(adapter);
//}

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

void DXRTRenderer::createGPUTexture()
{
	textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 800, 600);
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	const HRESULT hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		nullptr,
		IID_PPV_ARGS(&renderTarget)
	);

	assert(SUCCEEDED(hr));
}

void DXRTRenderer::createRenderTargetView()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 1;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	assert(SUCCEEDED(d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap))));

	rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	d3d12Device->CreateRenderTargetView(renderTarget, nullptr, rtvHandle);
}

void DXRTRenderer::generateConstColorTexture()
{
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	//FLOAT clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, rendColor, 0, nullptr);
}

void DXRTRenderer::createReadbackBuffer()
{
	UINT64 readbackBufferSize = 0;

	d3d12Device->GetCopyableFootprints(
		&textureDesc, 0, 1, 0, &renderTargetFootprint,
		nullptr, nullptr, &readbackBufferSize
	);

	D3D12_HEAP_PROPERTIES readbackHeapProps = { D3D12_HEAP_TYPE_READBACK };
	D3D12_RESOURCE_DESC readbackDesc = {};
	readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	readbackDesc.Width = readbackBufferSize;
	readbackDesc.Height = 1;
	readbackDesc.DepthOrArraySize = 1;
	readbackDesc.MipLevels = 1;
	readbackDesc.SampleDesc.Count = 1;
	readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = d3d12Device->CreateCommittedResource(
		&readbackHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&readbackDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&readbackBuffer)
	);

	assert(SUCCEEDED(hr));
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
	swapChainDesc.Width = 800;
	swapChainDesc.Height = 800;
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

void DXRTRenderer::createVertexBuffer()
{
	// Vertex data
	Vertex triangleVertices[] = {
		{  0.0f,  0.5f },
		{  0.5f, -0.5f },
		{ -0.5f, -0.5f }
	};
	const UINT vertexBufferSize = sizeof(triangleVertices);

	// Create default heap (GPU memory)
	D3D12_HEAP_PROPERTIES defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

	HRESULT hr = d3d12Device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, // Start as copy destination
		nullptr,
		IID_PPV_ARGS(&vertexBuffer)
	);
	assert(SUCCEEDED(hr));

	// Create upload heap (CPU accessible)
	D3D12_HEAP_PROPERTIES uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	hr = d3d12Device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexBufferUpload)
	);
	assert(SUCCEEDED(hr));

	// Copy vertex data to upload heap
	void* pVertexData;
	vertexBufferUpload->Map(0, nullptr, &pVertexData);
	memcpy(pVertexData, triangleVertices, vertexBufferSize);
	vertexBufferUpload->Unmap(0, nullptr);

	assert(SUCCEEDED(commandAllocator->Reset()));
	assert(SUCCEEDED(commandList->Reset(commandAllocator, nullptr)));

	// Schedule copy from upload heap -> default heap
	commandList->CopyBufferRegion(vertexBuffer, 0, vertexBufferUpload, 0, vertexBufferSize);

	// Transition default heap to VERTEX_AND_CONSTANT_BUFFER state
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Transition.pResource = vertexBuffer;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	
	commandList->ResourceBarrier(1, &barrier);

	// Create vertex buffer view
	vbView = D3D12_VERTEX_BUFFER_VIEW{};
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vbView.StrideInBytes = sizeof(Vertex);
	vbView.SizeInBytes = vertexBufferSize;

	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList* ppCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Signal fence
	hr = commandQueue->Signal(renderFramefence, renderFramefenceValue);
	assert(SUCCEEDED(hr));

	waitForGPURenderFrame();
}


void DXRTRenderer::createRootSignature()
{
	constant.ShaderRegister = 0;
	constant.RegisterSpace = 0;
	constant.Num32BitValues = 1;
	
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param.Constants = constant;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.NumParameters = 1;
	rootSignatureDesc.pParameters = &param;

	ID3DBlobPtr signature;
	ID3DBlobPtr error;
	D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signature,
		&error
	);

	HRESULT hr = d3d12Device->CreateRootSignature(
		0,
		signature->GetBufferPointer(),
		signature->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)
	);
	assert(SUCCEEDED(hr));
}

void DXRTRenderer::createPipelineState()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	psoDesc.pRootSignature = rootSignature;
	psoDesc.PS = { g_const_color, _countof(g_const_color) };
	psoDesc.VS = { g_const_color_vs, _countof(g_const_color_vs) };
	psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	HRESULT hr = d3d12Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&state));
}

void DXRTRenderer::createViewport()
{
	viewport = D3D12_VIEWPORT{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = 800.f;
	viewport.Height = 800.f;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	scissorRect = D3D12_RECT{};
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = 800;
	scissorRect.bottom = 800;
}

void DXRTRenderer::recordExecuteAndReadback()
{
	// Transition render target to COPY_SOURCE
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = renderTarget;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	// Copy to readback buffer
	D3D12_TEXTURE_COPY_LOCATION dest = {};
	dest.pResource = readbackBuffer;
	dest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dest.PlacedFootprint = renderTargetFootprint;

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = renderTarget;
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0;

	commandList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);

	// Transition back to RENDER_TARGET (optional)
	std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
	commandList->ResourceBarrier(1, &barrier);

	// Close command list after recording everything
	HRESULT hr = commandList->Close();
	assert(SUCCEEDED(hr));

	// Execute on GPU
	ID3D12CommandList* lists[] = { commandList };
	commandQueue->ExecuteCommandLists(1, lists);

	// Signal fence
	hr = commandQueue->Signal(renderFramefence, renderFramefenceValue);
	assert(SUCCEEDED(hr));
}


void DXRTRenderer::writeImageToFile()
{
	void* renderTargetData;
	HRESULT hr = readbackBuffer->Map(0, nullptr, &renderTargetData);
	assert(SUCCEEDED(hr));

	std::string filename("output.ppm");
	std::ofstream file(filename, std::ios::out | std::ios::binary);
	assert(file);

	file << "P3\n" << textureDesc.Width <<' '<< textureDesc.Height << "\n255\n";

	for (UINT rowIdx = 0; rowIdx < textureDesc.Height; rowIdx++)
	{
		UINT rowPitch = renderTargetFootprint.Footprint.RowPitch;
		uint8_t* rowData = reinterpret_cast<uint8_t*>(renderTargetData) + rowIdx * rowPitch;
		for (UINT64 colIdx = 0; colIdx < textureDesc.Width; colIdx++)
		{
			uint8_t* pixelData = rowData + colIdx * RGBA_COLOR_CHANNELS_COUNT;
			for (int channelIdx = 0; channelIdx < RGBA_COLOR_CHANNELS_COUNT - 1; channelIdx++)
			{
				file << static_cast<int>(pixelData[channelIdx]) << ' ';
			}
		}
		file << '\n';
	}

	file.close();
	readbackBuffer->Unmap(0, nullptr);
}

void DXRTRenderer::rotateTriangleVertices()
{
	float angle = static_cast<float>(frameIdx) * 0.01f;
	float cosA = cosf(angle);
	float sinA = sinf(angle);

	Vertex triangleVertices[] = {
		{  0.0f * cosA - 0.5f * sinA,  0.0f * sinA + 0.5f * cosA },
		{  0.5f * cosA - -0.5f * sinA,  0.5f * sinA + -0.5f * cosA },
		{ -0.5f * cosA - -0.5f * sinA, -0.5f * sinA + -0.5f * cosA }
	};

	void* pVertexData;
	vertexBuffer->Map(0, nullptr, &pVertexData);
	memcpy(pVertexData, triangleVertices, sizeof(triangleVertices));
	vertexBuffer->Unmap(0, nullptr);
}

void DXRTRenderer::frameBegin()
{
	assert(SUCCEEDED(commandAllocator->Reset()));
	assert(SUCCEEDED(dxrCmdList->Reset(commandAllocator, nullptr)));
	
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = renderTargets[swapChainFrameIdx];
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	dxrCmdList->ResourceBarrier(1, &barrier);

	barrier = {};
	barrier.Transition.pResource = raytracingOutput;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	dxrCmdList->ResourceBarrier(1, &barrier);

	//commandList->OMSetRenderTargets(1, &rtvHandles[swapChainFrameIdx], FALSE, nullptr);
	//commandList->ClearRenderTargetView(rtvHandles[swapChainFrameIdx], rendColor, 0, nullptr);

	////rotateTriangleVertices(); only if vertices are in an upload heap

	//float frameCoef = static_cast<float>(frameIdx % 1000) / 1000.f;	

	//rendColor[0] = frameCoef;
	//rendColor[1] = 1.f - frameCoef;
	//rendColor[2] = 0.f;
	//rendColor[3] = 1.f;
}

void DXRTRenderer::frameEnd()
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = raytracingOutput;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	dxrCmdList->ResourceBarrier(1, &barrier);

	dxrCmdList->CopyResource(renderTargets[swapChainFrameIdx], raytracingOutput);

	barrier.Transition.pResource = renderTargets[swapChainFrameIdx];
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	dxrCmdList->ResourceBarrier(1, &barrier);

	HRESULT hr = dxrCmdList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList* ppCommandLists[] = { dxrCmdList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Signal fence
	hr = commandQueue->Signal(renderFramefence, renderFramefenceValue);
	assert(SUCCEEDED(hr));

	hr = swapChain->Present(0, 0);
	assert(SUCCEEDED(hr));

	// Wait for GPU to finish
	waitForGPURenderFrame();

	frameIdx++;
	swapChainFrameIdx = swapChain->GetCurrentBackBufferIndex();
}

void DXRTRenderer::createGlobalRootSignature()
{
	D3D12_DESCRIPTOR_RANGE uavRange = {};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 1;
	uavRange.BaseShaderRegister = 0;
	uavRange.RegisterSpace = 0;

	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam.DescriptorTable.NumDescriptorRanges = 1;
	rootParam.DescriptorTable.pDescriptorRanges = &uavRange;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = 1;
	rootSigDesc.pParameters = &rootParam;
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers = nullptr;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlobPtr sigBlob;
	ID3DBlobPtr errorBlob;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = d3d12Device->CreateRootSignature(
		0,
		sigBlob->GetBufferPointer(),
		sigBlob->GetBufferSize(),
		IID_PPV_ARGS(&globalRootSignature)
	);
	assert(SUCCEEDED(hr));
}

void DXRTRenderer::createRayTracingPipelineState()
{
	D3D12_STATE_SUBOBJECT rayGenLibSubobject = createRayGenSubObject();
	D3D12_STATE_SUBOBJECT missLibSubobject = createMissLibSubObject();
	D3D12_STATE_SUBOBJECT shaderConfigSubobject = createShaderConfigSubObject();
	D3D12_STATE_SUBOBJECT pipelineConfigSubObject = createPipelineConfigSubObject();
	D3D12_STATE_SUBOBJECT rootSigSubobject = createGlobalRootSignatureSubObject();

	std::vector<D3D12_STATE_SUBOBJECT> subobjects = {
		rayGenLibSubobject,
		missLibSubobject,
		shaderConfigSubobject,
		pipelineConfigSubObject,
		rootSigSubobject
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
texDesc.Width = 800;
texDesc.Height = 800;
texDesc.DepthOrArraySize = 1;            // required
texDesc.MipLevels = 1;                   // recommended
texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
texDesc.SampleDesc.Count = 1;            // required
texDesc.SampleDesc.Quality = 0;          // required
texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&raytracingOutput)
	);

	if (FAILED(hr))
	{
#if defined(_DEBUG)
		ID3D12InfoQueue* infoQueue = nullptr;
		if (SUCCEEDED(d3d12Device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
		{
			const UINT64 messageCount = infoQueue->GetNumStoredMessages();
			for (UINT64 i = 0; i < messageCount; ++i)
			{
				SIZE_T messageLength = 0;
				infoQueue->GetMessage(i, nullptr, &messageLength);

				std::vector<char> bytes(messageLength);
				D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
				infoQueue->GetMessage(i, message, &messageLength);

				OutputDebugStringA("D3D12 ERROR: ");
				OutputDebugStringA(message->pDescription);
				OutputDebugStringA("\n");
			}
			infoQueue->ClearStoredMessages();
			infoQueue->Release();
		}
#endif
		assert(false && "CreateCommittedResource failed");
		return;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	
	
	hr = d3d12Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&uavHeap));
	assert(SUCCEEDED(hr));

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	d3d12Device->CreateUnorderedAccessView(
		raytracingOutput,
		nullptr,
		&uavDesc,
		uavHeap->GetCPUDescriptorHandleForHeapStart()
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

	const UINT shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	const UINT recordSize = alignedSize(shaderIDSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	const UINT sbtSize = alignedSize(recordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

	createSBTUploadHeap(sbtSize);
	createSBTDefaultHeap(sbtSize);
	copySBTDataToUploadHeap(rayGenID);
	copySBTDataToDefaultHeap();
	prepareDispatchRaysDesc(sbtSize);
}

IDxcBlobPtr DXRTRenderer::compileShader(
	const std::wstring& fileName,
	const std::wstring& entryPoint,
	const std::wstring& target)
{
	HRESULT hr;

	IDxcUtilsPtr utils;
	IDxcCompiler3Ptr compiler;

	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create IDxcUtils\n");
		return nullptr;
	}

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create IDxcCompiler3\n");
		return nullptr;
	}

	IDxcBlobEncodingPtr sourceBlob;
	hr = utils->LoadFile(fileName.c_str(), nullptr, &sourceBlob);
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to load shader file\n");
		return nullptr;
	}

	DxcBuffer source = {};
	source.Ptr = sourceBlob->GetBufferPointer();
	source.Size = sourceBlob->GetBufferSize();
	source.Encoding = DXC_CP_UTF8;

	LPCWSTR args[] =
	{
		L"-E", entryPoint.c_str(),
		L"-T", target.c_str(),
		L"-Zi",
		L"-Qembed_debug",
		L"-Od",
		L"-Zpr"
	};

	IDxcResultPtr result;
	hr = compiler->Compile(
		&source,
		args,
		_countof(args),
		nullptr,
		IID_PPV_ARGS(&result)
	);

	if (FAILED(hr))
	{
		OutputDebugStringA("DXC Compile() call failed\n");
		return nullptr;
	}

	// Print warnings / errors
	IDxcBlobUtf8Ptr errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
	{
		OutputDebugStringA(errors->GetStringPointer());
	}

	// CHECK ACTUAL COMPILE STATUS
	HRESULT compileStatus = S_OK;
	hr = result->GetStatus(&compileStatus);
	if (FAILED(hr) || FAILED(compileStatus))
	{
		OutputDebugStringA("Shader compilation failed\n");
		return nullptr;
	}

	// Get compiled DXIL
	IDxcBlobPtr shaderBlob;
	hr = result->GetOutput(
		DXC_OUT_OBJECT,
		IID_PPV_ARGS(&shaderBlob),
		nullptr
	);

	if (FAILED(hr) || shaderBlob == nullptr)
	{
		OutputDebugStringA("Failed to retrieve compiled shader blob\n");
		return nullptr;
	}

	return shaderBlob;
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
	shaderConfig.MaxPayloadSizeInBytes = 4 * 4;

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
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sbtDefaultBuff)
	);
}

void DXRTRenderer::copySBTDataToUploadHeap(void* rayGenID)
{
	uint8_t* pData = nullptr;
	sbtUploadBuff->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	memcpy(pData, rayGenID, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
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

void DXRTRenderer::prepareDispatchRaysDesc(const UINT sbtSize)
{
	raysDesc.RayGenerationShaderRecord.StartAddress = sbtDefaultBuff->GetGPUVirtualAddress();
	raysDesc.RayGenerationShaderRecord.SizeInBytes = sbtSize;
	raysDesc.Width = 800;
	raysDesc.Height = 800;
	raysDesc.Depth = 1;
	raysDesc.MissShaderTable = {};
	raysDesc.HitGroupTable = {};
	raysDesc.CallableShaderTable = {};
}

void DXRTRenderer::stopRendering()
{
	//waitForGPURenderFrame(); Bug arises
}

void DXRTRenderer::renderFrame()
{
	// Reset allocator & command list before recording
	frameBegin();

	ID3D12DescriptorHeap* heaps[] = { uavHeap };
	dxrCmdList->SetDescriptorHeaps(_countof(heaps), heaps);

	dxrCmdList->SetComputeRootSignature(globalRootSignature);

	dxrCmdList->SetComputeRootDescriptorTable(0, uavHeap->GetGPUDescriptorHandleForHeapStart());

	dxrCmdList->SetPipelineState1(rtStateObject);

	dxrCmdList->DispatchRays(&raysDesc);

	frameEnd();
}

void DXRTRenderer::renderFrameWithSwapChain()
{
	frameBegin();
	
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Transition.pResource = renderTargets[swapChainFrameIdx];
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier);

	commandList->OMSetRenderTargets(1, &rtvHandles[swapChainFrameIdx], FALSE, nullptr);
	commandList->ClearRenderTargetView(rtvHandles[swapChainFrameIdx], rendColor, 0, nullptr);

	barrier.Transition.pResource = renderTargets[swapChainFrameIdx];
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList->ResourceBarrier(1, &barrier);

	commandList->Close();

	ID3D12CommandList* ppCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	commandQueue->Signal(renderFramefence, renderFramefenceValue);

	swapChain->Present(0, 0);

	waitForGPURenderFrame();
	frameEnd();	
}

