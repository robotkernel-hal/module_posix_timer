//! ÜBER-control Module posix_timer
/*!
 * $Id$
 */

#include "modules/posix_timer/module_posix_timer.h"
#include "kernel.h"
#include "runnable.h"
#include "trigger_base.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>

#include "yaml-cpp/yaml.h"

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
    bool _direct_mode;
    timer_t _timer_id; 

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

    //! signal handler
    static void timer_handler(int signum, siginfo_t *si, void *uc) { 
        posix_timer *t = (posix_timer *)si->si_value.sival_ptr;

        if (signum == t->_signo) {
            if (t->_direct_mode)
                t->trigger_modules();
            else
                pthread_cond_broadcast(&t->_sync_cond);
        }
    } 
        
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
    _signo = node["signo"].to<int>();
    _direct_mode = true;
    _timer_id = NULL;

    if (node.FindValue("direct_mode"))
        _direct_mode = node["direct_mode"].to<bool>();
    else
        pt_log(_name, info, "direct_mode not specified, assuming true!\n");

    // set state to init
    _state = module_state_init;

    // create ipc structures
    pthread_mutex_init(&_sync_lock, NULL);
    pthread_cond_init(&_sync_cond, NULL);
};

//! destrcution
posix_timer::~posix_timer() {
    if (!_direct_mode)
        // stop running thread
        stop();

    pthread_mutex_destroy(&_sync_lock);
    pthread_cond_destroy(&_sync_cond);
}

//! handler function called if thread is running
void posix_timer::run() {
    pt_log(_name, info, "handler running with pid %d\n", pthread_self());

    while (_running) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        pthread_mutex_lock(&_sync_lock);
        int ret = pthread_cond_timedwait(&_sync_cond, &_sync_lock, &ts);

        if (ret == -1) {
            pthread_mutex_unlock(&_sync_lock);
            continue;
        }

        for (cb_list_t::iterator it = trigger_cbs.begin();
                it != trigger_cbs.end(); ++it) {
            it->cb(it->hdl);
        }
            
        pthread_mutex_unlock(&_sync_lock);
    }

    pt_log(_name, info, "handler stopped\n");
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
            if (!_direct_mode)
                stop();
    
            if (_timer_id) {
                sigaction(_signo, &_old, NULL);

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
            
            break;
        case module_state_op: {
            if (_state < module_state_safeop)
                // invalid state transition
                return -1;

            /* set up signal handler for timer signal */
            struct sigaction act; 
            sigfillset(&act.sa_mask); 
            act.sa_flags = SA_SIGINFO;
            act.sa_sigaction = timer_handler;
            sigaction(_signo, &act, &_old);

            /* set up timer to send out signal */
            struct sigevent se;
            memset(&se, 0, sizeof(se));
            se.sigev_notify = SIGEV_SIGNAL;
            se.sigev_signo = _signo;
            se.sigev_value.sival_ptr = this;

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

            if (!_direct_mode)
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
  \param hdl module handle
  \return success or failure
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
  \param hdl module handle
  \param state requested state
  \return success or failure
  */
int mod_set_state(MODULE_HANDLE hdl, module_state_t state) {
    // cast struct
    posix_timer* t = (posix_timer*)hdl;
    return t->set_state(state);
}

//! get module state machine state
/*!
  \param hdl module handle
  \return current state
  */
module_state_t mod_get_state(MODULE_HANDLE hdl) {
    // cast struct
    posix_timer* t = (posix_timer*)hdl;
    return t->_state;
}

//! send a request to module
/*! 
  \param hdl module handle
  \param reqcode request code
  \param ptr pointer to request structure
  \return success or failure
 */
int mod_request(MODULE_HANDLE hdl, int reqcode, void* ptr) {
    // cast struct
    posix_timer* t = (posix_timer*)hdl;
    return t->request(reqcode, ptr);
}

#ifdef __cplusplus
}
#endif

