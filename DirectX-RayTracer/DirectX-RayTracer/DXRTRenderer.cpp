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
	createRootSignature();
	createVertexBuffer();
	createViewport();
	createPipelineState();

	/*createGPUTexture();
	createRenderTargetView();
	createReadbackBuffer();*/
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
}

void DXRTRenderer::createVertexBuffer()
{
	Vertex triangleVertices[] = {
		{  0.0f,  0.5f },
		{  0.5f, -0.5f },
		{ -0.5f, -0.5f }
	};

	D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(triangleVertices));

	HRESULT hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexBuffer)
	);

	assert(SUCCEEDED(hr));

	void* pVertexData;
	vertexBuffer->Map(0, nullptr, &pVertexData);
	memcpy(pVertexData, triangleVertices, sizeof(triangleVertices));
	vertexBuffer->Unmap(0, nullptr);

	vbView = D3D12_VERTEX_BUFFER_VIEW{};
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vbView.StrideInBytes = sizeof(Vertex);
	vbView.SizeInBytes = sizeof(triangleVertices);
}

void DXRTRenderer::createRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

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

void DXRTRenderer::frameBegin()
{
	assert(SUCCEEDED(commandAllocator->Reset()));
	assert(SUCCEEDED(commandList->Reset(commandAllocator, nullptr)));
	
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = renderTargets[swapChainFrameIdx];
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier);

	commandList->OMSetRenderTargets(1, &rtvHandles[swapChainFrameIdx], FALSE, nullptr);
	commandList->ClearRenderTargetView(rtvHandles[swapChainFrameIdx], rendColor, 0, nullptr);

	float frameCoef = static_cast<float>(frameIdx % 1000) / 1000.f;	

	rendColor[0] = frameCoef;
	rendColor[1] = 1.f - frameCoef;
	rendColor[2] = 0.f;
	rendColor[3] = 1.f;
}

void DXRTRenderer::frameEnd()
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = renderTargets[swapChainFrameIdx];
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList->ResourceBarrier(1, &barrier);

	HRESULT hr = commandList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList* ppCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Signal fence
	hr = commandQueue->Signal(renderFramefence, renderFramefenceValue);
	assert(SUCCEEDED(hr));

	hr = swapChain->Present(0, 0);
	assert(SUCCEEDED(hr));

	// Wait for GPU to finish
	waitForGPURenderFrame();
	renderFramefenceValue++;

	frameIdx++;

	swapChainFrameIdx = swapChain->GetCurrentBackBufferIndex();
}

void DXRTRenderer::stopRendering()
{
	//waitForGPURenderFrame(); Bug arises
}

void DXRTRenderer::renderFrame()
{
	// Reset allocator & command list before recording
	frameBegin();

	commandList->SetPipelineState(state);

	commandList->SetGraphicsRootSignature(rootSignature);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vbView);

	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	commandList->DrawInstanced(3, 1, 0, 0);

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

