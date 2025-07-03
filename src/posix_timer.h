//! robotkernel module posix timer
/*!
 * author: Robert Burger <robert.burger@dlr.de>
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

#ifndef MODULE_POSIX_TIMER_H
#define MODULE_POSIX_TIMER_H

#include "robotkernel/runnable.h"
#include "robotkernel/module_base.h"
#include "robotkernel/robotkernel.h"
#include "robotkernel/trigger_base.h"

#include "service_provider/process_data_inspection/base.h"

namespace module_posix_timer {

// forward declarations
class posix_timer;

class timer_base : 
    public std::enable_shared_from_this<timer_base>,
    public robotkernel::runnable,
    public robotkernel::trigger
{
    protected:
        enum {
            skip_none,
            skip_normal,
            skip_strict
        } skip_missed;                               //!< skip missed cycles
                                                     
        std::string name;
        std::shared_ptr<posix_timer> parent;
        
        robotkernel::sp_process_data_t pdin;         //!< named process data
        robotkernel::sp_pd_provider_t prov;
        service_provider::process_data_inspection::sp_pd_inspection_t pdin_inspect;

    public:
        //! Construction
        timer_base(std::shared_ptr<posix_timer> parent, const YAML::Node& config);

        //! Initialize timer
        void init(void);

        //! Deinitialize timer.
        void deinit(void);
        
        //! set rate of trigger device
        /*!
         * set the rate of the current trigger
         *
         * \param new_rate new trigger rate to set
         */
        virtual void set_rate(double new_rate) override { rate = new_rate; }
};

//! Handler to wait with nanosleep.
class nanosleep : public timer_base {
    public:
        nanosleep(std::shared_ptr<posix_timer> parent, const YAML::Node& config) : 
            timer_base(parent, config) {}

        //! handler function called if thread is running
        virtual void run() override;
};
 
//! Handler doing busywait (active wait on cpu).
class busywait : public timer_base {
    public:
        busywait(std::shared_ptr<posix_timer> parent, const YAML::Node& config) : 
            timer_base(parent, config) {}

        //! handler function called if thread is running
        virtual void run() override;
};

//! Handler for timer_create
class timer : public timer_base {
    private:
        int signo;                                   //!< signal number
        timer_t timer_id;                            //!< timer id

    public:
        timer(std::shared_ptr<posix_timer> parent, const YAML::Node& config) : 
            timer_base(parent, config) {}

        //! handler function called if thread is running
        virtual void run() override;
};

// forward declaration
class posix_timer : 
    public std::enable_shared_from_this<posix_timer>,
    public robotkernel::module_base
{
    private:
        YAML::Node config;
        std::list<std::shared_ptr<timer_base> > timers;

    private:
        posix_timer();                               //!< prevent default cons
        posix_timer(const posix_timer&);             //!< prevent copy-construction
        posix_timer& operator=(const posix_timer&);  //!< prevent assignment

    public:
        //! default construction
        /*!
         * \param node yaml configuration node
         */
        posix_timer(const char *name, const YAML::Node& node);

        //! destrcution
        ~posix_timer() {};

        //! additional module init stuff
        virtual void init() override;

        //*********************************************
        // STATE MACHINE FUNCTIONS
        //*********************************************

        //! State transition from PREOP to SAFEOP
        virtual void set_state_safeop_2_preop() override
        { for (const auto& t : timers) { t->stop(); }; }  

        //! State transition from PREOP to SAFEOP
        virtual void set_state_preop_2_init() override
        { for (const auto& t : timers) { t->deinit(); }; }  

        //! State transition from PREOP to SAFEOP
        virtual void set_state_init_2_preop() override
        { for (const auto& t : timers) { t->init(); }; }  

        //! State transition from PREOP to SAFEOP
        virtual void set_state_preop_2_safeop() override
        { for (const auto& t : timers) { t->start(); }; }  
};

};

#endif // MODULE_POSIX_TIMER_H

