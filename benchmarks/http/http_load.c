/* Compiled load generator for the HTTP servers of /std/http/server (benchmarks/http/README.md).
 *
 * The same command line, the same request shapes and the same JSON result as scripts/http_load.py, but each
 * thread keeps many connections in flight with non-blocking sockets and poll(), so a request costs a few
 * microseconds of CPU instead of the hundreds a Python process per client costs. Every request uses a new
 * connection and reads the response until the server closes it.
 *
 * Exit codes: 0 when every response was 2xx or 3xx; 1 when a response had another status or was not HTTP; 2 when
 * a connection could not be made or failed; 64 for a bad command line.
 *
 *   cc -O2 -pthread benchmarks/http/http_load.c -o http_load
 *   http_load http://127.0.0.1:8080/form --form-bytes 32768 --connections 16 --seconds 10 --json
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

enum { ExitBadStatus = 1, ExitConnection = 2, ExitUsage = 64 };
enum { Idle, Connecting, Sending, Reading };

typedef struct {
  int fd;
  int state;
  size_t sent;
  size_t head;
  char first[16];
  int64_t startedUs;
  long remaining;
} Client;

typedef struct {
  struct sockaddr_storage address;
  socklen_t addressLength;
  const char *request;
  size_t requestLength;
  double seconds;
  double timeout;
  long requestsPerClient;
  int64_t deadlineUs;
} Job;

typedef struct {
  const Job *job;
  int clients;
  uint32_t *latencies;
  size_t count;
  size_t capacity;
  int badStatus;
  char error[160];
  int64_t endedUs;
} Worker;

static int64_t nowUs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

static int statusOf(const char *first, size_t length) {
  if (length < 12 || memcmp(first, "HTTP/", 5) != 0) return 0;
  const char *space = memchr(first, ' ', length);
  if (space == NULL || space + 3 >= first + length) return 0;
  int status = 0;
  for (int i = 1; i <= 3; i++) {
    if (space[i] < '0' || space[i] > '9') return 0;
    status = status * 10 + (space[i] - '0');
  }
  return status;
}

static void fail(Worker *worker, const char *what) {
  if (worker->error[0] == 0) snprintf(worker->error, sizeof worker->error, "%s: %s", what, strerror(errno));
}

static int startConnect(Worker *worker, Client *client) {
  const Job *job = worker->job;
  int fd = socket(job->address.ss_family, SOCK_STREAM, 0);
  if (fd < 0) {
    fail(worker, "cannot create a socket");
    return -1;
  }
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
  client->fd = fd;
  client->sent = 0;
  client->head = 0;
  client->startedUs = nowUs();
  int rc = connect(fd, (const struct sockaddr *)&job->address, job->addressLength);
  if (rc != 0 && errno != EINPROGRESS) {
    fail(worker, "cannot connect");
    close(fd);
    client->fd = -1;
    return -1;
  }
  client->state = Connecting;
  return 0;
}

static void record(Worker *worker, Client *client) {
  int status = statusOf(client->first, client->head);
  if (status < 200 || status > 399) {
    if (!worker->badStatus) worker->badStatus = status ? status : -1;
    return;
  }
  if (worker->count == worker->capacity) {
    worker->capacity = worker->capacity ? worker->capacity * 2 : 4096;
    worker->latencies = realloc(worker->latencies, worker->capacity * sizeof(uint32_t));
    if (worker->latencies == NULL) abort();
  }
  worker->latencies[worker->count++] = (uint32_t)(nowUs() - client->startedUs);
}

static void *runWorker(void *argument) {
  Worker *worker = argument;
  const Job *job = worker->job;
  Client *clients = calloc((size_t)worker->clients, sizeof(Client));
  struct pollfd *fds = calloc((size_t)worker->clients, sizeof(struct pollfd));
  char buffer[65536];
  for (int i = 0; i < worker->clients; i++) {
    clients[i].fd = -1;
    clients[i].remaining = job->requestsPerClient;
  }
  int64_t timeoutUs = (int64_t)(job->timeout * 1000000.0);
  int live = 0;
  for (int i = 0; i < worker->clients; i++) {
    if (startConnect(worker, &clients[i]) != 0) goto finished;
    live++;
  }
  while (live > 0) {
    int64_t now = nowUs();
    for (int i = 0; i < worker->clients; i++) {
      fds[i].fd = clients[i].fd;
      fds[i].events = clients[i].state == Reading ? POLLIN : POLLOUT;
      fds[i].revents = 0;
      if (clients[i].fd >= 0 && now - clients[i].startedUs > timeoutUs) {
        errno = ETIMEDOUT;
        fail(worker, "connection timed out");
        goto finished;
      }
    }
    if (poll(fds, (nfds_t)worker->clients, 1000) < 0) {
      if (errno == EINTR) continue;
      fail(worker, "poll failed");
      goto finished;
    }
    for (int i = 0; i < worker->clients; i++) {
      Client *client = &clients[i];
      if (client->fd < 0 || fds[i].revents == 0) continue;
      if (client->state == Connecting) {
        int err = 0;
        socklen_t length = sizeof err;
        getsockopt(client->fd, SOL_SOCKET, SO_ERROR, &err, &length);
        if (err != 0) {
          errno = err;
          fail(worker, "cannot connect");
          goto finished;
        }
        client->state = Sending;
      }
      if (client->state == Sending) {
        ssize_t n = send(client->fd, job->request + client->sent, job->requestLength - client->sent, 0);
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
          fail(worker, "connection failed");
          goto finished;
        }
        if (n > 0) client->sent += (size_t)n;
        if (client->sent == job->requestLength) client->state = Reading;
        else continue;
        fds[i].revents |= POLLIN;
      }
      if (client->state == Reading && (fds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
        for (;;) {
          ssize_t n = recv(client->fd, buffer, sizeof buffer, 0);
          if (n > 0) {
            if (client->head < sizeof client->first) {
              size_t take = sizeof client->first - client->head;
              memcpy(client->first + client->head, buffer, take < (size_t)n ? take : (size_t)n);
              client->head += take < (size_t)n ? take : (size_t)n;
            }
            continue;
          }
          if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
          if (n < 0 && errno == EINTR) continue;
          if (n < 0) {
            /* a reset after the request was sent counts as a failed connection */
            fail(worker, "connection failed");
            goto finished;
          }
          close(client->fd);
          client->fd = -1;
          record(worker, client);
          if (worker->badStatus) goto finished;
          if (client->remaining > 0) client->remaining--;
          int more = job->requestsPerClient > 0 ? client->remaining > 0 : nowUs() < job->deadlineUs;
          if (!more) {
            live--;
          } else if (startConnect(worker, client) != 0) {
            goto finished;
          }
          break;
        }
      }
    }
  }
finished:
  worker->endedUs = nowUs();
  for (int i = 0; i < worker->clients; i++)
    if (clients[i].fd >= 0) close(clients[i].fd);
  free(clients);
  free(fds);
  return NULL;
}

static int compareLatency(const void *a, const void *b) {
  uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
  return x < y ? -1 : x > y;
}

static void jsonString(const char *text) {
  putchar('"');
  for (; *text; text++) {
    if (*text == '"' || *text == '\\') putchar('\\');
    putchar(*text);
  }
  putchar('"');
}

static int usage(const char *message) {
  fprintf(stderr, "http_load: error: %s\n", message);
  fprintf(stderr,
          "usage: http_load URL [--connections N] [--threads N] [--seconds S] [--requests N] [--method M]\n"
          "                 [--body TEXT | --form-bytes N] [--timeout S] [--json]\n");
  return ExitUsage;
}

int main(int argc, char **argv) {
  const char *url = NULL, *method = NULL, *body = NULL;
  int connections = 16, threads = 0, json = 0;
  long requests = 0, formBytes = 0;
  double seconds = 10.0, timeout = 30.0;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    int hasValue = i + 1 < argc;
    if (!strcmp(a, "--json")) json = 1;
    else if (!strcmp(a, "--connections") && hasValue) connections = atoi(argv[++i]);
    else if (!strcmp(a, "--threads") && hasValue) threads = atoi(argv[++i]);
    else if (!strcmp(a, "--seconds") && hasValue) seconds = atof(argv[++i]);
    else if (!strcmp(a, "--requests") && hasValue) requests = atol(argv[++i]);
    else if (!strcmp(a, "--method") && hasValue) method = argv[++i];
    else if (!strcmp(a, "--body") && hasValue) body = argv[++i];
    else if (!strcmp(a, "--form-bytes") && hasValue) formBytes = atol(argv[++i]);
    else if (!strcmp(a, "--timeout") && hasValue) timeout = atof(argv[++i]);
    else if (a[0] != '-' && url == NULL) url = a;
    else return usage("unknown or incomplete option");
  }
  if (url == NULL || strncmp(url, "http://", 7) != 0) return usage("the URL must be http://host:port/path");
  if (connections < 1 || requests < 0 || seconds <= 0 || formBytes < 0 || timeout <= 0)
    return usage("--connections must be at least 1, --seconds above 0, --requests and --form-bytes not negative");
  if (body != NULL && formBytes > 0) return usage("give --body or --form-bytes, not both");

  char host[256], path[2048] = "/";
  const char *rest = url + 7;
  size_t hostEnd = strcspn(rest, ":/");
  if (hostEnd == 0 || hostEnd >= sizeof host) return usage("the URL must be http://host:port/path");
  memcpy(host, rest, hostEnd);
  host[hostEnd] = 0;
  rest += hostEnd;
  int port = 80;
  if (*rest == ':') port = atoi(rest + 1), rest += strcspn(rest, "/");
  if (*rest == '/') snprintf(path, sizeof path, "%s", rest);

  char portText[16];
  snprintf(portText, sizeof portText, "%d", port);
  struct addrinfo hints, *found = NULL;
  memset(&hints, 0, sizeof hints);
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, portText, &hints, &found) != 0 || found == NULL) {
    fprintf(stderr, "http_load: cannot resolve %s\n", host);
    return ExitConnection;
  }

  char *bodyBytes = NULL;
  size_t bodyLength = 0;
  if (body != NULL) {
    bodyLength = strlen(body);
    bodyBytes = strdup(body);
  } else if (formBytes > 0) {
    bodyLength = formBytes > 5 ? (size_t)formBytes : 5;
    bodyBytes = malloc(bodyLength);
    memcpy(bodyBytes, "text=", 5);
    memset(bodyBytes + 5, 'a', bodyLength - 5);
  }
  if (method == NULL) method = bodyBytes != NULL ? "POST" : "GET";
  size_t capacity = strlen(path) + strlen(host) + bodyLength + 256;
  char *request = malloc(capacity);
  int length = snprintf(request, capacity, "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n", method, path, host,
                        port);
  if (bodyBytes != NULL)
    length += snprintf(request + length, capacity - (size_t)length,
                       "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %zu\r\n", bodyLength);
  length += snprintf(request + length, capacity - (size_t)length, "\r\n");
  if (bodyBytes != NULL) memcpy(request + length, bodyBytes, bodyLength), length += (int)bodyLength;

  if (threads <= 0) threads = connections >= 64 ? 2 : 1;
  if (threads > connections) threads = connections;

  Job job;
  memset(&job, 0, sizeof job);
  memcpy(&job.address, found->ai_addr, found->ai_addrlen);
  job.addressLength = found->ai_addrlen;
  job.request = request;
  job.requestLength = (size_t)length;
  job.seconds = seconds;
  job.timeout = timeout;
  job.requestsPerClient = requests;
  int64_t startedUs = nowUs();
  job.deadlineUs = startedUs + (int64_t)(seconds * 1000000.0);

  Worker *workers = calloc((size_t)threads, sizeof(Worker));
  pthread_t *ids = calloc((size_t)threads, sizeof(pthread_t));
  for (int t = 0; t < threads; t++) {
    workers[t].job = &job;
    workers[t].clients = connections / threads + (t < connections % threads ? 1 : 0);
    pthread_create(&ids[t], NULL, runWorker, &workers[t]);
  }
  size_t total = 0;
  int badStatus = 0;
  const char *error = NULL;
  int failedClients = 0;
  int64_t endedUs = startedUs;
  for (int t = 0; t < threads; t++) {
    pthread_join(ids[t], NULL);
    total += workers[t].count;
    if (workers[t].badStatus && !badStatus) badStatus = workers[t].badStatus;
    if (workers[t].error[0]) {
      failedClients++;
      if (error == NULL) error = workers[t].error;
    }
    if (workers[t].endedUs > endedUs) endedUs = workers[t].endedUs;
  }
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  double cpu = (double)(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) +
               (double)(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1e6;

  uint32_t *all = malloc((total + 1) * sizeof(uint32_t));
  size_t at = 0;
  for (int t = 0; t < threads; t++) {
    memcpy(all + at, workers[t].latencies, workers[t].count * sizeof(uint32_t));
    at += workers[t].count;
  }
  qsort(all, total, sizeof(uint32_t), compareLatency);
  double p50 = 0, p99 = 0;
  if (total > 0) {
    size_t r50 = (total * 50 + 99) / 100, r99 = (total * 99 + 99) / 100;
    p50 = all[(r50 ? r50 : 1) - 1] / 1000.0;
    p99 = all[(r99 ? r99 : 1) - 1] / 1000.0;
  }
  double elapsed = (double)(endedUs - startedUs) / 1e6;
  double rate = elapsed > 0 ? (double)total / elapsed : 0;
  if (json) {
    printf("{\"url\": ");
    jsonString(url);
    printf(", \"connections\": %d, \"requests\": %zu, \"requests_per_second\": %.1f, \"p50_ms\": %.3f, "
           "\"p99_ms\": %.3f, \"threads\": %d, \"cpu_us_per_request\": %.2f}\n",
           connections, total, rate, p50, p99, threads, total > 0 ? cpu * 1e6 / (double)total : 0.0);
  } else {
    printf("%zu requests on %d connections: %.1f requests/s, p50 %.3f ms, p99 %.3f ms (%.2f us of CPU per request)\n",
           total, connections, rate, p50, p99, total > 0 ? cpu * 1e6 / (double)total : 0.0);
  }
  if (error != NULL) {
    fprintf(stderr, "http_load: %d of %d threads failed against %s:%d: %s\n", failedClients, threads, host, port,
            error);
    return ExitConnection;
  }
  if (badStatus) {
    if (badStatus > 0) fprintf(stderr, "http_load: a response was not 2xx or 3xx (status %d) from %s\n", badStatus, url);
    else fprintf(stderr, "http_load: a response was not 2xx or 3xx (not an HTTP response) from %s\n", url);
    return ExitBadStatus;
  }
  return 0;
}
