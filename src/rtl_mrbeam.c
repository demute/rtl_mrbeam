/** @file
    rtl_433, turns your Realtek RTL2832 based DVB dongle into a 433.92MHz generic data receiver.

    Copyright (C) 2012 by Benjamin Larsson <benjamin@southpole.se>

    Based on rtl_sdr
    Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#include "sdr.h"
#include "rtl_mrbeam.h"

static r_cfg_t g_cfg;

// TODO: SIGINFO is not in POSIX...
#ifndef SIGINFO
#define SIGINFO 29
#endif


static void sdr_callback(unsigned char *iq_buf, uint32_t len, void *ctx)
{
    fprintf (stderr, "callback called\n");
}

static void sighandler(int signum)
{
    if (signum == SIGPIPE) {
        signal(SIGPIPE, SIG_IGN);
    }
    else if (signum == SIGINFO/* TODO: maybe SIGUSR1 */) {
        g_cfg.stats_now++;
        return;
    }
    else if (signum == SIGUSR1) {
        g_cfg.do_exit_async = 1;
        sdr_stop(g_cfg.dev);
        return;
    }
    else if (signum == SIGALRM) {
        fprintf(stderr, "Async read stalled, exiting!\n");
    }
    else {
        fprintf(stderr, "Signal caught, exiting!\n");
    }
    g_cfg.do_exit = 1;
    sdr_stop(g_cfg.dev);
}

void r_init_cfg(r_cfg_t *cfg)
{
    cfg->out_block_size  = DEFAULT_BUF_LENGTH;
    cfg->samp_rate       = DEFAULT_SAMPLE_RATE;
    cfg->conversion_mode = CONVERT_NATIVE;
}

int main(int argc, char **argv) {
    struct sigaction sigact;
    FILE *in_file;
    int r = 0;
    unsigned i;
    r_cfg_t *cfg = &g_cfg;
    cfg->dev_query = NULL;
    cfg->verbosity = 0;

    r_init_cfg(cfg);

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // Normal case, no test data, no in files
    int sample_size = 1;
    r = sdr_open(& cfg->dev, & sample_size, cfg->dev_query, cfg->verbosity);
    if (r < 0) {
        exit(1);
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT,  &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGPIPE, &sigact, NULL);
    sigaction(SIGUSR1, &sigact, NULL);
    sigaction(SIGINFO, &sigact, NULL);
    /* Set the sample rate */
    r = sdr_set_sample_rate(cfg->dev, cfg->samp_rate, 1); // always verbose
    r = sdr_apply_settings(cfg->dev, cfg->settings_str, 1); // always verbose for soapy
    r = sdr_set_tuner_gain(cfg->dev, cfg->gain_str, 1); // always verbose

    if (cfg->ppm_error)
        r = sdr_set_freq_correction(cfg->dev, cfg->ppm_error, 1); // always verbose

    /* Reset endpoint before we start reading from it (mandatory) */
    r = sdr_reset(cfg->dev, cfg->verbosity);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to reset buffers.\n");
    r = sdr_activate(cfg->dev);

    uint32_t samp_rate = cfg->samp_rate;
    while (!cfg->do_exit) {
        time(&cfg->hop_start_time);

        /* Set the cfg->frequency */
        cfg->center_frequency = cfg->frequency[cfg->frequency_index];
        r = sdr_set_center_freq(cfg->dev, cfg->center_frequency, 1); // always verbose

        if (samp_rate != cfg->samp_rate) {
            r = sdr_set_sample_rate(cfg->dev, cfg->samp_rate, 1); // always verbose
            samp_rate = cfg->samp_rate;
        }

        signal(SIGALRM, sighandler);
        alarm(3); // require callback to run every 3 second, abort otherwise
        r = sdr_start(cfg->dev, sdr_callback, (void *)cfg,
                DEFAULT_ASYNC_BUF_NUMBER, cfg->out_block_size);
        if (r < 0) {
            fprintf(stderr, "WARNING: async read failed (%i).\n", r);
            break;
        }
        alarm(0); // cancel the watchdog timer
        cfg->do_exit_async = 0;
        cfg->frequency_index = (cfg->frequency_index + 1) % cfg->frequencies;
    }

    return r >= 0 ? r : -r;
}
//#include "common.h"
//#include "stream_buffer.h"
//
//#define DECIMATION 69
//
//typedef float float_type;
//struct mixer_t
//{
//    float_type ival;
//    float_type qval;
//    float_type v;
//    float_type cosv;
//    float_type sinv;
//    float_type f;
//    float_type Fs;
//};
//typedef struct mixer_t Mixer;
//
//struct channel_state_t
//{
//    long   lastSample;
//    double eventTsp;
//    int    count;
//};
//typedef struct channel_state_t ChannelState;
//
//
//struct filter_t
//{
//    int cnt;
//    int nTaps;
//    int decimation;
//    float_type *taps;
//    StreamBuffer *sb;
//};
//typedef struct filter_t Filter;
//
//Filter *filter_new (float_type Fs, float_type *taps, int nTaps, int decimation)
//{
//    Filter *f = malloc (sizeof (Filter));
//    assert (f);
//
//    f->taps = malloc (sizeof (float_type) * nTaps);
//    assert (f->taps);
//
//    f->cnt = 0;
//    f->nTaps = nTaps;
//    f->decimation = decimation;
//    memcpy (f->taps, taps, sizeof (float_type) * nTaps);
//
//    uint len = (uint) pow (2.0, ceil (log2 (nTaps)));
//    f->sb = stream_buffer_create (len, sizeof (float_type [2]));
//
//    float_type zero[2] = {0,0};
//
//    for (int i=0; i<f->nTaps; i++)
//        stream_buffer_insert (f->sb, zero);
//
//    return f;
//}
//
//int filter_filter (Filter *f, float_type *in, float_type *out)
//{
//    stream_buffer_insert (f->sb, in);
//
//    if (++f->cnt < f->decimation)
//        return 0;
//
//    f->cnt = 0;
//    float_type (*buf)[2];
//    uint len;
//    stream_buffer_get (f->sb, & buf, & len);
//
//    out[0] = 0;
//    out[1] = 0;
//    for (int i=0; i<f->nTaps; i++)
//    {
//        out[0] += buf[len - 1 - i][0] * f->taps[i];
//        out[1] += buf[len - 1 - i][1] * f->taps[i];
//    }
//    return 1;
//}
//
//Mixer *mixer_new (float_type Fs, float_type f)
//{
//    Mixer *m = malloc (sizeof (Mixer));
//    assert (m);
//
//    m->f    = f;
//    m->Fs   = Fs;
//    m->ival = 1.0;
//    m->qval = 0.0;
//    m->v    = 2 * M_PI * f / Fs;
//    m->cosv = cos (m->v);
//    m->sinv = sin (m->v);
//
//    return m;
//}
//
//Mixer *mixer_iterate (Mixer *m)
//{
//    float_type ival = m->cosv * m->ival - m->sinv * m->qval;
//    float_type qval = m->sinv * m->ival + m->cosv * m->qval;
//    float_type mag2 = ival * ival + qval * qval;
//    float_type mag  = 0.5f * (1.0f + mag2);
//    float_type correction = 2.0f - mag;
//
//    m->ival = ival * correction;
//    m->qval = qval * correction;
//
//    //print_debug ("Fs:%f f:%f ival:%f qval:%f", m->Fs, m->f, m->ival, m->qval);
//    return m;
//}
//
//
//Mixer *mixer_mix (Mixer *m, float_type *iqsrc, float_type *iqdst)
//{
//    mixer_iterate (m);
//    iqdst[0] = m->ival * iqsrc[0] - m->qval * iqsrc[1];
//    iqdst[1] = m->qval * iqsrc[0] + m->ival * iqsrc[1];
//    return m;
//}
//
//int main (int argc, char **argv)
//{
//    float_type Fs = 948000;
//
//    Mixer *m[4];
//    m[0] = mixer_new (Fs, -300e3);
//    m[1] = mixer_new (Fs,  300e3);
//    m[2] = mixer_new (Fs,  100e3);
//    m[3] = mixer_new (Fs, -100e3);
//
//    float_type taps[] =
//    {
//        0.0123713309415827,
//        0.0243551437758347,
//        0.0568127504584979,
//        0.1002740326690650,
//        0.1408965394176662,
//        0.1652902027373532,
//        0.1652902027373533,
//        0.1408965394176663,
//        0.1002740326690651,
//        0.0568127504584979,
//        0.0243551437758347,
//        0.0123713309415827
//    };
//
//    int n = (int) (sizeof (taps) / sizeof (taps[0]));
//
//    Filter *f[4];
//    f[0] = filter_new (Fs, taps, n, DECIMATION);
//    f[1] = filter_new (Fs, taps, n, DECIMATION);
//    f[2] = filter_new (Fs, taps, n, DECIMATION);
//    f[3] = filter_new (Fs, taps, n, DECIMATION);
//
//    ChannelState channelStates[4];
//    bzero (channelStates, sizeof (channelStates));
//
//    uint8_t iqRaw[2];
//    float_type rtlLookup[256];
//    for (int i=0; i<256; i++)
//        rtlLookup[i] = (i - 127.4) * (1.0/128.0);
//
//    int cnt = 0;
//    float_type maxs[4] = {0};
//    long sample = 0;
//    while (fread (iqRaw, sizeof (iqRaw), 1, stdin) == 1)
//    {
//        sample++;
//        float_type iqIn[2] = { rtlLookup[iqRaw[0]], rtlLookup[iqRaw[1]] };
//        float_type iqMixed[2] = {0};
//        float_type iqFiltered[2] = {0};
//
//        int nl = 0;
//        if (cnt && --cnt == 0)
//        {
//            float_type m = 0;
//            int channel = 0;
//            for (int i=0; i<4; i++)
//            {
//                if (m < maxs[i])
//                {
//                    m = maxs[i];
//                    channel = i;
//                }
//            }
//
//            double periodTime = 1.0 / 254.5;
//            long   elapsedSampels = sample - channelStates[channel].lastSample;
//            double timeElapsed = elapsedSampels / Fs;
//            int nPeriods = timeElapsed / periodTime;
//
//            if (nPeriods > 16)
//                channelStates[channel].count = 0;
//
//            channelStates[channel].lastSample = sample;
//            channelStates[channel].count++;
//            //print_debug ("channel:%d count:%d", channel, channelStates[channel].count);
//
//            if (channelStates[channel].count > 300)
//            {
//               double tsp = get_time ();
//               if (tsp - channelStates[channel].eventTsp > 3)
//               {
//                   channelStates[channel].eventTsp = tsp;
//                   print_debug ("%f channel %d triggered!\n", get_time (), channel);
//                   printf ("%d\n", channel);
//                   fflush (stdout);
//               }
//            }
//        }
//
//        for (int i=0; i<4; i++)
//        {
//            mixer_mix (m[i], iqIn, iqMixed);
//            if (filter_filter (f[i], iqMixed, iqFiltered))
//            {
//                float_type mag2 = iqFiltered[0] * iqFiltered[0] + iqFiltered[1] * iqFiltered[1];
//                if (cnt)
//                {
//                    if (maxs[i] < mag2)
//                        maxs[i] = mag2;
//                }
//                else if (mag2 > 0.2)
//                {
//                    cnt = DECIMATION * 10;
//                    maxs[0] = 0;
//                    maxs[1] = 0;
//                    maxs[2] = 0;
//                    maxs[3] = 0;
//
//                    maxs[i] = mag2;
//                }
//            }
//        }
//    }
//    return 0;
//}
