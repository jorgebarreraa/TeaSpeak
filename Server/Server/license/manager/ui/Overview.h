#pragma once

#include "shared/License.h"
#include <QtCore/QtCore>
#include <QtWidgets/QMainWindow>
#include <QProgressDialog>
#include <ui_loginwindow.h>
#include <memory>
#include <deque>
#include <ui_owerview.h>

namespace license {
	namespace ui {
	    class Overview;
		struct RowEntry : public QObject {
			Q_OBJECT;
			public:
				enum Row {
					ID,
					USERNAME,
					NAME,
					END,
					ACTIVE,

					ACTION_EDIT,
					ACTION_DELETE,

					ENDMARKER
				};

                Overview* handle;

				QTableWidgetItem* widget_id;
				QTableWidgetItem* widget_username;
				QTableWidgetItem* widget_name;
				QTableWidgetItem* widget_end_timestamp;
				QTableWidgetItem* widget_active;

				QPushButton* button_edit;
				QPushButton* button_delete;

				std::shared_ptr<LicenseInfo> license;
				std::string licenseId;

				RowEntry(const std::shared_ptr<LicenseInfo> &license, const std::string &licenseId);
				~RowEntry();
			private slots:
				void btn_edit_clicked();
			    void btn_delete_clicked();
		};

		class Overview : public QMainWindow {
			Q_OBJECT;
			public:
				Overview();
				virtual ~Overview();

				void refresh_license();
			private slots:
				void clickedNewLicense();
				void btn_refresh_clicked();
			private:
				Ui::Overview ui;
				std::deque<std::shared_ptr<RowEntry>> entries;

				void addLicenseEntry(std::string, std::shared_ptr<LicenseInfo>);
		};
	}
}