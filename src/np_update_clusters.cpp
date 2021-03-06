#include <np_update_clusters.h>

#include <pretty_print.hpp>

#include <statistics/dirichlet.h>

using namespace std;

/**
 * Initiate update cluster class and set likelihood and posterior predictive.
 */
UpdateClusters::UpdateClusters(
			random_engine_t & generator,
			distribution_t & likelihood,
			distribution_t & nonparametrics
		): 
			_generator(generator),
			_likelihood(likelihood),
			_nonparametrics(nonparametrics),
			_distribution(0.0, 1.0)
{
	_verbosity = 5;
}

Suffies * UpdateClusters::propose() {
	// proposed parameters (can also be Brownian walk)
	return ((dirichlet_process&)_nonparametrics).sample_base(_generator);
}

/**
 * Update cluster parameters. This function should leave the number of clusters invariant. The membership matrix should 
 * not be different after the function has been executed. There are not more, not less clusters. Only the parameters
 * of the clusters are updated. The update of the parameters uses the data by shifting the prior towards the 
 * likelihood, but does not perform maximum likelihood. In this way ergodicity of the chain is maintained and it
 * evolves to the right target distribution.
 *
 * The parameters of a cluster is called "suffies". This does not mean that the algorithm uses conjugacy, only that
 * there is a limited set of parameters that are sufficient to describe the probability density function.
 */
void UpdateClusters::update(
			membertrix & cluster_matrix,
			int number_mh_steps
		) 
{
	double likelihood;
	double proposed_likelihood;

	// This update procedure exists out of a number of MH steps
	for (int t = 0; t < number_mh_steps; ++t) {

		fout << "Update step " << t << endl;

		// here getClusters got corrupted...
		const clusters_t &clusters = cluster_matrix.getClusters();
		for (auto cluster_pair: clusters) {
			auto const &key = cluster_pair.first;
			
			fout << "Update cluster " << key << " parameters" << endl;

			auto const &cluster = cluster_pair.second;

			if (cluster_matrix.empty(key)) {
				// shouldn't happen normally
				foutvar(7) << "This cluster (" << key << ") does not exist!" << endl;
				continue;
			}

			// current likelihood for this cluster, compare with new sample and reuse the _likelihood object
			_likelihood.init(cluster->getSuffies());
			dataset_t *dataset = cluster_matrix.getData(key);
#define USE_LOGARITHMS
#ifdef USE_LOGARITHMS
			likelihood = _likelihood.logprobability(*dataset);
#else
			likelihood = _likelihood.probability(*dataset);
#endif
			fout << "likelihood cluster " << key << " is " << likelihood << endl;
			
			Suffies *proposed_suffies = propose();
			_likelihood.init(*proposed_suffies);
#ifdef USE_LOGARITHMS
			proposed_likelihood = _likelihood.logprobability(*dataset);
#else
			proposed_likelihood = _likelihood.probability(*dataset);
#endif		
			if (!likelihood) {
				// likelihood can be zero at start, in that case accept any proposal
				cluster->setSuffies(*proposed_suffies);
				continue;
			} 

			double alpha;
#ifdef USE_LOGARITHMS
			alpha = std::exp(proposed_likelihood - likelihood);
#else
			alpha = proposed_likelihood / likelihood;
#endif	
			double reject = _distribution(_generator);

			if (reject < alpha) {
				// accept new cluster parameters
				cluster->setSuffies(*proposed_suffies);
			} else {
				// reject, keep cluster as is
			}

		}
	}

}

