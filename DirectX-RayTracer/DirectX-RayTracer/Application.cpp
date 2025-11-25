#include <iostream>
#include <dxgi1_6.h>
#include "DXRTApp.h"
#include <QApplication>

#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi.lib")

#include "CDXCRenderer.h"

void printAdapters()
{
	IDXGIFactory4* dxgiFactory = nullptr;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

	if (FAILED(hr))
	{
		std::cout << "Couldn't initialise factory!" << std::endl;
	}

	IDXGIAdapter1* adapter = nullptr;
	for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		double vramGB = static_cast<double>(desc.DedicatedVideoMemory) / (1024.0 * 1024.0 * 1024.0);

		std::wcout << desc.Description << std::endl;
		std::wcout << " - VRAM: " << vramGB << " GB" << std::endl;

		IDXGIOutput* output = nullptr;
		for (UINT j = 0; adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND; j++)
		{
			DXGI_OUTPUT_DESC outputDesc;
			output->GetDesc(&outputDesc);

			std::wcout << "Monitor " << j << ": " << outputDesc.DeviceName << "\n" << std::endl;
			output->Release();
		}

		adapter->Release();
	}
}

void setTheme()
{
	QApplication::setStyle("Fusion");

	QPalette dark;
	dark.setColor(QPalette::Window, QColor(37, 37, 38));
	dark.setColor(QPalette::WindowText, Qt::white);
	dark.setColor(QPalette::Base, QColor(30, 30, 30));
	dark.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
	dark.setColor(QPalette::Text, Qt::white);
	dark.setColor(QPalette::Button, QColor(45, 45, 48));
	dark.setColor(QPalette::ButtonText, Qt::white);

	QApplication::setPalette(dark);
}

int main(int argc, char** argv)
{
	QApplication app(argc, argv); // Qt event loop
	setTheme();

	DXRTApp rendererApp;
	if (!rendererApp.init())
		return -1;

	return app.exec(); // Starts the Qt event loop
}