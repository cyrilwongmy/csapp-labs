#include "csapp.h"
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *DEFAULT_HOST_HDR = "Host: www.cmu.edu\r\n";
static const char *USER_AGENT_HDR =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *CONNECTION_HDR = "Connection: close\r\n";
static const char *PROXY_CONN_HDR = "Proxy-Connection: close\r\n";
static const char *HTTP_VERSION = "HTTP/1.0";

typedef struct {
  char *host;
  char *user_agent;
  char *connection;
  char *proxy_connection;
  char *other_hdr;
} http_header;

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

// HTTP header related functions
http_header *init_http_headers();
char *deep_copy_string(const char *src);
void set_http_header(http_header *headers, const char *key, const char *value);
int parse_header(char *header_line, http_header *http_headers);
void free_http_headers(http_header *headers);
char *get_hostname_from_host(const char *host);
char *get_port_from_host(const char *host);
void print_http_header(http_header *header);
char *get_relative_uri(const char *uri);

http_header *get_http_header_from_input(rio_t *rio);

int main(int argc, char **argv) {
  int listenfd, connfd, clientlen;
  struct sockaddr_in clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    doit(connfd);                // line:netp:tiny:doit
    Close(connfd);               // line:netp:tiny:close
  }
}

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);             // line:netp:doit:readrequest
  sscanf(buf, "%s %s %s", method, uri, version); // line:netp:doit:parserequest
  if (strcasecmp(method, "GET")) { // line:netp:doit:beginrequesterr
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  } // line:netp:doit:endrequesterr

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs); // line:netp:doit:staticcheck

  char *relative_uri = get_relative_uri(uri);
  printf("[doit] relative_uri: %s\n", relative_uri);
  // TODO: extract the relative uri from the uri and pass to actual web server
  // override uri to "/home.html"
  // strncpy(uri, "/home.html", strlen("/home.html"));
  // uri[strlen("/home.html")] = '\0';

  // read header from clients
  // GET request should only have headers
  http_header *http_req_hdr = get_http_header_from_input(&rio);
  if (http_req_hdr == NULL) {
    clienterror(fd, "Header format error", "400", "Bad Request",
                "Header format error");
    return;
  }

  print_http_header(http_req_hdr);

  // connect to actual web server
  char *hostname = get_hostname_from_host(http_req_hdr->host);
  printf("[doit] hostname: %s\n", hostname);
  char *port = get_port_from_host(http_req_hdr->host);
  printf("[doit] port: %s\n", port);
  int clientfd = open_clientfd(hostname, port);
  rio_t rio_to_server;
  Rio_readinitb(&rio_to_server, clientfd);
  printf("[doit] connected to actual server hostname: %s, port: %s\n", hostname,
         port);

  /* Send request headers to the actual server */
  char proxy_req_hdr[MAXBUF];
  sprintf(proxy_req_hdr, "%s %s %s\r\n", method, relative_uri, HTTP_VERSION);
  sprintf(proxy_req_hdr, "%s%s", proxy_req_hdr, http_req_hdr->host);
  sprintf(proxy_req_hdr, "%s%s", proxy_req_hdr, http_req_hdr->user_agent);
  sprintf(proxy_req_hdr, "%s%s", proxy_req_hdr, http_req_hdr->connection);
  sprintf(proxy_req_hdr, "%s%s", proxy_req_hdr, http_req_hdr->proxy_connection);
  if (http_req_hdr->other_hdr[0] != '\0') {
    sprintf(proxy_req_hdr, "%s%s", proxy_req_hdr, http_req_hdr->other_hdr);
  }

  // print proxy_req_hdr
  printf("[doit] proxy_req_hdr: %s\n", proxy_req_hdr);
  Rio_writen(clientfd, proxy_req_hdr, strlen(proxy_req_hdr));

  char response_line[MAXLINE];
  int cnt = 0;
  while ((cnt = Rio_readlineb(&rio_to_server, response_line, MAXLINE)) != 0) {
    printf("[doit] response_line from tiny server: %s", response_line);
    Rio_writen(fd, response_line, cnt);
  }
  Close(clientfd);
  free(hostname);
  free(port);
  free(relative_uri);

  // if (is_static) { /* Serve static content */
  //   if (!(S_ISREG(sbuf.st_mode)) ||
  //       !(S_IRUSR & sbuf.st_mode)) { // line:netp:doit:readable
  //     clienterror(fd, filename, "403", "Forbidden",
  //                 "Tiny couldn't read the file");
  //     return;
  //   }
  //   serve_static(fd, filename, sbuf.st_size); // line:netp:doit:servestatic
  // } else {                                    /* Serve dynamic content */
  //   if (!(S_ISREG(sbuf.st_mode)) ||
  //       !(S_IXUSR & sbuf.st_mode)) { // line:netp:doit:executable
  //     clienterror(fd, filename, "403", "Forbidden",
  //                 "Tiny couldn't run the CGI program");
  //     return;
  //   }
  //   serve_dynamic(fd, filename, cgiargs); // line:netp:doit:servedynamic
  // }
}
/* $end doit */

/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) { // line:netp:readhdrs:checkterm
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  if (!strstr(uri, "cgi-bin")) {
    /* Static content */             // line:netp:parseuri:isstatic
    strcpy(cgiargs, "");             // line:netp:parseuri:clearcgi
    strcpy(filename, ".");           // line:netp:parseuri:beginconvert1
    strcat(filename, uri);           // line:netp:parseuri:endconvert1
    if (uri[strlen(uri) - 1] == '/') // line:netp:parseuri:slashcheck
      strcat(filename, "home.html"); // line:netp:parseuri:appenddefault
    return 1;
  } else { /* Dynamic content */ // line:netp:parseuri:isdynamic
    ptr = index(uri, '?');       // line:netp:parseuri:beginextract
    if (ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    } else
      strcpy(cgiargs, ""); // line:netp:parseuri:endextract
    strcpy(filename, "."); // line:netp:parseuri:beginconvert2
    strcat(filename, uri); // line:netp:parseuri:endconvert2
    return 0;
  }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);    // line:netp:servestatic:getfiletype
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // line:netp:servestatic:beginserve
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); // line:netp:servestatic:endserve

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // line:netp:servestatic:open
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd,
              0);                 // line:netp:servestatic:mmap
  Close(srcfd);                   // line:netp:servestatic:close
  Rio_writen(fd, srcp, filesize); // line:netp:servestatic:write
  Munmap(srcp, filesize);         // line:netp:servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* child */ // line:netp:servedynamic:fork
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // line:netp:servedynamic:setenv
    Dup2(fd, STDOUT_FILENO);
    /* Redirect stdout to client */ // line:netp:servedynamic:dup2
    Execve(filename, emptylist, environ);
    /* Run CGI program */ // line:netp:servedynamic:execve
  }
  Wait(NULL);
  /* Parent waits for and reaps child */ // line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body,
          "%s<body bgcolor="
          "ffffff"
          ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

http_header *init_http_headers() {
  http_header *headers = (http_header *)malloc(sizeof(http_header));
  if (!headers) {
    fprintf(stderr, "Memory allocation failed for http header\n");
    exit(1);
  }

  // Initialize all pointers to NULL except special cases
  headers->host = DEFAULT_HOST_HDR;
  headers->user_agent = USER_AGENT_HDR;
  headers->connection = CONNECTION_HDR;
  headers->proxy_connection = PROXY_CONN_HDR;
  headers->other_hdr = malloc(MAXBUF);
  // set all other headers to empty string
  headers->other_hdr[0] = '\0';
  return headers;
}

// Function to deep copy a string
char *deep_copy_string(const char *src) {
  if (src == NULL)
    return NULL;
  char *dest = (char *)malloc(strlen(src) + 1);
  if (!dest) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  strcpy(dest, src);
  return dest;
}

// Function to set a header in the HttpHeaders struct
void set_http_header(http_header *headers, const char *key, const char *value) {
  if (strcasecmp(key, "Host") == 0) {
    // concatenate the host header with the value and deep copy it
    char *host = (char *)malloc(strlen("Host: ") + strlen(value) + 3);
    sprintf(host, "Host: %s\r\n", value);
    headers->host = host;
  } else if (strcasecmp(key, "User-Agent") == 0) {
    // skip
  } else if (strcasecmp(key, "Connection") == 0) {
    // skip
  } else if (strcasecmp(key, "Proxy-Connection") == 0) {
    // skip
  } else {
    sprintf(headers->other_hdr, "%s%s: %s\r\n", headers->other_hdr, key, value);
  }
}

// Function to parse a single header line and set it in the HttpHeaders struct
int parse_header(char *header_line, http_header *http_headers) {
  const char *colon_ptr = strchr(header_line, ':');

  if (colon_ptr != NULL) {
    char *key = (char *)malloc(colon_ptr - header_line + 1);
    if (key != NULL) {
      strncpy(key, header_line, colon_ptr - header_line);
      key[colon_ptr - header_line] = '\0';
    }

    // Skip the colon and any following spaces
    const char *value_start = colon_ptr + 1;
    while (*value_start == ' ')
      value_start++;

    // Allocate memory for the value and copy it
    char *value = strdup(value_start);
    printf("[parse_header] key: %s, value: %s\n", key, value);
    set_http_header(http_headers, key, value);
  } else {
    // header format error
    return -1;
  }
}

// Function to free the HttpHeaders struct
void free_http_headers(http_header *headers) {
  if (headers->host && strncpy(headers->host, DEFAULT_HOST_HDR, strlen(DEFAULT_HOST_HDR)) != 0) {
    free(headers->host);
  }
  if (headers->other_hdr) {
    free(headers->other_hdr);
  }
  free(headers);
}

// Function to extract the hostname from the Host header
char *get_hostname_from_host(const char *host) {
  const char *second_colon = strrchr(host, ':');
  const char *first_colon = strchr(host, ':');
  const char *host_start_ptr = strrchr(host, ' ');
  host_start_ptr++;
  if (second_colon == first_colon) {
    // hostname without port
    return strdup(host_start_ptr);
  }

  if (second_colon == NULL) {
    printf("[get_hostname_from_host] format error for host: %s\n", host);
    return NULL;
  }

  const char *host_end_ptr = second_colon - 1;

  // Calculate the length of the hostname part
  size_t hostname_length = host_end_ptr - host_start_ptr + 1;

  // Allocate memory for the hostname and copy it
  char *hostname =
      (char *)malloc(hostname_length + 1); // +1 for null terminator
  if (hostname == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }

  strncpy(hostname, host_start_ptr, hostname_length);
  hostname[hostname_length] = '\0'; // Null-terminate the string

  return hostname;
}

// Function to extract the port number from the Host header
char *get_port_from_host(const char *host) {
  printf("[get_port_from_host] host: %s\n", host);
  // Look for the colon in the host string
  const char *colon_ptr = strrchr(host, ':');
  // malloc a string to store the port number
  char *port = (char *)malloc(6); // 5 digits + null terminator

  // If a colon is found, try to convert the following characters to an
  // integer
  if (colon_ptr != NULL) {
    // Convert the string after the colon to an integer (the port number)
    strncpy(port, colon_ptr + 1, 5); // Copy at most 5 characters
    port[5] = '\0';                  // Null-terminate the string
    return port;
  }

  // If no port is specified, return the default port (e.g., 80 for HTTP)
  strncpy(port, "80", 3); // Copy "80" and null terminator
  port[2] = '\0';         // Null-terminate the string
  return port;
}

http_header *get_http_header_from_input(rio_t *rio) {
  char line[MAXLINE];
  http_header *header = init_http_headers();

  printf("[get_http_header_from_input] reading request headers from clients\n");
  // read in all header lines and populate in the header struct.
  Rio_readlineb(rio, line, MAXLINE);
  while (strcmp(line, "\r\n")) { // line:netp:readhdrs:checkterm
    printf("[get_http_header_from_input] header line: %s",
           line); // this line contains \r\n
    int ok = parse_header(line, header);
    if (ok == -1) {
      return NULL;
    }
    Rio_readlineb(rio, line, MAXLINE);
  }
  printf("[get_http_header_from_input] last line %s", line);

  printf("[get_http_header_from_input] finished reading request headers from "
         "clients\n");
  return header;
}

void print_http_header(http_header *header) {
  printf("[print_http_header] start to print http header\n");
  printf("[print_http_header] host: %s\n", header->host);
  printf("[print_http_header] user_agent: %s\n", header->user_agent);
  printf("[print_http_header] connection: %s\n", header->connection);
  printf("[print_http_header] proxy_connection: %s\n",
         header->proxy_connection);
  printf("[print_http_header] other_hdr: %s\n", header->other_hdr);
  printf("[print_http_header] end to print http header\n");
}

char *get_relative_uri(const char *uri) {
  char *relative_uri = strchr(uri, '/');
  relative_uri += 2; // skip the first two // which is http://
  relative_uri = strchr(relative_uri, '/');

  return strdup(relative_uri);
}