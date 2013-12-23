#ifndef _sender__hh
#define _sender__hh

// 仅仅为了演示方便, 使用 udp 传输视频帧, 而且假设每个视频帧都能放到一个 udp 包中
//
//
#ifdef __cplusplus
extern "C" {
#endif // c++

void *sender_open (const char *target_ip, int target_port);
void sender_close (void *snd);
int sender_send (void *snd, const void *data, int len);

#ifdef __cplusplus
}
#endif // c++

#endif // sender.h

