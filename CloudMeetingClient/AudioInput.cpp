#include "AudioInput.h"
#include "netheader.h"
#include <qaudioformat.h>
#include <qdebug.h>
#include <QThread>
#include <zlib.h>

extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;

AudioInput::AudioInput(QObject *parent)
	: QObject(parent)
{
	recvbuf = (char*)malloc(MB * 2);
	QAudioFormat format;
	
	format.setSampleRate(8000);
	format.setChannelCount(1);
	format.setSampleSize(16);
	format.setCodec("audio/pcm");
	format.setByteOrder(QAudioFormat::LittleEndian);
	format.setSampleType(QAudioFormat::UnSignedInt);

	QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
	if (!info.isFormatSupported(format)){
		qWarning() << "Default format not supported, trying to use the nearest.";
		format = info.nearestFormat(format);
	}
	audio = new QAudioInput(format, this);
	connect(audio, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State)));

}

AudioInput::~AudioInput()
{
	delete audio;
	delete recvbuf;
}

void AudioInput::startCollect(){    //开启麦克风, 搜集语音
    if(audio->state() == QAudio::ActiveState) return;
	inputdevice = audio->start();
	connect(inputdevice, SIGNAL(readyRead()), this, SLOT(onreadyRead()));
}

void AudioInput::stopCollect(){    //关闭麦克风
	if (audio->state() == QAudio::StoppedState) return;
	disconnect(this, SLOT(onreadyRead()));
	audio->stop();
	inputdevice = nullptr;
}

void AudioInput::onreadyRead(){
	static int num = 0, totallen  = 0;
	if (inputdevice == nullptr) return;
	int len = inputdevice->read(recvbuf + totallen, 2 * MB - totallen);
	if (num < 2){
		totallen += len;
		num++;
		return;
	}
	totallen += len;
	qDebug() << "totallen = " << totallen;
	MESG* msg = (MESG*)malloc(sizeof(MESG));
	if (msg == nullptr){
		qWarning() << __LINE__ << "malloc fail";
	}
    else{
		memset(msg, 0, sizeof(MESG));
		msg->msg_type = AUDIO_SEND;

		uchar* dst = (uchar*)malloc(4*MB);
		if (dst == nullptr) {
			qWarning() << "malloc fail";
		}
		else {
			memset(dst, 0, totallen + 10);
			ulong dstlen = totallen + 10;
			int flag = compress(dst, &dstlen, (uchar*)recvbuf, totallen);
			if (flag != 0) {
				qDebug() << __FILE__ << __LINE__ << "compress error:" << flag;
			}
			else {
				QByteArray rr((char*)dst, dstlen);
				QByteArray str = rr.toBase64();
				msg->len = str.size();
				memcpy_s(dst, 2 * MB, str.data(), str.size());
                msg->data = dst;
//                qDebug()<<"audioSendData:";
//                for(int i = 0;i<10;i++){
//                    qDebug()<<QString::number(msg->data[i], 16)<<" size:"<<sizeof(msg->data[i]);
//                }
				queue_send.push_msg(msg);
			}
		}
	}
	totallen = 0;
	num = 0;
}

QString AudioInput::errorString()
{
	if (audio->error() == QAudio::OpenError)
	{
		return QString("AudioInput An error occurred opening the audio device").toUtf8();
	}
	else if (audio->error() == QAudio::IOError)
	{
		return QString("AudioInput An error occurred during read/write of audio device").toUtf8();
	}
	else if (audio->error() == QAudio::UnderrunError)
	{
		return QString("AudioInput Audio data is not being fed to the audio device at a fast enough rate").toUtf8();
	}
	else if (audio->error() == QAudio::FatalError)
	{
		return QString("AudioInput A non-recoverable error has occurred, the audio device is not usable at this time.");
	}
	else
	{
		return QString("AudioInput No errors have occurred").toUtf8();
	}
}

void AudioInput::handleStateChanged(QAudio::State newState)
{
	switch (newState)
	{
		case QAudio::StoppedState:
			if (audio->error() != QAudio::NoError)
			{
				stopCollect();
				emit audioinputerror(errorString());
			}
			else
			{
				qWarning() << "stop recording";
			}
			break;
		case QAudio::ActiveState:
			
			qWarning() << "start recording";
			break;
		default:
			//
			break;
	}
}

void AudioInput::setVolumn(int v)
{
	qDebug() << v;
	audio->setVolume(v / 100.0);
}
