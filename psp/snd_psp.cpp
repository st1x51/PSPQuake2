
#include <stdlib.h>
#include <stdio.h>

#include <pspkernel.h>
#include <pspaudiolib.h>

extern "C"
{
#include "../client/client.h"
#include "../client/snd_loc.h"
}

int snd_inited = 0;

namespace quake2
{
	namespace sound
	{
		struct Sample
		{
			short left;
			short right;
		};

		static const unsigned int	channelCount				= 2;
		static const unsigned int	inputBufferSize				= 16384;
		static const unsigned int	inputFrequency				= 22050;
		static const unsigned int	outputFrequency				= 44100;
		static const unsigned int	inputSamplesPerOutputSample	= outputFrequency / inputFrequency;
		static Sample		inputBuffer[inputBufferSize];
		static volatile unsigned int	samplesRead;

		static inline void copySamples(const Sample* first, const Sample* last, Sample* destination)
		{
			switch (inputSamplesPerOutputSample)
			{
			case 1:
				memcpy(destination, first, (last - first) * sizeof(Sample));
				break;

			case 2:
				for (const Sample* source = first; source != last; ++source)
				{
					const Sample sample = *source;
					*destination++ = sample;
					*destination++ = sample;
				}
				break;

			case 4:
				for (const Sample* source = first; source != last; ++source)
				{
					const Sample sample = *source;
					*destination++ = sample;
					*destination++ = sample;
					*destination++ = sample;
					*destination++ = sample;
				}
				break;

			default:
				break;
			}
		}

		static void fillOutputBuffer(void* buffer, unsigned int samplesToWrite, void* userData)
		{
			// Where are we writing to?
			Sample* const destination = static_cast<Sample*> (buffer);

			// Where are we reading from?
			const Sample* const firstSampleToRead = &inputBuffer[samplesRead];

			// How many samples to read?
			const unsigned int samplesToRead = samplesToWrite / inputSamplesPerOutputSample;

			// Going to wrap past the end of the input buffer?
			const unsigned int samplesBeforeEndOfInput = inputBufferSize - samplesRead;
			if (samplesToRead > samplesBeforeEndOfInput)
			{
				// Yes, so write the first chunk from the end of the input buffer.
				copySamples(
					firstSampleToRead,
					firstSampleToRead + samplesBeforeEndOfInput,
					&destination[0]);

				// Write the second chunk from the start of the input buffer.
				const unsigned int samplesToReadFromBeginning = samplesToRead - samplesBeforeEndOfInput;
				copySamples(
					&inputBuffer[0],
					&inputBuffer[samplesToReadFromBeginning],
					&destination[samplesBeforeEndOfInput * inputSamplesPerOutputSample]);
			}
			else
			{
				// No wrapping, just copy.
				copySamples(
					firstSampleToRead,
					firstSampleToRead + samplesToRead,
					&destination[0]);
			}

			// Update the read offset.
			samplesRead = (samplesRead + samplesToRead) % inputBufferSize;
		}
	}
}

using namespace quake2;
using namespace quake2::sound;

qboolean SNDDMA_Init(void)
{
    if(snd_inited)
	  return true;

	// Set up Quake's audio.
	dma.channels = channelCount;

	if (dma.channels < 1 || dma.channels > 2)
		dma.channels = 2;

    dma.samplebits          = 16;
	dma.speed				= inputFrequency;
	dma.samples			    = inputBufferSize * channelCount;
    dma.samplepos           = 0;
	dma.submission_chunk	= 1;
	dma.buffer				= (unsigned char *) inputBuffer;

	// Initialise the audio system. This initialises it for the CD audio module
	// too.
	pspAudioInit();

	// Set the channel callback.
	// Sound effects use channel 0, CD audio uses channel 1.
	pspAudioSetChannelCallback(0, fillOutputBuffer, 0);

    snd_inited = 1;

	return true;
}

void SNDDMA_Shutdown(void)
{
    if(!snd_inited)
	  return;

	// Clear the mixing buffer so we don't get any noise during cleanup.
	memset(inputBuffer, 0, sizeof(inputBuffer));

	// Clear the channel callback.
	pspAudioSetChannelCallback(0, 0, 0);

	// Stop the audio system?
	pspAudioEndPre();

	// Insert a false delay so the thread can be cleaned up.
	sceKernelDelayThread(50 * 1000);

	// Shut down the audio system.
	pspAudioEnd();

	snd_inited = 0;
}

int	SNDDMA_GetDMAPos(void)
{
    if (!snd_inited)
	    return 0;

	return samplesRead * channelCount;
}

void SNDDMA_BeginPainting(void) 
{
}

void SNDDMA_Submit(void) 
{
}
