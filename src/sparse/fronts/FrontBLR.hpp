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
 *
 */
#ifndef FRONTAL_MATRIX_BLR_HPP
#define FRONTAL_MATRIX_BLR_HPP

#include "Front.hpp"
#include "BLR/BLRMatrix.hpp"

namespace strumpack {

  template<typename scalar_t,typename integer_t> class FrontBLRMPI;

  template<typename scalar_t,typename integer_t> class FrontBLR
    : public Front<scalar_t,integer_t> {
    using DenseM_t = DenseMatrix<scalar_t>;
    using DenseMW_t = DenseMatrixWrapper<scalar_t>;
    using SpMat_t = CompressedSparseMatrix<scalar_t,integer_t>;
    using Opts_t = SPOptions<scalar_t>;
    using F_t = Front<scalar_t,integer_t>;
    using BLRM_t = BLR::BLRMatrix<scalar_t>;
#if defined(STRUMPACK_USE_MPI)
    using FMPI_t = FrontMPI<scalar_t,integer_t>;
    using FBLRMPI_t = FrontBLRMPI<scalar_t,integer_t>;
#endif

  public:
    FrontBLR(integer_t sep, integer_t sep_begin, integer_t sep_end,
             std::vector<integer_t>& upd);
    ~FrontBLR() {}

    void release_work_memory(VectorPool<scalar_t>& workspace) override;

    void build_front_cols(const SpMat_t& A, std::size_t i,
                          bool part, std::size_t CP,
                          const std::vector<Triplet<scalar_t>>& e11,
                          const std::vector<Triplet<scalar_t>>& e12,
                          const std::vector<Triplet<scalar_t>>& e21,
                          int task_depth, const Opts_t& opts);

    std::size_t get_device_F22_worksize() override { return 0; };
    scalar_t* get_device_F22(scalar_t*) override;

    void extend_add_to_dense(DenseM_t& paF11, DenseM_t& paF12,
                             DenseM_t& paF21, DenseM_t& paF22,
                             const F_t* p, int task_depth) override;
    void extend_add_to_dense(DenseM_t& paF11, DenseM_t& paF12,
                             DenseM_t& paF21, DenseM_t& paF22,
                             const F_t* p, VectorPool<scalar_t>& workspace,
                             int task_depth) override;
    void extend_add_to_blr(BLRM_t& paF11, BLRM_t& paF12,
                           BLRM_t& paF21, BLRM_t& paF22, const F_t* p,
                           VectorPool<scalar_t>& workspace,
                           int task_depth, const Opts_t& opts) override;
    void extend_add_to_blr_col(BLRM_t& paF11, BLRM_t& paF12,
                               BLRM_t& paF21, BLRM_t& paF22, const F_t* p,
                               integer_t begin_col, integer_t end_col,
                               int task_depth, const Opts_t& opts) override;
    void sample_CB(const Opts_t& opts, const DenseM_t& R, DenseM_t& Sr,
                   DenseM_t& Sc, F_t* pa, int task_depth) override;

    ReturnCode factor(const SpMat_t& A, const Opts_t& opts,
                      VectorPool<scalar_t>& workspace,
                      int etree_level=0, int task_depth=0) override;

    ReturnCode factor_node(const SpMat_t& A, const Opts_t& opts,
                           VectorPool<scalar_t>& workspace,
                           int etree_level=0, int task_depth=0);

    void extract_CB_sub_matrix(const std::vector<std::size_t>& I,
                               const std::vector<std::size_t>& J,
                               DenseM_t& B, int task_depth) const override;

    std::string type() const override { return "FrontBLR"; }

#if defined(STRUMPACK_USE_MPI)
    void
    extend_add_copy_to_buffers(std::vector<std::vector<scalar_t>>& sbuf,
                               const FMPI_t* pa) const override;
    void
    extadd_blr_copy_to_buffers(std::vector<std::vector<scalar_t>>& sbuf,
                               const FBLRMPI_t* pa) const override;
    void
    extadd_blr_copy_to_buffers_col(std::vector<std::vector<scalar_t>>& sbuf,
                                   const FBLRMPI_t* pa,
                                   integer_t begin_col, integer_t end_col,
                                   const Opts_t& opts) const override;
#endif

    void partition(const Opts_t& opts, const SpMat_t& A, integer_t* sorder,
                   bool is_root=true, int task_depth=0) override;

  private:
    BLRM_t F11blr_, F12blr_, F21blr_, F22blr_;
    DenseMW_t F22_;
#if defined(STRUMPACK_USE_GPU)
    gpu::DeviceMemory<char> CBdev_;
#endif
    std::vector<scalar_t,NoInit<scalar_t>> CBstorage_;
    std::vector<std::size_t> sep_tiles_, upd_tiles_;
    DenseMatrix<bool> admissibility_;

    FrontBLR(const FrontBLR&) = delete;
    FrontBLR& operator=(FrontBLR const&) = delete;

    void fwd_solve_phase2(DenseM_t& b, DenseM_t& bupd,
                          int etree_level, int task_depth) const override;
    void bwd_solve_phase1(DenseM_t& y, DenseM_t& yupd,
                          int etree_level, int task_depth) const override;

    void draw_node(std::ostream& of, bool is_root) const override;

    long long node_factor_nonzeros() const override;

    virtual ReturnCode node_subnormals(std::size_t& ns,
                                       std::size_t& nz) const override;

    using F_t::lchild_;
    using F_t::rchild_;
    using F_t::dim_sep;
    using F_t::dim_upd;
    using F_t::sep_begin_;
    using F_t::sep_end_;

    // suppress warnings
    using F_t::extend_add_to_dense;
    using F_t::sample_CB;
    using F_t::forward_multifrontal_solve;
    using F_t::backward_multifrontal_solve;
  };

} // end namespace strumpack

#endif
