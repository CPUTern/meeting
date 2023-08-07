#include "RecvDeal.h"
#include <qmetatype.h>
#include <qdebug.h>
#include <qmutex.h>

extern QUEUE_DATA<MESG> queue_recv;

RecvDeal::RecvDeal(QObject *parent)
	: QThread(parent)
{
	qRegisterMetaType<MESG*>();
	isRun = true;
}

RecvDeal::~RecvDeal()
{}

void RecvDeal::run(){    //循环取出queue_recv的消息，发送DataRecv信号
    while(1){
		{
			QMutexLocker locker(&m_lock);
			if (isRun == false) {
				return;
			}
		}
        MESG* msg = queue_recv.pop_msg();
		if (msg == nullptr)continue;
        emit sigDataFromRecvQueue(msg);
	}
}

void RecvDeal::stopImmediately() {
	QMutexLocker locker(&m_lock);
	isRun = false;
}
