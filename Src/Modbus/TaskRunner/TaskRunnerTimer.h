/******************************************************************************
 * Copyright ( C ) АО "Диаконт"
 * Project: Kernerl
 * File: TaskRunnerTimer.h
 * Creation: 22.05.2016, Пикулик С.
 * Description:
 *****************************************************************************/

#pragma once

#include "BaseTypes.h"

#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>

///////////////////////////////////////////////////////////////////////////////
namespace DkTools
{
///////////////////////////////////////////////////////////////////////////////

class TaskRunnerTimer
{
public:
    using clock_t = std::chrono::high_resolution_clock;
    using timep_point_t = clock_t::time_point;
    using duration_t = clock_t::duration;

    /** \brief Констркутор
     *  \param[in] threadName - Название потока (полезно при отладке).
     *  \param[in] manual - обработка запускается вызовом метода run()
     */
    explicit TaskRunnerTimer(const std::string& threadName, bool manual = false) : m_threadName(threadName)
    {
        if (!manual)
            run();
    }

    /** \brief Запустить цикл обработки очереди
     */
    void run()
    {
        if (m_thread.joinable())
            return;

        m_keepRunning = true;
        m_thread = std::thread(&TaskRunnerTimer::loop, this);
    }

    /** \brief Остановить работу цикла обработки очереди
     */
    void stop()
    {
        if (!m_thread.joinable())
            return;

        postQuitCommand();
        m_thread.join();
    }

    /** \brief Остановить работу цикла обработки очереди
     */
    void stopASAP()
    {
        if (!m_thread.joinable())
            return;

        quitASAP();
        m_thread.join();
    }

    /** \brief Очистить очередь с задачами
     */
    void clear()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_incomingQueue.clear();
    }

    /** \return true если поток с циклом обработки пока запущен
     */
    bool isWorking() const
    {   return m_thread.joinable();   }

    /** \brief Деструктор
     */
    ~TaskRunnerTimer()
    {   stop();   }

    /** \brief Отправить команду завершения цикла обработки как можно скорее
     *  Время через которое произойдет выход, в самом долгом случае определяется
     *  временем выполнения задач в m_processingQueue
     */
    void quitASAP()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_incomingQueue.push_front(std::bind(&TaskRunnerTimer::quit, this));
        m_notEmpty.notify_one();
    }

    /** \brief Отправить команду завершения цикла обработки, после выполнения
     *  задач уже добавленных в очередь.
     */
    void postQuitCommand()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_incomingQueue.push_back(std::bind(&TaskRunnerTimer::quit, this));
        m_notEmpty.notify_one();
    }

    /** \brief Поставить задачу в очередь
     *  \return true - если задача поставлена в очередь
     */
    bool postTask(Closure task)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_incomingQueue.push_back(task);
        m_notEmpty.notify_one();
        return true;
    }

    bool postDelayedTask(duration_t interval, Closure task)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        const auto nowTime = clock_t::now();
        m_delayedQueue.insert(std::make_pair(nowTime + interval, task));
        m_notEmpty.notify_one();
        return true;
    }

    /** \brief Вернуть количество задач в очереди
     */
    std::size_t queueSize() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return  m_processingQueue.size() + m_incomingQueue.size() + m_delayedQueue.size();
    }

    /** \brief Идентификатор потока на котором выполняются задачи.
     */
    std::thread::id threadId() const
    {   return m_thread.get_id();   }

    /** \brief Узнать, находимся ли мы сейчас в потоке выполнения задач.
     */
    bool isOnRunnerThread() const
    {   return std::this_thread::get_id() == m_thread.get_id();   }

private:
    // Не копируем, не перемещаем
    TaskRunnerTimer(const TaskRunnerTimer&) = delete;
    TaskRunnerTimer(TaskRunnerTimer&&) = delete;
    TaskRunnerTimer& operator =(const TaskRunnerTimer&) = delete;
    TaskRunnerTimer& operator =(TaskRunnerTimer&&) = delete;

    /** \brief Установить флаг завершения цикла обработки.
     *   Может быть вызвана только на потоке m_thread.
     */
    void quit()
    {   m_keepRunning = false;   }

    /** \brief Цикл обработки, запускается в потоке m_thread
     */
    void loop()
    {
        for (;;)
        {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                // Ждем сообщения о том что очередь не пуста
                std::cv_status status = std::cv_status::no_timeout;
                if (m_incomingQueue.empty())
                {
                    if (m_delayedQueue.empty())
                        m_notEmpty.wait(lock);
                    else if (m_delayedQueue.begin()->first > clock_t::now())
                        status = m_notEmpty.wait_until(lock, m_delayedQueue.begin()->first);
                }

                // Меняем очереди местами, чтобы минимизировать время блокировки mutex,
                // не блокируя очередь на каждый pop_front()
                m_processingQueue.swap(m_incomingQueue);

                auto itProcessing = m_processingQueue.begin();
                auto nowTime = clock_t::now();
                auto it = m_delayedQueue.begin();
                for (; it != m_delayedQueue.end(); ++it)
                {
                    if (it->first > nowTime)
                        break;
                    itProcessing = std::next(m_processingQueue.insert(itProcessing, it->second));
                }
                m_delayedQueue.erase(m_delayedQueue.begin(), it);
            }

            while (!m_processingQueue.empty())
            {
                Closure& task = m_processingQueue.front();
                task();
                m_processingQueue.pop_front();
                // Была комманда quite()
                if (!m_keepRunning)
                    return;
            }
        }
    }

    std::multimap<timep_point_t, Closure> m_delayedQueue; /**< Очередь входящих отложенных задач */

    std::list<Closure> m_incomingQueue;   /**< Очередь входящих задач */
    std::list<Closure> m_processingQueue; /**< Очередь обрабатываемых задач */
    bool m_keepRunning = true;            /**< Флаг продолжения обработки */
    mutable std::mutex m_mutex;           /**< Защита входящей очереди */
    std::condition_variable m_notEmpty;   /**< событие - очередь непуста */
    std::thread m_thread;                 /**< Поток обработки */
    std::string m_threadName;             /**< Название потока. */
}; // class TaskRunnerDefault

///////////////////////////////////////////////////////////////////////////////
} // namespace DkTools
///////////////////////////////////////////////////////////////////////////////
