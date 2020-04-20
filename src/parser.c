#include "common.h"
#include "stream_buffer.h"

#define DECIMATION 69

typedef float float_type;
struct mixer_t
{
    float_type ival;
    float_type qval;
    float_type v;
    float_type cosv;
    float_type sinv;
    float_type f;
    unsigned long Fs;
};
typedef struct mixer_t Mixer;

struct channel_state_t
{
    long   lastSample;
    double eventTsp;
    int    count;
};
typedef struct channel_state_t ChannelState;


struct filter_t
{
    int cnt;
    int nTaps;
    int decimation;
    float_type *taps;
    StreamBuffer *sb;
};
typedef struct filter_t Filter;

Filter *filter_new (float_type *taps, int nTaps, int decimation)
{
    Filter *f = malloc (sizeof (Filter));
    assert (f);

    f->taps = malloc (sizeof (float_type) * nTaps);
    assert (f->taps);

    f->cnt = 0;
    f->nTaps = nTaps;
    f->decimation = decimation;
    memcpy (f->taps, taps, sizeof (float_type) * nTaps);

    uint len = (uint) pow (2.0, ceil (log2 (nTaps)));
    f->sb = stream_buffer_create (len, sizeof (float_type [2]));

    float_type zero[2] = {0,0};

    for (int i=0; i<f->nTaps; i++)
        stream_buffer_insert (f->sb, zero);

    return f;
}

int filter_filter (Filter *f, float_type *in, float_type *out)
{
    stream_buffer_insert (f->sb, in);

    if (++f->cnt < f->decimation)
        return 0;

    f->cnt = 0;
    float_type (*buf)[2];
    uint len;
    stream_buffer_get (f->sb, & buf, & len);

    out[0] = 0;
    out[1] = 0;
    for (int i=0; i<f->nTaps; i++)
    {
        out[0] += buf[len - 1 - i][0] * f->taps[i];
        out[1] += buf[len - 1 - i][1] * f->taps[i];
    }
    return 1;
}

Mixer *mixer_new (unsigned long Fs, float_type f)
{
    Mixer *m = malloc (sizeof (Mixer));
    assert (m);

    m->f    = f;
    m->Fs   = Fs;
    m->ival = 1.0;
    m->qval = 0.0;
    m->v    = 2 * M_PI * f / (float_type) Fs;
    m->cosv = cos (m->v);
    m->sinv = sin (m->v);

    return m;
}

Mixer *mixer_iterate (Mixer *m)
{
    float_type ival = m->cosv * m->ival - m->sinv * m->qval;
    float_type qval = m->sinv * m->ival + m->cosv * m->qval;
    float_type mag2 = ival * ival + qval * qval;
    float_type mag  = 0.5f * (1.0f + mag2);
    float_type correction = 2.0f - mag;

    m->ival = ival * correction;
    m->qval = qval * correction;

    return m;
}


Mixer *mixer_mix (Mixer *m, float_type *iqsrc, float_type *iqdst)
{
    mixer_iterate (m);
    iqdst[0] = m->ival * iqsrc[0] - m->qval * iqsrc[1];
    iqdst[1] = m->qval * iqsrc[0] + m->ival * iqsrc[1];
    return m;
}

struct mrbeam_cfg_t
{
    unsigned long Fs;
    Mixer  *m[4];
    Filter *f[4];
    ChannelState channelStates[4];
    int cnt;
    float_type maxs[4];
    long sampleCounter;
};
typedef struct mrbeam_cfg_t MrbeamCfg;

static float_type rtlLookup[256];

void *mrbeam_setup (void)
{
    MrbeamCfg *cfg = calloc (1, sizeof (MrbeamCfg));
    assert (cfg);

    cfg->Fs = 948000;

    cfg->m[0] = mixer_new (cfg->Fs, -300e3);
    cfg->m[1] = mixer_new (cfg->Fs,  300e3);
    cfg->m[2] = mixer_new (cfg->Fs,  100e3);
    cfg->m[3] = mixer_new (cfg->Fs, -100e3);

    float_type taps[] =
    {
        0.0123713309415827,
        0.0243551437758347,
        0.0568127504584979,
        0.1002740326690650,
        0.1408965394176662,
        0.1652902027373532,
        0.1652902027373533,
        0.1408965394176663,
        0.1002740326690651,
        0.0568127504584979,
        0.0243551437758347,
        0.0123713309415827
    };

    int nTaps = (int) (sizeof (taps) / sizeof (taps[0]));

    cfg->f[0] = filter_new (taps, nTaps, DECIMATION);
    cfg->f[1] = filter_new (taps, nTaps, DECIMATION);
    cfg->f[2] = filter_new (taps, nTaps, DECIMATION);
    cfg->f[3] = filter_new (taps, nTaps, DECIMATION);

    bzero (cfg->channelStates, sizeof (cfg->channelStates));

    for (int i=0; i<256; i++)
        rtlLookup[i] = (i - 127.4) * (1.0/128.0);

    cfg->cnt = 0;
    cfg->sampleCounter = 0;

    return (void *) cfg;
}

void sdr_callback(unsigned char *iq_buf, uint32_t len, void *ctx)
{
    MrbeamCfg *cfg = ctx;
    //for (uint32_t i=0; i<len; i++)
    //    fprintf (stderr, "%02x%s", iq_buf[i], ((i == len-1) || ((i+1) % 64 == 0)) ? "\n" : ((i+1) % 2 == 0) ? " " : "");
    alarm(3); // require callback to run every 3 second, abort otherwise

    assert (len % 2 == 0);

    for (uint32_t i=0; i<len; i+=2)
    {
        cfg->sampleCounter++;
        float_type iqIn[2] = { rtlLookup[iq_buf[i]], rtlLookup[iq_buf[i+1]] };
        //fprintf (stderr, "%+f %+f\n", iqIn[0], iqIn[1]);
        float_type iqMixed[2] = {0};
        float_type iqFiltered[2] = {0};

        int nl = 0;
        if (cfg->cnt && --cfg->cnt == 0)
        {
            float_type m = 0;
            int channel = 0;
            for (int i=0; i<4; i++)
            {
                if (m < cfg->maxs[i])
                {
                    m = cfg->maxs[i];
                    channel = i;
                }
            }

            double periodTime = 1.0 / 254.5;
            long   elapsedSampels = cfg->sampleCounter - cfg->channelStates[channel].lastSample;
            double timeElapsed = elapsedSampels / (double) cfg->Fs;
            int nPeriods = timeElapsed / periodTime;

            if (nPeriods > 16)
                cfg->channelStates[channel].count = 0;

            cfg->channelStates[channel].lastSample = cfg->sampleCounter;
            cfg->channelStates[channel].count++;
            //print_debug ("channel:%d count:%d", channel, cfg->channelStates[channel].count);

            if (cfg->channelStates[channel].count > 175)
            {
               double tsp = get_time ();
               if (tsp - cfg->channelStates[channel].eventTsp > 3)
               {
                   cfg->channelStates[channel].eventTsp = tsp;
                   fprintf (stderr, "%f channel %d triggered\n", get_time (), channel);
                   printf ("%d\n", channel);
                   fflush (stdout);
               }
            }
        }

        for (int i=0; i<4; i++)
        {
            mixer_mix (cfg->m[i], iqIn, iqMixed);
            if (filter_filter (cfg->f[i], iqMixed, iqFiltered))
            {
                float_type mag2 = iqFiltered[0] * iqFiltered[0] + iqFiltered[1] * iqFiltered[1];
                if (cfg->cnt)
                {
                    if (cfg->maxs[i] < mag2)
                        cfg->maxs[i] = mag2;
                }
                else if (mag2 > 0.2)
                {
                    cfg->cnt = DECIMATION * 10;
                    cfg->maxs[0] = 0;
                    cfg->maxs[1] = 0;
                    cfg->maxs[2] = 0;
                    cfg->maxs[3] = 0;

                    cfg->maxs[i] = mag2;
                }
            }
        }
    }
}
