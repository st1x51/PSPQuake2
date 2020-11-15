#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <pspaudio.h>
#include <pspmp3.h>
#include <psputility.h>

#define MP3FILE "ms0:/MUSIC/test.mp3"

int fd;
int handle;
int samplingRate;
int numChannels;
int mp3_start = 0;
// Input and Output buffers
char   mp3Buf[16*1024]  __attribute__((aligned(64)));
short   pcmBuf[16*(1152/2)]  __attribute__((aligned(64)));

int fillStreamBuffer(int fd, int handle)
{
   char* dst;
   int write;
   int pos;

   // Get Info on the stream (where to fill to, how much to fill, where to fill from)
   sceMp3GetInfoToAddStreamData( handle, &dst, &write, &pos);
   // Seek file to position requested
  sceIoLseek32(fd, pos, SEEK_SET);
   // Read the amount of data
   int read = sceIoRead(fd, dst, write);
   if (!read)
      return 0;
   // Notify mp3 library about how much we really wrote to the stream buffer
   sceMp3NotifyAddStreamData(handle, read);
   return (pos>0);
}

extern float mp3_track;

int audio(SceSize args, void *argp)
{
   static int channel = -1;
   static int lastDecoded = 0;
   static int volume = PSP_AUDIO_VOLUME_MAX;
   while(1)
   {
     while(mp3_start)
     {
     //Con_Printf("0xffffffff\n");
	 if (sceMp3CheckStreamDataNeeded(handle) > 0)
         fillStreamBuffer(fd, handle);
     // Decode some samples
     short* buf;
     int bytesDecoded;
     bytesDecoded = sceMp3Decode(handle, &buf);
     if (bytesDecoded<=0)
         sceMp3CheckStreamDataNeeded(handle);
     // Nothing more to decode? Must have reached end of input buffer
     if (bytesDecoded==0 || bytesDecoded==0x80671402)
         sceMp3ResetPlayPosition(handle);
     else
     {
         // Reserve the Audio channel for our output if not yet done
         if (channel<0 || lastDecoded != bytesDecoded)
         {
           if (channel>=0)
              sceAudioSRCChRelease();
           channel = sceAudioSRCChReserve(bytesDecoded / (2 * numChannels), samplingRate, numChannels);
         }
         // Output the decoded samples and accumulate the number of played samples to get the playtime
         sceAudioSRCOutputBlocking(volume, buf);
	 }
    }
	mp3_track = mp3_track + 1;
  }
  return 0;
}
extern char mp3_path[256];
int Init_mp3()
{
   // Load modules
   sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
   sceUtilityLoadModule(PSP_MODULE_AV_MP3);

   // Init mp3 resources
   sceMp3InitResource();

   // Open the input file
   fd = sceIoOpen(mp3_path, PSP_O_RDONLY, 0777);

   // Reserve a mp3 handle for our playback
   SceMp3InitArg mp3Init;
   mp3Init.mp3StreamStart = 0;
   mp3Init.mp3StreamEnd = sceIoLseek32(fd, 0, SEEK_END);
   mp3Init.unk1 = 0;
   mp3Init.unk2 = 0;
   mp3Init.mp3Buf = mp3Buf;
   mp3Init.mp3BufSize = sizeof(mp3Buf);
   mp3Init.pcmBuf = pcmBuf;
   mp3Init.pcmBufSize = sizeof(pcmBuf);

   handle = sceMp3ReserveMp3Handle(&mp3Init);

   // Fill the stream buffer with some data so that sceMp3Init has something to work with
   fillStreamBuffer(fd, handle);

   sceMp3Init(handle);

   samplingRate = sceMp3GetSamplingRate(handle);
   numChannels = sceMp3GetMp3ChannelNum(handle);

   Con_Printf("AUDIO THREAD ENABLE->->->->->\n");

   SceUID thid;
   int ret;

   thid = sceKernelCreateThread("mp3decode_thread", audio, 30, 0x2000, 0, 0);
   if (thid < 0)
   {
	  Con_Printf("failed to create decode thread: %08x\n", thid);
	  ret = thid;
   }
   ret = sceKernelStartThread(thid, 0, 0);
   if (ret < 0)
   {
		Con_Printf("failed to start mp3 thread: %08x\n", ret);
   }

   return 0;
}

