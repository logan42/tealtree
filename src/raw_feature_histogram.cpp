#include "raw_feature_histogram.h"

#include <gheap.hpp>



inline RawFeatureHistogram::~RawFeatureHistogram() { }


template <typename T>
inline RawFeatureHistogramImpl<T>::RawFeatureHistogramImpl()
{
}

template <typename T>
inline RawFeatureHistogramImpl<T>::~RawFeatureHistogramImpl()
{
}


template <typename T>
void RawFeatureHistogramImpl<T>::compute_and_bucketize(const RawFeature<T> * raw_feature, uint32_t max_buckets)
{
    this->compute_histogram(raw_feature);
    this->compute_buckets(max_buckets);
    this->bucketize(raw_feature);
    this->compute_sparsity();
}

template <typename T>
uint32_t RawFeatureHistogramImpl<T>::get_number_of_buckets() const
{
    assert(bucket_min.size() > 0);
    return (uint32_t)bucket_min.size();
}

template <typename T>
std::vector<UNIVERSAL_BUCKET> * RawFeatureHistogramImpl<T>::get_bucketized_data() const
{
    return this->universal_data.get();
}

template <typename T>
std::unique_ptr<BucketsCollection> RawFeatureHistogramImpl<T>::get_buckets() const
{
    std::unique_ptr<BucketsCollection> result(new BucketsCollectionImpl<T>(this->bucket_min));
    return result;
}

template <typename T>
float_t RawFeatureHistogramImpl<T>::get_sparsity() const
{
    return sparsity;
}

template <typename T>
UNIVERSAL_BUCKET RawFeatureHistogramImpl<T>::get_default_bucket() const
{
    return default_bucket;
}

template <typename T>
void RawFeatureHistogramImpl<T>::compute_histogram(const RawFeature<T> * feature)
{
    assert(this->hist.empty());
    const std::vector<T> & data = *feature->get_data();
    for (size_t i = 0; i < data.size(); i++) {
        T value = data[i];
        this->hist[value]++;
    }
 
}

template <typename T>
void RawFeatureHistogramImpl<T>::compute_buckets(uint32_t max_buckets)
{
    assert(this->bucket_min.empty());
    assert(this->bucket_max.empty());
    size_t unique_values = this->hist.size();
    if (unique_values < 1) {
        throw std::runtime_error(std::string("Cannot bucketize feature with no values"));
    }
    if (unique_values <= max_buckets) {
        // easy case: every value gets its own bucket
        this->bucket_min.resize(unique_values);
        this->bucket_max.resize(unique_values);
        size_t i=0;
        DOC_ID highest_frequency = 0;
        DOC_ID n_docs = 0;
        for (auto it = this->hist.begin(); it != this->hist.end(); it++,i++) {
            bucket_min[i] = bucket_max[i] = it->first;
            DOC_ID frequency = it->second;
            n_docs += frequency;
            if (frequency > highest_frequency) {
                highest_frequency = frequency;
                this->default_bucket = (UNIVERSAL_BUCKET)i;
            }
        }
        this->sparsity = (float_t)(1.0 - 1.0 * highest_frequency / n_docs);
    } else {
        if (unique_values < 2 * max_buckets) {
            this->compute_buckets_hard(max_buckets);
        }
        else {
            this->compute_buckets_fast(max_buckets);
        }
    }
}

template <typename T>
void RawFeatureHistogramImpl<T>::compute_buckets_fast(uint32_t max_buckets)
{
    size_t unique_values = this->hist.size();
    assert(max_buckets > 0);
    assert(unique_values > max_buckets);
    bucket_min.resize(max_buckets);
    bucket_max.resize(max_buckets);
    DOC_ID n_docs = 0;
    DOC_ID highest_frequency = 0;
    auto it = this->hist.begin();
    size_t counter = 0;
    size_t i = 0;
    for (uint32_t bucket = 0; bucket < max_buckets; bucket++) {
        size_t di = (unique_values - counter + (max_buckets - 1)) / max_buckets;
        bucket_min[bucket] = it->first;
        DOC_ID frequency = it->second;
        for (size_t j = 1; j < di; j++) {
            it++;
            assert(it != this->hist.end());
            frequency += it->second;
        }
        bucket_max[bucket] = it->first;
        n_docs += frequency;
        if (frequency > highest_frequency) {
            highest_frequency = frequency;
            this->default_bucket = bucket;
        }

        it++;
        i += di;
        counter += max_buckets * di;
        assert(counter / unique_values == 1);
        counter %= unique_values;
    }
    assert(i == unique_values);
    assert(it == this->hist.end());
    this->sparsity = (float_t)(1.0 - 1.0 * highest_frequency / n_docs);
}

template <typename T>
void RawFeatureHistogramImpl<T>::compute_buckets_hard(uint32_t max_buckets)
{
    typedef gheap<> gh;
    CompareValuesRanges<T> comparator;
    size_t unique_values = this->hist.size();
    std::vector<ValuesRange<T>> ranges;
    ranges.reserve(unique_values);
    std::vector<ValuesRange<T> *>heap;
    heap.reserve(unique_values - 1);
    size_t i = 0;
    for (auto it = this->hist.begin(); it != this->hist.end(); it++,i++) {
        ranges.push_back(ValuesRange<T>(it->first, it->second));
        if (i > 0) {
            ranges[i-1].next_range = &ranges[i];
            ranges[i].prev_range = &ranges[i-1];
            ranges[i-1].update();
            heap.push_back( &ranges[i-1]);
        }
    }
    gh::make_heap(heap.begin(), heap.end(), comparator);

    size_t unabsorbed_ranges = heap.size();
    size_t heap_size = heap.size();
    while (unabsorbed_ranges >= max_buckets) {
        // >= is not a mistake
        // We need to leave max_buckets-1 items in the heap.
        // That means we have max_buckets ranges to convert to buckets,
        // since the very last range is never in the heap.

        ValuesRange<T> * top;
        while (true) {
            top = heap[0];
            if (top->absorbed) {
                gh::pop_heap(heap.begin(), heap.begin() + (heap_size--), comparator);
                continue;
            }
            if (top->need_update) {
                top->update();
                top->need_update = false;
                gh::restore_heap_after_item_decrease(heap.begin(), heap.begin(), heap.begin() + heap_size, comparator);
continue;
            }
            break;
        }
        ValuesRange<T> * next = top->next_range;
        ValuesRange<T> * prev = top->prev_range;
        top->absorb(next);
        top->update();
        unabsorbed_ranges--;
        if (next->next_range != NULL) {
            gh::restore_heap_after_item_decrease(heap.begin(), heap.begin(), heap.begin() + heap_size, comparator);
        } else {
            gh::pop_heap(heap.begin(), heap.begin() + (heap_size--), comparator);
        }
        if (prev != NULL) {
            prev->need_update = true;
        }
    }
    
#ifndef NDEBUG
    size_t count = 0;
    for (size_t i=0; i < ranges.size(); i++) {
        if (!ranges[i].absorbed) count++;
    }
    assert(count == max_buckets);
#endif
    
    bucket_min.resize(max_buckets);
    bucket_max.resize(max_buckets);
    uint32_t bucket_counter = 0;
    DOC_ID n_docs = 0;
    DOC_ID highest_frequency = 0;
    for (size_t i=0; i < ranges.size(); i++) {
        if (!ranges[i].absorbed) {
            bucket_min[bucket_counter] = ranges[i].min_value;
            bucket_max[bucket_counter] = ranges[i].max_value;

            DOC_ID frequency = ranges[i].frequency;
            n_docs += frequency;
            if (frequency > highest_frequency) {
                highest_frequency = frequency;
                this->default_bucket = bucket_counter;
            }
            bucket_counter++;
        }
    }
    this->sparsity = (float_t)(1.0 - 1.0 * highest_frequency / n_docs);
}

template <typename T>
void RawFeatureHistogramImpl<T>::bucketize(const RawFeature<T> * feature)
{
    assert(bucket_min.size() == bucket_max.size());
    std::unique_ptr<std::vector<UNIVERSAL_BUCKET>> result(new std::vector<UNIVERSAL_BUCKET>);
    size_t n_buckets = bucket_min.size();
    size_t initial_pivot_bucket = 1;
    while (initial_pivot_bucket * 2 < n_buckets)
        initial_pivot_bucket *= 2;
    
    const std::vector<T> & data = *feature->get_data();
    for (size_t i = 0; i < data.size(); i++) {
        T value = data[i];
        size_t rmin = 0, rmax = n_buckets;
        UNIVERSAL_BUCKET ub = 0;
        size_t pivot_size = initial_pivot_bucket;
        while (rmax - rmin > 1) {
            size_t pivot_bucket = rmin + pivot_size;
            UNIVERSAL_BUCKET bit = 0;
            if (pivot_bucket < n_buckets) {
                if (value >= this->bucket_min[pivot_bucket]) {
                    bit = 1;
                }
            }
            ub = (ub << 1) | bit;
            if (bit == 1) {
                rmin = pivot_bucket;
            } else {
                rmax = pivot_bucket;
            }
            pivot_size /= 2;
        }
        result->push_back(ub);
    }

    this->universal_data = std::move(result);
}

template <typename T>
void RawFeatureHistogramImpl<T>::compute_sparsity()
{

}

INSTANTITATE_TEMPLATES_FOR_RAW_FEATURE_TYPES(RawFeatureHistogramImpl)


