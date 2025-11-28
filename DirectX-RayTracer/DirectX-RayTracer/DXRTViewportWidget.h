#pragma once
#include <QWidget>
#include <QPainter>

class DXRTViewportWidget : public QWidget
{
	Q_OBJECT

public:
	explicit DXRTViewportWidget(QWidget* parent = nullptr);

	void updateImage(const QImage& image);

	// Returns the native window handle that represents the underlying window for this widget
	HWND getNativeWindowHandle();

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	QImage image;
};

