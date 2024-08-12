#ifndef HTP_H
#define HTP_H

// INCLUDES
#include <stddef.h>

// FUNCTION DECS
const char *get_mime_type(const char *ext);
void build_http_response(const char *filename, const char *ext, char *resp,
			 size_t *resp_len);
char *url_decode(const char *encoded_filename);
const char *get_file_extension(char *filename);
void *handle_client(void *arg);
void htp_send_header(int sock, char *header, char *value);
void htp_log(char *msg);

#endif // !HTP_H
