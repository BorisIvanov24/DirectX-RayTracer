#include "CDXCRenderer.h"
#include <iostream>
#include <assert.h>
#include <DXGItype.h>
#include <fstream>

void CDXCRenderer::render()
{
	prepareForRendering();
	//renderFrame();
	//cleanUp();
}

void CDXCRenderer::prepareForRendering()
{
	createDevice();
	createCommandsManagers();
	createGPUTexture();
	createRenderTargetView();
	createReadbackBuffer();
	createFence();
}

void CDXCRenderer::createDevice()
{
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	for (UINT adapterIndex = 0; dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; adapterIndex++)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12Device))))
		{
			std::wcout << "Using GPU: " << desc.Description << "\n";
			break;
		}
		else
		{
			assert(false);
		}
	}
	assert(adapter);
}

void CDXCRenderer::createCommandsManagers()
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
}

void CDXCRenderer::createGPUTexture()
{
	textureDesc->Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc->Width = 800;
	textureDesc->Height = 600;
	textureDesc->DepthOrArraySize = 1;
	textureDesc->Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc->SampleDesc.Count = 1;
	textureDesc->Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	const HRESULT hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		textureDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		nullptr,
		IID_PPV_ARGS(&renderTarget)
	);

	assert(SUCCEEDED(hr));
}

void CDXCRenderer::createRenderTargetView()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 1;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	assert(SUCCEEDED(d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap))));

	rtvHandle = &rtvHeap->GetCPUDescriptorHandleForHeapStart();
	d3d12Device->CreateRenderTargetView(renderTarget, nullptr, *rtvHandle);
}

void CDXCRenderer::generateConstColorTexture()
{
	commandList->OMSetRenderTargets(1, rtvHandle, FALSE, nullptr);

	FLOAT clearColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(*rtvHandle, clearColor, 0, nullptr);
}

void CDXCRenderer::createReadbackBuffer()
{
	UINT64 readbackBufferSize = 0;

	d3d12Device->GetCopyableFootprints(
		textureDesc, 0, 1, 0, renderTargetFootprint,
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

	//////

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Transition.pResource = renderTarget;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

	commandList->ResourceBarrier(1, &barrier);

	D3D12_TEXTURE_COPY_LOCATION dest = {};
	dest.pResource = readbackBuffer;
	dest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dest.PlacedFootprint = *renderTargetFootprint;

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = renderTarget;
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0;

	commandList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	commandList->ResourceBarrier(1, &barrier);

	HRESULT hr = commandList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList* ppCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	commandQueue->Signal(renderFramefence, renderFramefenceValue);
}

void CDXCRenderer::createFence()
{
	HRESULT hr = d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderFramefence));
	assert(SUCCEEDED(hr));

	renderFrameEventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(renderFrameEventHandle);
}

void CDXCRenderer::waitForGPURenderFrame()
{
	if (renderFramefence->GetCompletedValue() < renderFramefenceValue)
	{
		HRESULT hr = renderFramefence->SetEventOnCompletion(renderFramefenceValue, renderFrameEventHandle);
		assert(SUCCEEDED(hr));

		WaitForSingleObject(renderFrameEventHandle, INFINITE);
	}
}

void CDXCRenderer::writeImageToFile()
{
	void* renderTargetData;
	HRESULT hr = readbackBuffer->Map(0, nullptr, &renderTargetData);
	assert(SUCCEEDED(hr));

	std::string filename("output.ppm");
	std::ofstream file(filename, std::ios::out | std::ios::binary);
	assert(file);

	file << "P3\n" << textureDesc->Width << textureDesc->Height << "\n255\n";

	for (UINT rowIdx = 0; rowIdx < textureDesc->Height; rowIdx++)
	{
		UINT rowPitch = renderTargetFootprint->Footprint.RowPitch;
		uint8_t* rowData = reinterpret_cast<uint8_t*>(renderTargetData) + rowIdx * rowPitch;
		for (UINT64 colIdx = 0; colIdx < textureDesc->Width; colIdx++)
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
