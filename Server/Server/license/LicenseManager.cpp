#include <iostream>
#include <shared/include/license/license.h>
#include <shared/include/license/LicenseRequest.h>
#include <QtWidgets/QApplication>
#include <manager/ui/LicenseGenerator.h>
#include <manager/ServerConnection.h>
#include <manager/ui/LoginWindow.h>
#include <manager/ui/Overview.h>
#include <event2/thread.h>
#include <misc/base64.h>

using namespace std;
using namespace std::chrono;
using namespace license;
using namespace license::manager;

ServerConnection* connection = nullptr;
QWidget* current_widget = nullptr;

std::string string_to_hex(const std::string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();

    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

int main(int ac, char** av){
    evthread_use_pthreads();



    auto data = "AQBUAd26AAAAACnNuqvy++NGfMJU+Bvo5412Wi5jM5/JmmYyDbcxWKNaJV0FF1jpXtSrss3Gm7RUEQ6CdEEhPdyHVgHdugAAAAAphOsSMO1976JoEaTLru5boYKp2A62gUVdGvLZYqrDjBnsiumfxPlxfF/PYai/Khvth7FgDzDc7WIBBTPyOV8uw4Db2ZmOkA4dpjGWfY2L6QJaRyjh9XySDL+c6/eoKOv75Wbu1FO1mr83jr8H9pWoCalwm7SEPngSdYa3ZOa+Q7c=";
    string error;
    auto license = license::readLocalLicence(data, error);
    cout << "Key: " << base64::encode(license->key()) << endl;
    cout << "Key: " << string_to_hex(license->key()) << endl;
    //return false;

	connection = new ServerConnection();
    if(!connection->connect("mcgalaxy.de", 27786).waitAndGet(false)) {
        cerr << "Failed to connect!" << endl;
        return 2;
    }
    auto login = connection->login("WolverinDEV", "HelloWorld").waitAndGet(false);
    if(!login) {
        cerr << "Failed to login!" << endl;
        return 1;
    }
    /*
    cout << "Generating new license" << endl;

    std::string name = "Hyp3rX (timo-games@gmx.de )";
    auto licenses = createLocalLicence(LicenseType::PREMIUM, system_clock::now() + chrono::hours((int) (24 * 30.5 * 3))/* system_clock::time_point(), name);
    cout << name << " " << licenses << endl;

    readLocalLicence(licenses, name);
    cout << name << endl;
    return 0;
    string error = "";
    auto binary = readLocalLicence(licenses, error);
    if(!binary){
        cerr << "Could not read local license " << error << endl;
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;

    serv_addr.sin_addr.s_addr = ((in_addr*) gethostbyname("0.0.0.0")->h_addr)->s_addr;
    serv_addr.sin_port = htons(27786);
    LicenceRequest request(binary, serv_addr);

    try {
        auto info = request.requestInfo().waitAndGet(nullptr);
        cout << "Got info -> " << info->owner << "|" << info->licenseInfo << " -> " << info->valid << endl;
    } catch (const std::exception& ex){
        cerr << "Could not load info after throwing " << ex.what() << endl;
        cerr << "Exception: " << request.exception()->what() << endl;
    }
    return 0;
   */

    QApplication app(ac, av);
    //license::ui::LoginWindow gen;
	license::ui::Overview gen;
    gen.show();
    return app.exec();
}