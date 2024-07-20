#include "status.h"

#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define INITIAL_BUFFER_SIZE 1024

typedef struct {
	char method[8];
	char *url;
	char *headers;
	char *body;
} Request;

typedef struct {
	int status_code;
	char headers[INITIAL_BUFFER_SIZE];
	char *body;
} Response;

typedef struct {
	void (*onRequest)(Request *, Response *);
} HttpServer;

void print(const char *msg) {
	time_t time_info;
	char time_formatted[26];
	struct tm* tm_info;

	time_info = time(NULL);
	tm_info = localtime(&time_info);

	strftime(time_formatted, 26, "%Y-%m-%d %H:%M:%S", tm_info);

	printf("[%s] %s\n", time_formatted, msg);
}

void print_err(const char *msg) {
	time_t time_info;
	char time_formatted[26];
	struct tm* tm_info;

	time_info = time(NULL);
	tm_info = localtime(&time_info);

	strftime(time_formatted, 26, "%Y-%m-%d %H:%M:%S", tm_info);

	fprintf(stderr, "[%s] %s: %s\n", time_formatted, msg, strerror(errno));
}

void response_setHeader(Response *response, const char *header_name, const char **header_values, size_t num_values) {
	if (header_name && header_values && num_values > 0) {
		strncat(response->headers, header_name, INITIAL_BUFFER_SIZE - strlen(response->headers) - 1);
		strncat(response->headers, ": ", INITIAL_BUFFER_SIZE - strlen(response->headers) - 1);

		for (int i = 0; i < num_values; i++) {
			if (header_values[i]) {
				strncat(response->headers, header_values[i], INITIAL_BUFFER_SIZE - strlen(response->headers) - 1);

				// Separate each value with a comma, except for the last one
				if (i < num_values - 1) {
					strncat(response->headers, ",", INITIAL_BUFFER_SIZE - strlen(response->headers) - 1);
				}
			}
		}
		strncat(response->headers, "\r\n", INITIAL_BUFFER_SIZE - strlen(response->headers) - 1);
	}
}

void handle_client(SOCKET client_socket, HttpServer *server) {
	int buffer_size = INITIAL_BUFFER_SIZE;
	char *buffer = (char *)malloc(buffer_size);
	if (buffer == NULL) {
		print_err("Memory allocation failed");
		closesocket(client_socket);
		return;
	}

	int bytes_received = recv(client_socket, buffer, buffer_size, 0);
	while (bytes_received == buffer_size) {
		buffer_size *= 2;
		buffer = (char *)realloc(buffer, buffer_size);
		if (buffer == NULL) {
			print_err("Memory allocation failed");
			closesocket(client_socket);
			return;
		}
		bytes_received += recv(client_socket, buffer + bytes_received, buffer_size - bytes_received, 0);
	}

	if (bytes_received > 0) {
		buffer[bytes_received] = '\0';

		// Print only the first line of the request
		char *first_line_end = strstr(buffer, "\r\n");
		if (first_line_end) {
			*first_line_end = '\0';
		}
		print(buffer);

		Request request;
		Response response = { 200, "", NULL };

		// Parse request
		sscanf(buffer, "%s %s", &request.method, &request.url);

		// Find the start of headers
		char *headers_start = strstr(buffer, "\r\n\r\n");
		if (headers_start) {
			headers_start += 4;  // Move past "\r\n\r\n"

			// Calculate length of headers section
			size_t headers_length = bytes_received - (headers_start - buffer);

			// Allocate memory for headers and copy them
			request.headers = (char *)malloc(headers_length + 1);
			if (request.headers == NULL) {
				print_err("Memory allocation failed");
				free(buffer);
				closesocket(client_socket);
				return;
			}
			strncpy(request.headers, headers_start, headers_length);
			request.headers[headers_length] = '\0';
		} else {
			request.headers = strdup("");  // No headers found
		}

		if (server && server->onRequest) {
			server->onRequest(&request, &response);
		}

		const char *status_text = STATUS_TEXT(response.status_code);
		
		size_t response_buffer_size = INITIAL_BUFFER_SIZE;
		char *response_buffer = (char *)malloc(response_buffer_size);

		if (response_buffer == NULL) {
			print_err("Memory allocation failed");
			free(buffer);
			closesocket(client_socket);
			return;
		}

		int required_size = snprintf(response_buffer, response_buffer_size, "HTTP/1.1 %d %s\r\n%s\r\n%s",
									 response.status_code, status_text, response.headers, response.body ? response.body : "") + 1;

		if (response_buffer_size < required_size) {
			response_buffer = (char *)realloc(response_buffer, response_buffer_size);

			if (response_buffer == NULL) {
				print_err("Memory allocation failed");
				free(buffer);
				closesocket(client_socket);
				return;
			}

			snprintf(response_buffer, response_buffer_size, "HTTP/1.1 %d %s\r\n%s\r\n%s",
					 response.status_code, status_text, response.headers, response.body ? response.body : "");
		}

		send(client_socket, response_buffer, strlen(response_buffer), 0);

		if (response.body) {
			free(response.body);
		}
		free(request.headers);
	}

	free(buffer);
	closesocket(client_socket);
}

void create_server(int port, HttpServer *server) {
	WSADATA wsaData;
	SOCKET server_socket, client_socket;
	struct sockaddr_in server_addr, client_addr;
	int client_addr_size = sizeof(client_addr);

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		print_err("WSAStartup failed");
		return;
	}

	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		print_err("Could not create socket");
		WSACleanup();
		return;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		print_err("Bind failed");
		closesocket(server_socket);
		WSACleanup();
		return;
	}

	listen(server_socket, 3);
	char msg[INITIAL_BUFFER_SIZE];
	snprintf(msg, INITIAL_BUFFER_SIZE, "Listening on Port %d", port);
	print(msg);

	while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size)) != INVALID_SOCKET) {
		print("Connection accepted");
		handle_client(client_socket, server);
	}

	if (client_socket == INVALID_SOCKET) {
		print_err("Accept failed");
		closesocket(server_socket);
		WSACleanup();
		return;
	}

	closesocket(server_socket);
	WSACleanup();
}
