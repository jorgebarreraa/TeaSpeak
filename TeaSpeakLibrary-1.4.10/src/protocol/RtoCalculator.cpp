//
// Created by WolverinDEV on 03/05/2022.
//

#include "RtoCalculator.h"
#include <cmath>

using namespace ts::protocol;

RtoCalculator::RtoCalculator() {
    this->reset();
}

void RtoCalculator::reset() {
    this->rto = 1000;
    this->srtt = -1;
    this->rttvar = 0;
}

/* we're not taking the clock granularity into account because its nearly 1ms and it would only add more branches  */
void RtoCalculator::update(float r) {
    if(srtt == -1) {
        this->srtt = (float) r;
        this->rttvar = r / 2.f;
        this->rto = srtt + 4 * this->rttvar;
    } else {
        this->rttvar = (1.f - alpha) * this->rttvar + beta * std::abs(this->srtt - r);
        this->srtt = (1.f - alpha) * srtt + alpha * r;
        this->rto = std::max(200.f, this->srtt + 4 * this->rttvar);
    }
}