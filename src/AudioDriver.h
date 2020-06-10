#ifndef __AUDIO_DRIVER_H
#define __AUDIO_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#define BUF_SIZE		240

extern volatile bool g_bPDMDataReady;
extern int16_t i16PDMBuf[2][BUF_SIZE];
extern uint32_t u32PDMPingpong;

void PDMinit(void);
void PDMdeinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_DRIVER_H */
