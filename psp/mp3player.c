// mp3player.c: MP3 Player Implementation in C for Sony PSP
//
////////////////////////////////////////////////////////////////////////////

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspiofilemgr.h>
#include <pspdisplay.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include "mp3player.h"
#include "pspaudiolib.h"
#include "pspaudio_kernel.h"

#define FALSE 0
#define TRUE !FALSE
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))
#define MadErrorString(x) mad_stream_errorstr(x)
#define INPUT_BUFFER_SIZE	(5*8192)
#define OUTPUT_BUFFER_SIZE	2048	/* Must be an integer multiple of 4. */
#define MUTED_VOLUME 0x800
#define FASTFORWARD_VOLUME 0x2200

/* This table represents the subband-domain filter characteristics. It
* is initialized by the ParseArgs() function and is used as
* coefficients against each subband samples when DoFilter is non-nul.
*/
mad_fixed_t Filter[32];
double filterDouble[32] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* DoFilter is non-nul when the Filter table defines a filter bank to
* be applied to the decoded audio subbands.
*/
int DoFilter = 0;

u8 *ptr;
long size;
long samplesInOutput = 0;
long sampleNumber = 1;

int sceAudioSetFrequency(int frequency);

//////////////////////////////////////////////////////////////////////
// Global local variables
//////////////////////////////////////////////////////////////////////
//libmad lowlevel stuff

// The following variables contain the music data, ie they don't change value until you load a new file
struct mad_stream Stream;
struct mad_header Header; 
struct mad_frame Frame;
struct mad_synth Synth;
mad_timer_t Timer;
signed short OutputBuffer[OUTPUT_BUFFER_SIZE];
unsigned char InputBuffer[INPUT_BUFFER_SIZE + MAD_BUFFER_GUARD],
    *OutputPtr = (unsigned char *) OutputBuffer, *GuardPtr = NULL;
const unsigned char *OutputBufferEnd = (unsigned char *) OutputBuffer + OUTPUT_BUFFER_SIZE * 2;
int i;

// The following variables are maintained and updated by the tracker during playback
static int isPlaying;		// Set to true when a mod is being played

//////////////////////////////////////////////////////////////////////
// These are the public functions
//////////////////////////////////////////////////////////////////////
static int myChannel;
static int eos;

struct MP3Info MP3_info;

#define BOOST_OLD 0
#define BOOST_NEW 1

int MP3_volume_boost_type = BOOST_NEW;
double MP3_volume_boost = 0.0;
unsigned int MP3_volume_boost_old = 0;
double DB_forBoost = 1.0;
int MP3_playingSpeed = 0; // 0 = normal

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Applies a frequency-domain filter to audio data in the subband-domain.	
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void ApplyFilter(struct mad_frame *Frame)
{
    int Channel, Sample, Samples, SubBand;
    /* There is two application loops, each optimized for the number
     * of audio channels to process. The first alternative is for
     * two-channel frames, the second is for mono-audio.
     */
    Samples = MAD_NSBSAMPLES(&Frame->header);
    if (Frame->header.mode != MAD_MODE_SINGLE_CHANNEL)
	for (Channel = 0; Channel < 2; Channel++)
	    for (Sample = 0; Sample < Samples; Sample++)
		for (SubBand = 0; SubBand < 32; SubBand++)
			Frame->sbsample[Channel][Sample][SubBand] =
			mad_f_mul(Frame->sbsample[Channel][Sample][SubBand], Filter[SubBand]);
    else
	for (Sample = 0; Sample < Samples; Sample++)
	    for (SubBand = 0; SubBand < 32; SubBand++)
			Frame->sbsample[0][Sample][SubBand] = mad_f_mul(Frame->sbsample[0][Sample][SubBand], Filter[SubBand]);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Converts a sample from libmad's fixed point number format to a signed	
// short (16 bits).															
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static signed short MadFixedToSshort(mad_fixed_t Fixed)
{
    /* A fixed point number is formed of the following bit pattern:
     *
     * SWWWFFFFFFFFFFFFFFFFFFFFFFFFFFFF
     * MSB                          LSB
     * S ==> Sign (0 is positive, 1 is negative)
     * W ==> Whole part bits
     * F ==> Fractional part bits
     *
     * This pattern contains MAD_F_FRACBITS fractional bits, one
     * should alway use this macro when working on the bits of a fixed
     * point number. It is not guaranteed to be constant over the
     * different platforms supported by libmad.
     *
     * The signed short value is formed, after clipping, by the least
     * significant whole part bit, followed by the 15 most significant
     * fractional part bits. Warning: this is a quick and dirty way to
     * compute the 16-bit number, madplay includes much better
     * algorithms.
     */

    /* Clipping */
    if (Fixed >= MAD_F_ONE)
	return (SHRT_MAX);
    if (Fixed <= -MAD_F_ONE)
	return (-SHRT_MAX);

    /* Conversion. */
    Fixed = Fixed >> (MAD_F_FRACBITS - 15);
    return ((signed short) Fixed);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Volume boost for a single sample:
//OLD METHOD
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
short volume_boost(short Sample, unsigned int boost){
	int intSample = Sample;
	intSample *= (boost + 1);
	if (intSample > 32767){
		intSample = 32767;
	}else if (intSample < -32768){
		intSample = -32768;
	}
	Sample = intSample;
	return Sample;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//MP3 Callback for audio:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void MP3Callback(void *_buf2, unsigned int numSamples, void *pdata)
{
  short *_buf = (short *)_buf2;
    unsigned long samplesOut = 0;
	int framesSkipped = 0;

    if (isPlaying == TRUE) {	//  Playing , so mix up a buffer
		if (samplesInOutput > 0) {
			if (samplesInOutput > numSamples) {
			memcpy((char *) _buf, (char *) OutputBuffer, numSamples * 2 * 2);
			samplesOut = numSamples;
			samplesInOutput -= numSamples;
			} else {
			memcpy((char *) _buf, (char *) OutputBuffer, samplesInOutput * 2 * 2);
			samplesOut = samplesInOutput;
			samplesInOutput = 0;
			}
		}
		while (samplesOut < numSamples) {
			if (Stream.buffer == NULL || Stream.error == MAD_ERROR_BUFLEN) {
				mad_stream_buffer(&Stream, ptr, size);
				Stream.error = 0;
			}

			//Controllo la velocità:
			if (++framesSkipped <= MP3_playingSpeed){
				if (mad_header_decode(&Header, &Stream)) {
					if (MAD_RECOVERABLE(Stream.error)) {
						return;
					} else if (Stream.error == MAD_ERROR_BUFLEN) {
						eos = 1;
						return;
					} else {
						MP3_Stop();
					}
				}

				mad_timer_add(&Timer, Header.duration);

				//Instant bitrate:
				MP3_info.instantBitrate = Header.bitrate;
				continue;
			}else{
				if (mad_frame_decode(&Frame, &Stream)) {
					if (MAD_RECOVERABLE(Stream.error)) {
						return;
					} else if (Stream.error == MAD_ERROR_BUFLEN) {
						eos = 1;
						return;
					} else {
						MP3_Stop();
					}
				}

				mad_timer_add(&Timer, Frame.header.duration);

				//Instant bitrate:
				MP3_info.instantBitrate = Frame.header.bitrate;
			}
			framesSkipped = 0;

			if (DoFilter || MP3_volume_boost)
				ApplyFilter(&Frame);

			mad_synth_frame(&Synth, &Frame);

			for (i = 0; i < Synth.pcm.length; i++) {
				signed short Sample;
				if (samplesOut < numSamples) {
					/* Left channel */
					Sample = MadFixedToSshort(Synth.pcm.samples[0][i]);
					//Volume Boost (OLD METHOD):
					if (MP3_volume_boost_old)
						Sample = volume_boost(Sample, MP3_volume_boost_old);
					
					_buf[samplesOut * 2] = Sample;

					/* Right channel. If the decoded stream is monophonic then
					 * the right output channel is the same as the left one.
					 */
					if (MAD_NCHANNELS(&Frame.header) == 2){
						Sample = MadFixedToSshort(Synth.pcm.samples[1][i]);
						//Volume Boost (OLD METHOD):
						if (MP3_volume_boost_old)
							Sample = volume_boost(Sample, MP3_volume_boost_old);
						
                    }
					_buf[samplesOut * 2 + 1] = Sample;
					samplesOut += sampleNumber;
				} else {
					Sample = MadFixedToSshort(Synth.pcm.samples[0][i]);
					//Volume Boost (OLD METHOD):
					if (MP3_volume_boost_old)
						Sample = volume_boost(Sample, MP3_volume_boost_old);
					
					OutputBuffer[samplesInOutput * 2] = Sample;
					if (MAD_NCHANNELS(&Frame.header) == 2)
						Sample = MadFixedToSshort(Synth.pcm.samples[1][i]);
						//Volume Boost (OLD METHOD):
						if (MP3_volume_boost_old)
							Sample = volume_boost(Sample, MP3_volume_boost_old);
						
					OutputBuffer[samplesInOutput * 2 + 1] = Sample;
					samplesInOutput += sampleNumber;

				}
			}
		}
	} else {			//  Not Playing , so clear buffer
		int count;
		for (count = 0; count < numSamples * 2; count++)
		*(_buf + count) = 0;
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Init:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MP3_Init(int channel)
{
    myChannel = channel;
    isPlaying = FALSE;
	MP3_playingSpeed = 0;
	MP3_volume_boost = 0;
    MP3_volume_boost_old = 0;
    pspAudioSetChannelCallback(myChannel, MP3Callback,0);
    /* First the structures used by libmad must be initialized. */
    mad_stream_init(&Stream);
	mad_header_init(&Header);
    mad_frame_init(&Frame);
    mad_synth_init(&Synth);
    mad_timer_reset(&Timer);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Free tune
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MP3_FreeTune()
{
    /* The input file was completely read; the memory allocated by our
     * reading module must be reclaimed.
     */
    if (ptr)
		free(ptr);

    /* Mad is no longer used, the structures that were initialized must
     * now be cleared.
     */
    mad_synth_finish(&Synth);
    mad_header_finish(&Header);
    mad_frame_finish(&Frame);
    mad_stream_finish(&Stream);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Recupero le informazioni sul file:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void getInfo(){
	unsigned long FrameCount = 0;
	struct mad_stream stream;
	struct mad_header header; 
	mad_stream_init (&stream);
	mad_header_init (&header);

	MP3_info.fileSize = size;
	mad_timer_reset(&MP3_info.length);
	mad_stream_buffer (&stream, ptr, size);
	while (1){
		if (mad_header_decode (&header, &stream) == -1){
			if (MAD_RECOVERABLE(stream.error)){
				continue;				
			}else{
				break;
			}
		}
		//Informazioni solo dal primo frame:
	    if (FrameCount == 0){
			switch (header.layer) {
			case MAD_LAYER_I:
				strcpy(MP3_info.layer,"I");
				break;
			case MAD_LAYER_II:
				strcpy(MP3_info.layer,"II");
				break;
			case MAD_LAYER_III:
				strcpy(MP3_info.layer,"III");
				break;
			default:
				strcpy(MP3_info.layer,"unknown");
				break;
			}

			MP3_info.kbit = header.bitrate / 1000;
			MP3_info.hz = header.samplerate;
			switch (header.mode) {
			case MAD_MODE_SINGLE_CHANNEL:
				strcpy(MP3_info.mode, "single channel");
				break;
			case MAD_MODE_DUAL_CHANNEL:
				strcpy(MP3_info.mode, "dual channel");
				break;
			case MAD_MODE_JOINT_STEREO:
				strcpy(MP3_info.mode, "joint (MS/intensity) stereo");
				break;
			case MAD_MODE_STEREO:
				strcpy(MP3_info.mode, "normal LR stereo");
				break;
			default:
				strcpy(MP3_info.mode, "unknown");
				break;
			}

			switch (header.emphasis) {
			case MAD_EMPHASIS_NONE:
				strcpy(MP3_info.emphasis,"no");
				break;
			case MAD_EMPHASIS_50_15_US:
				strcpy(MP3_info.emphasis,"50/15 us");
				break;
			case MAD_EMPHASIS_CCITT_J_17:
				strcpy(MP3_info.emphasis,"CCITT J.17");
				break;
			case MAD_EMPHASIS_RESERVED:
				strcpy(MP3_info.emphasis,"reserved(!)");
				break;
			default:
				strcpy(MP3_info.emphasis,"unknown");
				break;
			}			
		}
		//Conteggio frame e durata totale:
		FrameCount++;
		mad_timer_add (&MP3_info.length, header.duration);
	}
	mad_header_finish (&header);
	mad_stream_finish (&stream);

	MP3_info.frames = FrameCount;
	//Formatto in stringa la durata totale:
	mad_timer_string(MP3_info.length, MP3_info.strLength, "%02lu:%02u:%02u", MAD_UNITS_HOURS, MAD_UNITS_MILLISECONDS, 0);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//MP3_End
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MP3_End()
{
    MP3_Stop();
    pspAudioSetChannelCallback(myChannel, 0,0);
    MP3_FreeTune();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Load mp3 into memory:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_Load(char *filename)
{
    int fd;
    eos = 0;
    if ((fd = sceIoOpen(filename, PSP_O_RDONLY, 0777)) > 0) {
		//  opened file, so get size now
		size = sceIoLseek(fd, 0, PSP_SEEK_END);
		sceIoLseek(fd, 0, PSP_SEEK_SET);
		ptr = (unsigned char *) malloc(size + 8);
		if (ptr) {		// Read file in
			memset(ptr, 0, size + 8);
			sceIoRead(fd, ptr, size);
		} else {
			sceIoClose(fd);
			//return FALSE;
            return ERROR_MEMORY;
		}
		// Close file
		sceIoClose(fd);
    } else {
		//return FALSE;
        return ERROR_OPENING;
    }
    isPlaying = FALSE;

	getInfo();

    //Controllo il sample rate:
	switch(MP3_info.hz){
		case (24000):
			sampleNumber = 2;
			//sceAudioSetFrequency(48000);
			break;
		case (48000):
			sampleNumber = 1;
			//sceAudioSetFrequency(48000);
			break;
		case (22050):
			sampleNumber = 2;
			//sceAudioSetFrequency(44100);
			break;
		case (44100):
			sampleNumber = 1;
			//sceAudioSetFrequency(44100);
			break;
		default:
			MP3_End();
			return ERROR_INVALID_SAMPLE_RATE;
			break;
	}
    return 1;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This function initialises for playing, and starts
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_Play()
{
	//Azzero il timer:
    if (eos == 1){
		mad_timer_reset(&Timer);
	}

	// See if I'm already playing
    if (isPlaying)
		return FALSE;

    isPlaying = TRUE;

	return TRUE;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pause:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MP3_Pause()
{
    isPlaying = !isPlaying;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stop:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_Stop()
{
    //stop playing
    isPlaying = FALSE;

    //clear buffer
    memset(OutputBuffer, 0, OUTPUT_BUFFER_SIZE);
    OutputPtr = (unsigned char *) OutputBuffer;

    return TRUE;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Get time string
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MP3_GetTimeString(char *dest)
{
    mad_timer_string(Timer, dest, "%02lu:%02u:%02u", MAD_UNITS_HOURS, MAD_UNITS_MILLISECONDS, 0);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Percentage
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_GetPercentace()
{
	//Calcolo posizione in %:
	float perc;
	 
	if (mad_timer_count(MP3_info.length, MAD_UNITS_SECONDS) > 0){
		perc = ((float)mad_timer_count(Timer, MAD_UNITS_SECONDS) / (float)mad_timer_count(MP3_info.length, MAD_UNITS_SECONDS)) * 100.0;
	}else{
		perc = 0;
	}
    return((int)perc);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Check EOS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_EndOfStream()
{
    if (eos == 1)
	return 1;
    return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Get info on file:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MP3Info MP3_GetInfo()
{
	return MP3_info;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Set volume boost type:
//NOTE: to be launched only once BEFORE setting boost volume or filter
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MP3_setVolumeBoostType(char *boostType){
    if (strcmp(boostType, "OLD") == 0){
        MP3_volume_boost_type = BOOST_OLD;
    }else{
        MP3_volume_boost_type = BOOST_NEW;
    }
    MP3_volume_boost_old = 0;
    MP3_volume_boost = 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Set volume boost:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MP3_setVolumeBoost(int boost){
    if (MP3_volume_boost_type == BOOST_NEW)
		{
        MP3_volume_boost_old = 0;
        MP3_volume_boost = boost;
    }
		else
		{
        MP3_volume_boost_old = boost;
        MP3_volume_boost = 0;
    }
    //Reapply the filter:
    MP3_setFilter(filterDouble, 0);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Get actual volume boost:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_getVolumeBoost(){
    if (MP3_volume_boost_type == BOOST_NEW){
    	return(MP3_volume_boost);
    }else{
        return(MP3_volume_boost_old);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Set Filter:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_setFilter(double tFilter[32], int copyFilter){
	//Converto i db:
	double AmpFactor;
	int i;

	for (i = 0; i < 32; i++){
		//Check for volume boost:
		if (MP3_volume_boost){
			AmpFactor=pow(10.,(tFilter[i] + MP3_volume_boost * DB_forBoost)/20);
		}else{
			AmpFactor=pow(10.,tFilter[i]/20);
		}
		if(AmpFactor>mad_f_todouble(MAD_F_MAX))
		{
			DoFilter = 0;
			return(0);
			//Filter[i] = mad_f_tofixed(mad_f_todouble(MAD_F_MAX));
		}else{
			Filter[i]=mad_f_tofixed(AmpFactor);
		}
		if (copyFilter){
			filterDouble[i] = tFilter[i];
		}
	}
	return(1);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Check if filter is enabled:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_isFilterEnabled(){
	return DoFilter;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Enable filter:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MP3_enableFilter(){
	DoFilter = 1;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Disable filter:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MP3_disableFilter(){
	DoFilter = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Get playing speed:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_getPlayingSpeed(){
	return MP3_playingSpeed;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Set playing speed:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_setPlayingSpeed(int playingSpeed){
	if (playingSpeed >= 0){
		MP3_playingSpeed = playingSpeed;
		if (playingSpeed == 0)
			pspAudioSetVolume(1, 0x8000, 0x8000);
		else
			pspAudioSetVolume(1, FASTFORWARD_VOLUME, FASTFORWARD_VOLUME);
		return 0;
	}else{
		return -1;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Set mute:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MP3_setMute(int onOff){
	if (onOff){
		pspAudioSetVolume(1, MUTED_VOLUME, MUTED_VOLUME);
	}else{
		pspAudioSetVolume(1, 0x8000, 0x8000);
	}
	return 0;
}
