/**
 * @file    dynamic_omp_runner.hpp
 * @ingroup
 * @author  tpan
 * @brief
 * @details
 *
 * Copyright (c) 2014 Georgia Institute of Technology.  All Rights Reserved.
 *
 * TODO add License
 */
#ifndef DYNAMIC_OMP_RUNNER_HPP_
#define DYNAMIC_OMP_RUNNER_HPP_

#include "omp.h"
#include <cassert>

#include "config.hpp"
#include "concurrent/lockfree_queue.hpp"

#include "taskrunner/runner.hpp"


namespace bliss
{
namespace concurrent
{

/**
 * @class      bliss::concurrent::DynamicOMPRunner
 * @brief       OpenMP task runner, processing a queue of tasks until queue is done (blocked)
 * @details     used for continuously changing queue, threads process tasks in demand driven way.
 *              also use for one thread per task
 *
 */
class DynamicOMPRunner : public Runner
{
  protected:
    bliss::concurrent::ThreadSafeQueue<std::shared_ptr<Runnable> > q;

    const int nThreads;

  public:
    DynamicOMPRunner(const int num_threads) : Runner(), nThreads(num_threads)
    {
#ifdef USE_OPENMP
      if (omp_get_nested() <= 0) omp_set_nested(1);
      if (omp_get_dynamic() <= 0) omp_set_dynamic(1);
#else
      static_assert(false, "OMPRunner Used When compilation is not set to use OpenMP");
#endif
    }


    virtual ~DynamicOMPRunner() {};

    void operator()()
    {
      size_t *proc = new size_t[nThreads];
#pragma omp parallel num_threads(nThreads) default(none) shared(proc)
      {
#pragma omp single nowait
        {  // one thread to do this
//#pragma omp task untied default(none)                     // this slows it down by ORDERS OF MAGNITUDE.  DO NOT USE
//          {  // untied so can move to different threads
            size_t gen = 0;
            while (q.canPop())  // flush the queue now.
            {
              ++gen;
#pragma omp task default(none) shared(proc)
              {
                auto v = std::move(q.waitAndPop());
                if (v.first) {
                  (v.second)->operator()();
                  ++(proc[omp_get_thread_num()]);
                }
              } // omp task
            } // while

            INFOF("tid %d done generating %lu tasks.\n", omp_get_thread_num(), gen);
//          } // omp task untied.
        } // omp single
      } // omp parallel

      size_t sum = 0;
      for (int i = 0; i < nThreads; ++i) {
        sum += proc[i];
      }

      INFOF("Dynamic runner completed %lu tasks.\n", sum);

      delete [] proc;
    }  // operator()



    virtual bool addTask(std::shared_ptr<Runnable> &&t)
    {
      DEBUGF("add to Dynamic runner.  size %lu, disabled %s\n", q.getSize(), (q.canPush() ? "n" : "y"));
      return q.waitAndPush(std::forward<std::shared_ptr<Runnable> >(t)).first;
    }

    virtual size_t getTaskCount() {
      return q.getSize();
    }

    virtual bool isAddDisabled() {
      return !(q.canPush());
    }

    /// flush currently queued tasks and disallow further new tasks
    virtual void disableAdd() {
      q.disablePush();
    };

    virtual void synchronize()
    {
#pragma omp barrier
    }

};

} /* namespace concurrent */
} /* namespace bliss */

#endif /* DYNAMIC_OMP_RUNNER_HPP_ */
