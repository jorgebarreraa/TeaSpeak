#pragma once

#include <QtCore/QtCore>
#include <QtWidgets/QMainWindow>
#include <ui_licenseinfo.h>
#include <shared/License.h>

namespace license {
    namespace ui {
        class UiLicenseInfo : public QDialog {
            Q_OBJECT;
            public:
                UiLicenseInfo(const std::shared_ptr<license::LicenseInfo>& info, const std::string& key, QWidget*);
                ~UiLicenseInfo();

            private slots:
                void btn_edit_clicked();

            private:
                std::string key;
                std::shared_ptr<license::LicenseInfo> info;
                Ui::LicenseInfo ui;

                bool flag_editable = false;
                void set_editable(bool);
                void update_length();
        };
    }
}