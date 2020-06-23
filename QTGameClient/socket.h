#pragma once

#include <winsock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)

#include <cstdlib>
#include <cstdio>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <ctime>
#include <algorithm>


typedef struct MyPackage {
	int id;
	int current;
	int total;
	int len;
	char data[1400];
} MyPackage;

typedef struct KeyPackage {
	char key;
	std::chrono::nanoseconds nsSent;
};


namespace Socket {

	static const int BUFLEN = 1600;

	static const char endcode[] = { 0, 0, 1, 0xb7 };


	static const char* host = "127.0.0.1";
	static const short port = 5005;

	class UDPReciever {
	private:
		SOCKET s;
		struct sockaddr_in server, si_other;
		int slen, recv_len;
		char buf[BUFLEN];
		WSADATA wsa;
	public:
		UDPReciever() {
			this->slen = sizeof(this->si_other);

			//Initialise winsock
			printf("\nInitialising Winsock...");
			if (WSAStartup(MAKEWORD(2, 2), &(this->wsa)) != 0)
			{
				printf("Failed.Error Code : %d", WSAGetLastError());
				exit(EXIT_FAILURE);
			}
			printf("Initialised.\n");

			//Create a socket
			if ((this->s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
			{
				printf("Could not create socket : %d", WSAGetLastError());
			}
			printf("Socket created.\n ");

			//Prepare the sockaddr_in structure
			this->server.sin_family = AF_INET;
			this->server.sin_addr.s_addr = INADDR_ANY;
			this->server.sin_port = htons(port);

			//Bind
			if (bind(this->s, (struct sockaddr*) & (this->server), sizeof(this->server)) == SOCKET_ERROR)
			{
				printf("Bind failed with error code : %d", WSAGetLastError());
				exit(EXIT_FAILURE);
			}
			printf("Bind done.");

		}

		MyPackage* recieve() {
			//clear the buffer by filling null, it might have previously received data
			memset(this->buf, 0, BUFLEN);

			//try to receive some data, this is a blocking call
			if ((this->recv_len = recvfrom(this->s, this->buf, BUFLEN, 0, (struct sockaddr*) & (this->si_other), &(this->slen))) == SOCKET_ERROR)
			{
				printf("recvfrom() failed with error code : %d", WSAGetLastError());
				exit(EXIT_FAILURE);
			}
			MyPackage* package = (MyPackage*)buf;
			for (int i = 0; i < std::fmin(package->len, 20); i++) {
				printf("%d", package->data[i]);
			}printf("\n");
			return package;
		}

		std::map<int, std::map<int, std::vector<char>>> read() {
			std::map<int, std::map<int, std::vector<char>>> packages;
			while (1) {
				MyPackage* package = this->recieve();
				if (package->len == 4) {
					bool same = true;
					for (int i = 0; i < 4; i++) {
						if (package->data[i] != endcode[i]) {
							same = false;
							break;
						}
					}
					if (same) {
						break;
					}
				}
				for (int i = 0; i < package->len; i++) {
					packages[package->id][package->current].push_back(package->data[i]);
				}
			}
			return packages;
		}

		~UDPReciever() {
			closesocket(this->s);
			WSACleanup();
		}
	};
	class UDPSender {
	private:
		int iResult;
		WSADATA wsaData;
		SOCKET SendSocket = INVALID_SOCKET;
		sockaddr_in RecvAddr;
		char buffer[1600];
	public:
		UDPSender(char* host, short port) {
			iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (iResult != NO_ERROR) {
				wprintf(L"WSAStartup failed with error: %d\n", iResult);
				exit(-1);
			}

			SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (SendSocket == INVALID_SOCKET) {
				wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
				WSACleanup();
				exit(-1);
			}

			RecvAddr.sin_family = AF_INET;
			RecvAddr.sin_port = htons(port);
			RecvAddr.sin_addr.s_addr = inet_addr(host);
		}
		int sendUDP(char* host, short port, char* message, int length, int id) {
			char* msgCpy = (char*)malloc(length);
			memcpy(msgCpy, message, length);


			/* send a message to the server */
			int pos = 0;
			int total = length / 1400;
			int current = 0;
			do
			{
				int messageLength = (std::min)(1400, length - pos);
				if (sendto(SendSocket, msgCpy, length, 0, (struct sockaddr*) & RecvAddr, sizeof(RecvAddr)) < 0) {
					perror("sendto failed");
					return -1;
				}
				//printf("Sent %d %d/%d\n", package.id, package.current, package.total);
				pos += messageLength;
			} while (pos < length);
			printf("Sent total %d\n", length);
			free(msgCpy);
			return 0;
		}

		~UDPSender() {
			// wprintf(L"Finished sending. Closing socket.\n");
			iResult = closesocket(SendSocket);
			if (iResult == SOCKET_ERROR) {
				wprintf(L"closesocket failed with error: %d\n", WSAGetLastError());
				WSACleanup();
				exit(-1);
			}
			WSACleanup();
		}

	};

	static void loadFromGame() {

		Socket::UDPReciever server;

		std::map<int, std::map<int, std::vector<char>>> packages = server.read();

		std::string filename = std::string("mpeg4-" + std::to_string(time(NULL)) + ".mkv");
		std::FILE* f;
		fopen_s(&f, filename.c_str(), "wb");

		for (auto package : packages) {
			int id = package.first;
			std::map<int, std::vector<char>> packets = package.second;
			for (auto packet : packets) {
				fwrite(packet.second.data(), 1, packet.second.size(), f);
			}
		}

		fclose(f);
	}
}




