#include "htp.h"
#include <ctype.h>
#include <pthread.h>
#include <stdarg.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <regex.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080
#define BUFFER_SIZE 1e6

const char *get_mime_type(const char *ext)
{
	if (strcmp(ext, "html") == 0) {
		return "text/html";
	} else if (strcmp(ext, "css") == 0) {
		return "text/css";
	} else if (strcmp(ext, "jpeg") == 0) {
		return "image/jpeg";
	} else if (strcmp(ext, "jpg") == 0) {
		return "image/jpeg";
	} else if (strcmp(ext, "JPG") == 0) {
		return "image/jpeg";
	} else if (strcmp(ext, "png") == 0) {
		return "image/png";
	} else {
		return "text/plain";
	}
}

void build_http_response(const char *filename, const char *extension,
			 char *response, size_t *response_len)
{
	const char *mime_type = get_mime_type(extension);

	char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
	snprintf(header, BUFFER_SIZE,
		 "HTTP/1.1 200 OK\r\n"
		 "Content-Type: %s\r\n",
		 mime_type);

	int file_fd = open(filename, O_RDONLY);
	if (file_fd == -1) {
		snprintf(response, BUFFER_SIZE,
			 "HTTP/1.1 404 Not Found\r\n"
			 "Content-Type: text/plain\r\n"
			 "404 Not Found");
		*response_len = strlen(response);
		return;
	}

	struct stat file_stat;
	fstat(file_fd, &file_stat);
	off_t file_size = file_stat.st_size;

	// abstract this into it's own method? feels weird to just exist here
	char *content_length_header;
	asprintf(&content_length_header, "Content-Length: %lld\r\n\r\n",
		 file_size);
	strcat(header, content_length_header);
	free(content_length_header);

	*response_len = 0;
	memcpy(response, header, strlen(header));
	*response_len += strlen(header);

	ssize_t bytes_read;
	while ((bytes_read = read(file_fd, response + *response_len,
				  BUFFER_SIZE - *response_len)) > 0) {
		*response_len += bytes_read;
	}
	free(header);
	close(file_fd);
}

char *url_decode(char *encoded_filename)
{
	char *t;
	char *decoded_filename = calloc(strlen(encoded_filename), sizeof(char));

	for (t = encoded_filename; *t != '\0'; t++) {
		if (*t == '%') {
			if (!isdigit(*(t + 1)) || !isdigit(*(t + 2))) {
				strncat(decoded_filename, t, sizeof(char));
				continue;
			}
			char encoded_char[2];
			char decoded_char[sizeof(int)];
			strncpy(encoded_char, t + 1, 2 * sizeof(char));
			int char_as_hex = (int)strtol(encoded_char, NULL, 16);
			snprintf(decoded_char, sizeof(int), "%c", char_as_hex);
			strncat(decoded_filename, decoded_char, sizeof(int));
			t += 2;
		} else {
			strncat(decoded_filename, t, sizeof(char));
		}
	}

	return decoded_filename;
}

const char *get_file_extension(char *filename)
{
	regex_t regex;
	regcomp(&regex, "[.](.*)$", REG_EXTENDED);
	regmatch_t matches[2];
	if (regexec(&regex, filename, 2, matches, 0) == 0) {
		filename[matches[1].rm_eo] = '\0';
		return filename + matches[1].rm_so;
	} else {
		return "";
	}
}

void *handle_client(void *arg)
{
	int client_fd = *((int *)arg); // gimmie that socket_fd
	char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
	ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
	if (bytes_received > 0) {
		regex_t regex;
		regcomp(&regex, "^(.*) /([^ ]*) HTTP/1", REG_EXTENDED);
		regmatch_t matches[3];
		char *response = (char *)malloc(BUFFER_SIZE * 2 * sizeof(char));
		size_t response_len;

		if (regexec(&regex, buffer, 3, matches, 0) == 0) {
			buffer[matches[1].rm_eo] = '\0';
			buffer[matches[2].rm_eo] = '\0';

			const char *request_type = buffer + matches[1].rm_so;
			char *encoded_filename = buffer + matches[2].rm_so;

			char *filename = url_decode(encoded_filename);
			fprintf(stderr, "%s\n", filename);

			if (strlen(filename) == 0) {
				free(filename);
				filename = strdup("index.html");
			}
			char extension[32];
			strcpy(extension, get_file_extension(filename));
			if (strlen(extension) == 0) {
				asprintf(&filename, "%s.html", filename);
				strcpy(extension, "html");
			}

			if (strcmp(request_type, "GET") == 0) {
				build_http_response(filename, extension,
						    response, &response_len);
			} else {
				// not supported request type
				snprintf(response, BUFFER_SIZE,
					 "HTTP/1.1 501 Not Implemented\r\n"
					 "Content-Type: text/plain\r\n"
					 "501 Not Implemented");
				response_len = strlen(response);
			}

			send(client_fd, response, response_len, 0);

			free(filename);
		} else {
			snprintf(response, BUFFER_SIZE,
				 "HTTP/1.1 404 Not Found\r\n"
				 "Content-Type: text/plain\r\n"
				 "404 Not Found");
			response_len = strlen(response);

			send(client_fd, response, response_len, 0);
		}

		free(response);
		regfree(&regex);
	}

	close(client_fd);
	free(arg); // client_fd
	free(buffer);
	return NULL;
}

int main(void)
{
	int server_fd;
	struct sockaddr_in server_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (server_fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	int yes = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);

	if (bind(server_fd, (struct sockaddr *)&server_addr,
		 sizeof(server_addr)) < 0) {
		perror("bind");
		close(server_fd);
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, 10) < 0) {
		perror("listen");
		close(server_fd);
		exit(EXIT_FAILURE);
	}

	while (1) {
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		int *client_fd = malloc(sizeof(int));

		*client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
				    &client_addr_len);

		if (*client_fd < 0) {
			perror("accept");
			continue;
		}

		pthread_t thread_id;
		pthread_create(&thread_id, NULL, handle_client,
			       (void *)client_fd);
		pthread_detach(thread_id);
	}

	close(server_fd);
	return 0;
}
