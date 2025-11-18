#include <manager/ServerConnection.h>
#include "LicenseGenerator.h"
#include <QProgressDialog>
#include <QMessageBox>
#include "LoginWindow.h"
#include "manager/qtHelper.h"
#include "Overview.h"

using namespace license;
using namespace license::manager;
using namespace license::ui;
using namespace std;
using namespace std::chrono;

LoginWindow::LoginWindow() : QDialog(nullptr) {
	this->ui.setupUi(this);
	this->setFixedSize(this->size());

	QObject::connect(ui.btn_login, SIGNAL(clicked()), this, SLOT(onClickConnect()));
	//this->setAttribute(Qt::WA_DeleteOnClose);
}

LoginWindow::~LoginWindow() { }

extern ServerConnection* connection;
extern QWidget* current_widget;
void LoginWindow::onClickConnect() {
	progress = unique_ptr<QProgressDialog>(new QProgressDialog(this));
	progress->setMinimum(0);
	progress->setMaximum(2);
	progress->setLabelText("Connecting...");
	progress->show();

	auto future = connection->connect("0.0.0.0", 27786);
	future.waitAndGetLater([&, future](bool* flag) {
		runOnThread(this->thread(), [&, future, flag](){
			if(!flag || !*flag) {
				if(future.state() == threads::FutureState::WORKING)
					QMessageBox::critical(this, "Connect error", "Could not connect to remote!<br>Error: Connect timeout");
				else
					QMessageBox::critical(this, "Connect error", QString::fromStdString("Could not connect to remote!<br>Error: " + future.errorMegssage()));
				progress.reset();
				connection->disconnect("timeout");
			} else {
				progress->setValue(1);
				progress->setLabelText("Logging in");
				auto login_future = connection->login(this->ui.field_user->text().toStdString(), this->ui.field_password->text().toStdString());
				login_future.waitAndGetLater([&, login_future](bool* flag){
					runOnThread(this->thread(), [&, login_future, flag](){
						if(!flag || !*flag) {
							if(future.state() == threads::FutureState::WORKING)
								QMessageBox::critical(this, "Login error", "Could not login!<br>Error: Login timeout");
							else
								QMessageBox::critical(this, "Login error", QString::fromStdString("Could not login!<br>Error: " + future.errorMegssage()));
							progress.reset();
							connection->disconnect("timeout");
						} else {
							current_widget = new Overview();
							current_widget->show();
							progress.reset();
							this->close();
						}
					});
				}, system_clock::now() + seconds(5));
			}
		});
	}, system_clock::now() + seconds(5));
};