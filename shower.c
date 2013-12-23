#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#include "vshow.h"
#include "recver.h"

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr    sockaddr;

/** 绑定 127.0.0.1:3020 udp 端口, 接收数据, 一个udp包, 总是完整帧, 交给 libavcodec 解码, 交给
 *  vshow 显示
 */

#define RECV_PORT 3021

int main (int argc, char **argv)
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		fprintf(stderr, "ERR: create sock err\n");
		exit(-1);
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(RECV_PORT);
	sin.sin_addr.s_addr = inet_addr("192.168.1.5");
	if (bind(sock, (sockaddr*)&sin, sizeof(sin)) < 0) {
		fprintf(stderr, "ERR: bind %d\n", RECV_PORT);
		exit(-1);
	}

	unsigned char *buf = (unsigned char*)alloca(65536);
	if (!buf) {
		fprintf(stderr, "ERR: alloca 65536 err\n");
		exit(-1);
	}

	avcodec_register_all();
	AVCodec *codec = avcodec_find_decoder(CODEC_ID_H264);
	AVCodecContext *dec = avcodec_alloc_context3(codec);
	if (avcodec_open2(dec, codec, NULL) < 0) {
		fprintf(stderr, "ERR: open H264 decoder err\n");
		exit(-1);
	}

	AVFrame *frame = avcodec_alloc_frame();

	void *shower = 0;	// 成功解码第一帧后, 才知道大小

	for (; ; ) {
		sockaddr_in from;
		socklen_t fromlen = sizeof(from);
		int rc = recvfrom(sock, buf, 65536, 0, (sockaddr*)&from, &fromlen);
		if (rc > 0) {
			// 解压
			int got;
			AVPacket pkt;
            memset(&pkt, 0, sizeof(pkt));
			pkt.data = buf;
			pkt.size = rc;
			int ret = avcodec_decode_video2(dec, frame, &got, &pkt);
			if (ret > 0 && got) {
				// 解码成功
				if (!shower) {
					shower = vs_open(dec->width, dec->height);
					if (!shower) {
						fprintf(stderr, "ERR: open shower window err!\n");
						exit(-1);
					}
				}

				// 显示
				vs_show(shower, frame->data, frame->linesize);
			}
		}
	}

	avcodec_close(dec);
	av_free(dec);
	av_free(frame);
	close(sock);
	vs_close(shower);

	return 0;
}

