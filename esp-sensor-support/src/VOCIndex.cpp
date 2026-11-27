#include "support.h"

#include "VOCIndex.h"

#include "sensirion_gas_index_algorithm.h"

struct VOCIndex::Params {
    GasIndexAlgorithmParams params;
};

VOCIndex::VOCIndex() : _params(std::make_unique<Params>()) {
    GasIndexAlgorithm_init(&_params->params, GasIndexAlgorithm_ALGORITHM_TYPE_VOC);
}

VOCIndex::~VOCIndex() = default;

int32_t VOCIndex::process(uint16_t raw) {
    // The algorithm counts its uptime up to the blackout's end without computing an index, as
    // GasIndexAlgorithm_process has it.
    _blackout = _params->params.mUptime <= GasIndexAlgorithm_INITIAL_BLACKOUT;

    int32_t index;
    GasIndexAlgorithm_process(&_params->params, raw, &index);

    return index;
}

void VOCIndex::reset() {
    GasIndexAlgorithm_reset(&_params->params);

    _blackout = true;
}
