
/*
 Copyright (C) 2003 Ferdinando Ametrano
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it under the
 terms of the QuantLib license.  You should have received a copy of the
 license along with this program; if not, please email quantlib-dev@lists.sf.net
 The license is also available online at http://quantlib.org/html/license.html

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file multipathgenerator.hpp
    \brief Generates a multi path from a random-array generator
*/

#ifndef quantlib_montecarlo_multi_path_generator_h
#define quantlib_montecarlo_multi_path_generator_h

#include <ql/diffusionprocess.hpp>
#include <ql/MonteCarlo/multipath.hpp>
#include <ql/MonteCarlo/sample.hpp>

namespace QuantLib {

    //! Generates a multipath from a random number generator
    /*! MultiPathGenerator<RAG> is a class that returns a random multi path.
        RAG is a sample generator which returns a random array.
        It must have the minimal interface:
        \code
        RAG{
            RAG();
            RAG(Matrix& covariance,
                long seed);
            Sample<Array> next();
        };        
        \endcode
    */
    template <class SG>
    class MultiPathGenerator {
      public:
        typedef Sample<MultiPath> sample_type;        
        MultiPathGenerator(const std::vector<Handle<DiffusionProcess> >& diffusionProcs,
                           const Array& drifts,
                           const Matrix& covariance,
                           const TimeGrid& timeGrid,
                           SG generator);
        const sample_type& next() const;
        const sample_type& antithetic() const;
      private:
        std::vector<Handle<DiffusionProcess> > diffusionProcs_;
        Size numAssets_;
        Matrix sqrtCovariance_;
        SG generator_;        
        mutable sample_type next_;        
    };

    
    // constructor
    template <class SG>
    inline MultiPathGenerator<SG>::MultiPathGenerator(
                                const std::vector<Handle<DiffusionProcess> >& diffusionProcs,
                                const Array& drifts, 
                                const Matrix& covariance,
                                const TimeGrid& times, 
                                SG generator)
    :   diffusionProcs_(diffusionProcs),
        numAssets_(covariance.rows()), 
        sqrtCovariance_(matrixSqrt(covariance)),
        generator_(generator),
        next_(MultiPath(covariance.rows(), times), 1.0) {

        QL_REQUIRE(generator_.dimension() == numAssets_*(times.size()-1),
                   "(2) MultiPathGenerator's dimension (" +
                   IntegerFormatter::toString(generator_.dimension()) + 
                   ") is not equal to (" +
                   IntegerFormatter::toString(numAssets_) + 
                   " * " +
                   IntegerFormatter::toString(times.size()-1) + 
                   ") the number of assets times the number of time steps");
        QL_REQUIRE(drifts.size() == numAssets_,
                   "MultiPathGenerator covariance and average "
                   "do not have the same size");
        QL_REQUIRE(sqrtCovariance_.columns() == numAssets_,
                   "MultiPathGenerator covariance is not "
                   "a square matrix");
        QL_REQUIRE(times.size() > 1,
                   "MultiPathGenerator: no times given");

        for (Size j=0; j<numAssets_; j++) {
            for (Size i = 0; i< times.size()-1; i++) {
                next_.value[j].drift()[i] = drifts[j] * 
                    next_.value[j].timeGrid().dt(i);
            }
        }
    }


    // next()
    template <class SG>
    inline const typename MultiPathGenerator<SG>::sample_type&
    MultiPathGenerator<SG>::next() const {
        typedef typename SG::sample_type sequence_type;
        const sequence_type& sequence_ = generator_.nextSequence();

        Array asset(numAssets_);
        Array temp(numAssets_);
        next_.weight = sequence_.weight;

        for (Size j = 0; j < numAssets_; j++) {
            asset[j] = diffusionProcs_[j]->x0();
        }

        TimeGrid timeGrid = next_.value[0].timeGrid();
        double dt;
        double sqrt_dt;
        Time t;            
        for (Size i = 0; i < next_.value[0].size(); i++) {
            Size offset = i*numAssets_;
            t = timeGrid[i+1];
            dt = timeGrid.dt(i);            
            sqrt_dt = QL_SQRT(dt);
            std::copy(sequence_.value.begin()+offset,
                      sequence_.value.begin()+offset+numAssets_,
                      temp.begin());
            // this needs to just be a rotation, as the time 
            // and covariance are in the diffusion process
            temp = sqrtCovariance_ * temp;
            temp[0] = temp[0]/ QL_SQRT(sqrtCovariance_[0][0]*sqrtCovariance_[0][0] + 
                                    sqrtCovariance_[0][1]*sqrtCovariance_[0][1]);
            temp[1] = temp[1]/ QL_SQRT(sqrtCovariance_[1][0]*sqrtCovariance_[1][0] + 
                                    sqrtCovariance_[1][1]*sqrtCovariance_[1][1]);
            for (Size j=0; j<numAssets_; j++) {
                next_.value[j].drift()[i] = dt * diffusionProcs_[j]->drift(t, asset[j]);
                next_.value[j].diffusion()[i] = - temp[j] * QL_SQRT(diffusionProcs_[j]->variance(t, asset[j], dt));
                //next_.value[j].diffusion()[i] = - sqrt_dt * temp[j];
                asset[j] *= QL_EXP(next_.value[j].drift()[i] + next_.value[j].diffusion()[i]);                    
            }
        }
        
        return next_;
    }

    // antithetic()
    template <class SG>
    inline const typename MultiPathGenerator<SG>::sample_type&
    MultiPathGenerator<SG>::antithetic() const {

        // brownian bridge?
                
        return next();
        
    }







    //! Generates a multipath from a random number generator
    /*! MultiPathGenerator_old<RAG> is a class that returns a random
        multi path.
        RAG is a sample generator which returns a random array.
        It must have the minimal interface:
        \code
        RAG{
            RAG();
            RAG(Matrix& covariance,
                long seed);
            Sample<Array> next();
        };
        \endcode
    */
    template <class RAG>
    class MultiPathGenerator_old {
      public:
        typedef Sample<MultiPath> sample_type;
        MultiPathGenerator_old(const Array& drifts,
                               const Matrix& covariance,
                               Time length,
                               Size timeSteps,
                               long seed);
        MultiPathGenerator_old(const Array& drifts,
                               const Matrix& covariance,
                               const TimeGrid& times,
                               long seed=0);
        const sample_type& next() const;
        const sample_type& antithetic() const {
            throw Error("old framework doesn't support antithetic here");}
      private:
        Size numAssets_;
        RAG rndArrayGen_;
        mutable sample_type next_;
        std::vector<Time> timeDelays_;
    };


    template <class RAG>
    inline MultiPathGenerator_old<RAG >::MultiPathGenerator_old(
                                const Array& drifts, const Matrix& covariance,
                                Time length, Size timeSteps, long seed)
    : numAssets_(covariance.rows()),
      rndArrayGen_(covariance, seed),
      next_(MultiPath(covariance.rows(),timeSteps),1.0) {

        QL_REQUIRE(drifts.size() == numAssets_,
                   "MultiPathGenerator_old covariance and average "
                   "do not have the same size");
        QL_REQUIRE(timeSteps > 0, "MultiPathGenerator_old: Time steps(" +
                   IntegerFormatter::toString(timeSteps) +
                   ") must be greater than zero");
        QL_REQUIRE(length > 0, "MultiPathGenerator_old: length must be > 0");
        Time dt = length/timeSteps;
        timeDelays_ = std::vector<Time>(timeSteps, dt);
        Array variances = covariance.diagonal();
        for (Size j=0; j<numAssets_; j++) {
            QL_REQUIRE(variances[j]>=0, 
                       "MultiPathGenerator_old: negative variance");
            for (Size i=0; i<timeSteps; i++) {
                next_.value[j].TimeGrid()[i] = (i+1)*dt;
                next_.value[j].drift()[i]=drifts[j]*timeDelays_[i];
            }
        }

    }

    template <class RAG>
    inline MultiPathGenerator_old<RAG >::MultiPathGenerator_old(
                                const Array& drifts, const Matrix& covariance,
                                const TimeGrid& times, long seed)
    : numAssets_(covariance.rows()),
      rndArrayGen_(covariance, seed),
      next_(MultiPath(covariance.rows(), times),1.0),
      timeDelays_(times.size()-1) {

        QL_REQUIRE(drifts.size() == numAssets_,
                   "MultiPathGenerator_old covariance and average "
                   "do not have the same size");
        QL_REQUIRE(times.size() > 1,
                   "MultiPathGenerator_old: no times given");
        QL_REQUIRE(times[0] >= 0, "MultiPathGenerator_old: first time(" +
                   DoubleFormatter::toString(times[0]) +
                   ") must be non negative");
        Array variances = covariance.diagonal();
        for(Size i = 1; i < times.size(); i++) {
            QL_REQUIRE(times[i] >= times[i-1],
                       "MultiPathGenerator_old: time(" +
                       IntegerFormatter::toString(i-1)+")=" +
                       DoubleFormatter::toString(times[i-1]) +
                       " is later than time(" +
                       IntegerFormatter::toString(i) + ")=" +
                       DoubleFormatter::toString(times[i]));
            timeDelays_[i-1] = times[i] - times[i-1];
        }

        for (Size j=0; j<numAssets_; j++) {
            QL_REQUIRE(variances[j]>=0, 
                       "MultiPathGenerator_old: negative variance");
            for (Size i = 0; i< times.size()-1; i++) {
                next_.value[j].drift()[i] = drifts[j] * timeDelays_[i];
            }
        }
    }

    template <class RAG>
    inline  const typename MultiPathGenerator_old<RAG >::sample_type&
    MultiPathGenerator_old<RAG >::next() const {

        Array randomExtraction(numAssets_);
        next_.weight = 1.0;
        for (Size i = 0; i < next_.value[0].size(); i++) {
            const Sample<Array>& randomExtraction = rndArrayGen_.next();
            next_.weight *= randomExtraction.weight;
            for (Size j=0; j<numAssets_; j++) {
                next_.value[j].diffusion()[i] =
                    randomExtraction.value[j] * QL_SQRT(timeDelays_[i]);
            }
        }
        return next_;
    }

}


#endif
