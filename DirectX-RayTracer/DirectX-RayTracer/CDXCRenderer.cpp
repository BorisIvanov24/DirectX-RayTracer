#include "CDXCRenderer.h"
#include <iostream>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <assert.h>

void CDXCRenderer::prepareForRendering()
{
	createDevice();
	createCommandsManagers();
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
