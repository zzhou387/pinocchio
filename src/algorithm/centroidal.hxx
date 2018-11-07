//
// Copyright (c) 2015-2018 CNRS INRIA
//
// This file is part of Pinocchio
// Pinocchio is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
//
// Pinocchio is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Lesser Public License for more details. You should have
// received a copy of the GNU Lesser General Public License along with
// Pinocchio If not, see
// <http://www.gnu.org/licenses/>.

#ifndef __se3_centroidal_hxx__
#define __se3_centroidal_hxx__

#include "pinocchio/multibody/visitor.hpp"
#include "pinocchio/spatial/act-on-set.hpp"
#include "pinocchio/algorithm/kinematics.hpp"
#include "pinocchio/algorithm/check.hpp"

/// @cond DEV

namespace se3
{
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl,
  typename ConfigVectorType, typename TangentVectorType1, typename TangentVectorType2 = void>
  struct CentroidalDynamicsForwardStep;
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl,
           typename ConfigVectorType, typename TangentVectorType>
  struct CentroidalDynamicsForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType>
  : public fusion::JointVisitorBase< CentroidalDynamicsForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &,
                                  const ConfigVectorType &,
                                  const TangentVectorType &
                                  > ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data,
                     const Eigen::MatrixBase<ConfigVectorType> & q,
                     const Eigen::MatrixBase<TangentVectorType> & v)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];
      const typename Data::Inertia & Y = model.inertias[i];
      
      jmodel.calc(jdata.derived(),q.derived(), v.derived());
      
      data.liMi[i] = model.jointPlacements[i]*jdata.M();

      data.mass[i] = Y.mass();
      data.com[i] = Y.mass() * Y.lever();
      
      if(parent > 0)
      {
        data.v[i] = data.liMi[i].actInv(data.v[parent]);
        data.v[i] += jdata.v();
      }
      else
        data.v[i] = jdata.v();
      
      data.h[i] = Y * data.v[i];
    }
    
  }; // struct CentroidalDynamicsForwardStep
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl,
           typename ConfigVectorType, typename TangentVectorType1, typename TangentVectorType2>
  struct CentroidalDynamicsForwardStep
  : public fusion::JointVisitorBase< CentroidalDynamicsForwardStep<Scalar,Options,JointCollectionTpl,ConfigVectorType,TangentVectorType1,TangentVectorType2> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
    Data &,
    const ConfigVectorType &,
    const TangentVectorType1 &,
    const TangentVectorType2 &
    > ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data,
                     const Eigen::MatrixBase<ConfigVectorType> & q,
                     const Eigen::MatrixBase<TangentVectorType1> & v,
                     const Eigen::MatrixBase<TangentVectorType2> & a)
    {
      typedef typename Model::JointIndex JointIndex;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];
      const typename Data::Inertia & Y = model.inertias[i];
      
      // Call the similar algo for centroidal dynamics
      CentroidalDynamicsForwardStep<Scalar,Options,JointCollectionTpl,
                                    ConfigVectorType,TangentVectorType1>
      ::algo(jmodel.derived(),jdata.derived(),model,data,
             q.derived(),v.derived());
      
      data.a[i] = jdata.S() * jmodel.jointVelocitySelector(a)
                + jdata.c() + (data.v[i] ^ jdata.v());
      if(parent > 0)
        data.a[i] += data.liMi[i].actInv(data.a[parent]);
      
      data.f[i] = Y * data.a[i] + data.v[i].cross(data.h[i]);
    }
    
  }; // struct CentroidalDynamicsForwardStep
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl,
           typename ConfigVectorType, typename TangentVectorType>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::Force &
  computeCentroidalDynamics(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                            DataTpl<Scalar,Options,JointCollectionTpl> & data,
                            const Eigen::MatrixBase<ConfigVectorType> & q,
                            const Eigen::MatrixBase<TangentVectorType> & v)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    assert(v.size() == model.nv && "The velocity vector is not of right size");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    typedef typename Model::JointIndex JointIndex;
    
    typedef CentroidalDynamicsForwardStep<Scalar,Options,JointCollectionTpl,
                                        ConfigVectorType,TangentVectorType> Pass1;
    
    for(JointIndex i=1; i<(JointIndex)(model.njoints); ++i)
    {
      Pass1::run(model.joints[i],data.joints[i],
                 typename Pass1::ArgsType(model,data,q.derived(),v.derived()));
    }
    
    data.mass[0] = 0.; data.com[0].setZero(); data.h[0].setZero();
    for(JointIndex i=(JointIndex)(model.njoints-1); i>0; --i)
    {
      const JointIndex & parent = model.parents[i];
      const typename Data::SE3 & liMi = data.liMi[i];
      
      data.mass[parent] += data.mass[i];
      data.com[parent] += liMi.rotation()*data.com[i] + data.mass[i]*liMi.translation();
      data.h[parent] += data.liMi[i].act(data.h[i]);
    }
    
    data.com[0] /= data.mass[0];
    
    data.hg = data.h[0];
    data.hg.angular() += data.hg.linear().cross(data.com[0]);
    
    return data.hg;
  }
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl,
  typename ConfigVectorType, typename TangentVectorType1, typename TangentVectorType2>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::Force &
  computeCentroidalDynamics(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
                            DataTpl<Scalar,Options,JointCollectionTpl> & data,
                            const Eigen::MatrixBase<ConfigVectorType> & q,
                            const Eigen::MatrixBase<TangentVectorType1> & v,
                            const Eigen::MatrixBase<TangentVectorType2> & a)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    assert(v.size() == model.nv && "The velocity vector is not of right size");
    assert(a.size() == model.nv && "The acceleration vector is not of right size");

    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    typedef typename Model::JointIndex JointIndex;

    typedef CentroidalDynamicsForwardStep<Scalar,Options,JointCollectionTpl,
            ConfigVectorType,TangentVectorType1,TangentVectorType2> Pass1;

    for(JointIndex i=1; i<(JointIndex)(model.njoints); ++i)
    {
      Pass1::run(model.joints[i],data.joints[i],
                 typename Pass1::ArgsType(model,data,q.derived(),v.derived(),a.derived()));
    }

    data.mass[0] = 0.; data.com[0].setZero();
    data.h[0].setZero();
    data.f[0].setZero();

    for(JointIndex i=(JointIndex)(model.njoints-1); i>0; --i)
    {
      const JointIndex & parent = model.parents[i];
      const typename Data::SE3 & liMi = data.liMi[i];

      data.mass[parent] += data.mass[i];
      data.com[parent] += liMi.rotation()*data.com[i] + data.mass[i]*liMi.translation();
      data.h[parent] += data.liMi[i].act(data.h[i]);
      data.f[parent] += data.liMi[i].act(data.f[i]);
    }

    data.com[0] /= data.mass[0];
    
    data.hg = data.h[0];
    data.hg.angular() += data.hg.linear().cross(data.com[0]);
    
    data.dhg = data.f[0];
    data.dhg.angular() += data.dhg.linear().cross(data.com[0]);

    return data.dhg;
  }
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl>
  struct CcrbaBackwardStep
  : public fusion::JointVisitorBase< CcrbaBackwardStep<Scalar,Options,JointCollectionTpl> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &
                                  > ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data)
    {
      typedef typename Model::JointIndex JointIndex;
      typedef typename SizeDepType<JointModel::NV>::template ColsReturn<typename Data::Matrix6x>::Type ColsBlock;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];
      
      data.Ycrb[parent] += data.liMi[i].act(data.Ycrb[i]);
      
      jdata.U() = data.Ycrb[i] * jdata.S();
      
      ColsBlock jF = jmodel.jointCols(data.Ag);
      //        = data.Ag.middleCols(jmodel.idx_v(), jmodel.nv());
      
      forceSet::se3Action(data.oMi[i],jdata.U(),jF);
    }
    
  }; // struct CcrbaBackwardStep
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::Matrix6x &
  ccrba(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
        DataTpl<Scalar,Options,JointCollectionTpl> & data,
        const Eigen::MatrixBase<ConfigVectorType> & q,
        const Eigen::MatrixBase<TangentVectorType> & v)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    assert(v.size() == model.nv && "The velocity vector is not of right size");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    typedef typename Model::JointIndex JointIndex;
    
    forwardKinematics(model, data, q);
    data.Ycrb[0].setZero();
    for(JointIndex i=1; i<(JointIndex)(model.njoints); ++i)
      data.Ycrb[i] = model.inertias[i];
    
    typedef CcrbaBackwardStep<Scalar,Options,JointCollectionTpl> Pass2;
    for(JointIndex i=(JointIndex)(model.njoints-1); i>0; --i)
    {
      Pass2::run(model.joints[i],data.joints[i],
                 typename Pass2::ArgsType(model,data));
    }
    
    // Express the centroidal map around the center of mass
    data.com[0] = data.Ycrb[0].lever();
    
    typedef Eigen::Block<typename Data::Matrix6x,3,-1> Block3x;
    const Block3x Ag_lin = data.Ag.template middleRows<3>(Force::LINEAR);
    Block3x Ag_ang = data.Ag.template middleRows<3>(Force::ANGULAR);
    for (long i = 0; i<model.nv; ++i)
      Ag_ang.col(i) += Ag_lin.col(i).cross(data.com[0]);
    
    data.hg.toVector().noalias() = data.Ag*v;
    
    data.Ig.mass() = data.Ycrb[0].mass();
    data.Ig.lever().setZero();
    data.Ig.inertia() = data.Ycrb[0].inertia();
    
    return data.Ag;
  }
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl>
  struct DCcrbaBackwardStep
  : public fusion::JointVisitorBase< DCcrbaBackwardStep<Scalar,Options,JointCollectionTpl> >
  {
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    
    typedef boost::fusion::vector<const Model &,
                                  Data &
                                  > ArgsType;
    
    template<typename JointModel>
    static void algo(const JointModelBase<JointModel> & jmodel,
                     JointDataBase<typename JointModel::JointDataDerived> & jdata,
                     const Model & model,
                     Data & data)
    {
      typedef typename Model::JointIndex JointIndex;
      typedef typename Data::Inertia Inertia;
      typedef typename SizeDepType<JointModel::NV>::template ColsReturn<typename Data::Matrix6x>::Type ColsBlock;
      
      const JointIndex & i = jmodel.id();
      const JointIndex & parent = model.parents[i];
      const Inertia & Y = data.oYcrb[i];
      const typename Inertia::Matrix6 & doYcrb = data.doYcrb[i];
      
      ColsBlock J_cols = jmodel.jointCols(data.J);
      J_cols = data.oMi[i].act(jdata.S());
      
      ColsBlock dJ_cols = jmodel.jointCols(data.dJ);
      motionSet::motionAction(data.ov[i],J_cols,dJ_cols);
      
      data.oYcrb[parent] += Y;
      if(parent > 0)
        data.doYcrb[parent] += doYcrb;
      
      // Calc Ag
      ColsBlock Ag_cols = jmodel.jointCols(data.Ag);
      motionSet::inertiaAction(Y,J_cols,Ag_cols);
      
      // Calc dAg = Ivx + vxI
      ColsBlock dAg_cols = jmodel.jointCols(data.dAg);
      dAg_cols.noalias() = doYcrb * J_cols;
      motionSet::inertiaAction<ADDTO>(Y,dJ_cols,dAg_cols);
    }
    
  }; // struct DCcrbaBackwardStep
  
  template<typename Scalar, int Options, template<typename,int> class JointCollectionTpl, typename ConfigVectorType, typename TangentVectorType>
  inline const typename DataTpl<Scalar,Options,JointCollectionTpl>::Matrix6x &
  dccrba(const ModelTpl<Scalar,Options,JointCollectionTpl> & model,
         DataTpl<Scalar,Options,JointCollectionTpl> & data,
         const Eigen::MatrixBase<ConfigVectorType> & q,
         const Eigen::MatrixBase<TangentVectorType> & v)
  {
    assert(model.check(data) && "data is not consistent with model.");
    assert(q.size() == model.nq && "The configuration vector is not of right size");
    assert(v.size() == model.nv && "The velocity vector is not of right size");
    
    typedef ModelTpl<Scalar,Options,JointCollectionTpl> Model;
    typedef DataTpl<Scalar,Options,JointCollectionTpl> Data;
    typedef typename Model::JointIndex JointIndex;
    
    forwardKinematics(model,data,q,v);
    data.oYcrb[0].setZero();
    for(JointIndex i=1; i<(JointIndex)(model.njoints); ++i)
    {
      data.oYcrb[i] = data.oMi[i].act(model.inertias[i]);
      data.ov[i] = data.oMi[i].act(data.v[i]); // v_i expressed in the world frame
      data.doYcrb[i] = data.oYcrb[i].variation(data.ov[i]);
    }
    
    typedef DCcrbaBackwardStep<Scalar,Options,JointCollectionTpl> Pass2;
    for(JointIndex i=(JointIndex)(model.njoints-1); i>0; --i)
    {
      Pass2::run(model.joints[i],data.joints[i],
                 typename Pass2::ArgsType(model,data));
    }
    
    // Express the centroidal map around the center of mass
    data.com[0] = data.oYcrb[0].lever();
    
    typedef Eigen::Block<typename Data::Matrix6x,3,-1> Block3x;
    
    const Block3x Ag_lin = data.Ag.template middleRows<3> (Force::LINEAR);
    Block3x Ag_ang = data.Ag.template middleRows<3>  (Force::ANGULAR);
    for(Eigen::DenseIndex i = 0; i<model.nv; ++i)
      Ag_ang.col(i) += Ag_lin.col(i).cross(data.com[0]);
    
    data.hg.toVector().noalias() = data.Ag*v;
    data.vcom[0].noalias() = data.hg.linear()/data.oYcrb[0].mass();
    
    const Block3x dAg_lin = data.dAg.template middleRows<3>(Force::LINEAR);
    Block3x dAg_ang = data.dAg.template middleRows<3>(Force::ANGULAR);
    for(Eigen::DenseIndex i = 0; i<model.nv; ++i)
      dAg_ang.col(i) += dAg_lin.col(i).cross(data.com[0]);
    
    data.Ig.mass() = data.oYcrb[0].mass();
    data.Ig.lever().setZero();
    data.Ig.inertia() = data.oYcrb[0].inertia();
    
    return data.dAg;
  }
  
} // namespace se3

/// @endcond

#endif // ifndef __se3_centroidal_hxx__
