#pragma once

#include <QtCore/QtCore>
#include <QtWidgets/QMainWindow>
#include <QProgressDialog>
#include <ui_loginwindow.h>
#include <memory>

namespace license {
	namespace ui {
		class LoginWindow : public QDialog {
			Q_OBJECT;
			public:
				LoginWindow();
				virtual ~LoginWindow();

			private slots:
				void onClickConnect();
			private:
				std::unique_ptr<QProgressDialog> progress;
				Ui::LoginWindow ui;
		};
	}
}