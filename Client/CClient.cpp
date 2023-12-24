#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <ctime>
#include <mutex>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <conio.h>
#include <iomanip>
#include <chrono>
#pragma warning(disable: 4996)

std::ofstream log_file("log_file.txt");

SOCKET Connection;
HANDLE hThread;

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

//админ или обычный пользователь
char* root;
int root_size;

void LogEvent(const std::string& logMessage) {
	std::ofstream logFile("log.txt", std::ios_base::app);
	if (logFile.is_open()) {
		auto current_time = std::chrono::system_clock::now();
		auto time_stamp = std::chrono::system_clock::to_time_t(current_time);
		std::stringstream time_stream;
		time_stream << std::put_time(std::localtime(&time_stamp), "%Y-%m-%d %X");
		logFile << "[" << time_stream.str() << "]" << logMessage;
		logFile.close();
	}
}
//обработка разных пакетов
void SetConsoleColor(WORD color) {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}
void PrintMessageOnRight(std::string  message) {
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(console, &csbi);

	int screenWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;

	COORD newPosition;
	newPosition.X = screenWidth - static_cast<SHORT>(message.size());
	newPosition.Y = csbi.dwCursorPosition.Y;

	SetConsoleCursorPosition(console, newPosition);
	std::cout << message << std::endl;

}
bool ProcessPacket(Packet packettype) {
	switch (packettype) {
	case P_ChatMessage:
	{
		char msg[64];
		char login[64];
		int rec_color;
		recv(Connection, login, sizeof(login), NULL);
		recv(Connection, msg, sizeof(msg), NULL);
		recv(Connection, (char*)&rec_color, sizeof(int), NULL);
		if (rec_color == 2) {
			SetConsoleColor(FOREGROUND_GREEN);
			//std::cout << login << ": " << msg << '\n';
			auto current_time = std::chrono::system_clock::now();
			auto time_stamp = std::chrono::system_clock::to_time_t(current_time);
			std::stringstream time_stream;
			time_stream << std::put_time(std::localtime(&time_stamp), "%Y-%m-%d %X");
			std::cout << login << "- [" << time_stream.str() << "]- " << msg;
		}
		else {
			SetConsoleColor(FOREGROUND_BLUE | FOREGROUND_RED);
			//std::cout << login << ": " << msg << std::endl;
			//std::string new_str = login + std::string(": ") + msg + '\n';
			auto current_time = std::chrono::system_clock::now();
			auto time_stamp = std::chrono::system_clock::to_time_t(current_time);
			std::stringstream time_stream;
			time_stream << std::put_time(std::localtime(&time_stamp), "%Y-%m-%d %X");
			std::string new_str = login + std::string("- [") + time_stream.str() + std::string("]- ") + msg;
			PrintMessageOnRight(new_str);
		}

		SetConsoleColor(FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);

		break;
	}
	case P_Auth:
	{
		recv(Connection, (char*)&root_size, sizeof(int), NULL);
		root = new char[root_size + 1];
		root[root_size] = '\0';

		recv(Connection, root, root_size, NULL);
		break;
	}
	case P_Error:
	{
		std::cout << "Error in login or password" << std::endl;

		/*time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);*/
		std::string new_msg = " - error in login or password\n";
		//log_file << timeString << " - error in login or password" << std::endl;
		LogEvent(new_msg);
		return false;
	}
	case P_Ban:
	{
		if (root != NULL && strcmp(root, "admin") == 0)
		{
			std::cout << "one admin wants to block u" << std::endl;

			time_t currentTime = time(nullptr);
			std::string timeString = ctime(&currentTime);

			log_file << timeString << " - one admin wants to block u" << std::endl;

			break;
		}

		std::cout << "Banned by admin" << std::endl;

		time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);

		log_file << timeString << " - banned by admin" << std::endl;

		return false;
	}
	case P_PrivateMessage:
	{
		char msg[64];
		char sender[64];
		char recipient[64];

		recv(Connection, sender, sizeof(sender), NULL);
		recv(Connection, recipient, sizeof(recipient), NULL);
		recv(Connection, msg, sizeof(msg), NULL);
		auto current_time = std::chrono::system_clock::now();
		auto time_stamp = std::chrono::system_clock::to_time_t(current_time);
		std::stringstream time_stream;
		time_stream << std::put_time(std::localtime(&time_stamp), "%Y-%m-%d %X");
		//std::cout << username << "- [" << time_stream.str() << "]- " << message;
		std::cout << sender << " to " << recipient << "- [" << time_stream.str() << "]- " << ": " << msg << std::endl;

		break;
	}
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

//обработчик сообщений с сервера
void ClientHandler() {
	Packet packettype;
	while (true) {
		if (recv(Connection, (char*)&packettype, sizeof(Packet), NULL) <= 0) {
			exit(1);
		}

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


void GetPassword(char* password, int max_size) {
	char ch;
	int ind = 0;
	while ((ch = _getch()) != 13 && ind < max_size - 1) {
		std::cout << '*';
		password[ind++] = ch;
	}

	password[ind] = '\0';
	std::cout << std::endl;
}
int main(int argc, char* argv[]) {
	/*if (log_file.is_open()) {
		std::cout << "Hi";
	}*/

	WSAData wsaData;
	WORD DLLVersion = MAKEWORD(2, 1);
	if (WSAStartup(DLLVersion, &wsaData) != 0) {
		std::cout << "Error" << std::endl;
		time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);
		log_file << timeString << " Error" << std::endl;
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
		time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);
		log_file << timeString << " Error: failed connect to server." << std::endl;

		return 1;
	}

	//создаем поток
	hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ClientHandler, NULL, NULL, NULL);

	if (hThread == NULL)
	{
		std::cout << "error with thread" << std::endl;

		time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);

		log_file << timeString << " - error in thread" << std::endl;

		return 1;
	}

	//регестрация или вход
	char answer[64];
	std::cout << "U wanna reg or login?" << std::endl;
	std::cin.getline(answer, 64);

	//проверка на правильность команды
	if (strcmp(answer, "login") != 0 && strcmp(answer, "reg") != 0)
	{
		std::cout << "wrong command" << std::endl;

		time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);

		log_file << timeString << " - error in command" << std::endl;

		return 1;
	}

	//авторизация
	//char credentials[128];
	char login[64];
	char password[64];
	std::cout << "Write ur login: " << std::endl;
	//std::cin.getline(credentials, 128);
	std::cin.getline(login, 64);
	std::cout << "Write your password:\n ";
	//std::cin.getline(password, 64);
	//разделяем логин и пароль

	//заменяем символы пароля на звездочку
	GetPassword(password, sizeof(password));
	//pars_login_and_password(login, password, credentials);

	//проверка на пустой логин или пароль
	if (/*password[0] == char(-52) ||*/ login[0] == char(-52))
	{
		time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);

		log_file << timeString << " - error in password or login" << std::endl;
		return 1;
	}

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
			log_file << timeString << login << " failed to send message to server." << std::endl;
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
			log_file << timeString << login << " failed to send message to server." << std::endl;
			closesocket(Connection);
			WSACleanup();
			return 1;
		}

		send(Connection, login, sizeof(login), NULL);
		send(Connection, password, sizeof(password), NULL);

	}

	//проверка, что с логин и паролем все хорошо и пользователь не забанен
	Sleep(500);
	DWORD dwExitCode;
	if (GetExitCodeThread(hThread, &dwExitCode) && dwExitCode == STILL_ACTIVE) {
		std::cout << login << " entere to chat." << std::endl;

		time_t currentTime = time(nullptr);
		std::string timeString = ctime(&currentTime);

		log_file << timeString << " " << login << " - was connected" << std::endl;
	}
	else {
		return 1;
	}
	//получаем сообщения
	char msg1[64];
	while (true) {

		std::cin.getline(msg1, sizeof(msg1));

		//выход
		if (strcmp(msg1, "exit") == 0)
		{
			time_t currentTime = time(nullptr);
			std::string timeString = ctime(&currentTime);
			std::cout << login << " left the chat.\n";
			log_file << timeString << " " << login << " - left the chat." << std::endl;
			//TODO
			closesocket(Connection);

			return 0;
		}

		if (strcmp(msg1, "private.") == 0) {
			std::cout << "Online users:" << '\n';
			Packet packettype = P_Online;
			send(Connection, (char*)&packettype, sizeof(Packet), NULL);

			Sleep(150);
			char login_of_private[64];
			std::cout << "Write the name of user to send the private message:\n ";
			std::cin.getline(login_of_private, sizeof(login_of_private));

			packettype = P_PrivateMessage;
			send(Connection, (char*)&packettype, sizeof(Packet), NULL);
			//отправка времени на авторизацию
			time_t currentTime = time(nullptr);
			std::string timeString = ctime(&currentTime);
			int time_size = timeString.size();
			send(Connection, (char*)&time_size, sizeof(int), NULL);
			if (send(Connection, timeString.c_str(), timeString.size(), NULL) == SOCKET_ERROR) {
				std::cout << "Failed to send message to server." << std::endl;
				log_file << timeString << " " << login << " failed to send message to server." << std::endl;
				closesocket(Connection);
				WSACleanup();
				return 1;
			}
			char p_msg[64];
			std::cout << "Enter the private message: ";
			std::cin.getline(p_msg, sizeof(p_msg));
			send(Connection, login, sizeof(login), NULL);
			send(Connection, login_of_private, sizeof(login_of_private), NULL);
			send(Connection, p_msg, sizeof(p_msg), NULL);
			continue;
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
					log_file << timeString << " " << login << " failed to send message to server." << std::endl;
					closesocket(Connection);
					WSACleanup();
					return 1;
				}

				send(Connection, login_of_ban, sizeof(login_of_ban), NULL);
				log_file << timeString << ": " << login_of_ban << " was banned. " << std::endl;
			}
			else {
				std::cout << "u cannt ban" << std::endl;

				time_t currentTime = time(nullptr);
				std::string timeString = ctime(&currentTime);

				log_file << timeString << " " << login << " - u cannt ban" << std::endl;
			}

			continue;
		}


		//обработка неправильного сообщения и отправка - можно не проверять на длину, тк, если цепочка больше 64, то не будет заканчиваться знаками конца
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
				log_file << timeString << " " << login << " failed to send message to server." << std::endl;
				closesocket(Connection);
				WSACleanup();
				return 1;
			}

			//отправляем логин пользователя
			send(Connection, login, sizeof(login), NULL);

			//отправляем есообщение
			send(Connection, msg1, sizeof(msg1), NULL);
			log_file << timeString << " " << login << ": " << msg1 << std::endl;
		}
		else {
			Packet packettype = P_Error;
			send(Connection, (char*)&packettype, sizeof(Packet), NULL);
			send(Connection, login, sizeof(login), NULL);
			send(Connection, msg1, sizeof(msg1), NULL);

			std::cout << "something wrong with msg or you sent the private message" << std::endl;


			//log_file << timeString << " " << login << " - something wrong with msg" << std::endl;
			std::string new_msg = login + std::string(" - something wrong with msg.\n");
			LogEvent(new_msg);
		}

		Sleep(10);
	}
}