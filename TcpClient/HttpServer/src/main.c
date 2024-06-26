#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "HTTP_Server.h"
#include "Routes.h"
#include "Response.h"
#include "Code_Run_Lib.h"
#include "Logger.h"
#include "cJSON.h"

#define CLIENT_BUFFER_SIZE 4096
#define RESPONSE_DATA_BUFFER_SIZE 4096
#define HEADER_BUFFER_SIZE 20
#define RESPONSE_BUFFER_SIZE RESPONSE_DATA_BUFFER_SIZE + HEADER_BUFFER_SIZE
#define PORT 6969

void run_code(char *json_string, char *response_data, int readSize)
{
	// write_log("json string: %s\n", json_string);
	ServerRequest req;
	cJSON *server_req_json = cJSON_Parse(json_string);
	if (server_req_json == NULL)
	{
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			write_log("Error before: %s\n", error_ptr);
		}
		return;
	}

	cJSON *language = cJSON_GetObjectItemCaseSensitive(server_req_json, "language");
	cJSON *code = cJSON_GetObjectItemCaseSensitive(server_req_json, "code");

	if (cJSON_IsString(language) && language->valuestring != NULL && cJSON_IsString(code) && code->valuestring != NULL)
	{
		req.language = language->valuestring;
		req.code = code->valuestring;
	}

	CodeRunnerResponse *codeRunResp = NULL;
	ServerResponse server_resp;

	if (strcmp(req.language, "C") == 0)
	{
		// write_log("Running the C code: %s\n", req.code);
		CodeRunnerResponse *resp = CompileCCode(req.code);
		if (resp == NULL)
		{
			write_log("Error: compiling C code\n");
			return;
		}
		codeRunResp = RunCCode(resp->filename);
		free(resp);
	}
	else if (strcmp(req.language, "RUST") == 0)
	{
		CodeRunnerResponse *resp = CompileRustCode(req.code);
		if (resp == NULL)
		{
			write_log("Error: compiling Rust code\n");
			return;
		}
		codeRunResp = RunRustCode(resp->filename);
		free(resp);
	}
	else if (strcmp(req.language, "PYTHON") == 0)
	{
		codeRunResp = RunPythonCode(req.code);
	}
	else
	{
		write_log("Error: language not supported\n");
		return;
	}

	server_resp.stdout = codeRunResp->stdout;
	server_resp.stderr = codeRunResp->stderr;

	cJSON *server_resp_json = cJSON_CreateObject();

	cJSON_AddStringToObject(server_resp_json, "stdout", server_resp.stdout);
	cJSON_AddStringToObject(server_resp_json, "stderr", server_resp.stderr);

	char *server_resp_json_string = cJSON_Print(server_resp_json);
	if (server_resp_json_string == NULL)
	{
		write_log("ERROR: Failed to print server_resp_json_string.");
	}

	snprintf(response_data, readSize, "%s", server_resp_json_string);
	cJSON_Delete(server_req_json);
	cJSON_Delete(server_resp_json);
	free(codeRunResp);
}

void MatchRoute(struct Route *route, char *urlRoute, char *body_start, char *response_data)
{
	struct Route *matchedRoute = search(route, urlRoute);

	// pages
	char templatePath[255] = "templates/";
	if (matchedRoute != NULL)
	{
		strcat(templatePath, matchedRoute->value);
		render_static_file(templatePath, response_data, RESPONSE_DATA_BUFFER_SIZE);
		// write_log("response_data: %s\n", response_data);
		return;
	}

	// resources like css and js
	if (strstr(urlRoute, "/static/") != NULL)
	{
		render_static_file("static/index.css", response_data, RESPONSE_DATA_BUFFER_SIZE);
		return;
	}

	// Rest API endpoints
	if (strstr(urlRoute, "/api/") != NULL)
	{

		if (strstr(urlRoute, "/run") != NULL)
		{
			run_code(body_start, response_data, RESPONSE_DATA_BUFFER_SIZE);
		}

		return;
	}

	render_static_file("templates/404.html", response_data, RESPONSE_DATA_BUFFER_SIZE);
}

int main()
{
	create_log_file();
	InitContainersThreadpool();
	// just for testing
	// codeRunLib_RunDemo();
	// return 0;

	HTTP_Server http_server;
	struct Route *route;
	int client_socket;
	// initiate HTTP_Server
	init_server(&http_server, PORT);
	// registering Routes
	route = initRoute("/", "index.html");
	addRoute(&route, "/about", "about.html");
	addRoute(&route, "/sth", "sth.html");
	addRoute(&route, "/chicken", "chicken.html");

	// display all available routes
	printf("\n====================================\n");
	printf("=========ALL VAILABLE ROUTES========\n");
	inorder(route);

	// request got from client
	char client_msg[CLIENT_BUFFER_SIZE] = "";
	// response data to be sent to client. No header included
	char *response_data = malloc(RESPONSE_DATA_BUFFER_SIZE);
	// response data with header
	char *response = malloc(RESPONSE_BUFFER_SIZE);
	// start listening to client
	while (1)
	{
		memset(client_msg, 0, CLIENT_BUFFER_SIZE);
		memset(response_data, 0, RESPONSE_DATA_BUFFER_SIZE);
		memset(response, 0, RESPONSE_BUFFER_SIZE);

		client_socket = accept(http_server.socket, NULL, NULL);
		read(client_socket, client_msg, CLIENT_BUFFER_SIZE - 1);
		write_log("Request from client: %s\n", client_msg);
		// parsing client socket header to get HTTP method, route
		char *method = "";
		char *urlRoute = "";
		char *body_start = strstr(client_msg, "\r\n\r\n");

		if (body_start == NULL)
			body_start = strstr(client_msg, "\n\n");

		if (body_start != NULL)
			body_start += (body_start[1] == '\n') ? 2 : 4;

		char *client_http_header = strtok(client_msg, "\n");

		printf("\n\n%s\n\n", client_http_header);

		char *header_token = strtok(client_http_header, " ");
		method = header_token;
		header_token = strtok(NULL, " ");
		urlRoute = header_token;

		// if (body_start != NULL)
		// 	write_log("Body start: %s\n", body_start);
		printf("The method is %s\n", method);
		printf("The route is %s\n", urlRoute);

		// match route
		MatchRoute(route, urlRoute, body_start, response_data);
		// compose response
		char http_header[HEADER_BUFFER_SIZE] = "HTTP/1.1 200 OK\r\n\r\n";
		snprintf(response, RESPONSE_BUFFER_SIZE, "%s%s", http_header, response_data);
		// write_log("Response: %s\n", response);
		ssize_t bytes_sent = send(client_socket, response, RESPONSE_BUFFER_SIZE, 0);
		if (bytes_sent == -1)
		{
			perror("Error sending response");
			close(client_socket);
			break;
		}
		close(client_socket);
	}
	free(response_data);
	free(response);
	return 0;
}