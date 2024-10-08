//=======================================================================
/** @file Chromagram.cpp
 *  @brief Chromagram - a class for calculating the chromagram in real-time
 *  @author Adam Stark
 *  @copyright Copyright (C) 2008-2014  Queen Mary University of London
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
//=======================================================================

#include "Chromagram.h"
#include "arcougen.h"
//==================================================================================
Chromagram::Chromagram (int frameSize, int fs) :
        referenceFrequency (130.81278265),
        bufferSize (8192),
        numHarmonics (2),
        numOctaves (2),
        numBinsToSearch (2) {
    // calculate note frequencies
    for (int i = 0; i < 12; i++)
    {
        noteFrequencies[i] = referenceFrequency * pow (2,(((float) i) / 12));
    }
    
    // set up FFT
    setupFFT();
    
    // set buffer size
    buffer = O2_MALLOCNT(bufferSize, float);
    
    // setup chromagram vector
    chromagram = O2_MALLOCNT(12, float);
    
    // initialise chromagram
    for (int i = 0; i < 12; i++)
    {
        chromagram[i] = 0.0;
    }
    
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
    
    // set chroma calculation interval (in samples at the input audio sampling frequency)
    chromaCalculationInterval = 4096;
    
    // initialise chroma ready variable
    chromaReady = false;
    
    // initialize buffer index
    bufferIndex = 0;
    
    // initialize downsampling low pass filter states
    x_1 = 0;
    x_2 = 0;
    y_1 = 0;
    y_2 = 0;
}

//==================================================================================
Chromagram::~Chromagram()
{
    O2_FREE(window);
    O2_FREE(buffer);
    O2_FREE(magnitudeSpectrum);
    O2_FREE(downsampledInputAudioFrame);
    O2_FREE(chromagram);
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


//==============================================================================
void Chromagram::processAudioFrame (float* inputAudioFrame)
{
    // our default state is that the chroma is not ready
    chromaReady = false;
    
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
        
        // If buffer cannot take one more frame, shift it left by
        // downSampledCalcInterval and update bufferIndex
        int downSampledCalcInterval = chromaCalculationInterval / 4;
        if (bufferIndex + downSampledAudioFrameSize >= bufferSize) {
            
            for (int i = 0; i < bufferSize - downSampledCalcInterval; i++)
            {
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
    // True number of samples buffer can represent is bufferSize * 4 since we downsample
    if (numSamples > bufferSize * 4) {
        printf("Failed to set Chroma Calculation Interval. Cannot be greater than window size\n");
        return;
    }
    chromaCalculationInterval = numSamples;
}

//==================================================================================
float* Chromagram::getChromagram()
{
    return chromagram;
}

//==================================================================================
bool Chromagram::isReady()
{
    return chromaReady;
}

//==================================================================================
void Chromagram::setupFFT()
{
    // ------------------------------------------------------
#ifdef USE_FFTW
    // complex array to hold fft data:
    complexIn = (fftw_complex*) fftw_malloc (sizeof (fftw_complex) * bufferSize);
    // complex array to hold fft data
    complexOut = (fftw_complex*) fftw_malloc (sizeof (fftw_complex) * bufferSize);
    // FFT plan initialisation
    p = fftw_plan_dft_1d (bufferSize, complexIn, complexOut, FFTW_FORWARD, FFTW_ESTIMATE);
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
void Chromagram::calculateChromagram()
{
    calculateMagnitudeSpectrum();
    
    double divisorRatio = (((double) samplingFrequency) / 4.0) / ((double)bufferSize);
    
    for (int n = 0; n < 12; n++)
    {
        double chromaSum = 0.0;
        
        for (int octave = 1; octave <= numOctaves; octave++)
        {
            double noteSum = 0.0;
            
            for (int harmonic = 1; harmonic <= numHarmonics; harmonic++)
            {
                int centerBin = round((noteFrequencies[n] * octave * harmonic) /
                                      divisorRatio);
                int minBin = centerBin - (numBinsToSearch * harmonic);
                int maxBin = centerBin + (numBinsToSearch * harmonic);
                
                double maxVal = 0.0;
                
                for (int k = minBin; k < maxBin; k++)
                {
                    if (magnitudeSpectrum[k] > maxVal)
                    {
                        maxVal = magnitudeSpectrum[k];
                    }
                }
            
                noteSum += (maxVal / (double) harmonic);
            }
            
            chromaSum += noteSum;
        }
        
        chromagram[n] = chromaSum;
    }
    
    chromaReady = true;
}

//==================================================================================
void Chromagram::calculateMagnitudeSpectrum()
{
    
    // Original code by Adam Stark takes the square root of each magnitude. We have removed it.
    
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
    }
#endif


#ifdef USE_PFFFT
    // -----------------------------------------------
    // PFFFT VERSION
    // -----------------------------------------------
    int i = 0;
    
    for (int i = 0; i < bufferSize; i++) {
        fft_data[i] = buffer[i] * window[i];
    }
    
    rffts(fft_data, log2_fft_size, 1);
    
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

//==================================================================================
void Chromagram::downSampleFrame (float* inputAudioFrame)
{
    float* filteredFrame = O2_MALLOCNT(inputAudioFrameSize, float);
    
    float b0,b1,b2,a1,a2;
    
    b0 = 0.2929;
    b1 = 0.5858;
    b2 = 0.2929;
    a1 = -0.0000;
    a2 = 0.1716;
    
    for (int i = 0; i < inputAudioFrameSize; i++)
    {
        filteredFrame[i] = inputAudioFrame[i] * b0 + x_1 * b1 + x_2 * b2 - y_1 * a1 - y_2 * a2;
        
        x_2 = x_1;
        x_1 = inputAudioFrame[i];
        y_2 = y_1;
        y_1 = filteredFrame[i];
    }
    
    for (int i = 0; i < inputAudioFrameSize / 4; i++)
    {
        downsampledInputAudioFrame[i] = filteredFrame[i * 4];
    }
    
    O2_FREE(filteredFrame);
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
