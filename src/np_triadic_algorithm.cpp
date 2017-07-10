#include <np_triadic_algorithm.h>

#include <iostream>
#include <iomanip>
#include <pretty_print.hpp>
#include <dim1algebra.hpp>
#include <cmath>

using namespace std;

// note, you'll need g++-7 and C++17 to obtain special_math function beta.

TriadicAlgorithm::TriadicAlgorithm(
			random_engine_t & generator,
			distribution_t & likelihood,
			dirichlet_process & nonparametrics
		): 
			_generator(generator),
			_likelihood(likelihood),
			_nonparametrics(nonparametrics)
{
	_alpha = _nonparametrics.getSuffies().alpha;

	_verbosity = Debug;
	_verbosity = Warning;
	
	_statistics.step[0].type = "merge from 2 to 1";
	_statistics.step[1].type = "split from 1 to 2";
	_statistics.step[2].type = "merge from 3 to 2";
	_statistics.step[3].type = "split from 2 to 3";

	fout << "likelihood: " << _likelihood.getSuffies() << endl;

	_beta = 0.5;
	_beta = 1;

	_split_method = sams_prior;
	//_split_method = simple_random_split;
}

double TriadicAlgorithm::ratioStateProb(bool split, const std::vector<int> & more, const std::vector<int> & less) {
	double logfraction = 0.0;
	for (int i = 0; i < (int)more.size(); ++i) {
		logfraction += lgamma(more[i]);
	}
	for (int i = 0; i < (int)less.size(); ++i) {
		logfraction -= lgamma(less[i]);
	}
	double result = std::log(_alpha) + logfraction;

	if (split) { 
		return result;
	} else {
		return -result;
	}
}

/**
 * @param[in] N                                  number of data points assigned to clustesr
 * @param[in] C                                  maximum number of clusters (in merge/split between 2 to 3, it is 3)
 */
double TriadicAlgorithm::ratioProposal(bool split, int N, int C) {
	if (_split_method == sams_prior) {
		return .0;
	}
	//double result = pow(3, nc - 3) * pow(2, 2 - nc);
	// we can generalize to  (c/(c-1))^(n-c+1)/c
	//double f = C / (C - 1.0);
	//double result = pow(f, N - C + 1) / C;
	//double result = (N - C + 1 ) * std::log(f) - std::log(C);
	//double result = (N - C + 1 ) * ( std::log(C) - std::log( C-1 ) ) - std::log(C);
	double result = (C - N - 1) * std::log(C - 1) - (C - N) * std::log(C);
	if (split) { 
		return result;
	} else {
		return -result;
	}
}

/**
 * The last item in data_picks[] corresponds with the last cluster id in cluster_ids[] and this cluster will be
 * empty after this function.
 */
void TriadicAlgorithm::propose_merge(std::vector<data_ids_t> &pdata, const data_ids_t &data_picks,
		cluster_ids_t &cluster_ids, split_method_t split_method) {
	
	int C = cluster_ids.size();
	int Q = C - 1;
	std::vector<cluster_t*> cluster(Q);

	// assign Q of the D uniformly sampled data points to separate clusters
	for (int i = 0; i < Q; ++i) {
		pdata[i].push_back(data_picks[i]);
	}
	
	// get assignments (just ids, not the data itself)
	data_ids_t data_ids;
	for (int i = 0; i < C; ++i) {
		cluster[i] = _cluster_matrix->getCluster(cluster_ids[i]);
		_cluster_matrix->getAssignments(cluster_ids[i], data_ids);
	}
	fout << "The number of data points assigned to the cluster now is " << data_ids.size() << endl;
	
	// shuffle the data_id items
	algebra::random_order(data_ids.begin(), data_ids.end());
	
	std::vector<double> ldata(Q, 1.0); 

	// assign all data points
	for (int i = 0; i < (int)data_ids.size(); ++i) {
		int data_id = data_ids[i];
		bool skip = false;
		for (int i = 0; i < Q; ++i) {
			if (data_picks[i] == data_id) {
				skip = true;
				break;
			}
		}
		if (skip) continue;

		switch (split_method) {
			case random_mixing:
				{
					assert(false);
					break;
				}
			case simple_random_split:  
				{
					int index = algebra::random_weighted_pick(ldata.begin(), ldata.end(), _generator);
					pdata[index].push_back(data_id);
					break;
				} 
			case sams_prior: 
				{
					data_t * datum = _cluster_matrix->getDatum(data_id);
					for (int i = 0; i < Q; i++) {
						_likelihood.init(cluster[i]->getSuffies());
						ldata[i] = _likelihood.probability(*datum) * pdata[i].size();
					}
					int index = algebra::random_weighted_pick(ldata.begin(), ldata.end(), _generator);
					pdata[index].push_back(data_id);
					break;
				}
			case sams_random_walk: 
				{
					assert(false);
					break;
				}
		}
	}
}

// we are gonna assume that the duplicates are at the end of the data_picks and cluster_ids vectors
void TriadicAlgorithm::propose_split(std::vector<data_ids_t> &pdata, const data_ids_t &data_picks, 
		cluster_ids_t &cluster_ids, cluster_t *new_cluster, split_method_t split_method) {

	int C, Q, D1, D2;
	C = cluster_ids.size() - 1;
	Q = C + 1;
	D1 = data_picks.size();
	D2 = pdata.size();
	
	assert (D1 == Q);
	assert (D2 == Q);

	std::vector<cluster_t*> cluster(Q);
	data_ids_t data_ids;

	// assign uniformly sampled data points to separate clusters
	for (int i = 0; i < Q; ++i) {
		fout << "Assign " << data_picks[i] << " to " << i << endl;
		pdata[i].push_back(data_picks[i]);
	}
	
	// existing clusters
	for (int i = 0; i < C; ++i) {
		cluster[i] = _cluster_matrix->getCluster(cluster_ids[i]);
		_cluster_matrix->getAssignments(cluster_ids[i], data_ids);
	}
	cluster[C] = new_cluster;

	fout << "The number of data points assigned to the cluster now is " << data_ids.size() << endl;
	
	// shuffle the data items
	algebra::random_order(data_ids.begin(), data_ids.end());

	std::vector<double> ldata(Q, 1.0);

	// assign all data points
	for (int i = 0; i < (int)data_ids.size(); ++i) {
		int data_id = data_ids[i];
		bool skip = false;
		for (int i = 0; i < Q; ++i) {
			if (data_picks[i] == data_id) {
				skip = true;
				break;
			}
		}
		if (skip) continue;

		switch (split_method) {
			case random_mixing:
				{
					assert(false);
					break;
				}
			case simple_random_split:  
				{
					int index = algebra::random_weighted_pick(ldata.begin(), ldata.end(), _generator);
					pdata[index].push_back(data_id);
					break;
				} 
			case sams_prior: 
				{
					data_t * datum = _cluster_matrix->getDatum(data_id);
					for (int i = 0; i < Q; i++) {
						_likelihood.init(cluster[i]->getSuffies());
						ldata[i] = _likelihood.probability(*datum) * pdata[i].size();
					}
					int index = algebra::random_weighted_pick(ldata.begin(), ldata.end(), _generator);
					pdata[index].push_back(data_id);
					break;
				}
			case sams_random_walk: 
				{
					assert(false);
					break;
				}
		}
	}
	for (int i = 0; i < Q; i++) {
		assert (_cluster_matrix->count(cluster_ids[i]) > 0);
	}
	D2 = pdata.size();
	assert (D2 == Q);
}

bool TriadicAlgorithm::split(
		const data_ids_t & data_picks,
		cluster_ids_t & cluster_ids
		) {

	int D, C, Q, S;
	D = data_picks.size();
	C = cluster_ids.size() - 1;
	Q = C + 1;
	S = (C - 1) * 2 + 1;
	fout << "Data picks: " << data_picks << endl;
	fout << "Clusters: " << cluster_ids << endl;
	assert (Q == D);

	step_t & statistics_step = _statistics.step[S];
	statistics_step.attempts++;

	// dimension of dataX is C, pdataX is Q, ldataX is set both to Q, cluster as well
	// data can be obtained as pointers, pdata has to be constructed by copying elements
	std::vector<data_ids_t> data_ids(C), pdata_ids(Q);
	std::vector<dataset_t*> data(C);
	std::vector<dataset_t> pdata(Q);
	std::vector<cluster_t*> cluster(Q);
	std::vector<int> ndata(C), npdata(Q);
	std::vector<double> ldata(Q), lpdata(Q);
	int N = 0;
	double rP, rQ, rL = 0.0;
	bool accept = true, overwrite = false;

	for (int i = 0; i < C; i++) {
		cluster[i] = _cluster_matrix->getCluster(cluster_ids[i]);
		_cluster_matrix->getAssignments(cluster_ids[i], data_ids[i]);
		ndata[i] = data_ids[i].size();
		N += ndata[i];
	}
	
	// new cluster
	Suffies *suffies = _nonparametrics.sample_base(_generator);
	cluster[C] = new cluster_t(*suffies);
	
	// split also concerns moves from 1 to 2 and 2 to 1, not only from 1 to 3 and 2 to 3.
	propose_split(pdata_ids, data_picks, cluster_ids, cluster[C], _split_method);

	for (int i = 0; i < Q; ++i) {
		npdata[i] = pdata_ids[i].size();
	}

	fout << "Cluster sizes: from " << cluster_ids << ": " << ndata << " to " << npdata << endl;

	rP = ratioStateProb(true, npdata, ndata);
	rQ = ratioProposal(true, N, Q);

	fout << "The q(.|.)p(.) ratio for the MH split is " << rQ << " + " << rP << " = " << rQ + rP << endl;
	
	// before split
	for (int i = 0; i < C; ++i) {
		cluster[i] = _cluster_matrix->getCluster(cluster_ids[i]);
		_likelihood.init(cluster[i]->getSuffies());
		data[i] = _cluster_matrix->getData(cluster_ids[i]);
		ldata[i] = _likelihood.logprobability(*data[i]);
		double tmp = _likelihood.probability(*data[i]);
		fout << "Cluster " << cluster_ids[i] << ": " << ldata[i] << " or prob: " << tmp << endl;
	}
	// we have only C-1 clusters, set loglikelihood of non-existing cluster to 0
	ldata[C] = 0.0;

	// after split
	for (int i = 0; i < C; ++i) {
		cluster[i] = _cluster_matrix->getCluster(cluster_ids[i]);
		_likelihood.init(cluster[i]->getSuffies());
		_cluster_matrix->getData(pdata_ids[i], pdata[i]);
		lpdata[i] = _likelihood.logprobability(pdata[i]);
		double tmp = _likelihood.probability(pdata[i]);
		fout << "Cluster " << cluster_ids[i] << ": " << lpdata[i] << " or prob: " << tmp << endl;
	}
	// add loglikelihood of new cluster
	_likelihood.init(cluster[C]->getSuffies());
	_cluster_matrix->getData(pdata_ids[C], pdata[C]);
	lpdata[C] = _likelihood.logprobability(pdata[C]);

	double tmp = _likelihood.probability(pdata[C]);
	fout << "Cluster new " << ": " << lpdata[C] << " or prob: " << tmp << endl;

	double rLd = 0.0, rLdp = 0.0;
	for (int i = 0; i < Q; ++i) {
		rLd += ldata[i];
		rLdp += lpdata[i];
	}
		
	rL = rLdp - rLd;
	fout << "Likelihood goes from: " << rLd << " to " << rLdp << ", diff " << rL << endl;

//#define TEST
#ifdef TEST
	// we move pdata[0] to new cluster
	dataset_t pdata;
	_cluster_matrix->getData(pdata_ids[1], pdata);

	auto cluster0 = _cluster_matrix->getCluster(cluster_ids[0]);
	_likelihood.init(cluster0->getSuffies());
	double lsrc  = _likelihood.logprobability(pdata);

	_likelihood.init(cluster[C]->getSuffies());
	double ldest  = _likelihood.logprobability(pdata);

	cout << "-. Likelihood goes from: " << lsrc << " to " << ldest << ", diff " << ldest - lsrc << endl;
#endif

	statistics_step.likelihood_both_nonzero++;

	// normal MH step
	if (!overwrite) {
		double a_split = std::exp(rQ + rP + rL);
		fout << "Acceptance for split: " << rQ + rP + rL << " becomes a = " << a_split << endl;

		double u = _distribution(_generator);

		if (a_split < u) {
			statistics_step.likelihood_both_nonzero_reject++;
			accept = false;
		} else {
			statistics_step.likelihood_both_nonzero_accept++;
			accept = true;
		}
	}

	if (accept) {
		fout << "Accept split!" << endl;
		cluster_id_t new_cluster_id = _cluster_matrix->addCluster(cluster[C]);
		cluster_ids[C] = new_cluster_id;
		// assign or reassign (even to same cluster data points)
		for (int i = 0; i < Q; ++i) {
			for (auto data_id: pdata_ids[i]) {
				fout << "Retract and reassign data " << data_id << " to cluster " << cluster_ids[i] << endl;
				_cluster_matrix->retract(data_id, false);
				_cluster_matrix->assign(cluster_ids[i], data_id);
			}
		}
		for (int i = 0; i < Q; i++) {
			assert (_cluster_matrix->count(cluster_ids[i]) > 0);
		}
		fout << "Performed split" << endl;
	} else {
		fout << "Reject split!" << endl;
		for (int i = 0; i < C; i++) {
			assert (_cluster_matrix->count(cluster_ids[i]) > 0);
		}
		delete cluster[C];
	}
	return accept;
}

bool TriadicAlgorithm::merge(
		const data_ids_t & data_picks,
		cluster_ids_t & cluster_ids
		) {

	int C, D, Q, S;
	C = cluster_ids.size();
	D = data_picks.size();
	Q = C - 1;
	S = (C - 2) * 2;

	assert(C==D);

	std::vector<data_ids_t> data_ids(C), pdata_ids(Q);
	std::vector<dataset_t*> data(C);
	std::vector<dataset_t> pdata(Q);
	std::vector<cluster_t*> cluster(C);
	std::vector<int> ndata(C), npdata(Q);
	std::vector<double> ldata(C), lpdata(C);
	int N = 0;
	double rP, rQ, rL = .0;
	bool accept = true, overwrite = false;
	
	step_t & statistics_step = _statistics.step[S];
	statistics_step.attempts++;

	for (int i = 0; i < C; ++i) {
		cluster[i] = _cluster_matrix->getCluster(cluster_ids[i]);
		_cluster_matrix->getAssignments(cluster_ids[i], data_ids[i]);
		ndata[i] = data_ids[i].size();
		N += ndata[i];
	}
	
	propose_merge(pdata_ids, data_picks, cluster_ids, _split_method);
	
	for (int i = 0; i < Q; ++i) {
		npdata[i] = pdata_ids[i].size();
	}
	
	fout << "Cluster sizes: from " << cluster_ids << ": " << ndata << " to " << npdata << endl;

	rP = ratioStateProb(false, npdata, ndata);
	rQ = ratioProposal(false, N, C);

	fout << "The q(.|.)p(.) ratio for the MH merge is " << rQ << " + " << rP << " = " << rQ + rP << endl;

	// before merge
	for (int i = 0; i < C; ++i) {
		cluster[i] = _cluster_matrix->getCluster(cluster_ids[i]);
		_likelihood.init(cluster[i]->getSuffies());
		data[i] = _cluster_matrix->getData(cluster_ids[i]);
		ldata[i] = _likelihood.logprobability(*data[i]);
	}
	
	// after merge
	for (int i = 0; i < Q; ++i) {
		cluster[i] = _cluster_matrix->getCluster(cluster_ids[i]);
		_likelihood.init(cluster[i]->getSuffies());
		_cluster_matrix->getData(pdata_ids[i], pdata[i]);
		lpdata[i] = _likelihood.logprobability(pdata[i]);
	}
	lpdata[Q] = 0.0;

	double rLd = 0.0, rLdp = 0.0;
	for (int i = 0; i < C; ++i) {
		rLd += ldata[i];
		rLdp += lpdata[i];
	}
	
	rL = rLdp - rLd;
	fout << "Likelihood goes from: " << rLd << " to " << rLdp << ", diff " << rL << endl;
	
#ifdef TEST
	auto data1 = _cluster_matrix->getData(cluster_ids[1]);
	auto cluster1 = _cluster_matrix->getCluster(cluster_ids[1]);
	_likelihood.init(cluster1->getSuffies());
	double lsrc = _likelihood.logprobability(*data1);
	
	auto cluster0 = _cluster_matrix->getCluster(cluster_ids[0]);
	_likelihood.init(cluster0->getSuffies());
	double ldest = _likelihood.logprobability(*data1);
	
	fout << "Likelihood check: " << lsrc << " to " << ldest << ", diff " << ldest - lsrc << endl;
#endif
		
	statistics_step.likelihood_both_nonzero++;

	if (!overwrite) {
		double a_merge = std::exp(rQ + rP + rL);
		fout << "Acceptance for merge: " << rQ + rP + rL << " becomes a = " << a_merge << endl;

		double u = _distribution(_generator);
		fout << "Sampled u: " << u << endl;

		if (a_merge < u) {
			statistics_step.likelihood_both_nonzero_reject++;
			accept = false;
		} else {
			statistics_step.likelihood_both_nonzero_accept++;
			accept = true;
		}
	} else {
		assert(false);
	}

	if (accept) {
		fout << "Accept merge!" << endl;
		for (int i = 0; i < Q; ++i) {
			fout << "Reassign cluster " << i << " in clusters_ids[.] array" << endl;
			fout << "This cluster has id " << cluster_ids[i] << endl;
			for (auto data_id: pdata_ids[i]) {
				fout << "Retract and reassign data " << data_id << " to cluster " << cluster_ids[i] << endl;
				_cluster_matrix->retract(data_id, false);
				_cluster_matrix->assign(cluster_ids[i], data_id);
			}
		}
		for (int i = 0; i < Q; i++) {
			fout << "Check cluster " << cluster_ids[i] << endl;
			assert (_cluster_matrix->count(cluster_ids[i]) > 0);
		}
		fout << "Check cluster " << cluster_ids[Q] << endl;
		assert (_cluster_matrix->empty(cluster_ids[Q]));

		fout << "Remove cluster " << cluster_ids[Q] << endl;
		_cluster_matrix->remove(cluster_ids[Q]);
		fout << "Performed merge" << endl;
	} else {
		fout << "Reject merge!" << endl;
	}
	return accept;
}

/*
 * This function assigns the given data item and updates the number of clusters if it is assigned to a new cluster.
 * It does not reassign other data items. Hence, it is a single update step. This naturally has disadvantages for
 * problems where we might converge faster if we would be able to re-assign a set of data points at once. The latter
 * is the idea behind split and merge samplers.
 *
 * The partition under consideration emerges from calling this update() function regularly. The data points only
 * enter the equation through the likelihood function where the number of data points per cluster determines their
 * relative weight. This can be seen as some kind of voting mechanism. However, there is no "guidance" of the chain
 * to partitions of data that might make more sense. The MCMC is not data-driven. Sampling of the cluster parameters
 * is a not data-driven either. We just sample from the base distribution "_nonparametrics.sample_base" without 
 * regard for that part of parameter space that makes sense from the perspective of the data.
 */
void TriadicAlgorithm::update(
			membertrix & cluster_matrix,
			const data_ids_t & cdata_picks
		) {

	static int step = 0;
	fout << "Update step " << ++step << " in Triadic split-merge algorithm" << endl;

	// copy data points (because we will need to be removing points for the dyadic sampler steps)
	data_ids_t data_picks = cdata_picks;

	// there should be exactly three data points sampled
	assert (data_picks.size() == 3);

	// the data ids should be unique
	assert (data_picks[0] != data_picks[1]);
	assert (data_picks[0] != data_picks[2]);
	
	_cluster_matrix = &cluster_matrix;

	// store cluster indices of current observations
	cluster_ids_t cluster_ids; 
	cluster_ids.clear();
	for (auto data_id: data_picks) {
		cluster_id_t cluster_id = _cluster_matrix->getClusterId(data_id);
		fout << "We found data item " << data_id << " at cluster " << cluster_id << endl;
		cluster_ids.push_back( cluster_id );
	}
#define TEST_DELETE_POINT
#ifdef TEST_DELETE_POINT
	data_picks.erase(data_picks.end() - 1);
	cluster_ids.erase(cluster_ids.end() - 1);
#endif
	int uniq_cluster_count = algebra::count_unique(cluster_ids.begin(), cluster_ids.end());
	fout << "The data comes from " << uniq_cluster_count << " unique cluster(s)" << endl;

	bool jain_neal_split = false;
	double u = _distribution(_generator);
	if (uniq_cluster_count == 1) {
		fout << "Dyadic step: split 1 cluster into 2" << endl;
		
#ifndef TEST_DELETE_POINT
		// remove one of the data points that points to the same cluster as one of the other data points
		int index = algebra::duplicate_pick(cluster_ids.begin(), cluster_ids.end(), _generator);
		data_picks.erase(data_picks.begin() + index);
		cluster_ids.erase(cluster_ids.begin() + index);
#endif
		size_t Ka = _cluster_matrix->getClusterCount();
		fout << "There are " << Ka << " clusters" << endl;

		bool accept = split(data_picks, cluster_ids);
	
		step_t & statistics_step = _statistics.step[1];
		if (accept) {
			size_t Kb = _cluster_matrix->getClusterCount();
			fout << "There are now " << Kb << " clusters" << endl;
			assert (Kb == Ka + 1);
			statistics_step.cluster_events_accept++;
		} else {
			statistics_step.cluster_events_reject++;
		}
		jain_neal_split = true;
	} else if (u < _beta) {
		fout << "Dyadic step: merge 2 clusters into 1" << endl;
		
#ifndef TEST_DELETE_POINT
		// remove one of the data points that points to the same cluster as one of the other data points
		int index = algebra::duplicate_pick(cluster_ids.begin(), cluster_ids.end(), _generator);
		data_picks.erase(data_picks.begin() + index);
		cluster_ids.erase(cluster_ids.begin() + index);
#endif
		size_t Ka = _cluster_matrix->getClusterCount();
		fout << "There are " << Ka << " clusters" << endl;

		bool accept = merge(data_picks, cluster_ids);

		step_t & statistics_step = _statistics.step[0];
		if (accept) {
			size_t Kb = _cluster_matrix->getClusterCount();
			fout << "There are now " << Kb << " clusters" << endl;
			assert (Kb == Ka - 1);
			statistics_step.cluster_events_accept++;
		} else {
			statistics_step.cluster_events_reject++;
		}

		jain_neal_split = true;
	}

	if (!jain_neal_split) {
		// a single cluster
		switch (uniq_cluster_count) {
			case 0: default:
				assert(false);
				break;
			case 1:
				assert(false);
				break;
			case 2: 
				{
					fout << "Triadic step: split 2 clusters into 3" << endl;

					size_t Ka = _cluster_matrix->getClusterCount();
					fout << "There are " << Ka << " clusters" << endl;

					// make sure the cluster_ids and data_picks are sorted such that the duplicate cluster id is 
					// present at the end, for that pick the first duplicate
					int index = algebra::duplicate_pick(cluster_ids.begin(), cluster_ids.end(), _generator);
					fout << "Found first duplicate at " << index << endl;
					std::iter_swap(cluster_ids.begin() + index, cluster_ids.end() - 1);
					std::iter_swap(data_picks.begin() + index, data_picks.end() - 1);
					
					bool accept = split(data_picks, cluster_ids);

					step_t & statistics_step = _statistics.step[3];
					if (accept) {
						size_t Kb = _cluster_matrix->getClusterCount();
						fout << "There are now " << Kb << " clusters" << endl;
						assert (Kb == Ka + 1);

						statistics_step.cluster_events_accept++;
					} else {
						statistics_step.cluster_events_reject++;
					}
					break;
				}
			case 3: 
				{ // merge step
					fout << "Triadic step: merge 3 clusters into 2" << endl;
		
					size_t Ka = _cluster_matrix->getClusterCount();
					fout << "There are " << Ka << " clusters" << endl;

					bool accept = merge(data_picks, cluster_ids);

					step_t & statistics_step = _statistics.step[2];
					if (accept) {
						size_t Kb = _cluster_matrix->getClusterCount();
						fout << "There are now " << Kb << " clusters" << endl;
						assert (Kb == Ka - 1);

						statistics_step.cluster_events_accept++;
					} else {
						statistics_step.cluster_events_reject++;
					}
					break;
				}
		} 
	}
	// check
	for (auto data_id: cdata_picks) {
		fout << "Check data id " << data_id << endl;
		assert(_cluster_matrix->assigned(data_id));
	}
	fout << "Completed update step " << step << endl;
}

void TriadicAlgorithm::printStatistics() {
	int verbosity = _verbosity;
	_verbosity = Debug;

	fout << endl;
	fout << "Statistics:" << endl;
	for (int i = 0; i < 4; ++i) {
		if (_beta >= 1 && i >= 2) break;
		step_t step = _statistics.step[i];
		// attempts is accepted + rejected correct for merge
		fout << " # of " << step.type << " attempts: " << step.attempts << endl;
		fout << "   o of accepted " << step.type << " cluster events: " << step.cluster_events_accept << endl;
		fout << "     - of accept 0->? overrides: " << step.source_likelihood_zero << endl;
		fout << "     - of events accepted after consideration: " << step.likelihood_both_nonzero_accept << endl;

		fout << "   o of rejected " << step.type << " cluster events: " << step.cluster_events_reject << endl;
		fout << "   # of reject ?->0 overrides: " << step.target_likelihood_zero << endl;

		// below is correct
		fout << "   # of metropolis-hastings decisions: " << step.likelihood_both_nonzero << endl;
		fout << "     - accepted: " << step.likelihood_both_nonzero_accept << endl;
		fout << "     - rejected: " << step.likelihood_both_nonzero_reject << endl;

	//	assert (step.attempts == step.cluster_events_accept + step.cluster_events_reject);
	//	assert (step.likelihood_both_nonzero == step.likelihood_both_nonzero_accept + step.likelihood_both_nonzero_reject);
	}

	// set back to zero
	_statistics = {};
	_statistics.step[0].type = "merge from 2 to 1";
	_statistics.step[1].type = "split from 1 to 2";
	_statistics.step[2].type = "merge from 3 to 2";
	_statistics.step[3].type = "split from 2 to 3";

	_verbosity = verbosity;
} 
