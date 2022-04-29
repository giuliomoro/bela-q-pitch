/*
 ____  _____ _        _
| __ )| ____| |      / \
|  _ \|  _| | |     / _ \
| |_) | |___| |___ / ___ \
|____/|_____|_____/_/   \_\
http://bela.io
*/
#include <Bela.h>
#include <libraries/AudioFile/AudioFile.h>
#include <vector>
#include "q/support/literals.hpp"
#include "q/pitch/pitch_detector.hpp"
#include "q/fx/envelope.hpp"

//#define INPUT_PLAYBACK // do not use audio inputs, use playback from file instead
//#define OUTPUT_WRITE // write output to file and stop after

#ifdef OUTPUT_WRITE
std::vector<std::vector<float>> gOutputs;
std::string gFilenameOutputs = "outputs.wav";
unsigned int gWrittenFrames = 0; // how many frame have actually been written to gOutputs
#endif // OUTPUT_WRITE

#ifdef INPUT_PLAYBACK
#define BUFFER_LEN 22050   // BUFFER LENGTH

// https://freesound.org/data/previews/368/368313_3395229-hq.mp3 and https://freesound.org/data/previews/329/329336_5725661-hq.mp3
// concatenated and expanded to 8 channels
std::string gFilename = "multiguit.wav";
int gNumFramesInFile;
int gNumChannelsInFile;

// Two buffers for each channel:
// one of them loads the next chunk of audio while the other one is used for playback
std::vector<std::vector<float> > gSampleBuf[2];

// read pointer relative current buffer (range 0-BUFFER_LEN)
// initialise at BUFFER_LEN to pre-load second buffer (see render())
int gReadPtr = BUFFER_LEN;
// read pointer relative to file, increments by BUFFER_LEN (see fillBuffer())
int gBufferReadPtr = 0;
// keeps track of which buffer is currently active (switches between 0 and 1)
int gActiveBuffer = 0;
// this variable will let us know if the buffer doesn't manage to load in time
int gDoneLoadingBuffer = 1;

AuxiliaryTask gFillBufferTask;

void fillBuffer(void*) {

	// increment buffer read pointer by buffer length
	gBufferReadPtr+=BUFFER_LEN;

	// reset buffer pointer if it exceeds the number of frames in the file
	if(gBufferReadPtr>=gNumFramesInFile)
	gBufferReadPtr=0;

	int endFrame = gBufferReadPtr + BUFFER_LEN;
	int zeroPad = 0;

	// if reaching the end of the file take note of the last frame index
	// so we can zero-pad the rest later
	if((gBufferReadPtr+BUFFER_LEN)>=gNumFramesInFile-1) {
		endFrame = gNumFramesInFile-1;
		zeroPad = 1;
	}

	for(unsigned int ch=0; ch < gSampleBuf[0].size(); ch++) {

		// fill (nonactive) buffer
		AudioFileUtilities::getSamples(gFilename,gSampleBuf[!gActiveBuffer][ch].data(),ch
			,gBufferReadPtr,endFrame);

		// zero-pad if necessary
		if(zeroPad) {
			int numFramesToPad = BUFFER_LEN - (endFrame-gBufferReadPtr);
			for(int n=0;n<numFramesToPad;n++)
				gSampleBuf[!gActiveBuffer][ch][n+(BUFFER_LEN-numFramesToPad)] = 0;
		}
	}
	gDoneLoadingBuffer = 1;
}
#endif// INPUT_PLAYBACK

#include <libraries/Oscillator/Oscillator.h>
using namespace cycfi::q::literals;
std::vector<cycfi::q::pitch_detector> pds;
std::vector<cycfi::q::envelope_follower> envs;
std::vector<Oscillator> oscs;
BelaCpuData mycpu = {
	.count = 100,
};

bool setup(BelaContext *context, void *userData)
{
#ifdef INPUT_PLAYBACK
	if((gFillBufferTask = Bela_createAuxiliaryTask(&fillBuffer, 90, "fill-buffer")) == 0)
		return false;

	gNumFramesInFile = AudioFileUtilities::getNumFrames(gFilename);
	if(gNumFramesInFile <= 0)
		return false;

	if(gNumFramesInFile <= BUFFER_LEN) {
		printf("Sample needs to be longer than buffer size. This example is intended to work with long samples.");
		return false;
	}

	gNumChannelsInFile = AudioFileUtilities::getNumChannels(gFilename);
	printf("Number of channels in file: %d\n", gNumChannelsInFile);

	gSampleBuf[0] = AudioFileUtilities::load(gFilename, BUFFER_LEN, 0);
	gSampleBuf[1] = gSampleBuf[0]; // initialise the inactive buffer with the same channels and frames as the first one
#endif // INPUT_PLAYBACK
	for(unsigned int c = 0; c < context->audioInChannels; ++c)
	{
		pds.push_back({50_Hz, 600_Hz, uint32_t(context->audioSampleRate), -40_dB});
		oscs.push_back({context->audioSampleRate, Oscillator::square});
		envs.push_back({2_ms, 100_ms, uint32_t(context->audioSampleRate)});
#ifdef OUTPUT_WRITE
#ifndef INPUT_PLAYBACK
		unsigned int gNumFramesInFile = context->audioSampleRate * 20; // 20 seconds
#endif // INPUT_PLAYBACK
		gOutputs.emplace_back(gNumFramesInFile);
#endif // OUTPUT_WRITE
	}

	return true;
}

void render(BelaContext *context, void *userData)
{
	Bela_cpuTic(&mycpu);
	for(unsigned int n = 0; n < context->audioFrames; n++) {
#ifdef INPUT_PLAYBACK
		// Increment read pointer and reset to 0 when end of file is reached
		if(++gReadPtr >= BUFFER_LEN) {
			if(!gDoneLoadingBuffer)
				rt_printf("Couldn't load buffer in time :( -- try increasing buffer size!");
			gDoneLoadingBuffer = 0;
			gReadPtr = 0;
			gActiveBuffer = !gActiveBuffer;
			Bela_scheduleAuxiliaryTask(gFillBufferTask);

		}
#endif // INPUT_PLAYBACK

		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			// Wrap channel index in case there are more audio output channels than the file contains
#ifdef INPUT_PLAYBACK
			float in = gSampleBuf[gActiveBuffer][channel%gSampleBuf[0].size()][gReadPtr];
#else // INPUT_PLAYBACK
			float in = audioRead(context, n, channel);
#endif // INPUT_PLAYBACK
			pds[channel](in);
			float env = envs[channel](in);
			float f = pds[channel].get_frequency();
			float syn = oscs[channel].process(f) * env;
			float out = (in + syn) * 0.5f;
			audioWrite(context, n, channel, out);
#ifdef OUTPUT_WRITE
			gOutputs[channel][gWrittenFrames] = out;
#endif // OUTPUT_WRITE
		}
#ifdef OUTPUT_WRITE
		++gWrittenFrames;
		if(gWrittenFrames >= gOutputs[0].size()) {
			// if we have processed enough samples an we have filled the pre-allocated buffer,
			// stop the program
			Bela_requestStop();
			return;
		}
#endif // OUTPUT_WRITE
	}
	Bela_cpuToc(&mycpu);
	static int count = 0;
	if(count++ == 1000)
	{
		for(unsigned int channel = 0; channel < context->audioInChannels; ++channel) {
			auto f = pds[channel].get_frequency();
			rt_printf("%.2f ", f);
		}
		rt_printf(" @ %.2f%%\n", mycpu.percentage);
		count = 0;
	}
}

void cleanup(BelaContext *context, void *userData)
{
#ifdef OUTPUT_WRITE
	for(auto& o : gOutputs)
		o.resize(gWrittenFrames);
	AudioFileUtilities::write(gFilenameOutputs, gOutputs, context->audioSampleRate);
#endif // OUTPUT_WRITE
}
