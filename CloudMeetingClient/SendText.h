#pragma once

#include <QThread>
#include <qmutex.h>
#include <qqueue.h>
#include "NetHeader.h"

struct textMsg {
	QString str;
	MSG_TYPE type;
    textMsg(QString s,MSG_TYPE t):str(s),type(t){}
};

class SendText  : public QThread
{
	Q_OBJECT

public:
	SendText(QObject *parent=nullptr);
	~SendText();
private:
    QQueue<textMsg> textqueue;
	QMutex queue_lock;
	QWaitCondition queue_cond;
	void run()override;
	QMutex m_lock;
	bool isRun;
public slots:
    void pushTextMsgToTextQueue(MSG_TYPE msgType, QString str = "");
	void stopImmediately();
};
