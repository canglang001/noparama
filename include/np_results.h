#include <string>
#include <unordered_map>

#include "membertrix.h"

typedef std::unordered_map<int, data_id_t> ground_truth_t;

class Results {
    private:
        const membertrix & _membertrix;

        ground_truth_t & _ground_truth;

        double _rand_index;

        double _adjusted_rand_index;

        int _verbosity;
    public:
        Results(const membertrix &membertrix, ground_truth_t & ground_truth);

        void calculateSimilarity();

        void write(const std::string & path, const std::string & basename);
};