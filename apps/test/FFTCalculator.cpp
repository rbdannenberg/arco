// Reduced and modified code from Chromagram.cpp

#include "FFTCalculator.h"
#include "arcougen.h"
//==================================================================================
FFTCalculator::FFTCalculator (int frameSize, int fs)
 :
    bufferSize (8192),
    numHarmonics (2),
    numOctaves (2),
{
    // calculate note frequencies
    for (int i = 0; i < 12; i++)
    {
        noteFrequencies[i] = referenceFrequency * pow (2,(((float) i) / 12));
    }
    
    // set up FFT
    setupFFT();
    
    // set buffer size
    buffer = O2_MALLOCNT(bufferSize, float);

    
    // setup magnitude spectrum vector
    magnitudeSpectrum = O2_MALLOCNT((bufferSize/2)+1, float);
    
    // make window function
    makeHammingWindow();
    
    // set sampling frequency
    setSamplingFrequency (fs);
    
    // set input audio frame size
    setInputAudioFrameSize (frameSize);
    
    // initialise num samples counter
    numSamplesSinceLastCalculation = 0;
    
    // set calculation interval (in samples at the input audio sampling frequency)
    calculationInterval = 4096;
    
    // initialise ready variable
    ready = false;
    
    // initialize buffer index
    bufferIndex = 0;
    
}

//==================================================================================
FFTCalculator::~FFTCalculator()
{
    O2_FREE(window);
    O2_FREE(buffer);
    O2_FREE(magnitudeSpectrum);
    // ------------------------------------
#ifdef USE_FFTW
    // destroy fft plan
    fftw_destroy_plan (p);
    fftw_free (complexIn);
    fftw_free (complexOut);
#endif
    
    // ------------------------------------
#ifdef USE_KISS_FFT
    // free the Kiss FFT configuration
    free (cfg);
    delete [] fftIn;
    delete [] fftOut;
#endif
    
#ifdef USE_PFFFT
    O2_FREE(fft_data);
    
#endif
}


//==================================================================================
void FFTCalculator::processAudioFrame (float* inputAudioFrame)
{
    // our default state is that the magnitude spectrum is not ready
    ready = false;
    
    // downsample the input audio frame by 4
    downSampleFrame(inputAudioFrame);
    
    int n = 0;
    
    // add new samples to buffer
    for (int i = bufferIndex; i < bufferIndex + downSampledAudioFrameSize; i++)
    {
        buffer[i] = downsampledInputAudioFrame[n];
        n++;
    }
    
    // add number of samples from calculation
    numSamplesSinceLastCalculation += inputAudioFrameSize;
    
    bufferIndex += downSampledAudioFrameSize;
    
    // if we have had enough samples
    if (numSamplesSinceLastCalculation >= chromaCalculationInterval)
    {
        // calculate the chromagram
        calculateChromagram();
        
        // If buffer cannot take one more frame, shift it left by downSampledCalcInterval and update bufferIndex
        int downSampledCalcInterval = chromaCalculationInterval / 4;
        if (bufferIndex + downSampledAudioFrameSize >= bufferSize) {
            
            for (int i = 0; i < bufferSize - downSampledCalcInterval; i++)
            {
                assert(i + downSampledCalcInterval < bufferSize);
                buffer[i] = buffer[i + downSampledCalcInterval];
            }
            
            bufferIndex -= downSampledCalcInterval;
        }
        
        // reset num samples counter
        numSamplesSinceLastCalculation = 0;
    }
    
}

//==================================================================================
void Chromagram::setInputAudioFrameSize (int frameSize)
{
    inputAudioFrameSize = frameSize;
    downsampledInputAudioFrame = O2_MALLOCNT(inputAudioFrameSize / 4, float);
    downSampledAudioFrameSize = inputAudioFrameSize / 4;
}

//==================================================================================
void Chromagram::setSamplingFrequency (int fs)
{
    samplingFrequency = fs;
}

//==================================================================================
void Chromagram::setChromaCalculationInterval (int numSamples)
{
    // True number of samples buffer can store is bufferSize * 4 since we downsample
    if (numSamples > bufferSize * 4) {
        printf("Failed to set Chroma Calculation Interval. Cannot be greater than window size\n");
        return;
    }
    chromaCalculationInterval = numSamples;
}

//==================================================================================
float* Chromagram::getChromagram()
{
    return magnitudeSpectrum;
}

//==================================================================================
bool Chromagram::isReady()
{
    return ready;
}

//==================================================================================
void Chromagram::setupFFT()
{
    // ------------------------------------------------------
#ifdef USE_FFTW
    complexIn = (fftw_complex*) fftw_malloc (sizeof (fftw_complex) * bufferSize);        // complex array to hold fft data
    complexOut = (fftw_complex*) fftw_malloc (sizeof (fftw_complex) * bufferSize);    // complex array to hold fft data
    p = fftw_plan_dft_1d (bufferSize, complexIn, complexOut, FFTW_FORWARD, FFTW_ESTIMATE);    // FFT plan initialisation
#endif

    // ------------------------------------------------------
#ifdef USE_KISS_FFT
    // initialise the fft time and frequency domain audio frame arrays
    fftIn = new kiss_fft_cpx[bufferSize];
    fftOut = new kiss_fft_cpx[bufferSize];
    cfg = kiss_fft_alloc (bufferSize,0,0,0);
#endif
    
#ifdef USE_PFFFT
    fft_data = O2_MALLOCNT(bufferSize, float);
    log2_fft_size = ilog2(bufferSize);
    int rslt = fftInit(log2_fft_size);
    assert(rslt == 1);
#endif
}


//==================================================================================
void FFTCalculator::calculateMagnitudeSpectrum()
{
    
#ifdef USE_FFTW
    // -----------------------------------------------
    // FFTW VERSION
    // -----------------------------------------------
    int i = 0;
    
    for (int i = 0; i < bufferSize; i++)
    {
        complexIn[i][0] = buffer[i] * window[i];
        complexIn[i][1] = 0.0;
    }
    
    // execute fft plan, i.e. compute fft of buffer
    fftw_execute (p);
    
    // compute first (N/2)+1 mag values
    for (i = 0; i < (bufferSize / 2) + 1; i++)
    {
        magnitudeSpectrum[i] = sqrt (pow (complexOut[i][0], 2) + pow (complexOut[i][1], 2));
        magnitudeSpectrum[i] = sqrt (magnitudeSpectrum[i]);
    }
#endif
    
    
#ifdef USE_KISS_FFT
    // -----------------------------------------------
    // KISS FFT VERSION
    // -----------------------------------------------
    int i = 0;
    
    for (int i = 0;i < bufferSize; i++)
    {
        fftIn[i].r = buffer[i] * window[i];
        fftIn[i].i = 0.0;
    }
    
    // execute kiss fft
    kiss_fft (cfg, fftIn, fftOut);
    
    // compute first (N/2)+1 mag values
    for (i = 0; i < (bufferSize / 2) + 1; i++)
    {
        magnitudeSpectrum[i] = sqrt (pow (fftOut[i].r, 2) + pow (fftOut[i].i, 2));
        magnitudeSpectrum[i] = sqrt (magnitudeSpectrum[i]);
    }
#endif


#ifdef USE_PFFFT
    // -----------------------------------------------
    // PFFFT VERSION
    // -----------------------------------------------
    
    
    // compute first (N/2)+1 mag values
    magnitudeSpectrum[0] = fft_data[0];
    magnitudeSpectrum[bufferSize / 2] = fft_data[1];  // nyquist term
    for (i = 1; i < (bufferSize / 2) + 1; i++) {
        magnitudeSpectrum[i] = sqrt(pow(fft_data[i * 2], 2) +
                                    pow (fft_data[i * 2 + 1], 2));
        magnitudeSpectrum[i] = sqrt(magnitudeSpectrum[i]);
    }
#endif

}

void FFTCalculator::calculateFFT()
{
    int i = 0;
    
    for (int i = 0; i < bufferSize; i++) {
        fft_data[i] = buffer[i] * window[i];
    }
    
    rffts(fft_data, log2_fft_size, 1);
    return 
}

//==================================================================================
void Chromagram::makeHammingWindow()
{
    // set the window to the correct size
    window = O2_MALLOCNT(bufferSize, float);
    
    // apply hanning window to buffer
    for (int n = 0; n < bufferSize;n++)
    {
        window[n] = 0.54 - 0.46 * cos (2 * M_PI * (((double) n) / ((double) bufferSize)));
    }
}

//==================================================================================
double Chromagram::round (double val)
{
    return floor (val + 0.5);
}
