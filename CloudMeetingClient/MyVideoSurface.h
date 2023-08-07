#pragma once

#include <QAbstractVideoSurface>

class MyVideoSurface : public QAbstractVideoSurface
{
    Q_OBJECT
public:
    MyVideoSurface(QObject* parent = 0);

    //supported pixel format
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle) const override;

    //Check if the format of the video stream is valid, return bool
    bool isFormatSupported(const QVideoSurfaceFormat& format) const override;
    bool start(const QVideoSurfaceFormat& format) override;
    bool present(const QVideoFrame& frame) override;

signals:
    void frameAvailable(QVideoFrame);

};
