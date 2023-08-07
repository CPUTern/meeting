#include "myvideosurface.h"
#include <qvideosurfaceformat.h>
#include <qdebug.h>
MyVideoSurface::MyVideoSurface(QObject* parent) :QAbstractVideoSurface(parent)
{

}

QList<QVideoFrame::PixelFormat> MyVideoSurface::supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const
{
    if (handleType == QAbstractVideoBuffer::NoHandle)
    {
        return QList<QVideoFrame::PixelFormat>() << QVideoFrame::Format_RGB32
            << QVideoFrame::Format_ARGB32
            << QVideoFrame::Format_ARGB32_Premultiplied
            << QVideoFrame::Format_RGB565
            << QVideoFrame::Format_RGB555;
    }
    else
    {
        return QList<QVideoFrame::PixelFormat>();
    }
}


bool MyVideoSurface::isFormatSupported(const QVideoSurfaceFormat& format) const
{
    // imageFormatFromPixelFormat: Return an image format equivalent to the pixel format of the video frame.
    //pixelFormat: Return the pixel format of the video stream.
    return QVideoFrame::imageFormatFromPixelFormat(format.pixelFormat()) != QImage::Format_Invalid;
}

//Convert the pixel format of the video stream to an equivalent image format
bool MyVideoSurface::start(const QVideoSurfaceFormat& format)
{
    if (isFormatSupported(format) && !format.frameSize().isEmpty())
    {
        QAbstractVideoSurface::start(format);
        return true;
    }
    return false;
}


//Capture each frame of the video, and present each frame
bool MyVideoSurface::present(const QVideoFrame& frame)
{
    if (!frame.isValid())
    {
        stop();
        return false;
    }
    if (frame.isMapped())
    {
        emit frameAvailable(frame);
    }
    else
    {
        QVideoFrame f(frame);
        f.map(QAbstractVideoBuffer::ReadOnly);
        emit frameAvailable(f);
    }

    return true;
}
