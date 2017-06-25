#include <np_jain_neal_algorithm.h>

#include <iostream>
#include <iomanip>
#include <pretty_print.hpp>

using namespace std;

JainNealAlgorithm::JainNealAlgorithm(
			random_engine_t & generator,
			distribution_t & likelihood,
			dirichlet_process & nonparametrics
		): 
			_generator(generator),
			_likelihood(likelihood),
			_nonparametrics(nonparametrics)
{
	_alpha = _nonparametrics.getSuffies().alpha;

	_verbosity = 5;

	fout << "likelihood: " << _likelihood.getSuffies() << endl;
		
	_statistics.new_clusters_events = 0;
}

int factorial(int n) {
	int ret = 1;
	for(int i = 1; i <= n; ++i)
		ret *= i;
	return ret;
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
void JainNealAlgorithm::update(
			membertrix & cluster_matrix,
			std::vector<data_id_t> data_ids
		) {
	static int step = 0;
	fout << "Update step " << ++step << " in Jain & Neal's split-merge algorithm for data item " << data_id << endl;

	// remove observation under consideration from cluster, store cluster indices
	std::vector<cluster_id_t> prev_clusters(data_ids.size());
	for (auto index: data_ids) {
		fout << "Retract observation " << index << " from cluster matrix" << endl;
		cluster_id_t cluster_id = cluster_matrix.getCluster(index);
		prev_clusters.push_back( cluster_id );
		cluster_matrix.retract(cluster_id, index);
	}

	std::set<cluster_id_t> prev_uniq_clusters;
	for (auto cl: prev_clusters) {
		prev_uniq_clusters.insert(cl);
	}
	int uniq_cluster_count = prev_uniq_clusters.size();

	// a single cluster
	switch (uniq_cluster_count) {
		case 0: default:
			assert(false);
		break;
		case 1: {
			// split step
		}
		break;
		case 2: {
			size_t Ka = cluster_matrix.getClusterCount();

			// merge step
			
			// copy each element by value, so we can correct the step later
			data_ids_t data_ids0 = *cluster_matrix.getAssignments(prev_clusters[0]);
			data_ids_t data_ids1 = *cluster_matrix.getAssignments(prev_clusters[1]);

			int nc0 = data_ids0.size();
			int nc1 = data_ids1.size();
			int nc = nc0 + nc1;

			// check if type is correct (no rounding off to ints by accident)
			double p12 = 1.0/_alpha * factorial(nc - 1) / (  factorial(nc0 - 1) * factorial(nc1 - 1) );
			double q21 = pow(0.5, nc - 2);

			// calculate likelihoods

			// move all items from i to j
			for (auto data: data_ids0) {
				cluster_matrix.retract(data);
				cluster_matrix.assign(prev_clusters[1], data);
			}
			
			size_t Kb = cluster_matrix.getClusterCount();
			assert (Kb == Ka - 1);

			// needs to be re-established on rejection!
		}
		break;
	} 
	
	// existing clusters
	// if data item removed one of the clusters, this is still in there with a particular weight
	// assume getClusters() will be a copy
	auto clusters = cluster_matrix.getClusters();


	// Create temporary data structure for K clusters
	size_t K = clusters.size();
	std::vector<int> cluster_ids(K);

	fout << "Calculate likelihood for K=" << K << " existing clusters" << endl;

	fout << "Calculate parameters for M=1 new clusters" << endl;
	// Calculate parameters for M new clusters
	int M = 1;
	clusters_t new_clusters(M);
	for (int m = 0; m < M; ++m) {
		Suffies *suffies = _nonparametrics.sample_base(_generator);
		cluster_t *temp = new cluster_t(*suffies);
		new_clusters[m] = temp;
		fout << "Suffies generated: " << new_clusters[m]->getSuffies() << endl;
	}	
	
	data_t & observation = *cluster_matrix.getDatum(data_id);
	fout << "Current observation " << data_id << " under consideration: " << observation << endl;

	// Get (weighted) likelihoods for each existing and new cluster given the above observation
	std::vector<double> weighted_likelihood(K+M);

	int k = 0;
	for (auto cluster_pair: clusters) {
		auto const &key = cluster_pair.first;
		auto const &cluster = cluster_pair.second;
	
		cluster_ids[k] = key;

		fout << "Obtained suffies from cluster " << key << ": " << cluster->getSuffies() << endl;

		// here _likelihood is sudden a niw distribution...
		_likelihood.init(cluster->getSuffies());
		fout << "calculate probability " << endl;
		double ll = _likelihood.probability(observation);
		fout << "which is: " << ll << endl;
		weighted_likelihood[k] = _likelihood.probability(observation) * cluster_matrix.count(key);
		/*
		fout << "Cluster ";
		cluster_matrix.print(key, std::cout);
		cout << '\t' << _likelihood.probability(observation) << endl;
		*/
		k++;
		
	}

	/*
	 * The Metropolis-Hastings step with generating M proposals at the same time does not look like the typical 
	 * alpha < u rule, however, it is. With a single proposal we can first sample for accept when it falls under
	 * a uniform random variable. And then select one of the existing proposals with a probability proportional to
	 * their (weighted) likelihoods. To remind you, the weight is determined by the number of observations tied to
	 * that cluster for existing clusters and alpha for proposed clusters. With multiple proposals we can just directly
	 * sample from the complete vector (existing and proposed clusters) with weighted likelihoods.
	 */
	for (int m = 0; m < M; ++m) {
		_likelihood.init(new_clusters[m]->getSuffies());
		weighted_likelihood[K+m] = _likelihood.probability(observation) * _alpha / (double)M;
		fout << "New cluster " << m << 
			"[~#" << _alpha/M << "]: " << \
			_likelihood.probability(observation) << \
			new_clusters[m]->getSuffies() << endl;
	}

	/*
	 * If samples come from the same cluster... (do we still know?)
	 */


	// sample uniformly from the vector "weighted_likelihood" according to the weights in the vector
	// hence [0.5 0.25 0.25] will be sampled in the ratio 2:1:1
	fout << "Pick a cluster given their weights" << endl;
	double pick = _distribution(_generator);
	fout << "Pick " << pick << endl;
	std::vector<double> cumsum_likelihood(weighted_likelihood.size());
	fout << "Weighted likelihood: " << weighted_likelihood << endl;
	std::partial_sum(weighted_likelihood.begin(), weighted_likelihood.end(), cumsum_likelihood.begin());
	fout << "Cumulative weights: " << cumsum_likelihood << endl;

	pick = pick * cumsum_likelihood.back();
	fout << "Pick scaled with maximum cumsum: " << pick << endl;

	auto lower = std::lower_bound(cumsum_likelihood.begin(), cumsum_likelihood.end(), pick);
	size_t index = std::distance(cumsum_likelihood.begin(), lower);

	assert (index < cumsum_likelihood.size());
	fout << "Pick value just below pick: " << index << endl;

	/*
	 * Pick either a new or old cluster using calculated values. With a new cluster generate new sufficient 
	 * statistics and update the membership matrix.
	 */
	if (index >= K) {
		// pick new cluster
		fout << "New cluster: " << index-K << endl;
		cluster_t *new_cluster = new cluster_t(new_clusters[index-K]->getSuffies());
		fout << "Add to membership matrix" << endl;
		cluster_id_t cluster_index = cluster_matrix.addCluster(new_cluster);
		fout << "Assign data to cluster with id " << cluster_index << endl;
		cluster_matrix.assign(cluster_index, data_id);
		_statistics.new_clusters_events++;
	} else {
		// pick existing cluster with given cluster_id
		fout << "Existing cluster" << endl;
		
		cluster_id_t cluster_id = cluster_ids[index];

		fout << "Assign data to cluster with id " << cluster_id << endl;
		assert (cluster_matrix.count(cluster_id) != 0);
		
		cluster_matrix.assign(cluster_id, data_id);
	}
	
	// deallocate new clusters that have not been used
	fout << "Deallocate temporary clusters" << endl;
	for (int m = 0; m < M; ++m) {
		delete new_clusters[m];
	}

	// check
	assert(cluster_matrix.assigned(data_id));
}

void JainNealAlgorithm::printStatistics() {
	int verbosity = _verbosity;
	_verbosity = 0;
	fout << "Statistics:" << endl;
	fout << " # of new cluster events: " << _statistics.new_clusters_events << endl;
	_verbosity = verbosity;
} 