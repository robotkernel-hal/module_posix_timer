//! ÜBER-control Module posix_timer
/*!
 * $Id$
 */

#include "module_posix_timer.h"
#include "robotkernel/kernel.h"
#include "robotkernel/runnable.h"
#include "robotkernel/trigger_base.h"
#include "robotkernel/helpers.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>

#include "yaml-cpp/yaml.h"

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

//! log to kernel logging facility
void pt_log(std::string mod_name, robotkernel::loglevel lvl, const char *format, ...) {
    char buf[1024];

    // format argument list
    va_list args;
    va_start(args, format);
    vsnprintf(buf, 1024, format, args);
    klog(lvl, "[module_posix_timer|%s] %s", mod_name.c_str(), buf);
}

typedef std::list<set_trigger_cb_t> cb_list_t;

class posix_timer : public runnable, public trigger_base {
    public:
        double _interval;       //! posix timer cyclic interval 
        int _signo;             //! signal number
        string _name;           //! posix timer name
        module_state_t _state;  //! module state
        timer_t _timer_id; 

        enum {
            posix_timer_mode_nanosleep,
            posix_timer_mode_timer,
        } _mode;

        //! default construction
        /*!
         * \param node yaml configuration node
         */
        posix_timer(const char *name, const YAML::Node& node);

        //! destrcution
        ~posix_timer();

        //! set module state machine to defined state
        /*!
          \param state requested state
          \return success or failure
          */
        int set_state(module_state_t state);

        //! send a request to module
        /*! 
          \param reqcode request code
          \param ptr pointer to request structure
          \return success or failure
          */
        int request(int reqcode, void* ptr);

        //! handler function called if thread is running
        void run();

        //! handler function for nanosleep mode
        void run_nanosleep();

        //! handler function for timer mode
        void run_timer();

        pthread_mutex_t _sync_lock;    
        pthread_cond_t _sync_cond;
        struct sigaction _old; 
};

//! default construction
/*!
 * \param node yaml configuration node
 */
posix_timer::posix_timer(const char* name, const YAML::Node& node) 
    : runnable(node) {
    _name = string(name);    
    _interval = node["interval"].to<double>();
    _signo = SIGRTMIN;
    _timer_id = NULL;
    _mode = posix_timer_mode_timer;

    if (node.FindValue("mode")) {
        if (node["mode"].to<string>() == string("nanosleep"))
            _mode = posix_timer_mode_nanosleep;
        else if (node["mode"].to<string>() == string("timer"))
            _mode = posix_timer_mode_timer;
    } else 
        pt_log(_name, info, "mode not specified, assuming timer mode!\n");

    if (node.FindValue("signo"))
        _signo = node["signo"].to<int>();

    // set state to init
    _state = module_state_init;

    // create ipc structures
    pthread_mutex_init(&_sync_lock, NULL);
    pthread_cond_init(&_sync_cond, NULL);
};

//! destrcution
posix_timer::~posix_timer() {
    stop();

    pthread_mutex_destroy(&_sync_lock);
    pthread_cond_destroy(&_sync_cond);
}

//! handler function called if thread is running
void posix_timer::run() {
    if (_mode == posix_timer_mode_nanosleep)
        return run_nanosleep();

    return run_timer();
}

//! handler function for nanosleep mode
void posix_timer::run_nanosleep() {
    pt_log(_name, info, "nanosleep handler running with pid %d\n", getpid());


    struct timespec ts_now;
    clock_gettime(CLOCK_REALTIME, &ts_now);

    while (_running) {
        struct timespec ts = { 1, 0 }, ts_diff;
        timespec_add(&ts_now, (int)(_interval), (_interval - (int)_interval)*1E9);

        clock_gettime(CLOCK_REALTIME, &ts);
        ts_diff = timespec_sub(ts_now, ts);

        nanosleep(&ts_diff, NULL);

        trigger_modules();
    }

    pt_log(_name, info, "nanosleep handler stopped\n");
}

//! handler function for timer mode
void posix_timer::run_timer() {
    pt_log(_name, info, "timer handler running with pid %d\n", getpid());

    sigset_t set;
    if (sigemptyset (&set) == -1)
        perror ("sigemptyset");

    if (sigaddset (&set, _signo) == -1)
        perror ("sigaddset");

    /* set up timer to send out signal */
    struct sigevent se;
    memset(&se, 0, sizeof(se));
    se.sigev_notify = SIGEV_SIGNAL;
    se.sigev_signo = _signo;

    if (timer_create(CLOCK_REALTIME, &se, &_timer_id) == -1) {
        pt_log(_name, error, "ERROR timer_create: %s\n",
                strerror(errno));
    }

    struct itimerspec value, value_old; 
    value.it_value.tv_sec = (int)(_interval);
    value.it_value.tv_nsec = (_interval-value.it_value.tv_sec)*1E9;
    value.it_interval.tv_sec = value.it_value.tv_sec;
    value.it_interval.tv_nsec = value.it_value.tv_nsec;

    if (timer_settime(_timer_id, 0, &value, &value_old) == -1) {
        pt_log(_name, error, "ERROR timer_settime: %s\n",
                strerror(errno));
    }

    while (_running) {
        struct timespec ts = { 1, 0 };
        siginfo_t si;

        int ret = sigtimedwait(&set, &si, &ts);

        if (ret == -1) {
            if (errno == EAGAIN)
                klog(info, "sigtimedwait timed out\n");
            if (errno == EINVAL)
                klog(info, "sigtimedwait einval\n");
            continue;
        }

        trigger_modules();
    }

    if (_timer_id) {
        struct itimerspec value; 
        value.it_value.tv_sec = 0;
        value.it_value.tv_nsec = 0;
        value.it_interval.tv_sec = 0;
        value.it_interval.tv_nsec = 0;

        if (timer_settime(_timer_id, 0, &value, NULL) == -1) {
            pt_log(_name, error, "ERROR timer_settime: %s\n",
                    strerror(errno));
        }

        timer_delete(_timer_id);
    }

    pt_log(_name, info, "timer handler stopped\n");
}

//! set module state machine to defined state
/*!
  \param state requested state
  \return success or failure
  */
int posix_timer::set_state(module_state_t state) {
    if (state == _state) {
        return 0;
    }

    pt_log(_name, info, "state %s requested\n", state_to_string(state));

    switch (state) {
        case module_state_init:
        case module_state_preop:
        case module_state_safeop:
            stop();
            break;
        case module_state_op: {
            if (_state < module_state_safeop)
                // invalid state transition
                return -1;

            start();
            break;
        }
        default:
            // invalid state 
            return -1;
    }

    // set actual state 
    _state = state;

    pt_log(_name, info, "state %s reached\n", state_to_string(_state));

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

    switch (reqcode) {
        case MOD_REQUEST_SET_TRIGGER_CB: {
            set_trigger_cb_t *cb = (set_trigger_cb_t *)ptr;
            add_trigger_module(*cb);
            break;
        }
        case MOD_REQUEST_UNSET_TRIGGER_CB: {
            set_trigger_cb_t *cb = (set_trigger_cb_t *)ptr;
            remove_trigger_module(*cb);
            break;
        }
        default:
            pt_log(_name, verbose, "not implemented request %d\n", reqcode);
            ret = -1;
            break;
    }

    return ret;
}


#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

//! configures module
/*!
 * \param name name of posix timer instance
 * \param config configure string
 * \return handle on success, NULL otherwise
 */
MODULE_HANDLE mod_configure(const char* name, const char* config) {
    pt_log(name, info, "build by: " BUILD_USER "@" BUILD_HOST "\n");
    pt_log(name, info, "build date: " BUILD_DATE "\n");

    posix_timer *t = NULL;
    stringstream stream(config);
    YAML::Parser parser(stream);
    YAML::Node doc;

    // parse yaml configuration string
    if (!parser.GetNextDocument(doc)) {
        pt_log(name, error, "ERROR parsing config file\n");
        goto ErrorExit;
    }

    t = new posix_timer(name, doc);
    if (!t) {
        pt_log(name, error, "ERROR cannot allocate memory");
        goto ErrorExit;
    }

    pt_log(name, info, "configured signo %d, interval %f\n", 
            t->_signo, t->_interval);

    return (MODULE_HANDLE)t;

ErrorExit:
    // delete already allocated structures
    if (t) {
        delete t;
    }

    return (MODULE_HANDLE)NULL;
}

//! unconfigure module
/*!
 * \param hdl module handle
 * \return success or failure
 */
int mod_unconfigure(MODULE_HANDLE hdl) {
    // cast struct
    posix_timer* t = (posix_timer*)hdl;
    if (t)
        delete t;

    return 0;
}

//! set module state machine to defined state
/*!
 * \param hdl module handle
 * \param state requested state
 * \return success or failure
 */
int mod_set_state(MODULE_HANDLE hdl, module_state_t state) {
    // cast struct
    posix_timer* t = (posix_timer*)hdl;
    return t->set_state(state);
}

//! get module state machine state
/*!
 * \param hdl module handle
 * \return current state
 */
module_state_t mod_get_state(MODULE_HANDLE hdl) {
    // cast struct
    posix_timer* t = (posix_timer*)hdl;
    return t->_state;
}

//! send a request to module
/*! 
 * \param hdl module handle
 * \param reqcode request code
 * \param ptr pointer to request structure
 * \return success or failure
 */
int mod_request(MODULE_HANDLE hdl, int reqcode, void* ptr) {
    // cast struct
    posix_timer* t = (posix_timer*)hdl;
    return t->request(reqcode, ptr);
}

#ifdef __cplusplus
}
#endif

