#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "sender.h"

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr    sockaddr;

struct Ctx
{
	int sock;
	sockaddr_in target;
};
typedef struct Ctx Ctx;

void *sender_open (const char *ip, int port)
{
	Ctx *ctx = malloc(sizeof(Ctx));
	ctx->sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (ctx->sock == -1) {
		fprintf(stderr, "%s: create sock err\n", __func__);
		exit(-1);
	}

	ctx->target.sin_family = AF_INET;
	ctx->target.sin_port = htons(port);
	ctx->target.sin_addr.s_addr = inet_addr(ip);

	return ctx;
}

void sender_close (void *snd)
{
	Ctx *c = (Ctx*)snd;
	close(c->sock);
	free(c);
}

int sender_send (void *snd, const void *data, int len)
{
	assert(len < 65536);
	Ctx *c = (Ctx*)snd;
	return sendto(c->sock, data, len, 0, (sockaddr*)&c->target, sizeof(sockaddr_in));
}

