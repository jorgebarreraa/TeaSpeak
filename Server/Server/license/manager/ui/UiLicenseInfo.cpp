#include <QProgressDialog>
#include <QMessageBox>

#include "manager/ServerConnection.h"
#include "UiLicenseInfo.h"
#include "LoginWindow.h"
#include "manager/qtHelper.h"
#include "Overview.h"

using namespace license;
using namespace license::manager;
using namespace license::ui;
using namespace std;
using namespace std::chrono;

#define q(str) QString::fromStdString(str)

UiLicenseInfo::UiLicenseInfo(const std::shared_ptr<license::LicenseInfo> &info, const std::string &key, QWidget* h) : QDialog(h), key(key), info(info) {
    this->ui.setupUi(this);
    this->set_editable(this->flag_editable);

    this->ui.edit_name_first->setText(q(info->first_name));
    this->ui.edit_name_last->setText(q(info->last_name));
    this->ui.edit_email->setText(q(info->email));
    this->ui.edit_username->setText(q(info->username));

    this->ui.edit_time_created->setSpecialValueText("never");
    this->ui.edit_time_begin->setSpecialValueText("never");
    this->ui.edit_time_end->setSpecialValueText("never");
    this->ui.edit_time_created->setDateTime(QDateTime::fromMSecsSinceEpoch(duration_cast<milliseconds>(this->info->creation.time_since_epoch()).count()));
    this->ui.edit_time_begin->setDateTime(QDateTime::fromMSecsSinceEpoch(duration_cast<milliseconds>(this->info->start.time_since_epoch()).count()));
    this->ui.edit_time_end->setDateTime(QDateTime::fromMSecsSinceEpoch(duration_cast<milliseconds>(this->info->end.time_since_epoch()).count()));

    this->update_length();

    QObject::connect(this->ui.btn_edit, SIGNAL(clicked()), this, SLOT(btn_edit_clicked()));
}

UiLicenseInfo::~UiLicenseInfo() {}

//The VA ars are for my ide
#define M(var, ...) \
do { \
    (var)->setFrame(flag); \
    (var)->setReadOnly(!flag); \
    QPalette p = (var)->palette(); \
    p.setColor(QPalette::Base, QColor(255,255,255,flag ? 255 : 0)); \
    (var)->setPalette(p); \
} while(0)

void UiLicenseInfo::set_editable(bool flag) {
    M(this->ui.edit_email);
    M(this->ui.edit_name_first);
    M(this->ui.edit_name_last);
    M(this->ui.edit_username);
    M(this->ui.edit_time_created);
    M(this->ui.edit_time_begin);
    M(this->ui.edit_time_end);

    if(flag) {
        this->ui.edit_time_created->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        this->ui.edit_time_begin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        this->ui.edit_time_end->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    } else {
        this->ui.edit_time_created->setButtonSymbols(QAbstractSpinBox::NoButtons);
        this->ui.edit_time_begin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        this->ui.edit_time_end->setButtonSymbols(QAbstractSpinBox::NoButtons);
    }
}

#undef M

void UiLicenseInfo::btn_edit_clicked() {
    this->set_editable(this->flag_editable ^= 1);
}

using days = std::chrono::duration<int, std::ratio_multiply<std::ratio<24>, std::chrono::hours::period>>;
using years = std::chrono::duration<int, std::ratio_multiply<std::ratio<365>, days::period>>;

//The VA ars are for my ide
#define M(unit, name, ...) \
do { \
    auto num = duration_cast<unit>(length); \
        if(num.count() > 0) { \
        result += " " + to_string(num.count()) + " " + name; \
        length -= num; \
    }  \
} while (0)

void UiLicenseInfo::update_length() {
    if(this->info->end.time_since_epoch().count() == 0)
        this->ui.text_length->setText("unlimited");
    else {
        if(this->info->end < this->info->start) {
            this->ui.text_length->setText("error");
        } else {
            auto length = this->info->end - this->info->start;
            length += seconds(1);

            string result;
            M(years, "years");
            M(days, "days");
            M(hours, "hours");
            M(minutes, "minutes");
            M(seconds, "seconds");
            if(!result.empty())
                result = result.substr(1);

            this->ui.text_length->setText(q(result));
        }
    }
}