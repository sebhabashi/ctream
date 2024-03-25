                                                 
//  ,-----.  ,--.                                       Ctream   
// '  .--./,-'  '-.,--.--. ,---.  ,--,--.,--,--,--.     STL container streams
// |  |    '-.  .-'|  .--'| .-. :' ,-.  ||        |     Version 1.0.0
// '  '--'\  |  |  |  |   \   --.\ '-'  ||  |  |  |     
//  `-----'  `--'  `--'    `----' `--`--'`--`--`--'     https://github.com/sebhabashi/ctream
//
//
// This file is part of the Ctream distribution (https://github.com/sebhabashi/ctream).
// Copyright (c) 2024 Sebastien Habashi.
// 
// This program is free software: you can redistribute it and/or modify  
// it under the terms of the GNU General Public License as published by  
// the Free Software Foundation, version 3.
//  
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of 
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
// General Public License for more details.
// 
// You should have received a copy of the GNU General Public License 
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#define CTREAM_VERSION_MAJOR 1
#define CTREAM_VERSION_MINOR 0
#define CTREAM_VERSION_PATCH 0

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>
#include <sstream>
#include <string>
#include <thread>

#include <iostream>

namespace ctream
{

namespace internal
{

// Optional value template: implemented to stay C++11-friendly
template<typename T>
struct Optional
{
    T val;
    bool set;

    Optional() : val{T()}, set{false} {}
    Optional(const T& t) : val{t}, set{true} {}
    operator bool() const { return set; }
    bool operator !() const { return !set; }
    T& value() { return val; }
    const T& value() const { return val; }
};

template<typename>
class Ctream;

namespace fine_tuning
{

#ifndef CTREAM_MULTITHREAD_MIN_SIZE
#define CTREAM_MULTITHREAD_MIN_SIZE 800
#endif
constexpr size_t MULTITHREAD_MIN_SIZE = CTREAM_MULTITHREAD_MIN_SIZE;

constexpr double THREADS_PER_CORE = 2;

#ifndef CTREAM_PAGE_SIZE
#define CTREAM_PAGE_SIZE (1 << 16)
#endif
constexpr size_t PAGE_SIZE = CTREAM_PAGE_SIZE;

} // namespace fine_tuning

} // namespace internal

namespace collectors {

/**
 * @defgroup collectors Collector API
 * @details
 *  `Collector<T = {input type}, A = {accumulator type}, R = {result type}>`
 * 
 * Collectors are objects that contain information on how to export the
 * data from the streams into usable output data. Their aim is to have a
 * simple syntax, and to be parallelized easily.
 * 
 * The logic behind collectors is to `accumulate()` the elements of type T of
 * the stream in to partial results of type A. These partial results are then
 * combined into a complete result of type A with function `combine()`. This
 * complete result is finally converted into the final output result of type R
 * by function `finish()`.
 * 
 * Using a different type for A and R is not always necessary, but it can
 * sometimes be very useful. For instance, to concatenate strings, a solution
 * to avoid copying strings endlessly (the one used in this library) is to use
 * stringstream objects as accumulators, and then finish by extracting the
 * string from the stringstream.
 * @{
 */

/**
 * @brief A class that outputs data from the stream
 * 
 * @tparam T Type of the input elements
 * @tparam A Accumulator type
 * @tparam R Output type
 */
template<typename T, typename A, typename R>
class Collector
{
public:
    using InputType = T;
    using AccumulatorType = A;
    using ReturnType = R;

    /**
     * @brief A function that provides a blank accumulator (an accumulator in
     * the expected starting state)
     * 
     * @return AccumulatorType A blank accumulator
     */
    virtual AccumulatorType supply() const = 0;

    /**
     * @brief Feed an element to an accumulator
     * 
     * @param a Accumulator that will receive the element
     * @param b Element that will be fed the accumulator
     */
    virtual void accumulate(AccumulatorType& a, const InputType& b) const = 0;

    /**
     * @brief Feed all the content of an accumulator to another.
     * 
     * @details
     * Implementations can be destructive the the source accumulator (b) without
     * risk, because b will never be used again. For large data types, it is
     * often useful to do a move of the data from b into a.
     * 
     * @param a Destination accumulator
     * @param b Source accumulator
     */
    virtual void combine(AccumulatorType& a, AccumulatorType& b) const = 0;

    /**
     * @brief Convert the final content of the accumulator into the output
     * 
     * @param a Final content of the accumulator
     * @return ReturnType Output of the collector
     */
    virtual ReturnType finish(AccumulatorType& a) const = 0;
};

/**
 * @brief Get the sum of the elements of the stream
 * 
 */
template<typename T>
class Sum : public Collector<T, T, T>
{
public:
    T supply() const override { return 0; }
    void accumulate(T& a, const T& b) const override { a += b; };
    void combine(T& a, T& b) const override { a += b; };
    T finish(T& a) const override { return a; }
};

/**
 * @brief Get the product of the elements of the stream
 * 
 */
template<typename T>
class Product : public Collector<T, T, T>
{
public:
    T supply() const override { return T{1}; }
    void accumulate(T& a, const T& b) const override { a *= b; };
    void combine(T& a, T& b) const override { a *= b; };
    T finish(T& a) const override { return a; }
};

/**
 * @brief Get the minimum of the elements of the stream (0 if empty)
 * 
 */
template<typename T>
class Min : public Collector<T, internal::Optional<T>, T>
{
public:
    Min() : m_comp{[] (const T& a, const T& b) { return a < b; }}
    {
    }
    Min(const std::function<bool(const T&, const T&)> comp)
            : m_comp{comp}
    {
    }
    internal::Optional<T> supply() const override
    {
        return internal::Optional<T>{};
    }
    void accumulate(internal::Optional<T>& a, const T& b) const override
    {
        if (!a || m_comp(b, a.value()))
            a = b;
    };
    void combine(internal::Optional<T>& a, internal::Optional<T>& b) const override
    {
        if (!a || (b && m_comp(b.value(), a.value())))
            a = b;
    };
    T finish(internal::Optional<T>& a) const override
    {
        if (a)
            return a.value();
        return T{};
    }

private:
    std::function<bool(const T&, const T&)> m_comp;
};

/**
 * @brief Get the maximum of the elements of the stream (0 if empty)
 * 
 */
template<typename T>
class Max : public Collector<T, internal::Optional<T>, T>
{
public:
    Max() : m_comp{[] (const T& a, const T& b) { return a < b; }}
    {
    }
    Max(const std::function<bool(const T&, const T&)> comp)
            : m_comp{comp}
    {
    }
    internal::Optional<T> supply() const override
    {
        return internal::Optional<T>{};
    }
    void accumulate(internal::Optional<T>& a, const T& b) const override
    {
        if (!a || m_comp(a.value(), b))
            a = b;
    };
    void combine(internal::Optional<T>& a, internal::Optional<T>& b) const override
    {
        if (!a || (b && m_comp(a.value(), b.value())))
            a = b;
    };
    T finish(internal::Optional<T>& a) const override
    {
        if (a)
            return a.value();
        return T{};
    }

private:
    std::function<bool(const T&, const T&)> m_comp;
};

/**
 * @brief Get a strings where all the elements of the stream are concatenated
 * 
 */
template<typename T>
class Concat : public Collector<T, std::stringstream, std::string>
{
public:
    std::stringstream supply() const override
    {
        return std::stringstream{};
    }
    void accumulate(std::stringstream& a, const T& b) const override
    {
        a << b;
    }
    void combine(std::stringstream& a, std::stringstream& b) const override
    {
        a << b.str();
    }
    std::string finish(std::stringstream& a) const override
    {
        return a.str();
    }
};

/**
 * @brief Get a list containing the elements of the stream
 * 
 */
template<typename T>
class ToList : public Collector<T, std::list<T>, std::list<T>>
{
public:
    std::list<T> supply() const override
    {
        return std::list<T>{};
    }
    void accumulate(std::list<T>& a, const T& b) const override
    {
        a.emplace_back(b);
    }
    void combine(std::list<T>& a, std::list<T>& b) const override
    {
        a.splice(a.end(), std::move(b));
    }
    std::list<T> finish(std::list<T>& a) const override
    {
        return std::move(a);
    }
};

/**
 * @brief Get a vector containing the elements of the stream
 * 
 */
template<typename T>
class ToVector : public Collector<T, std::vector<T>, std::vector<T>>
{
public:
    ToVector(size_t chunkSize = 0) : m_chunkSize{chunkSize} {}
    
    std::vector<T> supply() const override
    {
        std::vector<T> vec;
        if (m_chunkSize)
            vec.reserve(m_chunkSize);
        return vec;
    }
    void accumulate(std::vector<T>& a, const T& b) const override
    {
        a.emplace_back(b);
    }
    void combine(std::vector<T>& a, std::vector<T>& b) const override
    {
        a.insert(a.end(),
                 std::make_move_iterator(b.begin()),
                 std::make_move_iterator(b.end()));
        b.erase(b.begin(), b.end());
    }
    std::vector<T> finish(std::vector<T>& a) const override
    {
        return std::move(a);
    }
private:
    size_t m_chunkSize{0};
};

/**
 * @brief Create a custom Collector by specifying all functions implementations
 * 
 */
template<typename T, typename A, typename R>
class Custom : public Collector<T, A, R>
{
public:
    using InputType = T;
    using AccumulatorType = A;
    using ReturnType = R;

    /**
     * @brief Construct a Custom Collector (see @ref{Collector} documentation
     * for more information)
     * 
     * @param supplier Supplier function
     * @param accumulator Accumulator function
     * @param combiner Combiner function
     * @param finisher Finisher function
     */
    Custom(const std::function<AccumulatorType()>& supplier,
           const std::function<void(AccumulatorType&, const InputType&)>& accumulator,
           const std::function<void(AccumulatorType&, const AccumulatorType&)>& combiner,
           const std::function<ReturnType(const AccumulatorType&)>& finisher)
            : m_supplier{supplier}
            , m_accumulator{accumulator}
            , m_combiner{combiner}
            , m_finisher{finisher}
    {
    }
    AccumulatorType supply() const
    {
        return m_supplier();
    }
    void accumulate(AccumulatorType& a, const InputType& b) const
    {
        m_accumulator(a, b);
    }
    void combine(AccumulatorType& a, AccumulatorType& b) const
    {
        m_combiner(a, b);
    }
    ReturnType finish(AccumulatorType& a) const
    {
        return m_finisher(a);
    }
private:
    std::function<AccumulatorType()> m_supplier;
    std::function<void(AccumulatorType&, const InputType&)> m_accumulator;
    std::function<void(AccumulatorType&, const AccumulatorType&)> m_combiner;
    std::function<ReturnType(const AccumulatorType&)> m_finisher;
};

/** @} */ // end of collectors

} // namespace collectors

namespace internal
{

template<typename U>
void voidDeleterUseWithCaution(void* u)
{
    // Call the destructor but does not de-allocate the memory
    reinterpret_cast<U*>(u)->~U();
}
using VoidDeleterType = void(void*);

template<typename T>
class BasicArena
{
public:
    BasicArena() noexcept
    {
    }

    ~BasicArena()
    {
        // Destroy all the objects
        std::lock_guard<std::mutex> objLk{m_objectsMx};
        for (auto& ptr : m_objects)
            ptr.delFct(ptr.ptr);

        // Free the memory
        std::lock_guard<std::mutex> lk{m_pagesListMx};
        for (auto& page : m_pages)
        {
            std::lock_guard<std::mutex> pageLk{page.mx};
            freePage(page);
        }
    }

    void* allocate(size_t size) noexcept
    {
        // This is the only way to ensure page is protected exactly the right
        // amount of time
        // It cannot interlock because no thread has to wait for
        // m_pagesListMx to unlock page.mx

        // Access/create a page with enough space 
        m_pagesListMx.lock();
        auto& page = firstAvailablePage(size);
        page.mx.lock();
        m_pagesListMx.unlock();

        // Allocate on the page
        void* data = (void*) (size_t(page.data) + page.cursor);
        page.cursor += size;
        page.mx.unlock();

        return data;
    }

    template<typename U, typename... Args>
    U* construct(Args&&... args)
    {
        void* data = allocate(sizeof(U));
        auto* obj = new (data) U(std::forward<Args>(args)...);
        
        {
            std::lock_guard<std::mutex> lk{m_objectsMx};
            m_objects.emplace_back();
            auto& ptr = m_objects.back();
            ptr.ptr = data;
            ptr.delFct = voidDeleterUseWithCaution<U>;
        }

        return obj;
    }

private:
    struct ArenaPtr
    {
        void* ptr{nullptr};
        VoidDeleterType* delFct{nullptr};
    };
    struct Page
    {
        void* data{nullptr};
        size_t cursor{0};
        size_t size{0};
        std::mutex mx{};
    };

    std::list<Page> m_pages{};
    std::mutex m_pagesListMx{};

    std::vector<ArenaPtr> m_objects{};
    std::mutex m_objectsMx{};

    static void initPage(size_t minSize, Page& out) noexcept
    {
        out.size = (minSize > fine_tuning::PAGE_SIZE)
                ? minSize
                : fine_tuning::PAGE_SIZE;
        out.cursor = 0;
        out.data = malloc(out.size);
    }

    static void freePage(Page& d) noexcept
    {
        free(d.data);
    }

    inline static long occupiedSizeOnPage(const Page& p) noexcept
    {
        return long(p.cursor);
    }

    inline static long availableSizeOnPage(const Page& p) noexcept
    {
        return long(p.size) - occupiedSizeOnPage(p);
    }

    Page& firstAvailablePage(size_t size) noexcept
    {
        for (auto& p : m_pages)
        {
            if (availableSizeOnPage(p) > size)
                return p;
        }
        m_pages.emplace_back();
        auto& newPage = m_pages.back();
        initPage(size, newPage);
        return newPage;
    }
};
using Arena = BasicArena<void>;


/**
 * @defgroup ctream Ctream API
 * @details
 * This group contains templated class Ctream which is the entrypoint of this
 * library.
 * 
 * Ctream provides a clear syntax and parallelized implementation for inline
 * data manipulation. Ctream objects can be created using diverse STL
 * containers as source, then modified through data mapping extraction and
 * filtering, and then exported using @ref{Collector} objects into usable
 * types.
 * 
 * @{
 */

/**
 * @brief Pipeline for data manipulation
 * 
 * @tparam T Type of the streamed elements
 */
template<typename T>
class Ctream
{
public:
    using SourceDataRetriever = std::function<void const*(size_t)>;
    using PipelineStep = std::function<void const*(void const*)>;

    /**
     * @name Create from STL containers
     * @{
     */

    /**
     * @brief Construct from a vector
     * 
     * @param values Vector containing the values to stream
     */
    Ctream(const std::vector<T>& values)
            : m_sourceData{[&values] (size_t i) { return &values.at(i); }}
            , m_containerSize{values.size()}
    {
    }

    /**
     * @brief Construct from a list
     * 
     * @param values List containing the values to stream
     */
    Ctream(const std::list<T>& values)
            : m_sourceData{[this] (size_t i) { return m_elementsWithIndex.at(i); }}
            , m_containerSize{values.size()}
            , m_elementsWithIndex{ m_arena->construct<std::vector<const T*>>() }
    {
        // List does not provide random access so we have to store it here
        m_elementsWithIndex->reserve(values.size());
        for (const auto& v : values)
            m_elementsWithIndex->emplace_back(&v);
    }

    /**
     * @brief Construct from a C array
     * 
     * @param values Array containing the values to stream
     * @param size Number of elements in the array
     */
    Ctream(const T* values, size_t size)
            : m_sourceData{[values] (size_t i) { return &values[i]; }}
            , m_containerSize{size}
    {
    }

    /** @} */

    // Internal constructor please do not use
    Ctream(std::shared_ptr<Arena> arena,
           size_t containerSize,
           const SourceDataRetriever& sourceData,
           const std::vector<PipelineStep>& previousPipeline,
           const PipelineStep& newPipelineStep)
            : m_arena{arena}
            , m_sourceData{sourceData}
            , m_containerSize{containerSize}
            , m_pipeline{previousPipeline}
    {
        m_pipeline.emplace_back(newPipelineStep);
    }

    /**
     * @name Manipulate the data in the stream
     * @{
     */

    /**
     * @brief Remove from the stream the elements that do not match a filter
     * 
     * @param filter A condition on a stream element. The element remains in
     * the stream if and only if it matches the filter
     * @return Ctream<T> A stream with only the filtered data
     */
    Ctream<T> filter(const std::function<bool(const T&)> filter) const
    {
        PipelineStep newPipelineStep = [this, &filter] (const void* elt)
        {
            if (filter(*reinterpret_cast<const T*>(elt)))
                return elt;
            return (void const*)(0);
        };
        return Ctream<T>(m_arena, m_containerSize, m_sourceData, m_pipeline,
                         newPipelineStep);
    }

    /**
     * @brief Extract data from the elements of the stream.
     * 
     * @details
     * When the data can be extracted by reference from the elements, this is
     * better than @ref{map} because is does not copy and construct new elements
     * 
     * @tparam U The type of the extracted data
     * @param extractor A function that, given an element of the stream,
     * returns a const reference to type U data
     * @return Ctream<U> A stream with the extracted data
     */
    template<typename U>
    Ctream<U> extract(const std::function<const U&(const T&)>& extractor) const
    {
        PipelineStep newPipelineStep = [this, &extractor] (const void* elt)
        {
            return &extractor(*reinterpret_cast<const T*>(elt));
        };
        return Ctream<U>(m_arena, m_containerSize, m_sourceData, m_pipeline,
                         newPipelineStep);
    }

    /**
     * @brief Transform the data from the elements in the stream.
     * 
     * @details
     * If the mapper you want to use can return a const reference, it is
     * preferable to use @ref{extract} instead to avoid overhead
     * 
     * @tparam U The type of the transformed data
     * @param mapper A function that, given an element of the stream,
     * returns an element of type U
     * @return Ctream<U> A stream with the transformed data
     */
    template<typename U>
    Ctream<U> map(const std::function<U(const T&)>& mapper) const
    {
        PipelineStep newPipelineStep = [this, &mapper] (const void* elt)
        {
            return m_arena->construct<U>(mapper(*reinterpret_cast<const T*>(elt)));
        };
        return Ctream<U>(m_arena, m_containerSize, m_sourceData, m_pipeline,
                         newPipelineStep);
    }

    /**
     * @brief Cast the elements of the stream into a new type by construction.
     * 
     * @tparam U The type of the transformed data
     * @return Ctream<U> A stream with the transformed data
     */
    template<typename U>
    Ctream<U> map() const
    {
        PipelineStep newPipelineStep = [this] (const void* elt)
        {
            return m_arena->construct<U>(*reinterpret_cast<const T*>(elt));
        };
        return Ctream<U>(m_arena, m_containerSize, m_sourceData, m_pipeline,
                         newPipelineStep);
    }

    /** @} */

    /**
     * @name Output the streamed data
     * @{
     */

    /**
     * @brief Use a Collector to extract usable data from the stream
     * 
     * @tparam A Type of the collector's accumulator (see @ref{Collector} for
     * more info)
     * @tparam R Output type
     * @param collector Collector
     * @return R Output data
     */
    template<typename A, typename R = A>
    R collect(const collectors::Collector<T, A, R>& collector) const
    {
        constexpr size_t MULTITHREAD_MIN_SIZE =
                internal::fine_tuning::MULTITHREAD_MIN_SIZE;
        constexpr size_t THREADS_PER_CORE =
                internal::fine_tuning::THREADS_PER_CORE;

        const size_t nThreads = std::min(
            1 + (m_containerSize) / MULTITHREAD_MIN_SIZE,
            size_t(THREADS_PER_CORE * std::thread::hardware_concurrency()));

        if (nThreads < 2)
        {
            // If only 1 thread is used, do directly in current thread
            A a = collector.supply();
            for (size_t i = 0; i < m_containerSize; ++i)
            {
                // Compute the i'th item
                const T* item = computeItem(i);
                if (item)
                    collector.accumulate(a, *item);
            }
            return collector.finish(a);
        }
        else
        {
            // Initialize empty containers
            std::vector<A> chunks;
            std::vector<std::thread> threads;
            chunks.reserve(nThreads);
            threads.reserve(nThreads);

            // Accumulate values in separate chunks
            size_t first = 0;
            for (size_t i = 0; i < nThreads; ++i)
            {
                chunks.emplace_back(collector.supply());
                auto& chunk = chunks.back();

                const size_t last = (i == nThreads - 1)
                        ? (m_containerSize)
                        : (first + m_containerSize / nThreads);

                threads.emplace_back(
                        [this, first, last, &chunk, &collector] ()
                {
                    for (size_t j = first; j < last; ++j)
                    {
                        // Collect item (if not filtered out)
                        const T* item = computeItem(j);
                        if (item)
                            collector.accumulate(chunk, *item);
                    }
                });

                // Next thread picks up where this thread left
                first = last;
            }

            // Wait for all chunks to be ready
            for (auto& thread : threads)
                if (thread.joinable())
                    thread.join();

            // Combine all chunks
            A a = collector.supply();
            for (auto& chunk : chunks)
                collector.combine(a, chunk);

            return collector.finish(a);
        }
    }

    /**
     * @brief Get the sum of the elements in the stream
     * 
     * @details
     * Only for elements that are default-constructible and implement addition
     * For strings, use @ref{concat} instead for better performance
     * 
     * @return T Sum of the elements
     */
    T sum() const
    {
        return collect(collectors::Sum<T>{});
    }

    /**
     * @brief Get the minimum of the elements in the stream
     * 
     * @return T Minimum of the elements
     */
    T min() const
    {
        return collect(collectors::Min<T>{});
    }

    /**
     * @brief Get the maximum of the elements in the stream
     * 
     * @return T Maximum of the elements
     */
    T max() const
    {
        return collect(collectors::Max<T>{});
    }

    /**
     * @brief Get the product of the elements in the stream
     * 
     * @details
     * Only for elements that are constructible from `1` and implement product
     * 
     * @return T Product of the elements
     */
    T product() const
    {
        return collect(collectors::Product<T>{});
    }

    /**
     * @brief Get a string that concatenates all elements in the stream
     * 
     * @details
     * Only for elements that can be fed to an ostream
     * 
     * @return std::string Concatenated string
     */
    std::string concat() const
    {
        return collect(collectors::Concat<T>{});
    }

    /**
     * @brief Get a list containing the elements in the stream
     * 
     * @return std::list<T> Output list
     */
    std::list<T> toList() const
    {
        return collect(collectors::ToList<T>{});
    }

    /**
     * @brief Get a vector containing the elements in the stream
     * 
     * @return std::vector<T> Output vector
     */
    std::vector<T> toVector() const
    {
        const size_t estimatedNThreads = internal::fine_tuning::THREADS_PER_CORE
                * std::thread::hardware_concurrency();
        const size_t estimatedChunkSize = m_containerSize / estimatedNThreads;
        return collect(collectors::ToVector<T>{estimatedChunkSize});
    }

    /** @} */

private:
    // Allow other Ctream<...> classes to access private members
    template<typename U>
    friend class Ctream;

    /// An shared arena to store the data that has to be constructed
    /// Shared with all previous and next Ctreams in the pipeline
    std::shared_ptr<Arena> m_arena{ new Arena };

    /// A functor that allows to access item i in the source, or nullptr if it
    /// was filtered out
    SourceDataRetriever m_sourceData{};

    std::vector<PipelineStep> m_pipeline{};

    /// Size of the source container. This is all the elements still in the
    /// pipeline AND the elements that were filtered out. Calling m_sourceData
    /// on i is valid if and only if i is within [0, m_containerSize - 1]
    size_t m_containerSize{0};

    /// Only if source container does not handle random access
    /// This vector stored in the arena contains a pointer to each element by 
    /// index
    std::vector<const T*>* m_elementsWithIndex{nullptr};

    /// Compute the element of the stream at position i (or nullptr if it is
    /// filtered out)
    const T* computeItem(size_t i) const
    {
        void const* item = m_sourceData(i);
        for (const auto& step : m_pipeline)
        {
            item = step(item);
            if (!item)
                break;
        }
        return reinterpret_cast<const T*>(item);
    }
};

/** @} */ // end group ctream

} // namespace internal

/**
 * @brief Stream a vector
 * @ingroup ctream
 * 
 * @param values Vector containing the values to stream
 */
template<typename T>
internal::Ctream<T> toCtream(const std::vector<T>& vec)
{
    return internal::Ctream<T>(vec);
}

/**
 * @brief Stream a list
 * @ingroup ctream
 * 
 * @param values List containing the values to stream
 */
template<typename T>
internal::Ctream<T> toCtream(const std::list<T>& list)
{
    return internal::Ctream<T>(list);
}

/**
 * @brief Stream a C array
 * @ingroup ctream
 * 
 * @param values Array containing the values to stream
 * @param size Number of elements in the array
 */
template<typename T>
internal::Ctream<T> toCtream(const T* array, size_t size)
{
    return internal::Ctream<T>(array, size);
}

} // namespace ctream