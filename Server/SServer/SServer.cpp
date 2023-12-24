#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <vector>
#include <map>
#include <sstream>
#include <chrono>
#include <iomanip>
#pragma warning(disable: 4996)

#include <fstream>
#include <sstream>

std::string log_filename;
class ConfigParser {
public:
	ConfigParser(const std::string& filename) : filename(filename) {}

	bool load() {
		std::ifstream file(filename);
		if (!file.is_open()) {
			return false;
		}

		std::string line;
		while (std::getline(file, line)) {
			std::istringstream iss(line);
			std::string key, value;
			if (std::getline(iss, key, '=') && std::getline(iss, value)) {
				processKeyValue(key, value);
			}
		}

		file.close();
		return true;
	}

	std::string getLogFileName() const {
		return logFileName;
	}

	std::string getAddress() const {
		return address;
	}

	int getPort() const {
		return port;
	}

private:
	void processKeyValue(const std::string& key, const std::string& value) {
		if (key == "LogFile") {
			logFileName = value;
		}
		else if (key == "Address") {
			address = value;
		}
		else if (key == "Port") {
			port = std::stoi(value);
		}

	}

	std::string filename;
	std::string logFileName;
	std::string address;
	int port;
};

void LogEvent(const std::string& logMessage) {
	std::ofstream logFile(log_filename, std::ios_base::app);
	if (logFile.is_open()) {
		auto current_time = std::chrono::system_clock::now();
		auto time_stamp = std::chrono::system_clock::to_time_t(current_time);
		std::stringstream time_stream;
		time_stream << std::put_time(std::localtime(&time_stamp), "%Y-%m-%d %X");
		logFile << "[" << time_stream.str() << "]" << logMessage;
		logFile.close();
	}
}


struct UserInfo {
	SOCKET socket;
	std::string username;
};
bool isDisconnected;  //"сигнал"
int as;
int err;  //необязательно
int closedSocket; //индекс закрытого сокета
std::map<SOCKET, UserInfo> connected_users;
//лог-файл
//std::ofstream log_file("log_file.txt", std::ios::binary);

std::map<SOCKET, UserInfo> connectedUsers;

HANDLE hThread;

//map с соектами и логинами
std::vector<std::pair<SOCKET, std::string>> Connections(100);
int Counter = 0;

//все пакеты
enum Packet {
	P_ChatMessage,
	P_Auth,
	P_Error,
	P_Ban,
	P_Regestrehion,
	P_Online,
	P_PrivateMessage
};

//проверка на правильность логина и пароля
bool checkCredentials(const char login[64], const char password[64], const char* filename, int index) {
	std::ifstream file(filename);
	char fileLogin[64], filePassword[64], status[64];

	while (file >> fileLogin >> filePassword >> status) {
		if (std::strcmp(fileLogin, login) == 0 && std::strcmp(filePassword, password) == 0) {
			if (std::strcmp(status, "active") != 0)
			{
				Packet packettype = P_Ban;
				send(Connections[index].first, (char*)&packettype, sizeof(Packet), NULL);
				return false;
			}

			return true;
		}
	}

	return false;
}

//проверка на правильность сообщения(можно убрать и отсылать сразу с клиента)
char check(char msg[64]) {
	int index = 0;
	for (int i = 0; i < 64; ++i)
	{
		if (i + 1 == 64)
		{
			index = 64;
			break;
		}

		if (msg[i] == '\0')
		{
			index = i;
			break;
		}

		if (msg[i] < 32)
		{
			return msg[i];
		}
	}

	return msg[index - 1];
}

//перезапись файла, чтобы записать, что пользователь забанен
void rewrite(const char* file, char login[64]) {
	std::ifstream from(file, std::ios::binary);
	std::ofstream to("temp.txt", std::ios::binary);

	char fileLogin[64], filePassword[64], status[64];
	while (from >> fileLogin >> filePassword >> status) {
		to << fileLogin << ' ' << filePassword << ' ';
		if (strcmp(fileLogin, login) == 0)
		{
			to << "ban" << std::endl;
			continue;
		}

		to << status << std::endl;
	}

	from.close();
	to.close();

	std::ifstream from_new("temp.txt", std::ios::binary);
	std::ofstream to_new(file, std::ios::binary);

	while (from_new >> fileLogin >> filePassword >> status) {
		to_new << fileLogin << ' ' << filePassword << ' ' << status << std::endl;
	}

	from_new.close();
	to_new.close();
}

//обработка пакетов
bool ProcessPacket(int index, Packet packettype) {
	switch (packettype) {
	case P_ChatMessage:
	{
	
		char msg[64];
		char login[64];

		int time_size;
		recv(Connections[index].first, (char*)&time_size, sizeof(int), NULL);

		if (time_size < 0)
		{
			return false;
		}

		char* time = new char[time_size + 1];
		//time[time_size] = '\0';

		recv(Connections[index].first, time, time_size, NULL);

		recv(Connections[index].first, login, sizeof(login), NULL);
		recv(Connections[index].first, msg, sizeof(msg), NULL);
		if (msg != "private.") {
			//запись в log_file
			std::string str = std::string(login) + std::string(" sent the message :") + std::string(msg) + '\n';

			LogEvent(str);

			//отправка сообщения всем остальным людям
			for (int i = 0; i < Counter; i++) {
				if (i == index) {
					continue;
				}
				else {
					Packet msgtype = P_ChatMessage;
					send(Connections[i].first, (char*)&msgtype, sizeof(Packet), NULL);
					send(Connections[i].first, login, sizeof(login), NULL);
					send(Connections[i].first, msg, sizeof(msg), NULL);
					int colorCode = 7;
					send(Connections[i].first, (char*)&colorCode, sizeof(int), NULL);
				}
			}
		}
		delete[] time;
		break;
	}
	case P_Auth:
	{
		char login[64];
		char password[64];

		//принимаем время
		int time_size;
		recv(Connections[index].first, (char*)&time_size, sizeof(int), NULL);

		if (time_size < 0)
		{
			return false;
		}

		char* time = new char[time_size + 1];
		//time[time_size] = '\0';

		recv(Connections[index].first, time, time_size, NULL);

		recv(Connections[index].first, login, sizeof(login), NULL);
		recv(Connections[index].first, password, sizeof(password), NULL);

		if (login == nullptr || password == nullptr)
		{
			Packet msgtype = P_Error;
			send(Connections[index].first, (char*)&msgtype, sizeof(Packet), NULL);

			//log_file << time << ' ' << login << " - wasnt connected cause of wrong auth" << std::endl;
			std::string str = std::string(login) + std::string(" - wasnt connected cause of wrong auth\n");
			LogEvent(str);

			return false;
		}

		//проверяем, что такого логина и пароля нет в файле
		if (!checkCredentials(login, password, "config.txt", index))
		{
			Packet msgtype = P_Error;
			send(Connections[index].first, (char*)&msgtype, sizeof(Packet), NULL);

			//log_file << time << ' ' << login << " - wasnt connected cause of wrong auth" << std::endl;
			std::string str = std::string(login) + std::string(" - wasnt connected cause of wrong auth\n");
			LogEvent(str);

			return false;
		}

		//задаем права пользователям
		Packet msgtype = P_Auth;
		send(Connections[index].first, (char*)&msgtype, sizeof(Packet), NULL);
		std::string status;
		int status_size;
		if (strcmp(login, "jack") == 0)
		{
			status = "admin";
			status_size = status.size();
			send(Connections[index].first, (char*)&status_size, sizeof(int), NULL);
			send(Connections[index].first, status.c_str(), status.size(), NULL);
		}
		else {
			status = "simple";
			status_size = status.size();
			send(Connections[index].first, (char*)&status_size, sizeof(int), NULL);
			send(Connections[index].first, status.c_str(), status.size(), NULL);
		}

		//запись в массив
		Connections[index].second = login;

		//log_file << time << ' ' << login << " - was connected" << std::endl;
		std::string str = std::string(login) + std::string(" - was connected\n");
		LogEvent(str);
		break;
	}
	case P_Error:
	{
		char login[64];
		char msg[64];


		recv(Connections[index].first, login, sizeof(login), NULL);
		recv(Connections[index].first, msg, sizeof(msg), NULL);

		//где ошибка в msg
		char where_error = check(msg);

		//log_file << time << ' ' << login << ": " << msg << " - error in " << where_error << std::endl;
		std::string str = std::string(login) + std::string(" - error in ") + where_error + '\n';
		LogEvent(str);
		break;
	}
	case P_Ban:
	{
		//принимаем параметры
		int time_size;
		recv(Connections[index].first, (char*)&time_size, sizeof(int), NULL);
		char* time = new char[time_size + 1];
		//time[time_size] = '\0';

		recv(Connections[index].first, time, time_size, NULL);

		char login_of_user[64];
		recv(Connections[index].first, login_of_user, sizeof(login_of_user), NULL);

		//TODO - если не нашли
		//пытаемся найти пользователя в map
		for (std::pair<SOCKET, std::string>& element : Connections)
		{
			if (element.second == login_of_user)
			{
				Packet packettype = P_Ban;
				send(element.first, (char*)&packettype, sizeof(Packet), NULL);
				break;
			}
		}

		//jack админ, его забанить нельзя
		if (strcmp(login_of_user, "jack") != 0)
		{
			rewrite("config.txt", login_of_user);

			//log_file << time << ' ' << login_of_user << " - was banned by admin" << std::endl;
			std::string str = std::string(login_of_user) + std::string(" - was banned by admin")+ '\n';
			LogEvent(str);
		}
		else {
			//log_file << time << ' ' << login_of_user << " - admin was tryed to ban by another admin" << std::endl;
			std::string str = std::string(login_of_user) + std::string(" - admin was tryed to ban by another admin") + '\n';
			LogEvent(str);
			break;
		}
	}case P_PrivateMessage:
		{
			//принимаем параметры
			int time_size;
			recv(Connections[index].first, (char*)&time_size, sizeof(int), NULL);
			char* time = new char[time_size + 1];
			//time[time_size] = '\0';

			recv(Connections[index].first, time, time_size, NULL);

			char sender[64];
			char recipient[64];
			char msg[64];
			recv(Connections[index].first, sender, sizeof(sender), NULL);
			recv(Connections[index].first, recipient , sizeof(recipient), NULL);
			recv(Connections[index].first,msg, sizeof(msg), NULL);

			//TODO - если не нашли
			//пытаемся найти пользователя в map
			for (std::pair<SOCKET, std::string>& element : Connections)
			{
				if (element.second == recipient)
				{
					Packet packettype = P_PrivateMessage;
					send(element.first, (char*)&packettype, sizeof(Packet), NULL);
					send(element.first, sender, sizeof(sender), NULL);
					send(element.first, recipient, sizeof(recipient), NULL);
					send(element.first, msg, sizeof(msg), NULL);
					break;
				}
			}

			

	     //log_file << time << ": " << sender << "sent a private message to " << recipient <<" : " << msg << std::endl;
		 std::string str = std::string(sender) + std::string("sent a private message to ") + recipient + " : " + msg + '\n';
		 LogEvent(str);
		 delete[] time;
		 break;
			
		}
	   
	case P_Regestrehion:
	{
		char login[64];
		char password[64];

		//принимаем время
		int time_size;
		recv(Connections[index].first, (char*)&time_size, sizeof(int), NULL);

		if (time_size < 0)
		{
			return false;
		}

		char* time = new char[time_size + 1];
		//time[time_size] = '\0';

		recv(Connections[index].first, time, time_size, NULL);

		recv(Connections[index].first, login, sizeof(login), NULL);
		recv(Connections[index].first, password, sizeof(password), NULL);

		std::ifstream file("config.txt", std::ios::app, std::ios::binary);
		char fileLogin[64], filePassword[64], status[64];

		//проверка, что пользователь не забанен
		bool flag = false;
		while (file >> fileLogin >> filePassword >> status) {
			if (std::strcmp(fileLogin, login) == 0) {
				Packet packettype = P_Error;
				send(Connections[index].first, (char*)&packettype, sizeof(Packet), NULL);
				flag = true;
			}
		}
		file.close();

		if (flag)
		{
			//log_file << time << ' ' << login << " - try to reg again" << std::endl;
			std::string str = std::string(login) + std::string(" - try to reg again\n") ;
			LogEvent(str);
			break;
		}

		//запись в массив
		Connections[index].second = login;

		std::ofstream file_out("config.txt", std::ios::app, std::ios::binary);
		file_out << login << ' ' << password << " active" << std::endl;

		//повторение в логе
		//log_file << time << ' ' << login << " - was registrated" << std::endl;
		std::string str = std::string(login) + std::string(" - was registrated\n");
		LogEvent(str);
		break;
	}
	case P_Online:
	{
		Packet msgtype = P_Online;
		send(Connections[index].first, (char*)&msgtype, sizeof(Packet), NULL);

		//записываем логины в вектор, чтобы не отсылать map с соединениями
		std::vector<std::string> listOfAllUsers;
		for (int i = 0; i < Counter; ++i)
		{
			listOfAllUsers.push_back(Connections[i].second);
		}

		std::string serializedData;
		for (const auto& element : listOfAllUsers) {
			serializedData += element + ",";
		}

		//отправляем вектор онлайн логинов на клиент
		send(Connections[index].first, serializedData.c_str(), serializedData.size(), NULL);

		break;
	}
	default:
		std::cout << "Unrecognized packet: " << packettype << std::endl;
		break;
	}

	return true;
}

//обработка клиента
void ClientHandler(int index) {
	Packet packettype;
	bool flag = false;
	while (Counter > 0 && Connections[index].first != 0) {
		as = recv(Connections[index].first, (char*)&packettype, sizeof(Packet), NULL);

		//проверка на отключение
		if (as < 0)
		{
			flag = true;
			--Counter;

			if (shutdown(Connections[index].first, SD_BOTH) != 0)
			{
				err = WSAGetLastError();  //можно и без проверки
				std::cout << "Failed to shutdown socket, ERROR: " << err << std::endl;
			}
			closesocket(Connections[index].first);     //удаляем все возможные связи с сервером

		std::cout << "Client Disconnected!\n";

			time_t currentTime = time(nullptr);
			std::string timeString = ctime(&currentTime);

			//log_file << timeString << " " << Connections[index].second << " - was disconnected" << std::endl;
			std::string disconnect = Connections[index].second + " - was disconnected\n";
			LogEvent(disconnect);
			Connections[index].first = 0;              //затираем сокет
			closedSocket = index;                //записываем в переменную индекс закрытого сокета
			isDisconnected = true;               //для цикла for в мэйне, подаем "сигнал"
			break;                               //выходим из цикла
		}

		if (!ProcessPacket(index, packettype)) {
			break;
		}
	}

	//проверка на отключение
	if (!flag)
	{
		--Counter;

		std::cout << "Client Disconnected!\n";
		time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);
		//log_file << timeString << " " << Connections[index].second << " - was disconnected" << std::endl;
		std::string disconnect = Connections[index].second + " - was disconnected\n";
		LogEvent(disconnect);
		Connections[index].first = 0;              //затираем сокет
		Connections[index].second = "";
		closedSocket = index;                //записываем в переменную индекс закрытого сокета
		isDisconnected = true;               //для цикла for в мэйне, подаем "сигнал"
		flag = false;
	}
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " <config_file>" << std::endl;
		return 1;
	}

	std::string configFile = argv[1];

	
	ConfigParser configParser(configFile);
	if (!configParser.load()) {
		std::cerr << "Error loading configuration from " << configFile << std::endl;
		return 1;
	}

	
	std::string logFileName = configParser.getLogFileName();
	log_filename = logFileName;
	std::string address = configParser.getAddress();
	int port = configParser.getPort();

	
	WSAData wsaData;
	WORD DLLVersion = MAKEWORD(2, 1);
	if (WSAStartup(DLLVersion, &wsaData) != 0) {
		std::cout << "Error" << std::endl;
		exit(1);
	}


	SOCKADDR_IN addr;
	int sizeofaddr = sizeof(addr);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(1111);
	addr.sin_family = AF_INET;

	SOCKET sListen = socket(AF_INET, SOCK_STREAM, NULL);
	bind(sListen, (SOCKADDR*)&addr, sizeof(addr));
	listen(sListen, SOMAXCONN);

	//обработка соединений
	SOCKET newConnection;
	while (true)
	{
		for (int i = 0; i < 100; i++) {
			if (Connections[i].first == 0)
			{
				newConnection = accept(sListen, (SOCKADDR*)&addr, &sizeofaddr);

				if (newConnection == 0) {
					std::cout << "Error #2\n";
				}
				else {
					std::cout << "Client Connected!\n";

					if (isDisconnected) //наш "сигнал"
					{
						++Counter;
						Connections[closedSocket] = { newConnection, "0" }; //если обнаружили закрытый сокет, то записываем созданное соединение именно для него
						isDisconnected = false;
						hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ClientHandler, (LPVOID)(closedSocket), NULL, NULL); //передаем closedSocket параметром для нашего закрытого сокета

						if (hThread == NULL)
						{
							std::cout << "Error with Thread" << std::endl;
						}

						break; //выходим из цикла, чтобы заново перебрать весь массив (вдруг кто-то еще вышел)
					}

					Connections[i] = { newConnection, "0" };
					++Counter;

					hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ClientHandler, (LPVOID)(i), NULL, NULL);
					if (hThread == NULL)
					{
						std::cout << "error with Thread" << std::endl;
					}
				}
			}
		}
	}

	WSACleanup();

	system("pause");
	return 0;
}