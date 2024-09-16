// Reduced and modified code from Chromagram.cpp

#include "FFTCalculator.h"
#include "arcougen.h"
//==================================================================================
FFTCalculator::FFTCalculator (int frameSize, int fs) :
        bufferSize (8192)
{
    
    // set up FFT
    setupFFT();
    
    // set buffer size
    buffer = O2_MALLOCNT(bufferSize, float);

    
    // setup magnitude spectrum vector
    magnitudeSpectrum = O2_MALLOCNT((bufferSize/2)+1, float);
    
    // setup frequencies array
    FFTfrequencies = O2_MALLOCNT((bufferSize/2)+1, float);
    
    // Make Hann window function.
    makeHannWindow();
    
    // set sampling frequency
    setSamplingFrequency (fs);
    
    // set input audio frame size
    setInputAudioFrameSize (frameSize);
    
    // initialise num samples counter
    numSamplesSinceLastCalculation = 0;
    
    // set calculation interval (in samples at the input audio sampling frequency)
    calculationInterval = 1024;
    
    
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
    O2_FREE(FFTfrequencies);
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
void FFTCalculator::calculateFFTFrequencies()
{
    for (int i = 1; i < (bufferSize / 2) + 1; i++) {
        FFTfrequencies[i] = samplingFrequency * i / bufferSize;
    }
}

//==================================================================================
float* FFTCalculator::getFFTFrequencies()
{
    return FFTfrequencies;
}


//==================================================================================
void FFTCalculator::processAudioFrame (float* inputAudioFrame)
{
    // our default state is that the FFT result is not ready
    ready = false;
    
    int n = 0;
    
    // add new samples to buffer
    for (int i = bufferIndex; i < bufferIndex + inputAudioFrameSize; i++)
    {
        buffer[i] = inputAudioFrame[n];
        n++;
    }
    
    // add number of samples from calculation
    numSamplesSinceLastCalculation += inputAudioFrameSize;
    
    bufferIndex += inputAudioFrameSize;
    
    // if we have had enough samples
    if (numSamplesSinceLastCalculation >= calculationInterval)
    {
        // calculate the chromagram
        calculateFFT();
        
        // If buffer cannot take one more frame, shift it left by
        // calculationInterval and update bufferIndex
        if (bufferIndex + inputAudioFrameSize >= bufferSize) {
            
            for (int i = 0; i < bufferSize - calculationInterval; i++)
            {
                buffer[i] = buffer[i + calculationInterval];
            }
            
            bufferIndex -= calculationInterval;
        }
        
        // reset num samples counter
        numSamplesSinceLastCalculation = 0;
    }
    
}

//==================================================================================
void FFTCalculator::setInputAudioFrameSize (int frameSize)
{
    inputAudioFrameSize = frameSize;
}

//==================================================================================
void FFTCalculator::setSamplingFrequency (int fs)
{
    samplingFrequency = fs;
    calculateFFTFrequencies();
}

//==================================================================================
void FFTCalculator::setCalculationInterval (int numSamples)
{
    if (numSamples > bufferSize) {
        printf("Failed to set Calculation Interval. Cannot be greater than window size\n");
        return;
    }
    calculationInterval = numSamples;
}

//==================================================================================
float* FFTCalculator::getFFTData()
{
    return fft_data;
}

//==================================================================================
float* FFTCalculator::getMagnitudeSpectrum()
{
    calculateMagnitudeSpectrum();
    return magnitudeSpectrum;
}



//==================================================================================
bool FFTCalculator::isReady()
{
    return ready;
}

//==================================================================================
void FFTCalculator::setupFFT()
{
    // ------------------------------------------------------
#ifdef USE_FFTW
    // complex array to hold fft data:
    complexIn = (fftw_complex*) fftw_malloc (sizeof (fftw_complex) * bufferSize);
    // complex array to hold fft data:
    complexOut = (fftw_complex*) fftw_malloc (sizeof (fftw_complex) * bufferSize);
    // FFT plan initialisation:
    p = fftw_plan_dft_1d (bufferSize, complexIn, complexOut,
                          FFTW_FORWARD, FFTW_ESTIMATE);
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
    // We already calculated FFT array at this point.
    
#ifdef USE_FFTW
    // -----------------------------------------------
    // FFTW VERSION
    // -----------------------------------------------
    
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
    // now compute mag from index 1 to index N/2-1
    for (int i = 1; i < (bufferSize / 2); i++) {
        float re = fft_data[i * 2];
        float im = fft_data[i * 2 + 1];
        magnitudeSpectrum[i] = sqrt(re * re + im * im);
    }

    
#endif

}

void FFTCalculator::calculateFFT()
{
#ifdef USE_FFTW
    int i = 0;
    
    for (int i = 0; i < bufferSize; i++)
    {
        complexIn[i][0] = buffer[i] * window[i];
        complexIn[i][1] = 0.0;
    }
    
    // execute fft plan, i.e. compute fft of buffer
    fftw_execute (p);
#endif
    

#ifdef USE_KISS_FFT
    int i = 0;
    
    for (int i = 0;i < bufferSize; i++)
    {
        fftIn[i].r = buffer[i] * window[i];
        fftIn[i].i = 0.0;
    }
    
    // execute kiss fft
    kiss_fft (cfg, fftIn, fftOut);
#endif
    
    
#ifdef USE_PFFFT
    
    int i = 0;
    
    for (int i = 0; i < bufferSize; i++) {
        fft_data[i] = buffer[i] * window[i];
    }
    
    rffts(fft_data, log2_fft_size, 1);
#endif
    
    ready = true;
}

//==================================================================================
void FFTCalculator::makeHammingWindow()
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

void FFTCalculator::makeHannWindow()
{
    window = O2_MALLOCNT(bufferSize, float);
    
    for (int n = 0; n < bufferSize; n++)
    {
        window[n] = 0.5 * (1 - cos(2 * M_PI * n / (bufferSize - 1)));
    }
}

//==================================================================================
double FFTCalculator::round (double val)
{
    return floor (val + 0.5);
}
