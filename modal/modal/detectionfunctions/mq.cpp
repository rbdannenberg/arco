#include <string.h>
#include "o2.h"
#include "arcotypes.h"
#include "mq.h"

// ----------------------------------------------------------------------------
// Initialisation and destruction

int init_mq(MQParameters* params) {
    // allocate memory for window
    params->window = (sample*) malloc(sizeof(sample) * params->frame_size);
    int i;
    for(i = 0; i < params->frame_size; i++) {
        params->window[i] = 1.0;
    }
    hann_window(params->frame_size, params->window);

    // allocate memory for FFT
    // params->fft_in = (sample*) fftw_malloc(sizeof(sample) *
    //                                   params->frame_size);
    // params->fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) *
    //                                   params->num_bins);
    // params->fft_plan = fftw_plan_dft_r2c_1d(params->frame_size, params->fft_in,
    //                                   params->fft_out, FFTW_ESTIMATE);
    params->fft_data = O2_MALLOCNT(params->frame_size, sample);
    int m = ilog2(params->frame_size);
    fftInit(m);
    params->fft_plan = m;

    // set other variables to defaults
    params->prev_peaks = NULL;
    params->prev_peaks2 = NULL;
    reset_mq(params);
    return 0;
}

void reset_mq(MQParameters* params) {
    delete_peak_list(params->prev_peaks2);
    params->prev_peaks2 = NULL;
    delete_peak_list(params->prev_peaks);
    params->prev_peaks = NULL;
}

int destroy_mq(MQParameters* params) {
    if(params) {
        if(params->window) {
            free(params->window);
            params->window = NULL;
        }
        if(params->fft_data) {
            // fftw_free(params->fft_in);
            O2_FREE(params->fft_data);
            // params->fft_in = NULL;
            params->fft_data = NULL;
        }
        // if(params->fft_out) {
        //     fftw_free(params->fft_out);
        //     O2_FREE(params->fft_out);
        //     params->fft_out = NULL;
        // }
        // fftw_destroy_plan(params->fft_plan);

        delete_peak_list(params->prev_peaks2);
        params->prev_peaks2 = NULL;
        delete_peak_list(params->prev_peaks);
        params->prev_peaks = NULL;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Peak Detection

// Add new_peak to the doubly linked list of peaks, keeping peaks sorted
// with the largest amplitude peaks at the start of the list
void add_peak(Peak* new_peak, PeakList* peak_list) {
    do {
        if(peak_list->peak) {
            if(peak_list->peak->amplitude > new_peak->amplitude) {
                if(peak_list->next) {
                    peak_list = peak_list->next;
                }
                else {
                    PeakList* new_node = (PeakList*)malloc(sizeof(PeakList));
                    new_node->peak = new_peak;
                    new_node->prev = peak_list;
                    new_node->next = NULL;
                    peak_list->next = new_node;
                    return;
                }
            }
            else {
                PeakList* new_node = (PeakList*)malloc(sizeof(PeakList));
                new_node->peak = peak_list->peak;
                new_node->prev = peak_list;
                new_node->next = peak_list->next;
                peak_list->next = new_node;
                peak_list->peak = new_peak;
                return;
            }
        }
        else {
            // should only happen for the first peak
            peak_list->peak = new_peak;
            return;
        }
    }
    while(1);
}

// delete the given PeakList
void delete_peak_list(PeakList* peak_list) {
    while(peak_list && peak_list->next) {
        if(peak_list->peak) {
            free(peak_list->peak);
            peak_list->peak = NULL;
        }
        PeakList* temp = peak_list->next;
        free(peak_list);
        peak_list = temp;
    }

    if(peak_list) {
        if(peak_list->peak) {
            free(peak_list->peak);
            peak_list->peak = NULL;
        }
        peak_list->next = NULL;
        peak_list->prev = NULL;
        free(peak_list);
        peak_list = NULL;
    }
}

sample get_magnitude(sample x, sample y) {
    return sqrt((x*x) + (y*y));
}

sample get_phase(sample x, sample y) {
    return atan2(y, x);
}

PeakList* find_peaks(int signal_size, sample* signal, MQParameters* params) {
    int i;
    int num_peaks = 0;
    sample prev_amp, current_amp, next_amp;
    PeakList* peak_list = (PeakList*)malloc(sizeof(PeakList));
    peak_list->next = NULL;
    peak_list->prev = NULL;
    peak_list->peak = NULL;

    // take fft of the signal
    // memcpy(params->fft_in, signal, sizeof(sample)*params->frame_size);
    memcpy(params->fft_data, signal, sizeof(sample)*params->frame_size);
    for(i = 0; i < params->frame_size; i++) {
        // params->fft_in[i] *= params->window[i];
        params->fft_data[i] *= params->window[i];
    }
    // fftw_execute(params->fft_plan);
    rffts(params->fft_data, params->fft_plan, 1);

    // get initial magnitudes
    // prev_amp = get_magnitude(params->fft_out[0][0], params->fft_out[0][1]);
    prev_amp = params->fft_data[0];
    // current_amp = get_magnitude(params->fft_out[1][0], params->fft_out[1][1]);
    current_amp = get_magnitude(params->fft_data[2], params->fft_data[3]);

    // find all peaks in the amplitude spectrum
    // for(i = 1; i < params->num_bins-1; i++) {
    // for pffft, we ignore the nyquist bin which is at fft_data[1]
    for(i = 1; i < params->num_bins-2; i++) {        
        // next_amp = get_magnitude(params->fft_out[i+1][0],
        //                          params->fft_out[i+1][1]);
        next_amp = get_magnitude(params->fft_data[(i * 2) + 2],
                                 params->fft_data[(i * 2) + 3]);

        if((current_amp > prev_amp) &&
           (current_amp > next_amp) &&
           (current_amp > params->peak_threshold)) {
            Peak* p = (Peak*)malloc(sizeof(Peak));
            p->amplitude = current_amp;
            p->frequency = i * params->fundamental;
            // p->phase = get_phase(params->fft_out[i][0], params->fft_out[i][1]);
            p->phase = get_phase(params->fft_data[i * 2],
                                 params->fft_data[i * 2 + 1]);
            p->bin = i;
            p->next = NULL;
            p->prev = NULL;

            add_peak(p, peak_list);
            num_peaks++;
        }
        prev_amp = current_amp;
        current_amp = next_amp;
    }

    // limit peaks to a maximum of max_peaks
    if(num_peaks > params->max_peaks) {
        PeakList* current = peak_list;
        for(i = 0; i < params->max_peaks-1; i++) {
            current = current->next;
        }

        delete_peak_list(current->next);
        current->next = NULL;
        num_peaks = params->max_peaks;
    }

    return sort_peaks_by_frequency(peak_list, num_peaks);
}

// ----------------------------------------------------------------------------
// Sorting

PeakList* merge(PeakList* list1, PeakList* list2) {
    PeakList* merged_head = NULL;
    PeakList* merged_tail;

    while(list1 || list2) {
        if(list1 && list2) {
            if(list1->peak->frequency <= list2->peak->frequency) {
                if(!merged_head) {
                    merged_head = list1;
                    merged_tail = merged_head;
                }
                else {
                    merged_tail->next = list1;
                    merged_tail = merged_tail->next;
                }
                list1 = list1->next;
                merged_tail->next = NULL;
            }
            else {
                if(!merged_head) {
                    merged_head = list2;
                    merged_tail = merged_head;
                }
                else {
                    merged_tail->next = list2;
                    merged_tail = merged_tail->next;
                }
                list2 = list2->next;
                merged_tail->next = NULL;
            }
        }
        else if(list1) {
            if(!merged_head) {
                merged_head = list1;
                merged_tail = merged_head;
            }
            else {
                merged_tail->next = list1;
                merged_tail = merged_tail->next;
            }
            list1 = list1->next;
            merged_tail->next = NULL;
        }
        else if(list2) {
            if(!merged_head) {
                merged_head = list2;
                merged_tail = merged_head;
            }
            else {
                merged_tail->next = list2;
                merged_tail = merged_tail->next;
            }
            list2 = list2->next;
            merged_tail->next = NULL;
        }
    }

    return merged_head;
}

PeakList* merge_sort(PeakList* peak_list, int num_peaks) {
    if(num_peaks <= 1) {
        return peak_list;
    }

    PeakList* left;
    PeakList* right;
    PeakList* current = peak_list;
    int n = 0;

    // find the index of the middle peak. If we have an odd number,
    // give the extra peak to the left
    int middle;
    if(num_peaks % 2 == 0) {
        middle = num_peaks/2;
    }
    else {
        middle = (num_peaks/2) + 1;
    }

    // split the peak list into left and right at the middle value
    left = peak_list;
    while(current) {
        if(n == middle-1) {
            right = current->next;
            current->next = NULL;
            break;
        }

        n++;
        current = current->next;
    }

    // recursively sort and merge
    left = merge_sort(left, middle);
    right = merge_sort(right, num_peaks-middle);
    return merge(left, right);
}

// Sort peak_list into a list order from smaller to largest frequency.
PeakList* sort_peaks_by_frequency(PeakList* peak_list, int num_peaks) {
    if(!peak_list) {
        return NULL;
    }
    else if(num_peaks == 0) {
        return peak_list;
    }
    else {
        return merge_sort(peak_list, num_peaks);
    }

}

// ----------------------------------------------------------------------------
// Partial Tracking

// Find a candidate match for peak in frame if one exists. This is the closest
// (in frequency) match that is within the matching interval.
Peak* find_closest_match(Peak* p, PeakList* peak_list,
                         MQParameters* params, int backwards) {
    PeakList* current = peak_list;
    Peak* match = NULL;
    sample best_distance = 44100.0;
    sample distance;

    while(current && current->peak) {
        if(backwards) {
            if(current->peak->prev) {
                current = current->next;
                continue;
            }
        }
        else {
            if(current->peak->next) {
                current = current->next;
                continue;
            }
        }

        distance = fabs(current->peak->frequency - p->frequency);
        if((distance < params->matching_interval) &&
           (distance < best_distance)) {
            best_distance = distance;
            match = current->peak;
        }
        current = current->next;
    }

    return match;
}

// Returns the closest unmatched peak in frame with a frequency less
// than p.frequency.
Peak* free_peak_below(Peak* p, PeakList* peak_list) {
    PeakList* current = peak_list;
    Peak* free_peak = NULL;
    sample closest_frequency = 44100;

    while(current && current->peak) {
        if(current->peak != p) {
            // if current peak is unmatched, and it is closer to p than the
            // last unmatched peak that we saw, save it
            if(!current->peak->prev &&
               (current->peak->frequency < p->frequency) &&
               (fabs(current->peak->frequency - p->frequency)
                < closest_frequency)) {
                closest_frequency = fabs(current->peak->frequency -
                                         p->frequency);
                free_peak = current->peak;
            }
        }
        current = current->next;
    }
    return free_peak;
}


// A simplified version of MQ Partial Tracking.
PeakList* track_peaks(PeakList* peak_list, MQParameters* params) {
    PeakList* current = peak_list;

    // MQ algorithm needs 2 frames of data, so return if this is the
    // first frame
    if(params->prev_peaks) {
        // find all matches for previous peaks in the current frame
        current = params->prev_peaks;
        while(current && current->peak) {
            Peak* match = find_closest_match(
                current->peak, peak_list, params, 1
            );
            if(match) {
                Peak* closest_to_cand = find_closest_match(
                    match, params->prev_peaks, params, 0
                );
                if(closest_to_cand != current->peak) {
                    // see if the closest peak with lower frequency to the
                    // candidate is within the matching interval
                    Peak* lower = free_peak_below(match, peak_list);
                    if(lower) {
                        if(fabs(lower->frequency - current->peak->frequency)
                           < params->matching_interval) {
                            lower->prev = current->peak;
                            current->peak->next = lower;
                        }
                    }
                }
                // if closest_peak == peak, it is a definitive match
                else {
                    match->prev = current->peak;
                    current->peak->next = match;
                }
            }
            current = current->next;
        }
    }

    delete_peak_list(params->prev_peaks2);
    params->prev_peaks2 = params->prev_peaks;
    params->prev_peaks = peak_list;
    return peak_list;
}
