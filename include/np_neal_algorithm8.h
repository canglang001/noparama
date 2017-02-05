#pragma once

#include <random>

#include <statistics/distribution.h>

#include <membertrix>

#include <np_update_cluster_population.h>

/**
 * This class NealAlgorithm8 updates the cluster population as ...
 *
 */
class NealAlgorithm8: public UpdateClusterPopulation {
	private:
		distribution_t _likelihood;
		
		distribution_t _nonparametrics;

		double _alpha;
	public:
		/*!
		 * Construct update method for cluster population.
		 *
		 * @param[in] likelihood                 Likelihood function to be used in update()
		 * @param[in] nonparametrics             Sufficient statistics of the nonparametric prior (e.g. Dirichlet)
		 */
		NealAlgorithm8(
			distribution_t & likelihood,
			distribution_t & nonparametrics
		);

		/*!
		 * Update the cluster population. The observation has to be deleted beforehand.
		 *
		 * @param[inout] cluster_matrix          Cluster-observation membership matrix
		 * @param[in] data_id                    Observation to be considered for existing and new cluster
		 */
		void update(
			membertrix & cluster_matrix,
			data_id_t data_id
		);
};
