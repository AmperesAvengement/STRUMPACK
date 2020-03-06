/*
 * STRUMPACK -- STRUctured Matrices PACKage, Copyright (c) 2014, The
 * Regents of the University of California, through Lawrence Berkeley
 * National Laboratory (subject to receipt of any required approvals
 * from the U.S. Dept. of Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE. This software is owned by the U.S. Department of Energy. As
 * such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * Developers: Pieter Ghysels, Francois-Henry Rouet, Xiaoye S. Li.
 *             (Lawrence Berkeley National Lab, Computational Research
 *             Division).
 */
#include "StrumpackSparseSolverMPIDist.hpp"
#include "misc/TaskTimer.hpp"
#include "sparse/EliminationTreeMPIDist.hpp"
#include "sparse/iterative/IterativeSolversMPI.hpp"
#include "sparse/Redistribute.hpp"

namespace strumpack {

  template<typename scalar_t,typename integer_t>
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::
  StrumpackSparseSolverMPIDist(MPI_Comm comm, bool verbose) :
    StrumpackSparseSolverMPIDist<scalar_t,integer_t>
    (comm, 0, nullptr, verbose) {
  }

  template<typename scalar_t,typename integer_t>
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::
  ~StrumpackSparseSolverMPIDist() = default;

  template<typename scalar_t,typename integer_t>
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::
  StrumpackSparseSolverMPIDist
  (MPI_Comm comm, int argc, char* argv[], bool verbose) :
    StrumpackSparseSolverMPI<scalar_t,integer_t>(comm, argc, argv, verbose) {
    // Set the default reordering to PARMETIS?
    //opts_.set_reordering_method(ReorderingStrategy::PARMETIS);
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::set_matrix
  (const CSRMatrix<scalar_t,integer_t>& A) {
    mat_mpi_ = std::unique_ptr<CSRMatrixMPI<scalar_t,integer_t>>
      (new CSRMatrixMPI<scalar_t,integer_t>(&A, comm_.comm(), true));
    this->factored_ = this->reordered_ = false;
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::set_matrix
  (const CSRMatrixMPI<scalar_t,integer_t>& A) {
    mat_mpi_ = std::unique_ptr<CSRMatrixMPI<scalar_t,integer_t>>
      (new CSRMatrixMPI<scalar_t,integer_t>(A));
    this->factored_ = this->reordered_ = false;
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::set_csr_matrix
  (integer_t N, const integer_t* row_ptr, const integer_t* col_ind,
   const scalar_t* values, bool symmetric_pattern) {
    CSRMatrix<scalar_t,integer_t> mat_seq
      (N, row_ptr, col_ind, values, symmetric_pattern);
    mat_mpi_ = std::unique_ptr<CSRMatrixMPI<scalar_t,integer_t>>
      (new CSRMatrixMPI<scalar_t,integer_t>(&mat_seq, comm_.comm(), true));
    this->factored_ = this->reordered_ = false;
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::set_distributed_csr_matrix
  (integer_t local_rows, const integer_t* row_ptr, const integer_t* col_ind,
   const scalar_t* values, const integer_t* dist, bool symmetric_pattern) {
    mat_mpi_ = std::unique_ptr<CSRMatrixMPI<scalar_t,integer_t>>
      (new CSRMatrixMPI<scalar_t,integer_t>
       (local_rows, row_ptr, col_ind, values, dist,
        comm_.comm(), symmetric_pattern));
    this->factored_ = this->reordered_ = false;
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::set_MPIAIJ_matrix
  (integer_t local_rows, const integer_t* d_ptr, const integer_t* d_ind,
   const scalar_t* d_val, const integer_t* o_ptr, const integer_t* o_ind,
   const scalar_t* o_val, const integer_t* garray) {
    mat_mpi_ = std::unique_ptr<CSRMatrixMPI<scalar_t,integer_t>>
      (new CSRMatrixMPI<scalar_t,integer_t>
       (local_rows, d_ptr, d_ind, d_val, o_ptr, o_ind, o_val,
        garray, comm_.comm()));
    this->factored_ = this->reordered_ = false;
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::setup_reordering() {
    nd_mpi_ = std::unique_ptr<MatrixReorderingMPI<scalar_t,integer_t>>
      (new MatrixReorderingMPI<scalar_t,integer_t>(mat_mpi_->size(), comm_));
  }

  template<typename scalar_t,typename integer_t> int
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::compute_reordering
  (const int* p, int base, int nx, int ny, int nz,
   int components, int width) {
    if (p) return nd_mpi_->set_permutation(opts_, *mat_mpi_, p, base);
    return nd_mpi_->nested_dissection
      (opts_, *mat_mpi_, nx, ny, nz, components, width);
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::separator_reordering() {
    tree_mpi_dist_->separator_reordering(opts_, *mat_mpi_);
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::setup_tree() {
    tree_mpi_dist_ =
      std::unique_ptr<EliminationTreeMPIDist<scalar_t,integer_t>>
      (new EliminationTreeMPIDist<scalar_t,integer_t>
       (opts_, *mat_mpi_, *nd_mpi_, comm_));
  }

  template<typename scalar_t,typename integer_t> ReturnCode
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::solve
  (const DenseM_t& b, DenseM_t& x, bool use_initial_guess) {
    if (!this->factored_ &&
        opts_.Krylov_solver() != KrylovSolver::GMRES &&
        opts_.Krylov_solver() != KrylovSolver::BICGSTAB) {
      ReturnCode ierr = this->factor();
      if (ierr != ReturnCode::SUCCESS) return ierr;
    }
    assert(std::size_t(mat_mpi_->local_rows()) == b.rows());
    assert(b.rows() == x.rows());
    assert(b.cols() == x.cols());
    TaskTimer t("solve");
    this->perf_counters_start();
    t.start();
    auto n_local = x.rows();
    this->Krylov_its_ = 0;

    auto bloc = b;
    if (opts_.matching() == MatchingJob::MAX_DIAGONAL_PRODUCT_SCALING)
      bloc.scale_rows(this->matching_Dr_);

    auto spmv = [&](const scalar_t* x, scalar_t* y) {
      mat_mpi_->spmv(x, y);
    };

    auto gmres =
      [&](const std::function<void(scalar_t*)>& prec) {
        iterative::GMResMPI<scalar_t>
          (comm_, spmv, prec, n_local, x.data(), bloc.data(),
           opts_.rel_tol(), opts_.abs_tol(),
           this->Krylov_its_, opts_.maxit(),
           opts_.gmres_restart(), opts_.GramSchmidt_type(),
           use_initial_guess, opts_.verbose() && is_root_);
      };
    auto bicgstab =
      [&](const std::function<void(scalar_t*)>& prec) {
        iterative::BiCGStabMPI<scalar_t>
          (comm_, spmv, prec, n_local, x.data(), bloc.data(),
           opts_.rel_tol(), opts_.abs_tol(),
           this->Krylov_its_, opts_.maxit(),
           use_initial_guess, opts_.verbose() && is_root_);
      };
    auto MFsolve =
      [&](scalar_t* w) {
        DenseMW_t X(n_local, x.cols(), w, x.ld());
        tree()->multifrontal_solve_dist(X, mat_mpi_->dist());
      };
    auto refine =
      [&]() {
        iterative::IterativeRefinementMPI<scalar_t,integer_t>
          (comm_, *mat_mpi_,
           [&](DenseM_t& w) {
             tree()->multifrontal_solve_dist(w, mat_mpi_->dist()); },
           x, bloc, opts_.rel_tol(), opts_.abs_tol(),
           this->Krylov_its_, opts_.maxit(),
           use_initial_guess, opts_.verbose() && is_root_);
      };

    switch (opts_.Krylov_solver()) {
    case KrylovSolver::AUTO: {
      if (opts_.compression() != CompressionType::NONE && x.cols() == 1)
        gmres(MFsolve);
      else refine();
    }; break;
    case KrylovSolver::REFINE: {
      refine();
    }; break;
    case KrylovSolver::GMRES: {
      assert(x.cols() == 1);
      gmres([](scalar_t*){});
    }; break;
    case KrylovSolver::PREC_GMRES: {
      assert(x.cols() == 1);
      gmres(MFsolve);
    }; break;
    case KrylovSolver::BICGSTAB: {
      assert(x.cols() == 1);
      bicgstab([](scalar_t*){});
    }; break;
    case KrylovSolver::PREC_BICGSTAB: {
      assert(x.cols() == 1);
      bicgstab(MFsolve);
    }; break;
    case KrylovSolver::DIRECT: {
      // TODO bloc is already a copy, avoid extra copy?
      x = bloc;
      tree()->multifrontal_solve_dist(x, mat_mpi_->dist());
    }; break;
    }

    if (opts_.matching() != MatchingJob::NONE)
      // TODO do this in a single routine/comm phase
      for (std::size_t c=0; c<x.cols(); c++)
        permute_vector
          (x.ptr(0,c), this->matching_cperm_, mat_mpi_->dist(), comm_.comm());
    if (opts_.matching() == MatchingJob::MAX_DIAGONAL_PRODUCT_SCALING)
      x.scale_rows(this->matching_Dc_);

    t.stop();
    this->perf_counters_stop("DIRECT/GMRES solve");
    this->print_solve_stats(t);
    return ReturnCode::SUCCESS;
  }

  template<typename scalar_t,typename integer_t> ReturnCode
  StrumpackSparseSolverMPIDist<scalar_t,integer_t>::solve
  (const scalar_t* b, scalar_t* x, bool use_initial_guess) {
    auto N = mat_mpi_->local_rows();
    auto B = ConstDenseMatrixWrapperPtr(N, 1, b, N);
    DenseMW_t X(N, 1, x, N);
    return solve(*B, X, use_initial_guess);
  }

  // explicit template instantiations
  template class StrumpackSparseSolverMPIDist<float,int>;
  template class StrumpackSparseSolverMPIDist<double,int>;
  template class StrumpackSparseSolverMPIDist<std::complex<float>,int>;
  template class StrumpackSparseSolverMPIDist<std::complex<double>,int>;

  template class StrumpackSparseSolverMPIDist<float,long int>;
  template class StrumpackSparseSolverMPIDist<double,long int>;
  template class StrumpackSparseSolverMPIDist<std::complex<float>,long int>;
  template class StrumpackSparseSolverMPIDist<std::complex<double>,long int>;

  template class StrumpackSparseSolverMPIDist<float,long long int>;
  template class StrumpackSparseSolverMPIDist<double,long long int>;
  template class StrumpackSparseSolverMPIDist<std::complex<float>,long long int>;
  template class StrumpackSparseSolverMPIDist<std::complex<double>,long long int>;

} // end namespace strumpack
