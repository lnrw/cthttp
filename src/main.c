#include "cthttp.h"

void default_onRequest(Request *request, Response *response) {
	const char *default_body = "Hello from cthttp!";
	response->body = (char *)malloc(strlen(default_body) + 1);
	strcpy(response->body, default_body);

	response_setHeader(response, "Content-Type", (const char *[]) { "text/plain" }, 1);
}

int main() {
	HttpServer server;
	server.onRequest = default_onRequest;

	create_server(8080, &server);

	return 0;
}
