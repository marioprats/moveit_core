/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#include <planning_models/kinematic_model.h>
#include <boost/math/constants/constants.hpp>
#include <limits>
#include <cmath>

planning_models::KinematicModel::PlanarJointModel::PlanarJointModel(const std::string& name) : JointModel(name), angular_distance_weight_(1.0)
{
  type_ = PLANAR;
  
  local_names_.push_back("x");
  local_names_.push_back("y");
  local_names_.push_back("theta");
  for (int i = 0 ; i < 3 ; ++i)
    variable_names_.push_back(name_ + "/" + local_names_[i]);
  variable_bounds_.resize(3);
  variable_bounds_[0] = std::make_pair(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
  variable_bounds_[1] = std::make_pair(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
  variable_bounds_[2] = std::make_pair(-boost::math::constants::pi<double>(), boost::math::constants::pi<double>());
}

unsigned int planning_models::KinematicModel::PlanarJointModel::getStateSpaceDimension(void) const
{
  return 3;
}

double planning_models::KinematicModel::PlanarJointModel::getMaximumExtent(void) const
{
  double dx = variable_bounds_[0].first - variable_bounds_[0].second;
  double dy = variable_bounds_[1].first - variable_bounds_[1].second;
  return sqrt(dx*dx + dy*dy) + boost::math::constants::pi<double>() * angular_distance_weight_;
}

void planning_models::KinematicModel::PlanarJointModel::getDefaultValues(std::vector<double>& values, const Bounds &bounds) const
{
  assert(bounds.size() > 1);
  for (unsigned int i = 0 ; i < 2 ; ++i)
  {
    // if zero is a valid value
    if (bounds[i].first <= 0.0 && bounds[i].second >= 0.0)
      values.push_back(0.0);
    else
      values.push_back((bounds[i].first + bounds[i].second)/2.0);
  }
  values.push_back(0.0);
}

void planning_models::KinematicModel::PlanarJointModel::getRandomValues(random_numbers::RandomNumberGenerator &rng, std::vector<double> &values, const Bounds &bounds) const
{
  std::size_t s = values.size();
  values.resize(s + 3);
  if (bounds[0].second >= std::numeric_limits<double>::max() || bounds[0].first <= -std::numeric_limits<double>::max())
    values[s] = 0.0;
  else
    values[s] = rng.uniformReal(bounds[0].first, bounds[0].second);
  if (bounds[1].second >= std::numeric_limits<double>::max() || bounds[1].first <= -std::numeric_limits<double>::max())
    values[s + 1] = 0.0;
  else
    values[s + 1] = rng.uniformReal(bounds[1].first, bounds[1].second);
  values[s + 2] = rng.uniformReal(bounds[2].first, bounds[2].second);
}

void planning_models::KinematicModel::PlanarJointModel::getRandomValuesNearBy(random_numbers::RandomNumberGenerator &rng, std::vector<double> &values, const Bounds &bounds,
                                                                              const std::vector<double> &near, const double distance) const
{
  std::size_t s = values.size();
  values.resize(s + 3);
  if (bounds[0].second >= std::numeric_limits<double>::max() || bounds[0].first <= -std::numeric_limits<double>::max())
    values[s] = 0.0;
  else
    values[s] = rng.uniformReal(std::max(bounds[0].first, near[s] - distance),
                                std::min(bounds[0].second, near[s] + distance));
  if (bounds[1].second >= std::numeric_limits<double>::max() || bounds[1].first <= -std::numeric_limits<double>::max())
    values[s + 1] = 0.0;
  else
    values[s + 1] = rng.uniformReal(std::max(bounds[1].first, near[s + 1] - distance),
                                    std::min(bounds[1].second, near[s + 1] + distance));

  double da = angular_distance_weight_ * distance;
  values[s + 2] = rng.uniformReal(near[s + 2] - da, near[s + 2] + da);
  normalizeRotation(values);
}

void planning_models::KinematicModel::PlanarJointModel::interpolate(const std::vector<double> &from, const std::vector<double> &to, const double t, std::vector<double> &state) const
{
  // interpolate position
  state[0] = from[0] + (to[0] - from[0]) * t;
  state[1] = from[1] + (to[1] - from[1]) * t;
  
  // interpolate angle
  double diff = to[2] - from[2];
  if (fabs(diff) <= boost::math::constants::pi<double>())
    state[2] = from[2] + diff * t;
  else
  {
    if (diff > 0.0)
      diff = 2.0 * boost::math::constants::pi<double>() - diff;
    else
      diff = -2.0 * boost::math::constants::pi<double>() - diff;
    state[2] = from[2] - diff * t;
    // input states are within bounds, so the following check is sufficient
    if (state[2] > boost::math::constants::pi<double>())
      state[2] -= 2.0 * boost::math::constants::pi<double>();
    else
      if (state[2] < -boost::math::constants::pi<double>())
        state[2] += 2.0 * boost::math::constants::pi<double>();
  }
}

double planning_models::KinematicModel::PlanarJointModel::distance(const std::vector<double> &values1, const std::vector<double> &values2) const
{
  assert(values1.size() == 3);
  assert(values2.size() == 3);
  double dx = values1[0] - values2[0];
  double dy = values1[1] - values2[1];
  
  double d = fabs(values1[2] - values2[2]);
  d = (d > boost::math::constants::pi<double>()) ? 2.0 * boost::math::constants::pi<double>() - d : d;
  return sqrt(dx*dx + dy*dy) + angular_distance_weight_ * d;
}

bool planning_models::KinematicModel::PlanarJointModel::satisfiesBounds(const std::vector<double> &values, const Bounds &bounds, double margin) const
{
  assert(bounds.size() > 1);
  for (unsigned int i = 0 ; i < 3 ; ++i)
  if (values[0] < bounds[0].first - margin || values[0] > bounds[0].second + margin)
    return false;
  return true;
}

void planning_models::KinematicModel::PlanarJointModel::normalizeRotation(std::vector<double> &values) const
{
  double &v = values[2];
  v = fmod(v, 2.0 * boost::math::constants::pi<double>());
  if (v < -boost::math::constants::pi<double>())
    v += 2.0 * boost::math::constants::pi<double>();
  else
    if (v > boost::math::constants::pi<double>())
      v -= 2.0 * boost::math::constants::pi<double>();
}

void planning_models::KinematicModel::PlanarJointModel::enforceBounds(std::vector<double> &values, const Bounds &bounds) const
{
  normalizeRotation(values);
  for (unsigned int i = 0 ; i < 2 ; ++i)
  {
    const std::pair<double, double> &b = bounds[i];
    if (values[i] < b.first)
      values[i] = b.first;
    else
      if (values[i] > b.second)
        values[i] = b.second;
  }
}

void planning_models::KinematicModel::PlanarJointModel::computeTransform(const std::vector<double>& joint_values, Eigen::Affine3d &transf) const
{
  updateTransform(joint_values, transf);
}

void planning_models::KinematicModel::PlanarJointModel::updateTransform(const std::vector<double>& joint_values, Eigen::Affine3d &transf) const
{
  transf = Eigen::Affine3d(Eigen::Translation3d(joint_values[0], joint_values[1], 0.0) * Eigen::AngleAxisd(joint_values[2], Eigen::Vector3d::UnitZ()));
}

void planning_models::KinematicModel::PlanarJointModel::computeJointStateValues(const Eigen::Affine3d& transf, std::vector<double> &joint_values) const
{
  joint_values.resize(3);
  joint_values[0] = transf.translation().x();
  joint_values[1] = transf.translation().y();
  
  Eigen::Quaterniond q(transf.rotation());
  //taken from Bullet
  double s_squared = 1.0-(q.w()*q.w());
  if (s_squared < 10.0 * std::numeric_limits<double>::epsilon())
    joint_values[2] = 0.0;
  else
  {
    double s = 1.0/sqrt(s_squared);
    joint_values[2] = (acos(q.w())*2.0f)*(q.z()*s);
  }
}
