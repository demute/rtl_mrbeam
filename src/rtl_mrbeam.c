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
#include "parser.h"
#include "term_ctl.h"
#include "confparse.h"
#include "optparse.h"

static r_cfg_t g_cfg;

// TODO: SIGINFO is not in POSIX...
#ifndef SIGINFO
#define SIGINFO 29
#endif

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

static int hasopt(int test, int argc, char *argv[], char const *optstring)
{
    int opt;

    optind = 1; // reset getopt
    while ((opt = getopt(argc, argv, optstring)) != -1) {
        if (opt == test || optopt == test)
            return opt;
    }
    return 0;
}

static void parse_conf_option(r_cfg_t *cfg, int opt, char *arg);

static void usage(int exit_code)
{
    term_help_printf(
            "\nUsage:\n"
            "\t\t= General options =\n"
            "  [-V] Output the version string and exit\n"
            "  [-v] Increase verbosity (can be used multiple times).\n"
            "       -v : verbose, -vv : verbose decoders, -vvv : debug decoders, -vvvv : trace decoding).\n"
            "  [-d <RTL-SDR USB device index> | :<RTL-SDR USB device serial> | <SoapySDR device query> | rtl_tcp | help]\n"
            "  [-g <gain> | help] (default: auto)\n"
            "  [-h] Output this usage help and exit\n"
            "       Use -d, -g, -R, -X, -F, -M, -r, -w, or -W without argument for more help\n\n");
    exit(exit_code);
}

#define OPTSTRING "hVv:r:w:W:d:g:s"

// these should match the short options exactly
static struct conf_keywords const conf_keywords[] = {
        {"help", 'h'},
        {"verbose", 'v'},
        {"version", 'V'},
        {"device", 'd'},
        {"gain", 'g'},
        {"read_file", 'r'},
        {"write_file", 'w'},
        {"overwrite_file", 'W'},
        {NULL, 0}
};

static void parse_conf_args(r_cfg_t *cfg, int argc, char *argv[])
{
    int opt;

    optind = 1; // reset getopt
    while ((opt = getopt(argc, argv, OPTSTRING)) != -1) {
        if (opt == '?')
            opt = optopt; // allow missing arguments
        parse_conf_option(cfg, opt, optarg);
    }
}

static void help_device(void)
{
    term_help_printf(
            "\t\t= Input device selection =\n"
#ifdef RTLSDR
            "\tRTL-SDR device driver is available.\n"
#else
            "\tRTL-SDR device driver is not available.\n"
#endif
            "  [-d <RTL-SDR USB device index>] (default: 0)\n"
            "  [-d :<RTL-SDR USB device serial (can be set with rtl_eeprom -s)>]\n"
            "\tTo set gain for RTL-SDR use -g <gain> to set an overall gain in dB.\n"
#ifdef SOAPYSDR
            "\tSoapySDR device driver is available.\n"
#else
            "\tSoapySDR device driver is not available.\n"
#endif
            "  [-d \"\"] Open default SoapySDR device\n"
            "  [-d driver=rtlsdr] Open e.g. specific SoapySDR device\n"
            "\tTo set gain for SoapySDR use -g ELEM=val,ELEM=val,... e.g. -g LNA=20,TIA=8,PGA=2 (for LimeSDR).\n"
            "  [-d rtl_tcp[:[//]host[:port]] (default: localhost:1234)\n"
            "\tSpecify host/port to connect to with e.g. -d rtl_tcp:127.0.0.1:1234\n");
    exit(0);
}

static void help_gain(void)
{
    term_help_printf(
            "\t\t= Gain option =\n"
            "  [-g <gain>] (default: auto)\n"
            "\tFor RTL-SDR: gain in dB (\"0\" is auto).\n"
            "\tFor SoapySDR: gain in dB for automatic distribution (\"\" is auto), or string of gain elements.\n"
            "\tE.g. \"LNA=20,TIA=8,PGA=2\" for LimeSDR.\n");
    exit(0);
}

static void parse_conf_option(r_cfg_t *cfg, int opt, char *arg)
{
    int n;

    if (arg && (!strcmp(arg, "help") || !strcmp(arg, "?"))) {
        arg = NULL; // remove the arg if it's a request for the usage help
    }

    switch (opt) {
    case 'h':
        usage(0);
        break;
    case 'v':
        if (!arg)
            cfg->verbosity++;
        else
            cfg->verbosity = atobv(arg, 1);
        break;
    case 'd':
        if (!arg)
            help_device();

        cfg->dev_query = arg;
        break;
    case 'g':
        if (!arg)
            help_gain();

        cfg->gain_str = arg;
        break;
    default:
        usage(1);
        break;
    }
}


int main(int argc, char **argv) {
    struct sigaction sigact;
    FILE *in_file;
    int r = 0;
    unsigned i;
    r_cfg_t *cfg = &g_cfg;

    void *mrbeamCtx = mrbeam_setup ();

    cfg->out_block_size  = DEFAULT_BUF_LENGTH;
    cfg->samp_rate       = 948000;
    cfg->gain_str        = "13";
    cfg->conversion_mode = CONVERT_NATIVE;
    cfg->dev_query = NULL;
    cfg->verbosity = 0;
    cfg->center_frequency = DEFAULT_FREQUENCY;

    parse_conf_args(cfg, argc, argv);

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    fprintf (stderr, "dvb rtl gain: %s\n", cfg->gain_str);

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
    r = sdr_set_center_freq(cfg->dev, cfg->center_frequency, 1); // always verbose

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
        //cfg->center_frequency = cfg->frequency[cfg->frequency_index];
        //r = sdr_set_center_freq(cfg->dev, cfg->center_frequency, 1); // always verbose

        //if (samp_rate != cfg->samp_rate) {
        //    r = sdr_set_sample_rate(cfg->dev, cfg->samp_rate, 1); // always verbose
        //    samp_rate = cfg->samp_rate;
        //}

        signal(SIGALRM, sighandler);
        alarm(3); // require callback to run every 3 second, abort otherwise
        r = sdr_start(cfg->dev, sdr_callback, (void *)mrbeamCtx,
                DEFAULT_ASYNC_BUF_NUMBER, cfg->out_block_size);
        if (r < 0) {
            fprintf(stderr, "WARNING: async read failed (%i).\n", r);
            break;
        }
        alarm(0); // cancel the watchdog timer
        cfg->do_exit_async = 0;
        //cfg->frequency_index = (cfg->frequency_index + 1) % cfg->frequencies;
    }

    return r >= 0 ? r : -r;
}
