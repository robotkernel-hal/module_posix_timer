//! robotkernel module posix timer
/*!
 * author: Robert Burger
 *
 * $Id$
 */

/*
 * This file is part of robotkernel.
 *
 * robotkernel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * robotkernel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with robotkernel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "posix_timer.h"
#include "robotkernel/helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>

#include "yaml-cpp/yaml.h"

MODULE_DEF(module_posix_timer, module_posix_timer::posix_timer)

/**
 * set_normalized_timespec - set timespec sec and nsec parts and normalize
 *
 * @ts:		pointer to timespec variable to be set
 * @sec:	seconds to set
 * @nsec:	nanoseconds to set
 *
 * Set seconds and nanoseconds field of a timespec variable and
 * normalize to the timespec storage format
 *
 * Note: The tv_nsec part is always in the range of
 *	0 <= tv_nsec < NSEC_PER_SEC
 * For negative values only the tv_sec field is negative !
 */
#define NSEC_PER_SEC 1000000000
void set_normalized_timespec(struct timespec *ts, time_t sec, int64_t nsec)
{
    while (nsec >= NSEC_PER_SEC) {
        /*
         * The following asm() prevents the compiler from
         * optimising this loop into a modulo operation. See
         * also __iter_div_u64_rem() in include/linux/time.h
         */
        asm("" : "+rm"(nsec));
        nsec -= NSEC_PER_SEC;
        ++sec;
    }
    while (nsec < 0) {
        asm("" : "+rm"(nsec));
        nsec += NSEC_PER_SEC;
        --sec;
    }
    ts->tv_sec = sec;
    ts->tv_nsec = nsec;
}

inline struct timespec timespec_sub(struct timespec a, struct timespec b) {
    struct timespec ret;
    set_normalized_timespec(&ret, a.tv_sec - b.tv_sec, a.tv_nsec - b.tv_nsec);

    return ret;
}

using namespace std;
using namespace robotkernel;
using namespace module_posix_timer;

//! default construction
/*!
 * \param node yaml configuration node
 */
posix_timer::posix_timer(const char* name, const YAML::Node& node) 
    : runnable(node), module_base("module_posix_timer", name, node) {
    interval = get_as<double>(node, "interval");
    shift    = 0;
    signo    = get_as<int>(node, "signo", SIGRTMIN);
    timer_id = NULL;
    mode     = posix_timer_mode_timer;

    if (node["mode"]) {
        if (node["mode"].as<string>() == string("nanosleep"))
            mode = posix_timer_mode_nanosleep;
        else if (node["mode"].as<string>() == string("timer"))
            mode = posix_timer_mode_timer;
    } else 
        log(info, "mode not specified, assuming timer mode!\n");

    // create ipc structures
    pthread_mutex_init(&sync_lock, NULL);
    pthread_cond_init(&sync_cond, NULL);
};

//! destrcution
posix_timer::~posix_timer() {
    stop();

    pthread_mutex_destroy(&sync_lock);
    pthread_cond_destroy(&sync_cond);
}

//! handler function called if thread is running
void posix_timer::run() {
    if (mode == posix_timer_mode_nanosleep)
        return run_nanosleep();

    return run_timer();
}

//! handler function for nanosleep mode
void posix_timer::run_nanosleep() {
    log(info, "nanosleep handler running with pid %d\n", getpid());

    struct timespec ts_now;
    clock_gettime(CLOCK_REALTIME, &ts_now);

    while (running()) {
        struct timespec ts = { 1, 0 }, ts_diff;
        timespec_add(&ts_now, (int)(interval), (interval - (int)interval)*1E9);

        if (shift != 0) {
            timespec_add(&ts_now, (int)(shift), (shift - (int)shift)*1E9);
            shift = 0;
        }

        clock_gettime(CLOCK_REALTIME, &ts);
        ts_diff = timespec_sub(ts_now, ts);

        nanosleep(&ts_diff, NULL);

        trigger_modules();
    }

    log(info, "nanosleep handler stopped\n");
}

//! handler function for timer mode
void posix_timer::run_timer() {
    log(info, "timer handler running with pid %d\n", getpid());

    sigset_t set;
    if (sigemptyset (&set) == -1)
        log(error, "sigemptyset %s\n", strerror(errno));

    if (sigaddset (&set, signo) == -1)
        log(error, "sigaddset %s\n", strerror(errno));

    /* set up timer to send out signal */
    struct sigevent se;
    memset(&se, 0, sizeof(se));
    se.sigev_notify = SIGEV_SIGNAL;
    se.sigev_signo = signo;

    if (timer_create(CLOCK_REALTIME, &se, &timer_id) == -1)
        log(error, "ERROR timer_create: %s\n", strerror(errno));

    double old_interval = interval;

    struct itimerspec value, value_old; 
    value.it_value.tv_sec = (int)(interval);
    value.it_value.tv_nsec = (interval-value.it_value.tv_sec)*1E9;
    value.it_interval.tv_sec = value.it_value.tv_sec;
    value.it_interval.tv_nsec = value.it_value.tv_nsec;

    if (timer_settime(timer_id, 0, &value, &value_old) == -1)
        log(error, "timer_settime %s\n", strerror(errno));

    while (running()) {
        struct timespec ts = { 1, 0 };
        siginfo_t si;

        int ret = sigtimedwait(&set, &si, &ts);

        if (ret == -1) {
            if (errno == EAGAIN)
                log(info, "sigtimedwait timed out\n");
            if (errno == EINVAL)
                log(info, "sigtimedwait einval\n");
            continue;
        }

        if (old_interval != interval) {
            // reload timer with new value
            value.it_value.tv_sec = (int)(interval);
            value.it_value.tv_nsec = (interval-value.it_value.tv_sec)*1E9;
            value.it_interval.tv_sec = value.it_value.tv_sec;
            value.it_interval.tv_nsec = value.it_value.tv_nsec;

            if (timer_settime(timer_id, 0, &value, &value_old) == -1)
                log(error, "timer_settime %s\n", strerror(errno));

            old_interval = interval;
        }

        trigger_modules();
    }

    if (timer_id) {
        struct itimerspec value; 
        value.it_value.tv_sec = 0;
        value.it_value.tv_nsec = 0;
        value.it_interval.tv_sec = 0;
        value.it_interval.tv_nsec = 0;

        if (timer_settime(timer_id, 0, &value, NULL) == -1)
            log(error, "ERROR timer_settime: %s\n",
                    strerror(errno));

        timer_delete(timer_id);
    }

    log(info, "timer handler stopped\n");
}

//! set module state machine to defined state
/*!
  \param state requested state
  \return success or failure
  */
int posix_timer::set_state(module_state_t state) {
    if (state == this->state) {
        return 0;
    }

    log(info, "state %s requested\n", state_to_string(state));

    switch (state) {
        case module_state_init:
        case module_state_preop:
        case module_state_safeop:
            stop();
            break;
        case module_state_op: {
            if (this->state < module_state_safeop) {
                // invalid state transition
                log(error, "state transition from %s to OP "
                        "is invalid\n", state_to_string(state));

                return -1;
            }

            start();
            break;
        }
        default:
            // invalid state 
            log(error, "invalid state %d requested\n", (int)state);
            return -1;
    }

    // set actual state 
    this->state = state;

    log(info, "state %s reached\n", state_to_string(state));

    return 0;
}

//! send a request to module
/*! 
  \param hdl module handle
  \param reqcode request code
  \param ptr pointer to request structure
  \return success or failure
  */
int posix_timer::request(int reqcode, void* ptr) {
    int ret = 0;

    if (trigger_base::request(reqcode, ptr) == 0)
        return 0;

    switch (reqcode) {
        case MOD_REQUEST_GET_TRIGGER_INTERVAL: {
            double *result = (double *)ptr;
            *result = interval;
            break;
        }
        case MOD_REQUEST_SET_TRIGGER_INTERVAL: {
            double *value = (double *)ptr;
            interval = *value;
            break;
        }
        case MOD_REQUEST_SHIFT_NEXT_TRIGGER: {
            double *value = (double *)ptr;
            shift = *value;
            break;
        }
        case MOD_REQUEST_GET_PDIN: {
            process_data_t *pd = (process_data_t *)ptr;
            pd->pd = &interval;
            pd->len = sizeof(interval) + sizeof(shift);
            break;
        }
        default:
            ret = -1;
            break;
    }

    return ret;
}

