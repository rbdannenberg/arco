#include "string.h"
#include "assert.h"
#include "o2.h"
#include "arcougen.h"  // needed for AR, Sample
#include "detectionfunctions.h"
#include "mq.h"

// using namespace modal;

// ----------------------------------------------------------------------------
// Windowing

void hann_window(int window_size, sample* window) {
    int i;
    for(i = 0; i < window_size; i++) {
        window[i] *= 0.5 * (1.0 - cos(2.0*M_PI*i/(window_size-1)));
    }
}

// ----------------------------------------------------------------------------
// Linear Prediction

void burg(int signal_size, sample* signal, int order,
          int num_coefs, sample* coefs) {
    // initialise f and b - the forward and backwards predictors
    // sample* f = (sample*)calloc(signal_size, sizeof(sample));
    sample* f = O2_CALLOCNT(signal_size, sample);
    // sample* b = (sample*)calloc(signal_size, sizeof(sample));
    sample* b = O2_CALLOCNT(signal_size, sample);
    // sample* temp_coefs = (sample*)calloc(num_coefs+1, sizeof(sample));
    sample* temp_coefs = O2_CALLOCNT(num_coefs + 1, sample);
    // sample* reversed_coefs = (sample*)calloc(num_coefs+1, sizeof(sample));
    sample* reversed_coefs = O2_CALLOCNT(num_coefs + 1, sample);
    
    sample temp;

    int k;
    for(k = 0; k < signal_size; k++) {
        f[k] = b[k] = signal[k];
    }

    int f_loc = 0;
    int n = 0;
    sample mu = 0.0;
    sample sum = 0.0;
    sample fb_sum = 0.0;
    temp_coefs[0] = 1.0;
    int c = 0;

    for(k = 0; k < order; k++) {
        // update f_loc, which keeps track of the first element in f
        // this takes 1 element from the start of f each time.
        f_loc += 1;

        // calculate mu
        sum = 0.0;
        fb_sum = 0.0;
        for(n = f_loc; n < num_coefs; n++) {
            sum += (f[n]*f[n]) + (b[n-f_loc]*b[n-f_loc]);
            fb_sum += (f[n]*b[n-f_loc]);
        }

        if(sum > 0) {
            // check for division by zero
            mu = -2.0 * fb_sum / sum;
        }
        else {
            mu = 0.0;
        }

        // update coefficients
        c += 1;
        for(n = 0; n <= c; n++) {
            reversed_coefs[n] = temp_coefs[c-n];
        }
        for(n = 0; n <= c; n++) {
            temp_coefs[n] += mu * reversed_coefs[n];
        }
        // update f and b
        for(n = f_loc; n < num_coefs; n++) {
            temp = f[n];
            f[n] += mu * b[n-f_loc];
            b[n-f_loc] += mu * temp;
        }
    }

    memcpy(coefs, &temp_coefs[1], sizeof(sample)*num_coefs);

    O2_FREE(f);
    O2_FREE(b);
    O2_FREE(temp_coefs);
    O2_FREE(reversed_coefs);
}

void linear_prediction(int signal_size, sample* signal,
                       int num_coefs, sample* coefs,
                       int num_predictions, sample* predictions) {
    // check that the number of coefficients does not exceed the
    // number of samples in the signal
    int i, j;

    for(i = 0; i < num_predictions; i++) {
        // Each sample in the num_coefs past samples is multiplied
        // by a coefficient the corresponding coefficient. Results are summed.
        predictions[i] = 0.0;
        for(j = 0; j < i; j++) {
            predictions[i] -= coefs[j] * predictions[i-1-j];
        }
        for(j = i; j < num_coefs; j++) {
            predictions[i] -= coefs[j] * signal[signal_size-1-j+i];
        }
    }
}

// ----------------------------------------------------------------------------
// Onset Detection Function

int OnsetDetectionFunction::get_sampling_rate() {
    return sampling_rate;
}

int OnsetDetectionFunction::get_frame_size() {
    return frame_size;
}

int OnsetDetectionFunction::get_hop_size() {
    return hop_size;
}

void OnsetDetectionFunction::set_sampling_rate(int value) {
    sampling_rate = value;
}

void OnsetDetectionFunction::set_frame_size(int value) {
    frame_size = value;
}

void OnsetDetectionFunction::set_hop_size(int value) {
    hop_size = value;
}

void OnsetDetectionFunction::process(int signal_size, sample* signal,
                                     int odf_size, sample* odf) {
    // if(odf_size < (signal_size - frame_size) / hop_size) {
    //     throw Exception(std::string("ODF size is too small: must be at ") +
    //                    std::string("least (signal_size - frame_size) ") +
    //                    std::string("/ hop_size"));
    //    return;
    // }
    assert(odf_size >= (signal_size - frame_size) / hop_size); // ODF size is
                                                               // too small.
    sample odf_max = 0.0;
    int sample_offset = 0;
    int frame = 0;

    while(sample_offset <= signal_size - frame_size) {
        odf[frame] = process_frame(frame_size, &signal[sample_offset]);

        // keep track of the maximum so we can normalise later
        if(odf[frame] > odf_max) {
            odf_max = odf[frame];
        }
        sample_offset += hop_size;
        frame++;
    }

    // normalise ODF
    if(odf_max) {
        for(int i = 0; i < odf_size; i++) {
            odf[i] /= odf_max;
        }
    }
}

#ifndef IGNORE
// ----------------------------------------------------------------------------
// Energy

sample EnergyODF::process_frame(int signal_size, sample* signal) {
    if(signal_size != frame_size) {
        printf("Warning: size of signal passed to process_frame (%d) "
               "does not match frame_size (%d), updating frame size.\n",
               signal_size, frame_size);
        set_frame_size(signal_size);
    }

    // calculate signal energy
    sample energy = 0.0;
    sample diff;
    int i;
    for(i = 0; i < frame_size; i++) {
        energy += signal[i] * signal[i];
    }
    // get the energy difference between current and previous frame
    diff = fabs(energy - prev_energy);
    prev_energy = energy;
    return diff;
}

// ----------------------------------------------------------------------------
// SpectralDifference

SpectralDifferenceODF::SpectralDifferenceODF() {
    prev_amps = NULL;
    // in = NULL;
    data = NULL;
    // out = NULL;
    fftInit(log_frame_size);
    // p = fftw_plan_dft_r2c_1d(frame_size, in, out, FFTW_ESTIMATE);
    reset();
}

SpectralDifferenceODF::~SpectralDifferenceODF() {
    // if(window) delete [] window;
    if(window) O2_FREE(window);
    // if(prev_amps) delete [] prev_amps;
    if(prev_amps) O2_FREE(prev_amps);
    // if(in) fftw_free(in);
    // if(out) fftw_free(out);
    if(data) O2_FREE(data); data = NULL;
    // fftw_destroy_plan(p);
}

void SpectralDifferenceODF::reset() {
    num_bins = (frame_size/2) + 1;

    // if(window) delete [] window;
    if(window) O2_FREE(window);
    // window = new sample[frame_size];
    window = O2_MALLOCNT(frame_size, sample);
    for(int i = 0; i < frame_size; i++) {
        window[i] = 1.0;
    }
    hann_window(frame_size, window);

    // if(prev_amps) delete [] prev_amps;
    if(prev_amps) O2_FREE(prev_amps);
    // prev_amps = new sample[num_bins];
    // for(int i = 0; i < num_bins; i++) {
    //     prev_amps[i] = 0;
    // }
    prev_amps = O2_CALLOCNT(num_bins, sample);

    // if(in) fftw_free(in);
    // in = (sample*) fftw_malloc(sizeof(sample) * frame_size);
    if(data) O2_FREE(data);
    data = O2_MALLOCNT(frame_size, sample);

    // if(out) fftw_free(out);
    // out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * num_bins);

    // fftw_destroy_plan(p);
    // p = fftw_plan_dft_r2c_1d(frame_size, in, out, FFTW_ESTIMATE);
    fftInit(log_frame_size);
}

void SpectralDifferenceODF::set_frame_size(int value) {
    frame_size = value;
    log_frame_size = ilog2(value);
    reset();
}

sample SpectralDifferenceODF::process_frame(int signal_size, sample* signal) {
    if(signal_size != frame_size) {
        printf("Warning: size of signal passed to process_frame (%d) "
               "does not match frame_size (%d), updating frame size.\n",
               signal_size, frame_size);
        set_frame_size(signal_size);
    }

    sample sum = 0.0;
    sample amp;
    int bin;

    // do a FFT of the current frame
    // memcpy(in, &signal[0], sizeof(sample)*frame_size);
    memcpy(data, &signal[0], sizeof(sample)*frame_size);
    // window_frame(in);
    window_frame(data);
    // fftw_execute(p);
    rffts(data, log_frame_size, 1);

    // calculate the amplitude differences between bins from consecutive frames
    // sum = 0.0;
    // for(bin = 0; bin < num_bins; bin++) {
    //     amp = sqrt((out[bin][0]*out[bin][0]) + (out[bin][1]*out[bin][1]));
    amp = fabs(data[0]);
    sum = fabs(amp - prev_amps[0]);
    prev_amps[0] = amp;
    amp = fabs(data[1]);
    sum += fabs(amp - prev_amps[num_bins - 1]);
    prev_amps[num_bins - 1] = amp;
    for(bin = 1; bin < num_bins - 1; bin++) {
        amp = sqrt((data[bin * 2] * data[bin * 2]) +
                   (data[bin * 2 + 1] * data[bin * 2 + 1]));
        sum += fabs(prev_amps[bin] - amp);
        prev_amps[bin] = amp;
    }

    return sum;
}

// ----------------------------------------------------------------------------
// Complex

ComplexODF::ComplexODF() {
    prev_amps = NULL;
    prev_phases = NULL;
    prev_phases2 = NULL;
    // in = NULL;
    // out = NULL;
    // p = fftw_plan_dft_r2c_1d(frame_size, in, out, FFTW_ESTIMATE);
    data = NULL;
    fftInit(log_frame_size);
    reset();
}

ComplexODF::~ComplexODF() {
    // if(window) delete [] window;
    if(window) O2_FREE(window);
    // if(prev_amps) delete [] prev_amps;
    if(prev_amps) O2_FREE(prev_amps);
    // if(prev_phases) delete [] prev_phases;
    if(prev_phases) O2_FREE(prev_phases);
    // if(prev_phases2) delete [] prev_phases2;
    if(prev_phases2) O2_FREE(prev_phases2);
    // if(in) fftw_free(in);
    if(data) O2_FREE(data);
    // if(out) fftw_free(out);
    // fftw_destroy_plan(p);
}

void ComplexODF::reset() {
    num_bins = (frame_size/2) + 1;

    // if(window) delete [] window;
    if(window) O2_FREE(window);
    // window = new sample[frame_size];
    window = O2_MALLOCNT(frame_size, sample);
    for(int i = 0; i < frame_size; i++) {
        window[i] = 1.0;
    }
    hann_window(frame_size, window);

    // if(prev_amps) delete [] prev_amps;
    if(prev_amps) O2_FREE(prev_amps);
    // prev_amps = new sample[num_bins];
    // for(int i = 0; i < num_bins; i++) {
    //     prev_amps[i] = 0;
    // }
    prev_amps = O2_CALLOCNT(num_bins, sample);

    // if(prev_phases) delete [] prev_phases;
    if(prev_phases) O2_FREE(prev_phases);
    // prev_phases = new sample[num_bins];
    // for(int i = 0; i < num_bins; i++) {
    //     prev_phases[i] = 0;
    // }
    prev_phases = O2_CALLOCNT(num_bins, sample);

    // if(prev_phases2) delete [] prev_phases2;
    if(prev_phases2) O2_FREE(prev_phases2);
    // prev_phases2 = new sample[num_bins];
    // for(int i = 0; i < num_bins; i++) {
    //     prev_phases2[i] = 0;
    // }
    prev_phases2 = O2_CALLOCNT(num_bins, sample);

    // if(in) fftw_free(in);
    // in = (sample*) fftw_malloc(sizeof(sample) * frame_size);
    if(data) O2_FREE(data);
    // allocate extra space for nyquist term:
    data = O2_MALLOCNT(frame_size + 2, sample);

    // if(out) fftw_free(out);
    // out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * num_bins);

    // fftw_destroy_plan(p);
    // p = fftw_plan_dft_r2c_1d(frame_size, in, out, FFTW_ESTIMATE);
    fftInit(log_frame_size);
}

void ComplexODF::set_frame_size(int value) {
    frame_size = value;
    log_frame_size = ilog2(value);
    reset();
}


sample ComplexODF::process_frame(int signal_size, sample* signal) {
    if(signal_size != frame_size) {
        printf("Warning: size of signal passed to process_frame (%d) "
               "does not match frame_size (%d), updating frame size.\n",
               signal_size, frame_size);
        set_frame_size(signal_size);
    }

    sample phase_prediction;
    // fftw_complex prediction;
    sample prediction[2];
    sample sum = 0.0;

    // do a FFT of the current frame
    // memcpy(in, &signal[0], sizeof(sample)*frame_size);
    memcpy(data, &signal[0], sizeof(sample)*frame_size);
    // window_frame(in);
    window_frame(data);
    // fftw_execute(p);
    rffts(data, log_frame_size, 1);

    // calculate sum of prediction errors
    // data[0] is a special case with DC and Nyquist coefs, but we'll "fix" it:
    data[frame_size - 2] = data[1];
    data[frame_size - 1] = 0;
    data[1] = 0;
    for(int bin = 0; bin < num_bins; bin++) {
        // Magnitude prediction is just the previous magnitude.
        // Phase prediction is the previous phase plus the difference between
        // the previous two frames
        phase_prediction = (2.0 * prev_phases[bin]) - prev_phases2[bin];
        // bring it into the range +- pi
        while(phase_prediction > M_PI) phase_prediction -= 2.0 * M_PI;
        while(phase_prediction < M_PI) phase_prediction += 2.0 * M_PI;
        // convert back into the complex domain to calculate stationarities
        prediction[0] = prev_amps[bin] * cos(phase_prediction);
        prediction[1] = prev_amps[bin] * sin(phase_prediction);
        // get stationarity measures in the complex domain
        // sum += sqrt(
        //     ((prediction[0] - out[bin][0]) * (prediction[0] - out[bin][0])) +
        //     ((prediction[1] - out[bin][1]) * (prediction[1] - out[bin][1]))
        // );
        float a = prediction[0] - data[bin * 2];
        float b = prediction[1] - data[bin * 2 + 1];
        sum += sqrt(a * a + b * b);
        // prev_amps[bin] = sqrt(
        //     (out[bin][0] * out[bin][0]) + (out[bin][1] * out[bin][1])
        // );
        prev_amps[bin] = sqrt(data[bin * 2] * data[bin * 2] +
                              data[bin * 2 + 1] * data[bin * 2 + 1]);
        prev_phases2[bin] = prev_phases[bin];
        // prev_phases[bin] = atan2(out[bin][1], out[bin][0]);
        prev_phases[bin] = atan2(data[bin * 2 + 1], data[bin * 2]);
    }

    return sum;
}

// ----------------------------------------------------------------------------
// LPEnergy

LPEnergyODF::LPEnergyODF() {
    init();
}

LPEnergyODF::~LPEnergyODF() {
    destroy();
}

void LPEnergyODF::init() {
    coefs = new sample[order];
    for(int i = 0; i < order; i++) {
        coefs[i] = 0;
    }

    prev_values = new sample[order];
    for(int i = 0; i < order; i++) {
        prev_values[i] = 0;
    }
}

void LPEnergyODF::destroy() {
    if(coefs) delete [] coefs;
    if(prev_values) delete [] prev_values;
}

sample LPEnergyODF::process_frame(int signal_size, sample* signal) {
    if(signal_size != frame_size) {
        printf("Warning: size of signal passed to process_frame (%d) "
               "does not match frame_size (%d), updating frame size.\n",
               signal_size, frame_size);
        set_frame_size(signal_size);
    }

    sample odf = 0.0;
    sample prediction = 0.0;

    // calculate signal energy
    sample energy = 0.0;
    for(int i = 0; i < frame_size; i++) {
        energy += signal[i] * signal[i];
    }

    // get LP coefficients
    burg(order, prev_values, order, order, coefs);
    // get the difference between current and predicted energy values
    linear_prediction(order, prev_values, order, coefs, 1, &prediction);
    odf = fabs(energy - prediction);

    // move energies 1 frame back then update last energy
    for(int i = 0; i < order-1; i++) {
        prev_values[i] = prev_values[i+1];
    }
    prev_values[order-1] = energy;
    return odf;
}
#endif

// ----------------------------------------------------------------------------
// LPSpectralDifference

LPSpectralDifferenceODF::LPSpectralDifferenceODF(int frame_size) {
    prev_amps = NULL;
    // in = NULL;
    // out = NULL;
    // p = fftw_plan_dft_r2c_1d(frame_size, in, out, FFTW_ESTIMATE);
    data = NULL;
    set_frame_size(frame_size);
    init();
}

LPSpectralDifferenceODF::~LPSpectralDifferenceODF() {
    destroy();
}

void LPSpectralDifferenceODF::init() {
    // coefs = new sample[order];
    coefs = O2_MALLOCNT(order, sample);
    num_bins = (frame_size/2) + 1;

    // if(window) delete [] window;
    if(window) O2_FREE(window);
    // window = new sample[frame_size];
    window = O2_MALLOCNT(frame_size, sample);
    for(int i = 0; i < frame_size; i++) {
        window[i] = 1.0;
    }
    hann_window(frame_size, window);

    // prev_amps = new sample*[num_bins];
    prev_amps = O2_CALLOCNT(num_bins * order, sample);
    // for(int i = 0; i < num_bins; i++) {
    //     prev_amps[i] = new sample[order];
    //     for(int j = 0; j < order; j++) {
    //         prev_amps[i][j] = 0.0;
    //     }
    // }

    // in = (sample*) fftw_malloc(sizeof(sample) * frame_size);
    // out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * num_bins);
    // p = fftw_plan_dft_r2c_1d(frame_size, in, out, FFTW_ESTIMATE);
    fftInit(log_frame_size);
    // allocate extra 2 samples for the nyquist rate:
    data = O2_MALLOCNT(frame_size + 2, sample);
}

void LPSpectralDifferenceODF::destroy() {
    // if(window) delete [] window;
    // if(coefs) delete [] coefs;
    // if(prev_amps) {
    //     for(int i = 0; i < num_bins; i++) {
    //         if(prev_amps[i]) delete [] prev_amps[i];
    //     }
    //     delete [] prev_amps;
    // }
    // if(in) fftw_free(in);
    // if(out) fftw_free(out);
    // fftw_destroy_plan(p);
    if(window) O2_FREE(window);
    if(coefs) O2_FREE(coefs);
    if(prev_amps) O2_FREE(prev_amps);
    if(data) O2_FREE(data);

    window = NULL;
    coefs = NULL;
    prev_amps = NULL;
    // in = NULL;
    // out = NULL;
    data = NULL;
}

void LPSpectralDifferenceODF::set_frame_size(int value) {
    destroy();
    frame_size = value;
    log_frame_size = ilog2(value);
    init();
}

sample LPSpectralDifferenceODF::process_frame(int signal_size,
                                              sample* signal) {
    if(signal_size != frame_size) {
        printf("Warning: size of signal passed to process_frame (%d) "
               "does not match frame_size (%d), updating frame size.\n",
               signal_size, frame_size);
        set_frame_size(signal_size);
    }

    sample sum = 0.0;
    sample amp = 0.0;
    sample prediction = 0.0;

    // do a FFT of the current frame
    // memcpy(in, &signal[0], sizeof(sample)*frame_size);
    memcpy(data, &signal[0], sizeof(sample)*frame_size);
    window_frame(data);
    // fftw_execute(p);
    rffts(data, log_frame_size, 1);

    // calculate the amplitude differences between bins from consecutive frames
    // for(int bin = 0; bin < num_bins; bin++) {
    // data[0] is a special case with DC and Nyquist coefs, but we'll "fix" it:
    data[frame_size - 2] = data[1];
    data[frame_size - 1] = 0;
    data[1] = 0;
    for(int bin = 0; bin < num_bins; bin++) {
        // amp = sqrt((out[bin][0]*out[bin][0]) + (out[bin][1]*out[bin][1]));
        amp = sqrt((data[bin * 2] * data[bin * 2]) +
                   (data[bin * 2 + 1] * data[bin * 2 + 1]));
        // get LP coefficients
        burg(order, &prev_amps[bin * order], order, order, coefs);
        // get the difference between current and predicted values
        linear_prediction(order, &prev_amps[bin * order], order, coefs, 1, &prediction);
        sum += fabs(amp - prediction);
        // move frames back by 1
        for(int j = 0; j < order-1; j++) {
            prev_amps[bin * order + j] = prev_amps[bin * order + j + 1];
        }
        prev_amps[bin * order + order - 1] = amp;
    }

    return sum;
}

#ifndef IGNORE
// ----------------------------------------------------------------------------
// LPComplex

LPComplexODF::LPComplexODF() {
    prev_frame = NULL;
    distances = NULL;
    // in = NULL;
    // out = NULL;
    // p = fftw_plan_dft_r2c_1d(frame_size, in, out, FFTW_ESTIMATE);
    data = NULL;
    init();
}

LPComplexODF::~LPComplexODF() {
    destroy();
}

void LPComplexODF::init() {
    // coefs = new sample[order];
    coefs = O2_MALLOCNT(order, sample);
    num_bins = (frame_size / 2) + 1;

    if(window) delete [] window;
    // window = new sample[frame_size];
    window = O2_MALLOCNT(frame_size, sample);
    for(int i = 0; i < frame_size; i++) {
        window[i] = 1.0;
    }
    hann_window(frame_size, window);

    // distances = new sample*[num_bins];
    // for(int i = 0; i < num_bins; i++) {
    //     distances[i] = new sample[order];
    //     for(int j = 0; j < order; j++) {
    //         distances[i][j] = 0.0;
    //     }
    // }
    distances = O2_MALLOCNT(num_bins * order, sample);

    // prev_frame = new fftw_complex[num_bins];
    prev_frame = O2_CALLOCNT(num_bins * 2, sample);
    // for(int i = 0; i < num_bins; i++) {
    //     prev_frame[i][0] = 0.0;
    //     prev_frame[i][1] = 0.0;
    // }

    // in = (sample*) fftw_malloc(sizeof(sample) * frame_size);
    // out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * num_bins);
    // p = fftw_plan_dft_r2c_1d(frame_size, in, out, FFTW_ESTIMATE);
    // allocate extra space for nyquist frequency
    data = O2_MALLOCNT(frame_size + 2, sample);
}

void LPComplexODF::destroy() {
    // if(window) delete [] window;
    if(window) O2_FREE(window);
    // if(coefs) delete [] coefs;
    if(coefs) O2_FREE(coefs);
    if(distances) {
        // for(int i = 0; i < num_bins; i++) {
        //     if(distances[i]) delete [] distances[i];
        // }
        // delete [] distances;
        O2_FREE(distances);
    }
    // if(prev_frame) delete [] prev_frame;
    if(prev_frame) O2_FREE(prev_frame);
    // if(in) fftw_free(in);
    // if(out) fftw_free(out);
    // fftw_destroy_plan(p);
    if(data) O2_FREE(data);

    window = NULL;
    coefs = NULL;
    distances = NULL;
    prev_frame = NULL;
    // in = NULL;
    // out = NULL;
    data = NULL;
}

void LPComplexODF::set_frame_size(int value) {
    destroy();
    frame_size = value;
    log_frame_size = ilog2(value);
    init();
}

sample LPComplexODF::process_frame(int signal_size, sample* signal) {
    if(signal_size != frame_size) {
        printf("Warning: size of signal passed to process_frame (%d) "
               "does not match frame_size (%d), updating frame size.\n",
               signal_size, frame_size);
        set_frame_size(signal_size);
    }

    sample sum = 0.0;
    sample amp = 0.0;
    sample prediction = 0.0;
    sample distance = 0.0;

    // do a FFT of the current frame
    // memcpy(in, &signal[0], sizeof(sample)*frame_size);
    memcpy(data, &signal[0], sizeof(sample)*frame_size);
    // window_frame(in);
    window_frame(data);
    // fftw_execute(p);
    rffts(data, log_frame_size, 1);

    // data[0] is a special case with DC and Nyquist coefs, but we'll "fix" it:
    data[frame_size - 2] = data[1];
    data[frame_size - 1] = 0;
    data[1] = 0;
    for(int bin = 0; bin < num_bins; bin++) {
        // distance = sqrt(
        //     (out[bin][0]-prev_frame[bin][0])*(out[bin][0]-prev_frame[bin][0]) +
        //     (out[bin][1]-prev_frame[bin][1])*(out[bin][1]-prev_frame[bin][1])
        // );
        float a = data[bin * 2] - prev_frame[bin * 2];
        float b = data[bin * 2 + 1] - prev_frame[bin * 2 + 1];
        distance = sqrt(a * a + b * b);

        // get LP coefficients
        burg(order, distances + bin * 2, order, order, coefs);
        // get the difference between current and predicted values
        // linear_prediction(order, distances[bin], order, coefs, 1, &prediction);
        linear_prediction(order, distances + bin * 2, order, coefs, 1,
                          &prediction);
        sum += fabs(distance - prediction);

        // update distances
        for(int j = 0; j < order-1; j++) {
            distances[bin * order + j] = distances[bin * order + j + 1];
        }
        distances[bin * order + order - 1] = distance;

        // update previous frame
        // prev_frame[bin * 2] = out[bin * 2];
        // prev_frame[bin * 2 + 1] = out[bin * 2 + 1];
        prev_frame[bin * 2] = data[bin * 2];
        prev_frame[bin * 2 + 1] = data[bin * 2 + 1];
    }

    return sum;
}

// ----------------------------------------------------------------------------
// PeaksODF

PeakODF::PeakODF() {
    // mq_params = (MQParameters*)malloc(sizeof(MQParameters));
    mq_params = O2_MALLOCT(MQParameters);
    mq_params->max_peaks = 20;
    mq_params->frame_size = frame_size;
    mq_params->num_bins = (frame_size/2) + 1;
    mq_params->peak_threshold = 0.1;
    mq_params->matching_interval = 200.0;
    mq_params->fundamental = 44100.0 / frame_size;
    init_mq(mq_params);
}

PeakODF::~PeakODF() {
    destroy_mq(mq_params);
    O2_FREE(mq_params);
}

void PeakODF::reinit() {
    reset_mq(mq_params);
    destroy_mq(mq_params);
    mq_params->frame_size = frame_size;
    mq_params->num_bins = (frame_size/2) + 1;
    mq_params->fundamental = 44100.0 / frame_size;
    init_mq(mq_params);
}

void PeakODF::reset() {
    reset_mq(mq_params);
}

void PeakODF::set_frame_size(int value) {
    frame_size = value;
    log_frame_size = ilog2(value);
    reinit();
}

int PeakODF::get_max_peaks() {
    return mq_params->max_peaks;
}

void PeakODF::set_max_peaks(int value) {
    mq_params->max_peaks = value;
}

sample PeakODF::get_distance(Peak* peak1, Peak* peak2) {
    return 0.0;
}

sample PeakODF::process_frame(int signal_size, sample* signal) {
    if(signal_size != frame_size) {
        printf("Warning: size of signal passed to process_frame (%d) "
               "does not match frame_size (%d), updating frame size.\n",
               signal_size, frame_size);
        set_frame_size(signal_size);
    }

    PeakList* peaks = track_peaks(
        find_peaks(frame_size, &signal[0], mq_params), mq_params
    );

    // calculate the amplitude differences between bins from consecutive frames
    sample sum = 0.0;
    while(peaks && peaks->peak) {
        sum += get_distance(peaks->peak, peaks->peak->prev);
        peaks = peaks->next;
    }
    return sum;
}

sample UnmatchedPeaksODF::get_distance(Peak* peak1, Peak* peak2) {
    if(peak1 && !peak2) {
        return peak1->amplitude;
    }
    return 0.0;
}

sample PeakAmpDifferenceODF::get_distance(Peak* peak1, Peak* peak2) {
    if(peak1 && !peak2) {
        return peak1->amplitude;
    }
    return fabs(peak1->amplitude - peak2->amplitude);
}

sample PeakAmpDifferenceODF::max_odf_value() {
    return get_max_peaks();
}
#endif
