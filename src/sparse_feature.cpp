#include "sparse_feature.h"
#include "trainer_data.h"
#include "types.h"

template<typename T, const bool NEWTON_STEP>
class HistogramUpdater
{
private:
     Histogram * hist;
public:
    HistogramUpdater(Histogram * hist)
        : hist(hist)
    {}

    inline DOC_ID transform_doc_id(DOC_ID doc_id)
    {
        return doc_id;
    }

    inline void on_explicit_value(size_t index, T value, Document & document)
    {
        typedef HistGetter<NEWTON_STEP> HG;
        HistogramItem & item = hist->data[value];
        HG::get_weight(item) += HG::get_document_weight(document);
        item.gradient += document.gradient;
    }

    inline void on_default_value()
    {
        // Do nothing.
    }
};

template<typename T>
class SplitSignatureUpdater
{
private:
    SplitSignature * split_signature;
    T threshold;
public:
    SplitSignatureUpdater(SplitSignature * split_signature, T threshold)
        : split_signature(split_signature),
            threshold(threshold)
    {}

    inline DOC_ID transform_doc_id(DOC_ID doc_id)
    {
        return doc_id;
    }

    inline void on_explicit_value(size_t index, T value, Document & document)
    {
        split_signature->set(index, value >= threshold);
    }

    inline void on_default_value()
    {
        // Do nothing.
    }
};


template<const uint8_t BITS>
template<typename U>
void SparseFeatureImpl<BITS>::compute_on_values(const TreeNode * leaf, U & updater, DOC_ID n_docs, DOC_ID v_ptr, DOC_ID o_ptr)
{
    const std::vector<DOC_ID> & doc_ids = leaf->doc_ids;
    auto offset_iterator = this->offsets.iterator(o_ptr);
    
    if (n_docs == 0) {
        return;
    }
    DOC_ID d = 0;
    DOC_ID current_doc_id = offset_iterator.next();
    size_t i;
    for (i = 0; i < doc_ids.size(); i++) {
        DOC_ID leaf_doc_id = doc_ids[i];
        while (current_doc_id < leaf_doc_id) {
            d++;
            if (d >= n_docs) {
                // This is just a break out of both nested loops
                goto tail;
            }
            current_doc_id += offset_iterator.next();
        }
        if (leaf_doc_id == current_doc_id) {
            ValueType value = this->cv[v_ptr + d];
            Document & document= this->trainer_data->documents[updater.transform_doc_id(current_doc_id)];
            updater.on_explicit_value(i, value, document);
        }
        else {
            updater.on_default_value();
        }
    }

tail:
    for (; i < doc_ids.size(); i++) {
        updater.on_default_value();
    }
}


template<const uint8_t BITS>
UNIVERSAL_BUCKET SparseFeatureImpl<BITS>::get_value(DOC_ID doc_id)
{
    //Do we still need this?
    return 0;
}

template<const uint8_t BITS>
const std::string SparseFeatureImpl<BITS>::get_registry_name()
{
    return "s" + std::to_string(BITS);
}

template<const uint8_t BITS>
void SparseFeatureImpl<BITS>::init_from_raw_histogram(const RawFeatureHistogram * hist)
{
    this->set_buckets(hist->get_buckets());
    std::vector<UNIVERSAL_BUCKET> * ub_data = hist->get_bucketized_data();
    DOC_ID size = (DOC_ID)ub_data->size();
    this->n_buckets = hist->get_number_of_buckets();
    assert(this->n_buckets <= (1u << BITS));
    this->default_value = (ValueType)hist->get_default_bucket();
    auto offsets_writer = this->offsets.get_initial_writer();
    DOC_ID last_doc_id = 0;
    for (DOC_ID i = 0; i < size; i++) {
        UNIVERSAL_BUCKET ub_value = (*ub_data)[i];
        assert(ub_value < this->n_buckets);
        ValueType value = (ValueType)ub_value;
        if (value != this->default_value) {
            this->cv.push_back(value);
            offsets_writer.write(i - last_doc_id);
            last_doc_id = i;
        }
    }
    this->cv.push_back_flush();
    offsets_writer.flush();

}

template<const uint8_t BITS>
std::unique_ptr<SplitSignature> SparseFeatureImpl<BITS>::get_split_signature(TreeNode * leaf, Split * split)
{
    assert(split->feature == this);
    std::vector<DOC_ID> & doc_ids = leaf->doc_ids;
    std::unique_ptr<SplitSignature> split_signature(new SplitSignature(doc_ids.size(), this->default_value >= split->threshold));
    SplitSignatureUpdater<ValueType> updater(split_signature.get(), (ValueType)split->threshold);
    this->compute_on_values<SplitSignatureUpdater<ValueType>>(leaf, updater, this->cv.size());
    if (split->inverse) {
        split_signature->invert();
    }
    return split_signature;
}

template<const uint8_t BITS>
std::unique_ptr<Histogram> SparseFeatureImpl<BITS>::compute_histogram(const TreeNode * leaf, bool newton_step)
{
    if (newton_step) {
        return this->compute_histogram_impl<true>(leaf);
    }
    else {
        return this->compute_histogram_impl<false>(leaf);
    }
}

template<const uint8_t BITS>
template<const bool NEWTON_STEP>
inline std::unique_ptr<Histogram> SparseFeatureImpl<BITS>::compute_histogram_impl(const TreeNode * leaf)
{
    std::unique_ptr<Histogram> result(new Histogram(this->n_buckets));
    HistogramUpdater<ValueType, NEWTON_STEP> updater(result.get());
    this->compute_on_values<HistogramUpdater<ValueType, NEWTON_STEP>>(leaf, updater, this->cv.size());
    this->fix_histogram<NEWTON_STEP>(leaf, result.get());
    return result;
}

//template<const uint8_t BITS>
//template<const bool NEWTON_STEP>
//inline void SparseFeatureImpl<BITS>::fix_histogram(const TreeNode * leaf, Histogram * hist)

template class SparseFeatureImpl <1>;
template class SparseFeatureImpl <2>;
template class SparseFeatureImpl <4>;
template class SparseFeatureImpl <8>;
template class SparseFeatureImpl <16>;
