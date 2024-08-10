// Reduced and modified code from Chromagram.h

#ifndef __FFTCALCULATOR_H
#define __FFTCALCULATOR_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <vector>

#ifdef USE_FFTW
#include "fftw3.h"
#endif

#ifdef USE_KISS_FFT
#include "kiss_fft.h"
#endif

#if (!defined(USE_FFTW) && !defined(USE_KISS_FFT))
#define USE_PFFFT
#include "pffft.h"
#include "ffts_compat.h"
#endif

//=======================================================================
/** A class for calculating a Chromagram from input audio
 * in a real-time context */
class FFTCalculator
{
    
public:
    /** Constructor
     * @param frameSize the input audio frame size
     * @param fs the sampling frequency
     */
    FFTCalculator (int frameSize, int fs);

    /** Destructor */
    ~FFTCalculator();
    
    /** Process a single audio frame. This will determine whether enough samples
     * have been accumulated and if so, will calculate the FFT data array.
     * @param inputAudioFrame an array containing the input audio frame. This should be
     * the length indicated by the input audio frame size passed to the constructor
     * @see setInputAudioFrameSize
     */
    void processAudioFrame (float* inputAudioFrame);
    
    /** Sets the input audio frame size
     * @param frameSize the input audio frame size
     */
    void setInputAudioFrameSize (int frameSize);
    
    /** Set the sampling frequency of the input audio, updates FFT frequencies returned by getFFTFrequencies().
     * @param fs the sampling frequency in Hz
     */
    void setSamplingFrequency (int fs);
    
    /** Set the number of samples (hop size) for each FFT calculation
     * @param numSamples FFT hop size
     */
    void setCalculationInterval (int numSamples);
    
    /** @returns the FFT data array. This should be called after processAudioFrame */
    float* getFFTData();
    
    /** @returns the MagnitudeSpectrum array, calculated using the FFT data array. This should
     * be called after processAudioFrame
     */
    float* getMagnitudeSpectrum();
    
    /** @returns The frequencies array, such that frequencies[i] stores the frequency corresponding to FFT bin i.*/
    float* getFFTFrequencies();
    
    /** @returns true if a new FFT vector has been calculated at the current iteration. This should
     * be called after processAudioFrame
     */
    bool isReady();
    
    int bufferSize;
    
private:
    
    void setupFFT();
    void calculateFFT();
    void calculateMagnitudeSpectrum();
    void makeHammingWindow();
    void makeHannWindow();
    void calculateFFTFrequencies();
    double round (double val);
    
    float* window;
    float* buffer;
    float* magnitudeSpectrum;
    float* FFTfrequencies;
    
    
    
    int bufferIndex; // Where to add new samples in buffer
    int samplingFrequency;
    int inputAudioFrameSize;
    int numSamplesSinceLastCalculation;
    int calculationInterval;
    bool ready;

#ifdef USE_FFTW
    fftw_plan p;
    fftw_complex* complexOut;
    fftw_complex* complexIn;
#endif
    
#ifdef USE_KISS_FFT
    kiss_fft_cfg cfg;
    kiss_fft_cpx* fftIn;
    kiss_fft_cpx* fftOut;
#endif
    
#ifdef USE_PFFFT
    int log2_fft_size;
    float *fft_data;
#endif
    
};

#endif
