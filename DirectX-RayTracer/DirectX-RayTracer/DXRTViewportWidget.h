#pragma once
#include <QWidget>
#include <QPainter>
#include <QSet>
#include <QPoint>

class DXRTApp;

class DXRTViewportWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DXRTViewportWidget(DXRTApp* app, QWidget* parent = nullptr);

    void updateImage(const QImage& image);
    HWND getNativeWindowHandle();

    const QSet<int>& getPressedKeys() const;
protected:
    void paintEvent(QPaintEvent*) override;

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QImage image;
    DXRTApp* app = nullptr;

    QSet<int> keysPressed;
    QPoint lastMousePos;

    bool mouseCaptured = false;
    bool ignoreNextMouseMove = false;
};
