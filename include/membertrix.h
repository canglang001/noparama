#pragma once

#include <Eigen/Dense>
#include <np_data.h>
#include <np_cluster.h>

enum np_error_t { error_none, error_already_assigned, error_assignment_remaining, error_assignment_absent };
	
/*!
 * The binary matrix is currently defined as a dense matrix. This needs actually some profiling to know if a sparse 
 * matrix is faster. Namely, the content of the matrix is sparse with one non-false value per row. However, for now
 * a dense matrix is used, because it is assumed that how rows and columns are accessed favors a dense matrix.
 */
typedef Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic> binary_matrix_t;

/*!
 * The membertrix data structure is a binary matrix optimimized for storing membership information.
 * The membership is asymmetric. A data item can be assigned to only one cluster. In contrary, a cluster can have
 * multiple data points as members.
 *
 * The structure is stored with clusters as columns, and data items as rows. Reasons:
 *   + Eigen by default uses column-major storage [1]
 *   + The getData() function retrieves multiple data items
 *   + The getCluster() function can return on first item found
 *
 * References:
 *   [1] https://eigen.tuxfamily.org/dox/group__TopicStorageOrders.html
 */
class membertrix {
	private:
		// membership matrix
		binary_matrix_t _membership_matrix;

		// map from cluster pointers to column entries
		clusters_t _cluster_objects; 

		// map from data pointers to row entries
		dataset_t _data_objects; 

		// store data items per cluster
		clusters_dataset_t _clusters_dataset;
	public:

		membertrix();

		/*!
		 * Add a cluster to the membership matrix. The cluster is not physically stored, only a reference is kept. If
		 * the memory is deallocated, errors can be expected.
		 *
		 * The returned index should be kept as a reference for use in the functions assign() and retract().
		 *
		 * @param[in] cluster_t			A cluster object
		 * @return						An index to the given cluster object
		 */
		cluster_id_t addCluster(cluster_t & cluster);

		/*!
		 * Add a data point to the membership matrix. The data are not physically stored, only a reference is kept. If
		 * the memory is deallocated, errors can be expected.
		 *
		 * The returned index should be kept as a reference for use in the functions assign() and retract().
		 *
		 * @param[in] data_t			A data object
		 * @return						An index to the given data object
		 */
		data_id_t addData(data_t & data);

		np_error_t assign(cluster_id_t & cluster_id, data_id_t & data_id);
		
		np_error_t retract(cluster_id_t & cluster_id, data_id_t & data_id);

		np_error_t retract(data_id_t & data_id);

		cluster_id_t getCluster(data_id_t & data_id);

		clusters_t & getClusters();

		/*!
		 * Return all data points that are assigned to a particular cluster.
		 *
		 * @param[in] cluster_id		An index to a particular cluster
		 * @return						A dataset (vector) of data points that have been assigned through assign()
		 */
		dataset_t & getData(cluster_id_t & cluster_id);
};

