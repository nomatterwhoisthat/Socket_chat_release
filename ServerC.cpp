#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <time.h>
#include <vector>

#pragma warning(disable: 4996)

bool isDisconnected;  //"сигнал"
int as;
int err;  //необязательно
int closedSocket; //индекс закрытого сокета

//лог-файл
std::ofstream log_file("out.txt", std::ios::binary);

HANDLE hThread;

//map с соектами и логинами
std::vector<std::pair<SOCKET, std::string>> Connections(100);
int Counter = 0; //можно убрать

//все пакеты
enum Packet {
	P_ChatMessage,
	P_Auth,
	P_Error,
	P_Ban,
	P_Regestrehion,
	P_Online
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
		time[time_size] = '\0';

		recv(Connections[index].first, time, time_size, NULL);

		recv(Connections[index].first, login, sizeof(login), NULL);
		recv(Connections[index].first, msg, sizeof(msg), NULL);

		//запись в log_file
		log_file << time << ' ' << login << ": " << msg << std::endl;

		//отправка сообщения всем остальным людям
		for (int i = 0; i < Counter; i++) {
			if (i == index) {
				continue;
			}

			Packet msgtype = P_ChatMessage;
			send(Connections[i].first, (char*)&msgtype, sizeof(Packet), NULL);
			send(Connections[i].first, login, sizeof(login), NULL);
			send(Connections[i].first, msg, sizeof(msg), NULL);
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
		time[time_size] = '\0';

		recv(Connections[index].first, time, time_size, NULL);

		recv(Connections[index].first, login, sizeof(login), NULL);
		recv(Connections[index].first, password, sizeof(password), NULL);

		if (login == nullptr || password == nullptr)
		{
			Packet msgtype = P_Error;
			send(Connections[index].first, (char*)&msgtype, sizeof(Packet), NULL);

			log_file << time << ' ' << login << " - wasnt connected cause of wrong auth" << std::endl;

			return false;
		}

		//проверяем, что такого логина и пароля нет в файле
		if (!checkCredentials(login, password, "in.txt", index))
		{
			Packet msgtype = P_Error;
			send(Connections[index].first, (char*)&msgtype, sizeof(Packet), NULL);

			log_file << time << ' ' << login << " - wasnt connected cause of wrong auth" << std::endl;

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

		log_file << time << ' ' << login << " - was connected" << std::endl;

		break;
	}
	case P_Error:
	{
		char login[64];
		char msg[64];

		int time_size;
		recv(Connections[index].first, (char*)&time_size, sizeof(int), NULL);

		if (time_size < 0)
		{
			return false;
		}

		char* time = new char[time_size + 1];
		time[time_size] = '\0';

		recv(Connections[index].first, time, time_size, NULL);

		recv(Connections[index].first, login, sizeof(login), NULL);
		recv(Connections[index].first, msg, sizeof(msg), NULL);

		//где ошибка в msg
		char where_error = check(msg);

		log_file << time << ' ' << login << ": " << msg << " - error in " << where_error << std::endl;

		break;
	}
	case P_Ban:
	{
		//принимаем параметры
		int time_size;
		recv(Connections[index].first, (char*)&time_size, sizeof(int), NULL);
		char* time = new char[time_size + 1];
		time[time_size] = '\0';

		recv(Connections[index].first, time, time_size, NULL);

		char login_of_user[64];
		recv(Connections[index].first, login_of_user, sizeof(login_of_user), NULL);

		//TODO - если не нашли
		//пытаемся найти пользователя в map
		for (std::pair<SOCKET, std::string> &element : Connections)
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
			rewrite("in.txt", login_of_user);

			log_file << time << ' ' << login_of_user << " - was banned by admin" << std::endl;
		}
		else {
			log_file << time << ' ' << login_of_user << " - admin was tryed to ban by another admin" << std::endl;
		}

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
		time[time_size] = '\0';

		recv(Connections[index].first, time, time_size, NULL);

		recv(Connections[index].first, login, sizeof(login), NULL);
		recv(Connections[index].first, password, sizeof(password), NULL);

		std::ifstream file("in.txt", std::ios::app, std::ios::binary);
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
			log_file << time << ' ' << login << " - try to reg again" << std::endl;
			break;
		}

		//запись в массив
		Connections[index].second = login;

		std::ofstream file_out("in.txt", std::ios::app, std::ios::binary);
		file_out << login << ' ' << password << " active" << std::endl;

		//повторение в логе
		log_file << time << ' ' << login << " - was registrated" << std::endl;

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
			log_file << timeString << " " << Connections[index].second << " - was disconnected" << std::endl;
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
		log_file << timeString << " " << Connections[index].second << " - was disconnected" << std::endl;

		Connections[index].first = 0;              //затираем сокет
		Connections[index].second = "";
		closedSocket = index;                //записываем в переменную индекс закрытого сокета
		isDisconnected = true;               //для цикла for в мэйне, подаем "сигнал"
		flag = false;
	}
}

int main(int argc, char* argv[]) {
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
