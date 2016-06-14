/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*!
 Copyright (C) 2016 Michael von den Driesch

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file multicurvesensitivity.hpp
\brief compute sensitivities based of traits (zeroyield, discount, forward) to the input
input instruments (par quotes).
*/

#ifndef quantlib_multicurve_sensitivity_hpp
#define quantlib_multicurve_sensitivity_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <boost/shared_ptr.hpp>

namespace {
QuantLib::Real secondElement(const std::pair< QuantLib::Date, QuantLib::Real > &p) { return p.second; }
}

namespace QuantLib {

//! Multi curve sensitivities
/*! This class provides a simple (yet most likely not the fastest) way to create sensitivities
  to the <em>par quotes</em>, provided in the piecewiseyieldcurve for stripping. If constructed with more
than one curve, the class iterates over all quotes of the provided curves and shifts each quote of all provided curves taking interdependence into account.

The class computes the sensitvities as a QuantLib Matrix class in the form:
\f[
\frac{\partial z_i}{\partial q_j}
\f]
where the \f$(z_i)_{i\in \{1,...,n\}}\f$'s are the implied <em>values</em> (being the traits used during curve constructions, e.g. ZeroYield, Discountfactors or ForwardRates) and the the \f$(q_i)_{i\in \{1,...,n\}}\f$'s are the quoted par rates.


\note It's the users job to provide all curves that <em>influence</em> the implied rates.

    \ingroup yieldtermstructures

*/
class MultiCurveSensitivities : public LazyObject {
private:
  typedef std::map< std::string, Handle< YieldTermStructure > > curvespec;

public:
  //! Multi curve sensitivties
  /*! @param curves std::map of string (curve name) and handle to piecewiseyieldcurve
  */

  MultiCurveSensitivities(curvespec curves) : curves_(curves) {
    for (curvespec::const_iterator it = curves_.begin(); it != curves_.end(); ++it)
      registerWith((*it).second);
    for (curvespec::const_iterator it = curves_.begin(); it != curves_.end(); ++it) {
      boost::shared_ptr< PiecewiseYieldCurve< ZeroYield, Linear > > curve =
          boost::dynamic_pointer_cast< PiecewiseYieldCurve< ZeroYield, Linear > >(it->second.currentLink());
      QL_REQUIRE(curve != NULL, "Couldn't cast curvename: " << it->first);
      for (std::vector< boost::shared_ptr< BootstrapHelper< YieldTermStructure > > >::iterator it =
               curve->instruments_.begin();
           it != curve->instruments_.end(); ++it) {
        allQuotes_.push_back((*it)->quote());
      }
    }
  }

  Matrix sensitivities() const;
  Matrix inverseSensitivities() const;

private:
  //! \name LazyObject interface
  //@{
  void performCalculations() const;
  //@}
  // methods
  std::vector< Real > allZeros() const;
  std::vector< std::pair< Date, Real > > allNodes() const;
  mutable std::vector< Rate > origZeros_;
  std::vector< Handle< Quote > > allQuotes_;
  std::vector< std::pair< Date, Real > > origNodes_;
  mutable Matrix sensi_, invSensi_;
  curvespec curves_;
};

void MultiCurveSensitivities::performCalculations() const {
  std::vector< Rate > sensiVector;
  origZeros_ = allZeros();
  for (std::vector< Handle< Quote > >::const_iterator it = allQuotes_.begin(); it != allQuotes_.end(); ++it) {
    Rate bps = +1e-4;
    Rate origQuote = (*it)->value();
    boost::shared_ptr< SimpleQuote > q = boost::dynamic_pointer_cast< SimpleQuote >((*it).currentLink());
    q->setValue(origQuote + bps);
    try {
      std::vector< Rate > tmp(allZeros());
      for (Size i = 0; i < tmp.size(); ++i)
        sensiVector.push_back((tmp[i] - origZeros_[i]) / bps);
      q->setValue(origQuote);
    } catch (...) {
      q->setValue(origQuote);
      QL_FAIL("Application of shift to quote led to exception.");
    }
  }
  Matrix result(origZeros_.size(), origZeros_.size(), sensiVector);
  sensi_ = result;
  invSensi_ = inverse(sensi_);
}

Matrix MultiCurveSensitivities::sensitivities() const {
  calculate();
  return sensi_;
}

Matrix MultiCurveSensitivities::inverseSensitivities() const {
  calculate();
  return invSensi_;
}

std::vector< std::pair< Date, Real > > MultiCurveSensitivities::allNodes() const {
  std::vector< std::pair< Date, Real > > result;
  for (curvespec::const_iterator it = curves_.begin(); it != curves_.end(); ++it) {
    boost::shared_ptr< PiecewiseYieldCurve< ZeroYield, Linear > > curve =
        boost::dynamic_pointer_cast< PiecewiseYieldCurve< ZeroYield, Linear > >(it->second.currentLink());
    result.reserve(result.size() + curve->nodes().size() - 1);
    for (auto p = curve->nodes().begin() + 1; p != curve->nodes().end(); ++p)
      result.push_back(*p);
  }
  return result;
}

std::vector< Real > MultiCurveSensitivities::allZeros() const {
  std::vector< std::pair< Date, Real > > result = allNodes();
  std::vector< Real > zeros;
  std::transform(result.begin(), result.end(), std::back_inserter(zeros), secondElement);
  return zeros;
}
}

#endif
