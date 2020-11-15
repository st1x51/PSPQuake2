#ifndef CD_H
#define CD_H

void CDAudio_VolumeChange(float bgmvolume);
void CDAudio_Play(byte track, qboolean looping);
void CDAudio_Stop(void);
void CDAudio_Next(void);
void CDAudio_Prev(void);
void CDAudio_Pause(void);
void CDAudio_Resume(void);
void CDAudio_Update(void);
int CDAudio_Init(void);
void CDAudio_PrintMusicList(void);
void CDAudio_Shutdown(void);
void CDAudio_SysSuspend (void);
void CDAudio_SysResume(void);

#endif //CD_H
