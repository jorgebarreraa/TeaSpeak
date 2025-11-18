#include <manager/ServerConnection.h>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableWidgetItem>
#include <misc/base64.h>
#include <QtWidgets/QStyle>
#include <QtWidgets/QMessageBox>
#include <manager/qtHelper.h>
#include <QtWidgets/QtWidgets>
#include "LicenseGenerator.h"
#include "Overview.h"
#include "UiLicenseInfo.h"

using namespace license;
using namespace license::manager;
using namespace license::ui;
using namespace std;
using namespace std::chrono;

extern ServerConnection* connection;
Overview::Overview() {
	this->ui.setupUi(this);

	this->ui.licenses->setColumnCount(RowEntry::ENDMARKER);
	this->ui.licenses->setHorizontalHeaderItem(RowEntry::ID, new QTableWidgetItem("ID"));
	this->ui.licenses->setHorizontalHeaderItem(RowEntry::USERNAME, new QTableWidgetItem("Username"));
	this->ui.licenses->setHorizontalHeaderItem(RowEntry::NAME, new QTableWidgetItem("Name"));
	this->ui.licenses->setHorizontalHeaderItem(RowEntry::END, new QTableWidgetItem("End timestamp"));
	this->ui.licenses->setHorizontalHeaderItem(RowEntry::ACTIVE, new QTableWidgetItem("Active"));
	this->ui.licenses->setHorizontalHeaderItem(RowEntry::ACTION_EDIT, new QTableWidgetItem(""));
	this->ui.licenses->setHorizontalHeaderItem(RowEntry::ACTION_DELETE, new QTableWidgetItem(""));

	this->ui.licenses->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	this->ui.licenses->verticalHeader()->hide();
	this->ui.licenses->setSelectionBehavior(QAbstractItemView::SelectRows);
	//this->ui.licenses->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

	QObject::connect(this->ui.btn_create_license, SIGNAL(clicked()), this, SLOT(clickedNewLicense()));
    QObject::connect(this->ui.btn_refresh, SIGNAL(clicked()), this, SLOT(btn_refresh_clicked()));

    if(!connection) {
        auto test = make_shared<license::LicenseInfo>();
        test->type = LicenseType::PREMIUM;
        test->email = "test@test.de";
        test->first_name = "Markus";
        test->last_name = "Hadenfeldt";
        test->username = "WolverinDEV";
        test->end = system_clock::now() + minutes(60);
        test->creation = system_clock::now();
        test->start = system_clock::now();
        addLicenseEntry("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", test);
    } else {
       this->refresh_license();
    }
}

Overview::~Overview() {}

#define M(var) \
var = new QTableWidgetItem(); \
var->setData(Qt::UserRole, qVariantFromValue<void*>(this)); \
var->setFlags(var->flags() & ~Qt::ItemIsEditable)

RowEntry::RowEntry(const shared_ptr<license::LicenseInfo> &license, const string &licenseId) : license(license), licenseId(licenseId) {
	M(widget_id);
	M(widget_username);
	M(widget_name);
	M(widget_end_timestamp);
	M(widget_active);

	widget_id->setText(QString::fromStdString(base64::encode(licenseId)));
	widget_username->setText(QString::fromStdString(license->username));
	widget_name->setText(QString::fromStdString(license->first_name + " " + license->last_name));

    {
        if(license->end.time_since_epoch() > hours(1)) {
            char buffer[128];
            auto tm = chrono::system_clock::to_time_t(license->end);
            strftime(buffer, sizeof(buffer), "%Y-%m-%d.%X", localtime(&tm));
            widget_end_timestamp->setText(buffer);
        } else {
            widget_end_timestamp->setText("never");
        }
    }

    widget_active->setTextColor(license->isValid() ? Qt::green : Qt::red);
	widget_active->setText(QString::fromStdString(license->isValid() ? "yes" : "no"));

	this->button_delete = new QPushButton();
	this->button_edit = new QPushButton();
	this->button_delete->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserStop));
	this->button_edit->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton));

    QObject::connect(this->button_edit, SIGNAL(clicked()), this, SLOT(btn_edit_clicked()));
    QObject::connect(this->button_delete, SIGNAL(clicked()), this, SLOT(btn_delete_clicked()));
}

RowEntry::~RowEntry() {}

void RowEntry::btn_delete_clicked() {
    auto message = "Do you really want to delete this license?<br>"
                   "Name: " + this->license->first_name + " " + this->license->last_name + "<br>"
                   "Username: " + this->license->username;
    if(QMessageBox::question(this->handle, "Are you sure?", QString::fromStdString(message)) == QMessageBox::Yes) {
        bool full = QMessageBox::question(this->handle, "You want a full delete?", "Do you even want to erase the license from the database?") == QMessageBox::Yes;
        connection->deleteLicense(this->licenseId, full).waitAndGetLater([&](bool flag) {
            if(!flag)
                runOnThread(this->thread(), [&]{
                    QMessageBox::warning(this->handle, "Delete error", "Failed to delete license!");
                });
            else
                this->handle->refresh_license();
        }, false);
    }
}

void RowEntry::btn_edit_clicked() {
    auto info = new ui::UiLicenseInfo(this->license, this->licenseId, this->handle);
    info->show();
}

void Overview::addLicenseEntry(std::string licenseId, shared_ptr<license::LicenseInfo> info) {
    if(this->thread() != QThread::currentThread()) {
        runOnThread(this->thread(), [&, licenseId, info]{
            this->addLicenseEntry(licenseId, info);
        });
        return;
    }
	auto entry = make_shared<RowEntry>(info, licenseId);
	entry->handle = this;

	int row = this->ui.licenses->rowCount();
	this->ui.licenses->insertRow(row);
	this->ui.licenses->setItem(row, RowEntry::ID, entry->widget_id);
	this->ui.licenses->setItem(row, RowEntry::USERNAME, entry->widget_username);
	this->ui.licenses->setItem(row, RowEntry::NAME, entry->widget_name);
	this->ui.licenses->setItem(row, RowEntry::END, entry->widget_end_timestamp);
	this->ui.licenses->setItem(row, RowEntry::ACTIVE, entry->widget_active);


	this->ui.licenses->setIndexWidget(this->ui.licenses->model()->index(row, RowEntry::ACTION_EDIT), entry->button_edit);
	this->ui.licenses->setIndexWidget(this->ui.licenses->model()->index(row, RowEntry::ACTION_DELETE), entry->button_delete);

	this->entries.push_back(entry);
}

void Overview::clickedNewLicense() {
	auto instance = new LicenseGenerator(this);
	instance->show();
}

void Overview::refresh_license() {
    this->btn_refresh_clicked();
}

void Overview::btn_refresh_clicked() {
    runOnThread(this->thread(), [&]{
        while(this->ui.licenses->rowCount() > 0)
            this->ui.licenses->removeRow(0);
        this->entries.clear();

        auto fut = connection->list(0, 100);
        fut.waitAndGetLater([&, fut](std::map<std::string, std::shared_ptr<license::LicenseInfo>> response) {
            if(!fut.succeeded()) {
                runOnThread(this->thread(), [&, fut](){
                    QMessageBox::warning(this, "Failed to load data", "Failed to load data:<br>" + QString::fromStdString(fut.errorMegssage()));
                });
            } else {
                for(const auto& entry : response)
                    this->addLicenseEntry(entry.first, entry.second);
            }
        }, {});
    });
}