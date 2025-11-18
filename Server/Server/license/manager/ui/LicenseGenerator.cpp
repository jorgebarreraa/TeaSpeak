#include <shared/License.h>
#include <QtWidgets/QMessageBox>
#include <manager/qtHelper.h>
#include "LicenseGenerator.h"
#include "manager/ServerConnection.h"

using namespace std;
using namespace std::chrono;
using namespace license;
using namespace license::manager;
using namespace license::ui;

enum TimeType : uint8_t {
    PERM,
    YEARS_1,
    MONTHS_6,
    MONTHS_3,
    MONTHS_1,
    COSTUME
};

LicenseGenerator::LicenseGenerator(QWidget* owner) : QMainWindow(owner) {
    ui.setupUi(this);

    ui.licenseType->addItem("Demo", qVariantFromValue((uint8_t) license::DEMO));
    ui.licenseType->addItem("Premium", qVariantFromValue((uint8_t) license::PREMIUM));
    ui.licenseType->addItem("Hoster", qVariantFromValue((uint8_t) license::HOSTER));
    ui.licenseType->addItem("Private", qVariantFromValue((uint8_t) license::PRIVATE));
    ui.licenseType->setCurrentIndex(ui.licenseType->findData(qVariantFromValue((uint8_t) license::PREMIUM)));

    ui.datePickerType->addItem("Permanent", qVariantFromValue((uint8_t) TimeType::PERM));
    ui.datePickerType->addItem("1 Year", qVariantFromValue((uint8_t) TimeType::YEARS_1));
    ui.datePickerType->addItem("6 Months", qVariantFromValue((uint8_t) TimeType::MONTHS_6));
    ui.datePickerType->addItem("3 Months", qVariantFromValue((uint8_t) TimeType::MONTHS_3));
    ui.datePickerType->addItem("1 Month", qVariantFromValue((uint8_t) TimeType::MONTHS_1));
    ui.datePickerType->addItem("Costume", qVariantFromValue((uint8_t) TimeType::COSTUME));

    QObject::connect(ui.datePickerType, SIGNAL(currentIndexChanged(int)), this, SLOT(handleTimeTypeChanged(int)));
    ui.datePickerType->setCurrentIndex(ui.datePickerType->findData(qVariantFromValue((uint8_t) TimeType::MONTHS_3)));

    //QObject::connect(ui.generateLicense, SIGNAL(clicked()), this, SLOT(handleGenerateLicense()));
    QObject::connect(ui.registerLicense, SIGNAL(clicked()), this, SLOT(handleRegisterLicense()));
    QObject::connect(ui.username, SIGNAL(textChanged(const QString&)), this, SLOT(handleInformationChanged()));
    QObject::connect(ui.email, SIGNAL(textChanged(const QString&)), this, SLOT(handleInformationChanged()));
    QObject::connect(ui.name_last, SIGNAL(textChanged(const QString&)), this, SLOT(handleInformationChanged()));
    QObject::connect(ui.name_first, SIGNAL(textChanged(const QString&)), this, SLOT(handleInformationChanged()));

    this->handleInformationChanged();
    this->setAttribute(Qt::WA_DeleteOnClose);
}

LicenseGenerator::~LicenseGenerator() {}

void LicenseGenerator::handleTimeTypeChanged(int type) {
    if(type == TimeType::COSTUME) {
        ui.datePicker->setEnabled(true);
        ui.datePicker->setDateTime(QDateTime::currentDateTimeUtc());
    } else {
        ui.datePicker->setEnabled(false);
        auto current = system_clock::now();
        switch (type){
            case TimeType::YEARS_1:
                current += hours(24 * 30 * 12);
                break;
            case TimeType::MONTHS_6:
                current += hours(24 * 30 * 6);
                break;
            case TimeType::MONTHS_3:
                current += hours(24 * 30 * 3);
                break;
            case TimeType::MONTHS_1:
                current += hours(24 * 30 * 1);
                break;
            case TimeType::PERM:
                current = system_clock::time_point();
                break;
            default:
                break;
        }
        ui.datePicker->setDateTime(QDateTime::fromMSecsSinceEpoch(duration_cast<milliseconds>(current.time_since_epoch()).count()));
    }
}

#define BACKGROUND(var, color) \
do { \
        QPalette pal = (var)->palette(); \
        pal.setColor(QPalette::ColorRole::Base, color); \
        (var)->setPalette(pal); \
} while(false)

bool LicenseGenerator::validInput() {
    bool error = false;
    if(ui.username->text().isEmpty()) {
        BACKGROUND(ui.username, QColor(255, 0, 0));
        error |= true;
    } else
        BACKGROUND(ui.username, QColor(255, 255, 255));

    if(ui.email->text().isEmpty()) {
        BACKGROUND(ui.email, QColor(255, 0, 0));
        error |= true;
    } else BACKGROUND(ui.email, QColor(255, 255, 255));

    if(ui.name_last->text().isEmpty()) {
        BACKGROUND(ui.name_last, QColor(255, 0, 0));
        error |= true;
    } else BACKGROUND(ui.name_last, QColor(255, 255, 255));

    if(ui.name_first->text().isEmpty()) {
        BACKGROUND(ui.name_first, QColor(255, 0, 0));
        error |= true;
    } else BACKGROUND(ui.name_first, QColor(255, 255, 255));
    return !error;
}

void LicenseGenerator::handleInformationChanged() {
    std::string info = ui.username->text().toStdString() + "(" + ui.email->text().toStdString() + ")";

    QPalette pal = this->ui.character_counter->palette();
    if(info.length() >= 64) {
        this->ui.character_counter->setText(QString::fromStdString("Input is " + to_string(info.length() - 63) + " characters to long!"));
        pal.setColor(QPalette::ColorRole::Foreground, QColor(0xFF, 0, 0));
    } else {
        this->ui.character_counter->setText(QString::fromStdString(to_string(64 - info.length()) + " characters left"));
        pal.setColor(QPalette::ColorRole::Foreground, QColor(0, 0, 0));
    }
    this->ui.character_counter->setPalette(pal);
}

void LicenseGenerator::handleGenerateLicense() {
    if(!validInput()) {
        QMessageBox::warning(this, "Invalid arguments", "Please check your provided arguments");
        return;
    }

    std::string info = ui.username->text().toStdString() + "(" + ui.email->text().toStdString() + ")";
    if(info.length() >= 64) {
        QMessageBox::warning(this, "Invalid arguments", "Username + E-Mail are too long!");
        return;
    }

    system_clock::time_point duration;
    duration += milliseconds(ui.datePicker->dateTime().toMSecsSinceEpoch());

    if(duration.time_since_epoch().count() != 0 && system_clock::now() > duration) {
        auto res = QMessageBox::warning(this, "Invalid arguments", "Invalid end time. Are you sure you want to create this license?", QMessageBox::Ok | QMessageBox::Abort);
        if(res != QMessageBox::Ok) return;
    }

    auto ltType = (license::LicenseType) ui.licenseType->itemData(ui.licenseType->currentIndex()).toInt();

    auto license = license::createLocalLicence(ltType, duration, info);
    ui.license->setText(QString::fromStdString(license));
    QMessageBox::information(this, "License", "License successfully generated!");
}

extern ServerConnection* connection;
void LicenseGenerator::handleRegisterLicense() {
    if(!validInput()) {
        QMessageBox::warning(this, "Invalid arguments", "Please check your provided arguments");
        return;
    }

    std::string info = ui.username->text().toStdString() + "(" + ui.email->text().toStdString() + ")";
    if(info.length() >= 64) {
        QMessageBox::warning(this, "Invalid arguments", "Username + E-Mail are too long!");
        return;
    }

    system_clock::time_point duration;
    duration += milliseconds(ui.datePicker->dateTime().toMSecsSinceEpoch());

    if(duration.time_since_epoch().count() != 0 && system_clock::now() > duration) {
        auto res = QMessageBox::warning(this, "Invalid arguments", "Invalid end time. Are you sure you want to create this license?", QMessageBox::Ok | QMessageBox::Abort);
        if(res != QMessageBox::Ok) return;
    }

    auto ltType = (license::LicenseType) ui.licenseType->itemData(ui.licenseType->currentIndex()).toInt();
    auto result = connection->registerLicense(ui.name_first->text().toStdString(), ui.name_last->text().toStdString(), ui.username->text().toStdString(), ui.email->text().toStdString(), ltType, duration);
    result.waitAndGetLater([&, result](std::pair<std::shared_ptr<license::License>, std::shared_ptr<license::LicenseInfo>>* response) {
        if(result.state() == threads::FutureState::FAILED || !response) {
            runOnThread(this->thread(), [&, result]{
               QMessageBox::warning(this, "Creation failed", QString::fromStdString("Failed to create license (" + result.errorMegssage() + ")"));
               return;
            });
        } else {
            runOnThread(this->thread(), [&, result, response]{
                ui.license->setText(QString::fromStdString(license::exportLocalLicense(response->first)));
                QMessageBox::information(this, "License", "License successfully generated and registered!");
            });
        }
    });
}