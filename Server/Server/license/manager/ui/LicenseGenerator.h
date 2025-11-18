#pragma once

#include <QtCore/QtCore>
#include <QtWidgets/QMainWindow>
#include <ui_licensegenerator.h>

namespace license {
    namespace ui {
        class LicenseGenerator : public QMainWindow {
                Q_OBJECT;
            public:
                LicenseGenerator(QWidget*);
                ~LicenseGenerator();

            private slots:
                void handleTimeTypeChanged(int);
                void handleGenerateLicense();
                void handleRegisterLicense();
                void handleInformationChanged();
            private:
                Ui::LicenseGenerator ui;

                bool validInput();
        };
    }
}