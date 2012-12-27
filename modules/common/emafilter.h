#if !defined(EMA_FILTER_H)
#define EMA_FILTER_H

#include <cmath>

/*  Exponential Moving Average.

    Used to smooth out various metrics, like framerates.

    Reseed threshold:

    To avoid noticeable stabilization periods like after major
    mainloop() stalls, during which no useful data is provided,
    there's the reseed_threshold parameter, which triggers
    a reseed if abs((sample-value)/value) gets larger than it.

    So when mainloop() stalls for some reason, next sample, which is
    duration of the stall in ms is considerably larger than any previous
    and this triggers a reseed, while dropping the sample so that it doesn't
    skew the value.

    Same thing can be applied to graphics fps, if for some reason
    (large magnitude resize or zoom) it drops or raises abruptly.
    In this case the sample can be used as first seed value, but
    to keep the interface sane it is dropped anyway.

    Don't set it too low, or it'll keep dropping data
    and reseeding all the time.

    Default negative value disables this functionality.
*/

class ema_filter_t {
    float alpha;
    float value;
    float seedsum;
    unsigned seedceil;
    unsigned seedcount;
    float reseed_threshold;

public:
    ema_filter_t(float _alpha, float _value, unsigned _seedceil, float _reseed_threshold = -23) :
        alpha(_alpha),
        value(_value),
        seedsum(0),
        seedceil(_seedceil),
        seedcount(0),
        reseed_threshold(_reseed_threshold) { }

    float update(float sample) {
        if (fabsf((sample-value)/value) > reseed_threshold) {
            reset(value);
            return value;
        }

        if (seedcount < seedceil) {
            seedsum += sample;
            if (seedcount++ == seedceil)
                value = seedsum / seedcount;

        } else {
            value = alpha * sample + (1.0 - alpha) * value;
        }
        return value;
    }

    float get(void) { return value; }

    void reset(float value) {
        this->value = value;
        seedcount = 0;
        seedsum = 0;
    }
};

#endif
