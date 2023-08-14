/* resamp.h -- resample library
 *
 * Roger B. Dannenberg
 */

/* perform windowed sinc interpolation.
 */

typedef float rsfloat;

struct Sinc_point {
    float sinc;  // value of sinc function
    float dsinc; // slope of sinc function to next point
};


class Resamp {
  public:
    int span;  // the length of the interpolation window in sample periods
               // (must be even).
    int ov;    // how fine the span is oversampled to (span * ov) + 1 points
               // only (span / 2 * ov) are stored making use of symmetry.
    Vec<Sinc_point> sinc; // points for the windowed sinc function.
    
    Resamp() {  // you must initialize a Resamp with span and ov(ersampling)
    }

    void init(int span_, int ov_) {
        span = span_;
        ov = ov_;
        int m = (span / 2) * ov + 1;
        sinc.init(m);
        for (int i = 0; i <= m; i++) {
            double w = 0.42 - 0.5 * cos(2 * M_PI * (i + m) / (2 * m)) +
                              0.08 * cos(4 * M_PI * (i + m) / (2 * m));

            double phase = i * M_PI / ov;
            double sincval = (i == 0) ? 1.0 : sin(phase) / phase;
            sinc[i].sinc = w * sincval;
            if (i > 0) {
                sinc[i - 1].dsinc = sinc[i].sinc - sinc[i - 1].sinc;
            }
        }
        // sinc at m would be zero, so dsync at m-1 is sync at m-1:
        sinc[m - 1].dsinc = sinc[m - 1].sinc;
    }

    
    ~Resamp() { finish(); }

    
    void finish() {
        sinc.finish();
    }

    
    // Interpolate the value of the signal at input + offset by
    // forming a weighted sum of points with a range of span * scale
    // (half to the left and half to the right of input + offset).
    // The weight is obtained from sinc. If the distance from offset
    // is x, then the weight is sinc(x * ov / scale), obtained by
    // interpolating between stored points of sinc[].
    float interp(Sample_ptr input, rsfloat offset, rsfloat scale) {
        rsfloat start = offset - scale * (span / 2);
        int istart = std::ceil(start);
        int iend = std::floor(offset + scale * (span / 2));
        rsfloat sum = 0;
        input += istart;
        // First, do left side: all points from istart below offset.
        // sinc(d) will be the weight for input[i]
        rsfloat d_inc = ov / scale;
        rsfloat d = (offset - istart) * d_inc;
        int i;
        for (i = istart; i < offset; i++) {
            int j = std::floor(d);
            rsfloat j_frac = d - j;
            Sinc_point &sp = sinc[j];
            sum += (sp.sinc + j_frac * sp.dsinc) * *input++;
            assert(!isnan(sum));
            d -= d_inc;
        }
        // what is d now? Loop iterates ceil(offset - istart) times:
        // d = (offset - istart) * (ov / scale) -
        //     (ceil(offset - istart) * (ov / scale)
        // d = ((offset - istart) - ceil(offset - istart)) * (ov / scale)
        //   = (offset - ceil(offset)) * (ov /scale)
        // so d has crossed the center point and to do the right wing
        // we can set d = -d:
        d = -d;
        // do the right side: all points from i below iend:
        for ( ; i <= iend; i++) {
            int j = std::floor(d);
            rsfloat j_frac = d - j;
            Sinc_point &sp = sinc[j];
            sum += (sp.sinc + j_frac * sp.dsinc) * *input++;
            d += d_inc;
        }
        return sum;
    }


    // resample input with offset to len samples of output,
    // scaling windowed sinc function by scale.  The output sample rate
    // corresponds to a step size of period in the input.
    // Note: because of repeated addition of period to obtain offset, doubles
    // are used to get accurate phase. This gives a measurable improvement,
    // although it may just be more accurate effective sample rate rather
    // than a perceptual difference.
    void resamp(Sample_ptr output, int len, Sample_ptr input, double offset,
                rsfloat scale, double period) {
        rsfloat scale_inverse = 1.0 / scale;
        for (int i = 0; i < len; i++) {
            *output++ = interp(input, offset, scale) * scale_inverse;
            offset += period;
        }
    }
};

