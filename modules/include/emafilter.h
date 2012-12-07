#if !defined(EMA_FILTER_H)
#define EMA_FILTER_H

class ema_filter_t {
    float alpha;
    float value;
    float seedsum;
    unsigned seedceil;
    unsigned seedcount;

public:
    ema_filter_t(float _alpha, float _value, unsigned _seedceil) : 
        alpha(_alpha),
        value(_value),
        seedsum(0),
        seedceil(_seedceil),
        seedcount(0) { }
    
    float update(float sample) {
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
