// Copyright (c) 2017, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-734707. All Rights
// reserved. See files LICENSE and NOTICE for details.
//
// This file is part of CEED, a collection of benchmarks, miniapps, software
// libraries and APIs for efficient high-order finite element and spectral
// element discretizations for exascale applications. For more information and
// source code availability see http://github.com/ceed.
//
// The CEED research is supported by the Exascale Computing Project (17-SC-20-SC)
// a collaborative effort of two U.S. Department of Energy organizations (Office
// of Science and the National Nuclear Security Administration) responsible for
// the planning and preparation of a capable exascale ecosystem, including
// software, applications, hardware, advanced system engineering and early
// testbed platforms, in support of the nation's exascale computing imperative.

#include "../laghos_assembly.hpp"
#include "kMassPAOperator.hpp"
#include "kernels.hpp"

#ifdef MFEM_USE_MPI

using namespace std;

namespace mfem
{

namespace hydrodynamics
{

// *****************************************************************************
kMassPAOperator::kMassPAOperator(QuadratureData *qd_,
                                 ParFiniteElementSpace &pfes_,
                                 const IntegrationRule &ir_) :
   AbcMassPAOperator(pfes_.GetTrueVSize()),
   dim(pfes_.GetMesh()->Dimension()),
   nzones(pfes_.GetMesh()->GetNE()),
   quad_data(qd_),
   pfes(pfes_),
   fes(static_cast<FiniteElementSpace*>(&pfes_)),
   ir(ir_),
   ess_tdofs_count(0),
   ess_tdofs(0),
   paBilinearForm(new mfem::PABilinearForm(&pfes_)),
   massOperator(NULL)
{
   push(Wheat);
   pop();
}

// *****************************************************************************
void kMassPAOperator::Setup()
{
   push(Wheat);
   // PAMassIntegrator Setup
   mfem::PAMassIntegrator *paMassInteg = new mfem::PAMassIntegrator();
   // No setup, it is done in PABilinearForm::Assemble
   // paMassInteg->Setup(fes,&ir);
   assert(ir);
   paMassInteg->SetIntegrationRule(ir); // in NonlinearFormIntegrator

   // Add mass integretor to PA bilinear form
   paBilinearForm->AddDomainIntegrator(paMassInteg);
   paBilinearForm->Assemble();
   
   // Setup has to be done before, which is done in ->Assemble above
   paMassInteg->SetOperator(quad_data->rho0DetJ0w);

   paBilinearForm->FormOperator(mfem::Array<int>(), massOperator);
   //pop();
}

// *************************************************************************
void kMassPAOperator::SetEssentialTrueDofs(mfem::Array<int> &dofs)
{
   push(Wheat);
   ess_tdofs_count = dofs.Size();
   
   if (ess_tdofs.Size()==0){
#ifdef MFEM_USE_MPI
      int global_ess_tdofs_count;
      const MPI_Comm comm = pfes.GetParMesh()->GetComm();
      MPI_Allreduce(&ess_tdofs_count,&global_ess_tdofs_count,
                    1, MPI_INT, MPI_SUM, comm);
      assert(global_ess_tdofs_count>0);
      ess_tdofs.SetSize(global_ess_tdofs_count);
#else
      assert(ess_tdofs_count>0);
      ess_tdofs.Resize(ess_tdofs_count);
#endif
   } else{
      assert(ess_tdofs_count<=ess_tdofs.Size());
   } 
  
   if (ess_tdofs_count == 0) {
      pop();
      return;
   }
   assert(dofs.GetData());
   dbg("ess_tdofs = dofs");
   ess_tdofs = dofs;
   pop();
}

// *****************************************************************************
void kMassPAOperator::EliminateRHS(mfem::Vector &b)
{
   push(Wheat);
   if (ess_tdofs_count > 0){
      mm::Get().Push(ess_tdofs);
      b.SetSubVector(ess_tdofs, 0.0);
   }
   //b.Print();fflush(0);assert(false);
   pop();
}

// *************************************************************************
void kMassPAOperator::Mult(const mfem::Vector &x,
                                 mfem::Vector &y) const
{
   push(Wheat);
   
   if (distX.Size()!=x.Size()) {
      distX.SetSize(x.Size());
   }
   
   assert(distX.Size()==x.Size());
   //dbg("\033[32;7mXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
   //dbg("x:");x.Print();fflush(0);
   distX = x;


   if (ess_tdofs_count)
   {
      distX.SetSubVector(ess_tdofs, 0.0);
   }

   //distX = 1.123456789123456789;
   //distX.Print(); fflush(0);//assert(false);
   massOperator->Mult(distX, y);
   //y.Print(); fflush(0); //assert(false);

   if (ess_tdofs_count)
   {
      y.SetSubVector(ess_tdofs, 0.0);
   }
   //y.Print(); fflush(0); // assert(false);
   //dbg("\033[32;7mYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY");
   pop();
}

} // namespace hydrodynamics

} // namespace mfem

#endif // MFEM_USE_MPI