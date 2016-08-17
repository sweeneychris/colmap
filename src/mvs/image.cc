// COLMAP - Structure-from-Motion and Multi-View Stereo.
// Copyright (C) 2016  Johannes L. Schoenberger <jsch at inf.ethz.ch>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "mvs/image.h"

#include <Eigen/Core>

#include "base/projection.h"
#include "util/bitmap.h"
#include "util/logging.h"

namespace colmap {
namespace mvs {

void Image::Load(const float* K, const float* R, const float* T,
                 const std::string& path, const bool as_rgb,
                 const bool extract_principal_point) {
  CHECK(bitmap_.Read(path, as_rgb));

  memcpy(K_, K, 9 * sizeof(float));
  memcpy(R_, R, 9 * sizeof(float));
  memcpy(T_, T, 3 * sizeof(float));

  if (extract_principal_point) {
    K_[2] = static_cast<float>(bitmap_.Width()) / 2.0f;
    K_[5] = static_cast<float>(bitmap_.Height()) / 2.0f;
  }

  ComposeProjectionMatrix(K_, R_, T_, P_);
  ComposeInverseProjectionMatrix(K_, R_, T_, inv_P_);
}

void Image::Rescale(const float factor) { Rescale(factor, factor); }

void Image::Rescale(const float factor_x, const float factor_y) {
  const size_t new_width = std::round(bitmap_.Width() * factor_x);
  const size_t new_height = std::round(bitmap_.Height() * factor_y);
  const float scale_x = new_width / static_cast<float>(bitmap_.Width());
  const float scale_y = new_height / static_cast<float>(bitmap_.Height());
  bitmap_.Rescale(new_width, new_height);
  K_[0] *= scale_x;
  K_[2] *= scale_x;
  K_[4] *= scale_y;
  K_[5] *= scale_y;
  ComposeProjectionMatrix(K_, R_, T_, P_);
  ComposeInverseProjectionMatrix(K_, R_, T_, inv_P_);
}

void Image::Downsize(const size_t max_width, const size_t max_height) {
  if (static_cast<size_t>(bitmap_.Width()) <= max_width &&
      static_cast<size_t>(bitmap_.Height()) <= max_height) {
    return;
  }
  const float factor_x = static_cast<float>(max_width) / bitmap_.Width();
  const float factor_y = static_cast<float>(max_height) / bitmap_.Height();
  const float factor = std::min(factor_x, factor_y);
  Rescale(factor);
}

void ComputeRelativePose(const float R1[9], const float T1[3],
                         const float R2[9], const float T2[3], float R[9],
                         float T[3]) {
  const Eigen::Map<const Eigen::Matrix<float, 3, 3, Eigen::RowMajor>> R1_m(R1);
  const Eigen::Map<const Eigen::Matrix<float, 3, 3, Eigen::RowMajor>> R2_m(R2);
  const Eigen::Map<const Eigen::Matrix<float, 3, 1>> T1_m(T1);
  const Eigen::Map<const Eigen::Matrix<float, 3, 1>> T2_m(T2);
  Eigen::Map<Eigen::Matrix<float, 3, 3, Eigen::RowMajor>> R_m(R);
  Eigen::Map<Eigen::Vector3f> T_m(T);

  R_m = R2_m * R1_m.transpose();
  T_m = T2_m - R_m * T1_m;
}

void ComposeProjectionMatrix(const float K[9], const float R[9],
                             const float T[3], float P[12]) {
  Eigen::Map<Eigen::Matrix<float, 3, 4, Eigen::RowMajor>> P_m(P);
  P_m.leftCols<3>() =
      Eigen::Map<const Eigen::Matrix<float, 3, 3, Eigen::RowMajor>>(R);
  P_m.rightCols<1>() = Eigen::Map<const Eigen::Vector3f>(T);
  P_m = Eigen::Map<const Eigen::Matrix<float, 3, 3, Eigen::RowMajor>>(K) * P_m;
}

void ComposeInverseProjectionMatrix(const float K[9], const float R[9],
                                    const float T[3], float inv_P[12]) {
  Eigen::Matrix<float, 4, 4, Eigen::RowMajor> P;
  ComposeProjectionMatrix(K, R, T, P.data());
  P.row(3) = Eigen::Vector4f(0, 0, 0, 1);
  Eigen::Map<Eigen::Matrix<float, 3, 4, Eigen::RowMajor>> inv_P_m(inv_P);
  inv_P_m = P.inverse().topRows<3>();
}

void ComputeProjectionCenter(const float R[9], const float T[3], float C[3]) {
  const Eigen::Map<const Eigen::Matrix<float, 3, 3, Eigen::RowMajor>> R_m(R);
  const Eigen::Map<const Eigen::Matrix<float, 3, 1>> T_m(T);
  Eigen::Map<Eigen::Vector3f> C_m(C);
  C_m = -R_m.transpose() * T_m;
}

void RotatePose(const float RR[9], float R[9], float T[3]) {
  Eigen::Map<Eigen::Matrix<float, 3, 3, Eigen::RowMajor>> R_m(R);
  Eigen::Map<Eigen::Matrix<float, 3, 1>> T_m(T);
  const Eigen::Map<const Eigen::Matrix<float, 3, 3, Eigen::RowMajor>> RR_m(RR);
  R_m = RR_m * R_m;
  T_m = RR_m * T_m;
}

}  // namespace mvs
}  // namespace colmap
