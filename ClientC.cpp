#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <iostream>
#include <ctime>
#include <mutex>
#include <string>
#include <sstream>
#include <vector>

#pragma warning(disable: 4996)

SOCKET Connection;
HANDLE hThread;

enum Packet {
	P_ChatMessage,
	P_Auth,
	P_Error,
	P_Ban,
	P_Regestrehion,
	P_Online,
	P_Exit
};

char* root;
int root_size;

//обработка разных пакетов
bool ProcessPacket(Packet packettype) {
	switch (packettype) {
	case P_ChatMessage:
	{
		char msg[64];
		char login[64];

		recv(Connection, login, sizeof(login), NULL);
		recv(Connection, msg, sizeof(msg), NULL);
		std::cout << login << ": " << msg << std::endl;

		break;
	}
	case P_Auth:
		recv(Connection, (char*)&root_size, sizeof(int), NULL);
		root = new char[root_size + 1];
		root[root_size] = '\0';

		recv(Connection, root, root_size, NULL);
		break;
	case P_Error:
		std::cout << "Error in login or password" << std::endl;
		return false;
	case P_Ban:
		if (root != NULL && strcmp(root, "admin") == 0)
		{
			std::cout << "one admin wants to block u" << std::endl;
			break;
		}

		std::cout << "Banned by admin" << std::endl;
		return false;
	case P_Online:
	{
		char buffer[1024];
		int bytesRead = recv(Connection, buffer, 1024, 0);
		if (bytesRead == SOCKET_ERROR) {
			// Обработка ошибки и выход
			closesocket(Connection);
			WSACleanup();
			return 1;
		}

		// Десериализация данных обратно в вектор строк
		std::vector<std::string> receivedVector;
		std::string receivedData(buffer, bytesRead);
		std::string element;
		std::istringstream iss(receivedData);

		while (std::getline(iss, element, ',')) {
			receivedVector.push_back(element);
		}

		for (std::string var : receivedVector)
		{
			std::cout << var << std::endl;
		}

		break;
	}
	default:
		std::cout << "Unrecognized packet: " << packettype << std::endl;
		return false;
	}

	return true;
}

void ClientHandler() {
	Packet packettype;
	while (true) {
		recv(Connection, (char*)&packettype, sizeof(Packet), NULL);

		if (!ProcessPacket(packettype)) {
			break;
		}
	}

	CloseHandle(hThread);
	closesocket(Connection);
}

//проверка на валидность строки
bool check(char msg[64]) {
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
			return false;
		}
	}

	return msg[index - 1] == '?' || msg[index - 1] == '.' || msg[index - 1] == '!';
}

//разделяем строку на логин и пароль
void pars_login_and_password(char login[64], char password[64], char credentials[128]) {
	char* token = std::strtok(credentials, " ");
	if (token != nullptr) {
		std::strncpy(login, token, 64);
		login[63] = '\\0';
	}

	token = std::strtok(nullptr, " ");
	if (token != nullptr) {
		std::strncpy(password, token, 64);
		password[63] = '\\0';
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

	Connection = socket(AF_INET, SOCK_STREAM, NULL);
	if (connect(Connection, (SOCKADDR*)&addr, sizeof(addr)) != 0) {
		std::cout << "Error: failed connect to server.\n";
		return 1;
	}

	hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ClientHandler, NULL, NULL, NULL);

	if (hThread == NULL)
	{
		std::cout << "error with thread" << std::endl;
		return 1;
	}

	//регестрация или вход
	char answer[64];
	std::cout << "U wanna reg or login?" << std::endl;
	std::cin.getline(answer, 64);

	if (strcmp(answer, "login") != 0 && strcmp(answer, "reg") != 0)
	{
		std::cout << "wrong command" << std::endl;
		return 1;
	}

	//авторизация
	char credentials[128];

	std::cout << "Write ur login and password: " << std::endl;
	std::cin.getline(credentials, 128);

	char login[64];
	char password[64];
	pars_login_and_password(login, password, credentials);

	if (strcmp(answer, "login") == 0)
	{
		Packet packettype = P_Auth;
		send(Connection, (char*)&packettype, sizeof(Packet), NULL);

		//отправка времени на авторизацию
		time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);
		int time_size = timeString.size();
		send(Connection, (char*)&time_size, sizeof(int), NULL);
		if (send(Connection, timeString.c_str(), timeString.size(), NULL) == SOCKET_ERROR) {
			std::cout << "Failed to send message to server." << std::endl;
			closesocket(Connection);
			WSACleanup();
			return 1;
		}

		send(Connection, login, sizeof(login), NULL);
		send(Connection, password, sizeof(password), NULL);
	}
	else {
		Packet packettype = P_Regestrehion;
		send(Connection, (char*)&packettype, sizeof(Packet), NULL);

		//отправка времени на авторизацию
		time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);
		int time_size = timeString.size();
		send(Connection, (char*)&time_size, sizeof(int), NULL);
		if (send(Connection, timeString.c_str(), timeString.size(), NULL) == SOCKET_ERROR) {
			std::cout << "Failed to send message to server." << std::endl;
			closesocket(Connection);
			WSACleanup();
			return 1;
		}

		send(Connection, login, sizeof(login), NULL);
		send(Connection, password, sizeof(password), NULL);
	}

	Sleep(500);
	DWORD dwExitCode;
	if (GetExitCodeThread(hThread, &dwExitCode) && dwExitCode == STILL_ACTIVE) {
		std::cout << "Connected" << std::endl;
	}
	else {
		return 1;
	}

	/*std::cout << "Online users: " << std::endl;
	Packet packettype = P_Online;
	send(Connection, (char*)&packettype, sizeof(Packet), NULL);*/

	char msg1[64];
	while (true) {
		std::cin.getline(msg1, sizeof(msg1));

		if (strcmp(msg1, "exit") == 0)
		{
			Packet packettype = P_Exit;
			send(Connection, (char*)&packettype, sizeof(Packet), NULL);

			time_t currentTime = time(nullptr);
			std::string timeString = ctime(&currentTime);
			int time_size = timeString.size();
			send(Connection, (char*)&time_size, sizeof(int), NULL);
			if (send(Connection, timeString.c_str(), timeString.size(), NULL) == SOCKET_ERROR) {
				std::cout << "Failed to send message to server." << std::endl;
				closesocket(Connection);
				WSACleanup();
				return 1;
			}

			send(Connection, login, sizeof(login), NULL);

			return 0;
		}

		//забанить пользователя
		if (strcmp(msg1, "ban") == 0)
		{
			//проверка на права
			if (strcmp(root, "admin") == 0)
			{
				//online users
				std::cout << "Online users: " << std::endl;
				Packet packettype = P_Online;
				send(Connection, (char*)&packettype, sizeof(Packet), NULL);

				Sleep(150);
				char login_of_ban[64];
				std::cout << "Write the name of user: ";
				std::cin.getline(login_of_ban, sizeof(login_of_ban));

				packettype = P_Ban;
				send(Connection, (char*)&packettype, sizeof(Packet), NULL);

				//отправка времени на авторизацию
				time_t currentTime = time(nullptr);
				std::string timeString = ctime(&currentTime);
				int time_size = timeString.size();
				send(Connection, (char*)&time_size, sizeof(int), NULL);
				if (send(Connection, timeString.c_str(), timeString.size(), NULL) == SOCKET_ERROR) {
					std::cout << "Failed to send message to server." << std::endl;
					closesocket(Connection);
					WSACleanup();
					return 1;
				}

				send(Connection, login_of_ban, sizeof(login_of_ban), NULL);

				packettype = P_Exit;
				send(Connection, (char*)&packettype, sizeof(Packet), NULL);
				send(Connection, (char*)&time_size, sizeof(int), NULL);
				if (send(Connection, timeString.c_str(), timeString.size(), NULL) == SOCKET_ERROR) {
					std::cout << "Failed to send message to server." << std::endl;
					closesocket(Connection);
					WSACleanup();
					return 1;
				}

				send(Connection, login_of_ban, sizeof(login_of_ban), NULL);
			}
			else {
				std::cout << "u cannt do it" << std::endl;
			}

			continue;
		}

		//обработка неправильного сообщения и отправка
		if (check(msg1))
		{
			//отпправка типа пакета
			Packet packettype = P_ChatMessage;
			send(Connection, (char*)&packettype, sizeof(Packet), NULL);

			//отправка времени
			time_t currentTime = time(nullptr);
			std::string timeString = ctime(&currentTime);
			int time_size = timeString.size();
			send(Connection, (char*)&time_size, sizeof(int), NULL);
			if (send(Connection, timeString.c_str(), timeString.size(), NULL) == SOCKET_ERROR) {
				std::cout << "Failed to send message to server." << std::endl;
				closesocket(Connection);
				WSACleanup();
				return 1;
			}

			//отправляем логин пользователя
			send(Connection, login, sizeof(login), NULL);

			//отправляем есообщение
			send(Connection, msg1, sizeof(msg1), NULL);
		}
		else {
			Packet packettype = P_Error;
			send(Connection, (char*)&packettype, sizeof(Packet), NULL);

			//отправляем время
			time_t currentTime = time(nullptr);
			std::string timeString = ctime(&currentTime);
			int time_size = timeString.size();
			send(Connection, (char*)&time_size, sizeof(int), NULL);
			if (send(Connection, timeString.c_str(), timeString.size(), NULL) == SOCKET_ERROR) {
				std::cout << "Failed to send message to server." << std::endl;
				closesocket(Connection);
				WSACleanup();
				return 1;
			}

			send(Connection, login, sizeof(login), NULL);
			send(Connection, msg1, sizeof(msg1), NULL);

			std::cout << "something wrong with msg" << std::endl;
		}

		Sleep(10);
	}

	closesocket(Connection);
	CloseHandle(hThread);
	WSACleanup();

	system("pause");
	return 0;
}
