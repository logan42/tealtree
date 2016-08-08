#include "dense_feature.h"
#include "document.h"
#include "trainer.h"

template<const uint8_t BITS>
void DenseFeatureImpl<BITS>::init_from_raw_histogram(const RawFeatureHistogram * hist)
{
    this->set_buckets(hist->get_buckets());
    std::vector<UNIVERSAL_BUCKET> * ub_data = hist->get_bucketized_data();
    size_t size = ub_data->size();
    this->n_buckets = hist->get_number_of_buckets();
    assert(this->n_buckets <= (1u << BITS));
    cv.reserve(size);
    for (size_t i = 0; i < size; i++) {
        UNIVERSAL_BUCKET value = (*ub_data)[i];
        assert(value < this->n_buckets);
        cv.push_back((ValueType)value);
    }
    cv.push_back_flush();
}

template<const uint8_t BITS>
std::unique_ptr<SplitSignature> DenseFeatureImpl<BITS>::get_split_signature(TreeNode * leaf, Split * split)
{
    assert(split->feature == this);
    std::vector<DOC_ID> & doc_ids = leaf->doc_ids;
    uint8_t truth_bits[2] = {
        static_cast<uint8_t>(split->inverse ? 1 : 0),
        static_cast<uint8_t>(split->inverse ? 0 : 1) };

    std::unique_ptr<SplitSignature> result(new SplitSignature);
    result->reserve(doc_ids.size());
    for (size_t i = 0; i < doc_ids.size(); i++) {
        DOC_ID doc_id = doc_ids[i];
        ValueType value = cv[doc_id];;
        result->push_back(truth_bits[value >= split->threshold]);
    }
    result->push_back_flush();
    return result;
}


template<const uint8_t BITS>
std::unique_ptr<Histogram> DenseFeatureImpl<BITS>::compute_histogram(const TreeNode * leaf, bool newton_step)
{
    if (newton_step) {
        return this->compute_histogram_impl<true>(leaf);
    }
    else {
        return this->compute_histogram_impl<false>(leaf);
    }
}

template<const uint8_t BITS>
template <const bool NEWTON_STEP>
inline std::unique_ptr<Histogram> DenseFeatureImpl<BITS>::compute_histogram_impl(const TreeNode * leaf)
{
    typedef HistGetter<NEWTON_STEP> HG;
    std::unique_ptr<Histogram> result(new Histogram(this->n_buckets));
    for (size_t i = 0; i < leaf->doc_ids.size(); i++) {
        DOC_ID doc_id = leaf->doc_ids[i];
        Document & document = this->trainer_data->documents[doc_id];
        ValueType value = this->cv[doc_id];
        HistogramItem & item = result->data[value];
        item.gradient += document.gradient;
        HG::get_weight(item) += HG::get_document_weight(document);
    }
    return result;
}

template<const uint8_t BITS>
UNIVERSAL_BUCKET DenseFeatureImpl<BITS>::get_value(DOC_ID doc_id)
{
    return this->cv[doc_id];
}

template<const uint8_t BITS>
inline DenseFeatureImpl<BITS>::~DenseFeatureImpl()
{
}

template<const uint8_t BITS>
const std::string DenseFeatureImpl<BITS>::get_registry_name()
{
    return "d" + std::to_string(BITS);
}

/*
std::unique_ptr<DenseFeature> create_dense_feature_from_histogram(const RawFeatureHistogram * hist)
{
    uint32_t n_buckets = hist->get_number_of_buckets();
    std::unique_ptr<DenseFeature> result;
    if (n_buckets <= 2) {
        result = std::unique_ptr<DenseFeature>(new DenseFeatureImpl<1>());
    } else 
        if (n_buckets <= 256) {
        result = std::unique_ptr<DenseFeature>(new DenseFeatureImpl<8>());
    } else {
        throw std::runtime_error(std::string("Unsupported dense feature with ") + std::to_string(n_buckets) + " buckets.");
    }
    result->init_from_raw_histogram(hist);
    return result;
}
*/

template class DenseFeatureImpl <1>;
template class DenseFeatureImpl <2>;
template class DenseFeatureImpl <4>;
template class DenseFeatureImpl <8>;
template class DenseFeatureImpl <16>;
