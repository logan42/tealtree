#include "ranking_cost_function.h"

#include "document.h"
#include "log_trivial.h"
#include "thread_specific_ptr.h"

#include <algorithm>
#include <boost/thread/tss.hpp>

void LambdaRank::precompute(TrainerData * trainer_data, DOC_ID ndcg_at)
{
    std::vector<Document> & documents = trainer_data->documents;
    std::vector<DOC_ID> & query_limits = trainer_data->query_limits;
    std::vector<DOC_ID> & sorted_doc_ids = trainer_data->sorted_doc_ids;
    std::vector<DOC_ID> & ranks = trainer_data->ranks;
    std::vector<float_t> & IDCGs = trainer_data->IDCGs;

    sorted_doc_ids.resize(documents.size());
    ranks.resize(documents.size());
    for (size_t i = 0; i < query_limits.size() - 1; i++) {
        DOC_ID query_begin = query_limits[i];
        DOC_ID query_end = query_limits[i + 1];
        for (DOC_ID j = query_begin; j < query_end; j++) {
            sorted_doc_ids[j] = j;
        }
        std::stable_sort(sorted_doc_ids.begin() + query_begin, sorted_doc_ids.begin() + query_end, [&documents](DOC_ID doc_id1, DOC_ID doc_id2) {
            return documents[doc_id1].target_score > documents[doc_id2].target_score;
        });
        for (DOC_ID j = query_begin; j < query_end; j++) {
            ranks[sorted_doc_ids[j]] = j - query_begin;
        }
        float_t IDCG = 0;
        DOC_ID n = query_end - query_begin;
        if (ndcg_at > 0) {
            n = std::min(n, ndcg_at);
        }
        DOC_ID * buffer = &sorted_doc_ids[query_begin];
        for (size_t i = 0; i < n; i++) {
            IDCG += get_dcg_coefficient(i) * documents[buffer[i]].target_score;
        }
        IDCGs.push_back(IDCG);
    }
}

//#define HACK_NEWTON
void LambdaRank::compute_gradient_for_query(TrainerData * trainer_data, DOC_ID ndcg_at, DOC_ID query_index, bool newton_step)
{
#ifdef HACK_NEWTON
    newton_step = true;
#endif //HACK_NEWTON

    std::vector<Document> & documents = trainer_data->documents;
    std::vector<DOC_ID> & sorted_doc_ids = trainer_data->sorted_doc_ids;
    std::vector<DOC_ID> & ranks = trainer_data->ranks;
    std::vector<float_t> & IDCGs = trainer_data->IDCGs;

    assert(buffer.size() == 0);
    assert(buffer2.size() == 0);
    DOC_ID query_begin = trainer_data->query_limits[query_index];
    DOC_ID query_end = trainer_data->query_limits[query_index + 1];
    DOC_ID n = query_end - query_begin;
    for (DOC_ID doc_id = query_begin; doc_id < query_end; doc_id++) {
        documents[doc_id].gradient = 0;
        documents[doc_id].hessian = 0;
    }

    float_t IDCG = IDCGs[query_index];
    if (IDCG == 0) {
        // No relevant results, no gradients will come from this query. Skip it.
        return;
    }

    for (DOC_ID doc_id = query_begin; doc_id < query_end; doc_id++) {
        buffer.push_back(doc_id);
    }
    
    std::stable_sort(buffer.begin(), buffer.end(), [&documents](DOC_ID doc_id1, DOC_ID doc_id2) {
        return documents[doc_id1].score> documents[doc_id2].score;
    });

    // Here is the explanation of what buffer2 means.
    // Suppose we have the documents of the current query sorted by their label (ground truth), that is also an ideal ranking order.
    // Then buffer2[i] will correspond to i-th document in the ideal ranking.
    // But the value of buffer2 is going to be the rank of that (i-th document in ideal ranking) when ranked by current model's score.
    // In other words, buffer2[i] is the rank (according to the current model) of the i-th best (according to the ideal ranking) document of the current query.
    // We need that because we need to compute delta_NDCG, which is computed based on current model's score ranking,
    // but we are going to iterate through the documents in the ideal ranking order.
    buffer2.resize(buffer.size());
    for (size_t i = 0; i < buffer.size(); i++) {
        buffer2[ranks[buffer[i]]] = i;
    }

    DOC_ID * this_sorted_doc_ids = &sorted_doc_ids[query_begin];
    size_t index = 0;
    while (index < n) {
        size_t index2 = index;
        while ((index2 < n) && (documents[this_sorted_doc_ids[index2]].target_score == documents[this_sorted_doc_ids[index]].target_score)) {
        index2++;
}
if (index2 >= n) {
    break;
}
for (size_t i = index; i < index2; i++) {
    for (size_t j = index2; j < n; j++) {
        DOC_ID di = this_sorted_doc_ids[i];
        DOC_ID dj = this_sorted_doc_ids[j];
        DOC_ID ranki = buffer2[i];
        DOC_ID rankj = buffer2[j];
            assert(documents[di].target_score - documents[dj].target_score > 0);
            if ((ndcg_at > 0) && (ranki >= ndcg_at) && (rankj >= ndcg_at)) {
                continue;
            }
        float_t delta_NDCG = (documents[di].target_score - documents[dj].target_score) * (get_dcg_coefficient(ranki) - get_dcg_coefficient(rankj)) / IDCG;
        delta_NDCG = std::abs(delta_NDCG);
        float_t score_i = documents[di].score;
        float_t score_j = documents[dj].score;
        float grad_delta = delta_NDCG * sigmoid(score_j - score_i);
        documents[di].gradient -= grad_delta;
        documents[dj].gradient += grad_delta;
        if (newton_step) {
            float_t hessian_delta = delta_NDCG * sigmoid_prime(score_i - score_j);
            documents[di].hessian += hessian_delta;
            documents[dj].hessian += hessian_delta;
        }
    }
}
index = index2;
    }

    if (newton_step) {
        for (DOC_ID d = query_begin; d < query_end; d++) {
            //documents[d].hessian = std::max(0, documents[d].hessian);
#ifdef HACK_NEWTON
            documents[d].gradient /= documents[d].hessian;
#endif //HACK_NEWTON
        }
    }
    buffer.clear();
    buffer2.clear();
}

template <typename T>
void RankingCostFunction<T>::compute_gradient(TrainerData * trainer_data, bool newton_step)
{
    T cf;
    for (size_t i = 0; i < trainer_data->query_limits.size() - 1; i++) {
cf.compute_gradient_for_query(trainer_data, this->depth, i, newton_step);
    }
    }

template <typename T>
void RankingCostFunction<T>::compute_gradient(TrainerData * trainer_data, bool newton_step, ThreadPool * tp)
{
    //boost::thread_specific_ptr<T> cf;
    thread_specific_ptr<T> cf;
    std::vector<std::future<void>> futures;
    futures.reserve(trainer_data->query_limits.size());
    for (size_t i = 0; i < trainer_data->query_limits.size() - 1; i++) {
        futures.push_back(tp->enqueue(false, 
            [&cf, i, newton_step, this, trainer_data]() {
            T* this_cf = cf.get();
            if (this_cf == nullptr) {
                this_cf = new T();
                cf.reset(this_cf);
            }
            this_cf->compute_gradient_for_query(trainer_data, this->depth, i, newton_step);
        }));
    }
    for (size_t i = 0; i < futures.size(); i++) {
        futures[i].get();
    }
}

template class RankingCostFunction<LambdaRank>;
