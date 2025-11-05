//! robotkernel module posix timer
/*!
 * author: Robert Burger <robert.burger@dlr.de>
 */

/*
 * This file is part of module_posix_timer.
 *
 * module_posix_timer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * module_posix_timer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with module_posix_timer; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "posix_timer.h"
#include "robotkernel/helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>

#include <iostream>
#include <chrono>

#include "yaml-cpp/yaml.h"

MODULE_DEF(module_posix_timer, module_posix_timer::posix_timer);

using namespace std;
using namespace std::chrono;

using namespace robotkernel;
using namespace module_posix_timer;

//! Construction
timer_base::timer_base(std::shared_ptr<posix_timer> parent, const YAML::Node& config) :
    runnable(config),
    trigger(parent->name, get_as<std::string>(config, "name"), 1./get_as<double>(config, "interval")),
    parent(parent) 
{
    name = get_as<std::string>(config, "name");
    runnable::set_name(parent->name + "." + name);
    
    string tmp_skip_missed = get_as<string>(config, "skip_missed", "none");

    if (tmp_skip_missed == string("strict")) {
        skip_missed = skip_strict;
    } else if (tmp_skip_missed == string("normal")) {
        skip_missed = skip_normal;
    } else {
        skip_missed = skip_none;
    }
}

//! Initialize timer
void timer_base::init(void) {
    // add trigger device
    robotkernel::add_device(shared_from_this());

    string pdin_desc = "- double: interval\n";
    pdin = make_shared<robotkernel::triple_buffer>(
            sizeof(double), parent->name, name + string(".inputs"), pdin_desc, trigger::id());

    prov = make_shared<pd_provider>(name);
    pdin->set_provider(prov);

    // register process_data
    robotkernel::add_device(pdin);

    pdin_inspect = make_shared<service_provider_process_data_inspection::pd_inspection>(parent->name, name + string(".inputs"), pdin);
    robotkernel::add_device(pdin_inspect);
}

//! Deinitialize timer
void timer_base::deinit(void) {
    robotkernel::remove_device(pdin_inspect);
    pdin_inspect = nullptr;

    // register devices (trigger, process_data)
    robotkernel::remove_device(shared_from_this());
    robotkernel::remove_device(pdin);

    pdin->reset_provider(prov);
    pdin = nullptr;
    prov = nullptr;
}

//! handler function for nanosleep mode
void nanosleep::run() {
    parent->log(info, "%s -> nanosleep handler running with pid %d\n", name.c_str(), getpid());
    auto now = std::chrono::high_resolution_clock::now();
    double interval = 0.;

    while (running()) {
        interval = 1. / trigger::get_rate();
        now += std::chrono::nanoseconds((long)(1E9 * interval));

        if (skip_missed != skip_none) {
            int skipped_cycles = 0;
            std::chrono::nanoseconds add_time = std::chrono::nanoseconds((long)0);
            if (skip_missed == skip_normal) {
                add_time += std::chrono::nanoseconds((long)(1E9 * interval));
            }

            while ((now + add_time) < std::chrono::high_resolution_clock::now()) {
                skipped_cycles++;
                now += std::chrono::nanoseconds((long)(1E9 * interval));
            }

            if (skipped_cycles > 0) {
                parent->log(warning, "%s -> skipped %d cylces!\n", name.c_str(), skipped_cycles);
            }
        }

        pdin->write(prov, 0, (uint8_t *)&interval, sizeof(interval), true, false);

        do {
            std::this_thread::sleep_until(now);
        } while (now > std::chrono::high_resolution_clock::now());

        trigger::do_trigger();
    }

    parent->log(info, "%s -> nanosleep handler stopped\n", name.c_str());
}

//! handler function for nanosleep mode
void busywait::run() {
    parent->log(info, "busywait handler running with pid %d\n", getpid());

    steady_clock::time_point next = steady_clock::now(), act;
    double interval = 0.;

    while (running()) {
        interval = 1. / trigger::get_rate();
        next += nanoseconds((uint64_t)(1E9 * interval));

        if (skip_missed != skip_none) {
            int skipped_cycles = 0;
            auto add_time = nanoseconds((long)0);
            if (skip_missed == skip_normal) {
                add_time += nanoseconds((long)(1E9 * interval));
            }

            while ((next + add_time) < steady_clock::now()) {
                skipped_cycles++;
                next += nanoseconds((long)(1E9 * interval));
            }

            if (skipped_cycles > 0) {
                parent->log(warning, "skipped %d cylces!\n", skipped_cycles);
            }
        }

        pdin->write(prov, 0, (uint8_t *)&interval, sizeof(interval), true, false);

        do {
            act = steady_clock::now();
        } while (act < next);

        trigger::do_trigger();
    }

    parent->log(info, "busywait handler stopped\n");
}

//! handler function for timer mode
void timer::run() {
    parent->log(info, "timer handler running with pid %d\n", getpid());
    
    sigset_t set;
    if (sigemptyset (&set) == -1)
        parent->log(error, "sigemptyset %s\n", strerror(errno));

    if (sigaddset (&set, signo) == -1)
        parent->log(error, "sigaddset %s\n", strerror(errno));

    /* set up timer to send out signal */
    struct sigevent se;
    memset(&se, 0, sizeof(se));
    se.sigev_notify = SIGEV_SIGNAL;
    se.sigev_signo = signo;

    if (timer_create(CLOCK_REALTIME, &se, &timer_id) == -1) {
        parent->log(error, "ERROR timer_create: %s\n", strerror(errno));
    }

    double interval = 1. / get_rate();
// disabled for now
#ifdef TIMER_SET_RATE_ENABLED
    double old_interval = interval;
#endif
    pdin->write(prov, 0, (uint8_t *)&interval, sizeof(interval), true, false);

    struct itimerspec value, value_old; 
    value.it_value.tv_sec = (int)(interval);
    value.it_value.tv_nsec = (interval-value.it_value.tv_sec)*1E9;
    value.it_interval.tv_sec = value.it_value.tv_sec;
    value.it_interval.tv_nsec = value.it_value.tv_nsec;

    if (timer_settime(timer_id, 0, &value, &value_old) == -1) {
        parent->log(error, "timer_settime %s\n", strerror(errno));
    }

    while (running()) {
        struct timespec ts = { 1, 0 };
        siginfo_t si;

        int ret = sigtimedwait(&set, &si, &ts);

        if (ret == -1) {
            if (errno == EAGAIN) {
                parent->log(info, "sigtimedwait timed out\n");
            } if (errno == EINVAL) {
                parent->log(info, "sigtimedwait einval\n");
            }
            continue;
        }

// disabled for now
#ifdef TIMER_SET_RATE_ENABLED
        interval = 1. / trigger::get_rate();

        if (old_interval != interval) {
            // reload timer with new value
            value.it_value.tv_sec = (int)(interval);
            value.it_value.tv_nsec = (interval-value.it_value.tv_sec)*1E9;
            value.it_interval.tv_sec = value.it_value.tv_sec;
            value.it_interval.tv_nsec = value.it_value.tv_nsec;

            if (timer_settime(timer_id, 0, &value, &value_old) == -1) {
                parent->log(error, "timer_settime %s\n", strerror(errno));
            }

            old_interval = interval;
        }
#endif

        trigger::do_trigger();
    }

    if (timer_id) {
        struct itimerspec value; 
        value.it_value.tv_sec = 0;
        value.it_value.tv_nsec = 0;
        value.it_interval.tv_sec = 0;
        value.it_interval.tv_nsec = 0;

        if (timer_settime(timer_id, 0, &value, NULL) == -1) {
            parent->log(error, "ERROR timer_settime: %s\n", strerror(errno));
        }

        timer_delete(timer_id);
    }

    parent->log(info, "timer handler stopped\n");
}

//! default construction
/*!
 * \param node yaml configuration node
 */
posix_timer::posix_timer(const char* name, const YAML::Node& node) : 
    module_base("module_posix_timer", name, node)
{
    config = YAML::Clone(node);
}

// additional module init stuff
void posix_timer::init() {
    std::function<void(const YAML::Node& timer_config)> create_timer = [&](const YAML::Node& timer_config) { 
        if (timer_config["mode"]) {
            if (timer_config["mode"].as<string>() == string("nanosleep")) {
                timers.push_back(std::make_shared<nanosleep>(shared_from_this(), timer_config));
            } else if (timer_config["mode"].as<string>() == string("timer")) {
                timers.push_back(std::make_shared<timer>(shared_from_this(), timer_config));
            } else if (timer_config["mode"].as<string>() == string("busywait")) {
                timers.push_back(std::make_shared<busywait>(shared_from_this(), timer_config));
            }
        } else {
            log(info, "mode not specified, assuming nanosleep mode!\n");
            timers.push_back(std::make_shared<nanosleep>(shared_from_this(), timer_config));
        }
    };

    if (config["timers"]) {
        for (const auto& entry : config["timers"]) {
            create_timer(entry);
        }
    } else { 
        create_timer(config);
    }
}
 
