/**
 * Multivariate Normal distribution sampling using C++11 and Eigen matrices.
 * 
 * This is taken from http://stackoverflow.com/questions/16361226/error-while-creating-object-from-templated-class
 * (also see http://lost-found-wandering.blogspot.fr/2011/05/sampling-from-multivariate-normal-in-c.html)
 * 
 * I have been unable to contact the original author, and I've performed
 * the following modifications to the original code:
 * - removal of the dependency to Boost, in favor of straight C++11;
 * - ability to choose from Solver or Cholesky decomposition (supposedly faster);
 * - fixed Cholesky by using LLT decomposition instead of LDLT that was not yielding
 *   a correctly rotated variance 
 *   (see this http://stats.stackexchange.com/questions/48749/how-to-sample-from-a-multivariate-normal-given-the-pt-ldlt-p-decomposition-o )
 */

/**
 * Copyright (c) 2014 by Emmanuel Benazera beniz@droidnik.fr, All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.
 */

#ifndef __MultivariateNormal_HPP
#define __MultivariateNormal_HPP

#include <Eigen/Dense>
#include <random>
#include <ctime>
#include <cmath>

/*
  We need a functor that can pretend it's const,
  but to be a good random number generator 
  it needs mutable state.  The standard Eigen function 
  Random() just calls rand(), which changes a global
  variable.
*/
namespace Eigen {

  namespace internal {

    template<typename Scalar>
    struct scalar_normal_dist_op
    {
      static std::mt19937 rng;                        // The uniform pseudo-random algorithm

      // std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::microseconds> time_val;
      // uint64_t seed_val;
      struct timespec ts;

      mutable std::normal_distribution<Scalar> norm; // gaussian combinator
      
      EIGEN_EMPTY_STRUCT_CTOR(scalar_normal_dist_op)

      template<typename Index>
      inline const Scalar operator() (Index, Index = 0) const { return norm(rng); }

      inline void seed() {
        timespec_get(&ts, TIME_UTC);
        // std::cout << (uint64_t) ts.tv_nsec << std::endl;
        rng.seed((uint64_t) ts.tv_nsec);
      }
    };

    template<typename Scalar>
    std::mt19937 scalar_normal_dist_op<Scalar>::rng;
      
    template<typename Scalar>
    struct functor_traits<scalar_normal_dist_op<Scalar> >
    { 
      enum { 
        Cost = 50 * NumTraits<Scalar>::MulCost, 
        PacketAccess = false, 
        IsRepeatable = false
      }; 
    };

  } // end namespace internal

  /**
    Find the eigen-decomposition of the covariance matrix
    and then store it for sampling from a multi-variate normal 
  */
  template<typename Scalar>
  class MultivariateNormal
  {

    private:
      
      Matrix<Scalar,Dynamic,Dynamic> _covar;
      Matrix<Scalar,Dynamic,Dynamic> _transform;
      Matrix< Scalar, Dynamic, 1> _mean;
      internal::scalar_normal_dist_op<Scalar> randN; // Gaussian functor
      bool _use_cholesky;
      SelfAdjointEigenSolver<Matrix<Scalar,Dynamic,Dynamic> > _eigenSolver; // drawback: this creates a useless eigenSolver when using Cholesky decomposition, but it yields access to eigenvalues and vectors
    
    public:

      MultivariateNormal(
        const Matrix<Scalar,Dynamic,1>& mean,
        const Matrix<Scalar,Dynamic,Dynamic>& covar,
        const bool use_cholesky = false
      )
      : _use_cholesky(use_cholesky)
      {
        setMean(mean);
        setCovar(covar);
      }

      inline void setMean(
        const Matrix<Scalar,Dynamic,1>& mean
      ) {
        _mean = mean;
      }

      void setCovar(
        const Matrix<Scalar,Dynamic,Dynamic>& covar
      ) {
        _covar = covar;
        
        // Assuming that we'll be using this repeatedly,
        // compute the transformation matrix that will
        // be applied to unit-variance independent normals
        
        if (_use_cholesky) {
          Eigen::LLT<Eigen::Matrix<Scalar,Dynamic,Dynamic> > cholSolver(_covar);
          // We can only use the cholesky decomposition if 
          // the covariance matrix is symmetric, pos-definite.
          // But a covariance matrix might be pos-semi-definite.
          // In that case, we'll go to an EigenSolver
          if (cholSolver.info()==Eigen::Success) {
            // Use cholesky solver
            _transform = cholSolver.matrixL();
          } else {
            throw std::runtime_error("Failed computing the Cholesky decomposition. Use solver instead");
          }
        } else {
          _eigenSolver = SelfAdjointEigenSolver<Matrix<Scalar,Dynamic,Dynamic> >(_covar);
          _transform = _eigenSolver.eigenvectors()*_eigenSolver.eigenvalues().cwiseMax(0).cwiseSqrt().asDiagonal();
        }
      }

      /// Draw nn samples from the gaussian and return them
      /// as columns in a Dynamic by nn matrix
      Matrix<Scalar,Dynamic,-1> sample(
        int nn
      ) {
        this->randN.seed();
        return (_transform * Matrix<Scalar,Dynamic,-1>::NullaryExpr(_covar.rows(),nn,randN)).colwise() + _mean;
      }

      // Vector<Scalar, Dynamic> pdf(
      //   const Matrix<Scalar, Dynamic, Dynamic> & X,     // particles
      //   const Matrix<Scalar, Dynamic, 1> & mu,          // mean
      //   const Matrix<Scalar, Dynamic, Dynamic> & Sigma  // covariance
      // ) const {

      // }

  }; // end class MultivariateNormal


/*  
  Evaluate the normal PDF specified by mean and covariance for a set of 
  multivariate samples
*/
template<typename Scalar>
void mvnpdf(
  const Matrix<Scalar, Dynamic, Dynamic> & X,     // particles
  const Matrix<Scalar, Dynamic, 1> & mu,          // mean
  const Matrix<Scalar, Dynamic, Dynamic> & Sigma,  // covariance
  Matrix<Scalar, Dynamic, 1> * p
) {
    // todo check size
  Matrix<Scalar, Dynamic, Dynamic> inv_Sigma = Sigma.inverse();
  auto diff = X - mu.rowwise().replicate(X.cols());
  // auto det_Sigma = Sigma.determinant();
  // Scalar two_pi_k = 
  Scalar scale = 1.0 / std::sqrt( (Scalar) std::pow( (Scalar) 2*3.1415926535897932384626433832795029, (Scalar) X.rows()) * Sigma.determinant());
  for (int i=0; i<X.cols(); i++) {
    (*p)(i) = scale * std::exp( -0.5 * diff.col(i).transpose() * inv_Sigma * diff.col(i) );
  }
}



} // end namespace Eigen

#endif
