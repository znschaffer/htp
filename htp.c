#include "htp.h"
#include <pthread.h>
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
#define BUF_SIZE 1000000

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

void build_http_response(const char *filename, const char *ext, char *resp,
			 size_t *resp_len)
{
	const char *mime_type = get_mime_type(ext);

	char *header = (char *)malloc(BUF_SIZE * sizeof(char));
	snprintf(header, BUF_SIZE,
		 "HTTP/1.1 200 OK\r\n"
		 "Content-Type: %s\r\n",
		 mime_type);

	int file_fd = open(filename, O_RDONLY);
	if (file_fd == -1) {
		snprintf(resp, BUF_SIZE,
			 "HTTP/1.1 404 Not Found\r\n"
			 "Content-Type: text/plain\r\n"
			 "404 Not Found");
		*resp_len = strlen(resp);
		return;
	}

	struct stat file_stat;
	fstat(file_fd, &file_stat);
	off_t file_size = file_stat.st_size;

	char *clen;
	asprintf(&clen, "Content-Length: %lld\r\n\r\n", file_size);
	strcat(header, clen);
	free(clen);

	*resp_len = 0;
	memcpy(resp, header, strlen(header));
	*resp_len += strlen(header);

	ssize_t bytes_read;
	while ((bytes_read = read(file_fd, resp + *resp_len,
				  BUF_SIZE - *resp_len)) > 0) {
		*resp_len += bytes_read;
	}
	free(header);
	close(file_fd);
}

char *url_decode(const char *encoded_filename)
{
	return (char *)encoded_filename;
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
	int client_fd = *((int *)arg);
	char *buf = (char *)malloc(BUF_SIZE * sizeof(char));
	ssize_t bytes_recv = recv(client_fd, buf, BUF_SIZE, 0);
	if (bytes_recv > 0) {
		regex_t regex;
		regcomp(&regex, "^(.*) /([^ ]*) HTTP/1", REG_EXTENDED);
		regmatch_t matches[3];

		if (regexec(&regex, buf, 3, matches, 0) == 0) {
			buf[matches[1].rm_eo] = '\0';
			buf[matches[2].rm_eo] = '\0';

			const char *req_type = buf + matches[1].rm_so;
			char *filename = strdup(buf + matches[2].rm_so);

			if (strlen(filename) == 0) {
				free(filename);
				filename = strdup("index.html");
			}

			char ext[32];
			strcpy(ext, get_file_extension(filename));
			if (strlen(ext) == 0) {
				asprintf(&filename, "%s.html", filename);
				strcpy(ext, "html");
			}

			char *resp =
				(char *)malloc(BUF_SIZE * 2 * sizeof(char));
			size_t resp_len;
			if (strcmp(req_type, "GET") == 0) {
				build_http_response(filename, ext, resp,
						    &resp_len);
			} else {
				// not supported request type
				snprintf(resp, BUF_SIZE,
					 "HTTP/1.1 501 Not Implemented\r\n"
					 "Content-Type: text/plain\r\n"
					 "501 Not Implemented");
				resp_len = strlen(resp);
			}

			send(client_fd, resp, resp_len, 0);

			free(resp);
			free(filename);
		} else {
			char *resp =
				(char *)malloc(BUF_SIZE * 2 * sizeof(char));

			snprintf(resp, BUF_SIZE,
				 "HTTP/1.1 404 Not Found\r\n"
				 "Content-Type: text/plain\r\n"
				 "404 Not Found");
			size_t resp_len = strlen(resp);

			send(client_fd, resp, resp_len, 0);
			free(resp);
		}

		regfree(&regex);
	}
	close(client_fd);
	free(arg);
	free(buf);
	return NULL;
}

void htp_send_header(int sock, char *header, char *value)
{
	size_t len = snprintf(NULL, 0, "%s %s\n", header, value);
	char *buf = malloc(len + 1);
	snprintf(buf, len + 1, "%s %s\n", header, value);
	send(sock, buf, strlen(buf), 0);
	free(buf);
}

void htp_log(char *msg)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	char time_data[64];
	strftime(time_data, sizeof(time_data), "%x %X", tm);
	fprintf(stdout, "[htp] %s %s\n", time_data, msg);
}

int main(void)
{
	int server_fd;
	struct sockaddr_in server_addr;

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	int yes = 1;
	setsockopt(server_fd, 0, SO_REUSEADDR, &yes, sizeof(yes));

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

		if ((*client_fd = accept(server_fd,
					 (struct sockaddr *)&client_addr,
					 &client_addr_len)) < 0) {
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
