#include "register.h"
#include <qvariant.h>
#include <QDebug>

#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

Register::Register(QWidget *parent): QWidget(parent),ui(new Ui::RegisterClass())
{
	ui->setupUi(this);
}

Register::~Register()
{
	delete ui;
}

void Register::on_reg_clicked() // 点击注册页面的确定
{
	if (ui->password->text() != ui->password2->text())
        ui->status->setText("两次密码不一致");
	else if (ui->username->text().isNull() || ui->password->text().isNull()) {
        ui->status->setText("用户名或密码为空");
	}
	else {
		ui->reg->setDisabled(true);
		ui->login->setDisabled(true);
		QString msg = ui->username->text() + "&" + ui->password->text();
//        qDebug() << msg;
		emit send_L(msg);
	}
}

void Register::on_login_clicked() {//back to login
	emit sendok();
}

void Register::recv_L(QString msg) {
	ui->reg->setDisabled(false);
	ui->login->setDisabled(false);
	ui->status->setText(msg);
}
