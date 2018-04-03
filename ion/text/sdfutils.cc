/**
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include "ion/text/sdfutils.h"

#include "ion/base/logging.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"

namespace ion {
namespace text {

namespace {

using base::Array2;
using math::Vector2d;
using math::Vector2i;

// Convenience typedef for a Grid, which is a 2D array of doubles, typically
// pixel values or signed distances.
typedef Array2<double> Grid;

//-----------------------------------------------------------------------------
//
// The DistanceComputer class implements the main functions to compute signed
// distance fields.
//
// This code is an implementation of the algorithm described at
// <http://contourtextures.wikidot.com>. The advantage of this algorithm over
// other SDF generators is that it uses an antialiased rendered font instead of
// a bitmapped font. A bitmapped font would have to be rendered at much higher
// resolution to achieve the same quality as provided here.
//
//-----------------------------------------------------------------------------

class DistanceComputer {
 public:
  DistanceComputer() {}
  ~DistanceComputer() {}

  // Computes and returns a Grid containing signed distances for a Grid
  // representing a grayscale image.  Each pixel's value ("signed distance") is
  // the distance from the center of that pixel to the nearest boundary/edge,
  // signed so that pixels inside the boundary are negative and those outside
  // the boundary are positive.
  const Grid Compute(const Grid& image);

 private:
  // This struct is used to pass most of the current data to the main
  // computation functions.
  struct Data {
    Data(const Grid& image_in, const Array2<Vector2d>& gradients_in)
        : image(image_in),
          gradients(gradients_in),
          cur_pixel(0, 0),
          any_distance_changed(false) {}

    // Convenience access functions.
    void SetCurDistance(double dist) {
      distances.Set(cur_pixel[0], cur_pixel[1], dist);
    }
    double GetCurDistance() const {
      return distances.Get(cur_pixel[0], cur_pixel[1]);
    }
    void SetCurDistanceToEdge(const Vector2i& dist) {
      distances_to_edges.Set(cur_pixel[0], cur_pixel[1], dist);
    }
    const Vector2i& GetDistanceToEdge(const Vector2i& pixel) const {
      return distances_to_edges.Get(pixel[0], pixel[1]);
    }

    // The original monochrome image data, as doubles (0 - 1).
    const Grid& image;
    // Local gradients in X and Y.
    const Array2<Vector2d>& gradients;
    // Current pixel distances in X and Y to edges.
    Array2<Vector2i> distances_to_edges;
    // Final distance values.
    Grid distances;
    // Indices of the current pixel being operated on.
    Vector2i cur_pixel;
    // This is set to true when a value in the distances grid is modified.
    bool any_distance_changed;
  };

  // Computes the local gradients of an image in the X and Y dimensions and
  // returns them as an Array2<Vector2d>.
  static const Array2<Vector2d> ComputeGradients(const Grid& image);

  // Applies a 3x3 filter kernel to an image pixel to get the gradients.
  static const Vector2d FilterPixel(const Grid& image, size_t x, size_t y);

  // Creates and initializes a grid containing the distances.
  static const Grid InitializeDistanceGrid(
      const Grid& image, const Array2<Vector2d>& gradients);

  // Approximates the distance to an image edge from a pixel using the pixel
  // value and the local gradient.
  static double ApproximateDistanceToEdge(double value,
                                          const Vector2d& gradient);

  // Computes and returns the distances.
  static void ComputeDistances(Data* data);

  // Computes the distance from data->cur_pixel to an edge pixel based on the
  // information at the pixel at (data->cur_pixel + offset). If the new
  // distance is smaller than the current distance (dist), this modifies dist
  // and sets data->any_distance_changed to true.
  static void UpdateDistance(Data* data, const Vector2i& offset, double* dist);

  // Computes the new distance from a pixel to an edge pixel based on previous
  // information.
  static double ComputeDistanceToEdge(Data* data, const Vector2i& pixel,
                                      const Vector2d& vec_to_edge_pixel);

  // Represents a large distance during computation.
  static const double kLargeDistance;
};

const double DistanceComputer::kLargeDistance = 1e6;

const Grid DistanceComputer::Compute(const Grid& image) {
  // Compute the local gradients in both dimensions.
  const Array2<Vector2d> gradients = ComputeGradients(image);

  // Store everything in a struct to pass to the main computation function.
  Data data(image, gradients);
  data.distances_to_edges =
      Array2<Vector2i>(image.GetWidth(), image.GetHeight(), Vector2i::Zero());
  data.distances = InitializeDistanceGrid(image, gradients);
  ComputeDistances(&data);

  return data.distances;
}

const Array2<Vector2d> DistanceComputer::ComputeGradients(const Grid& image) {
  const size_t h = image.GetHeight();
  const size_t w = image.GetWidth();

  Array2<Vector2d> gradients(w, h, Vector2d::Zero());

  // This computes the local gradients at pixels near black/white boundaries in
  // the image using convolution filters. The gradient is not needed at other
  // pixels, where it's mostly zero anyway.

  // The 3x3 kernel does not work at the edges, so skip those pixels.
  // 
  for (size_t y = 1; y < h - 1; ++y) {
    for (size_t x = 1; x < w - 1; ++x) {
      const double value = image.Get(x, y);
      // If the pixel is fully on or off, leave the gradient as (0,0).
      // Otherwise, compute it.
      if (value > 0.0 && value < 1.0)
        gradients.Set(x, y, FilterPixel(image, x, y));
    }
  }
  return gradients;
}

const Vector2d DistanceComputer::FilterPixel(const Grid& image,
                                             size_t x, size_t y) {
  // 3x3 filter kernel. The X gradient uses the array as is and the Y gradient
  // uses the transpose.
  static const double kSqrt2 = sqrt(2.0);
  static const double kFilter[3][3] = {
    { -1.0,    0.0, 1.0    },
    { -kSqrt2, 0.0, kSqrt2 },
    { -1.0,    0.0, 1.0    },
  };

  Vector2d filtered(0.0, 0.0);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      const double val = image.Get(x + j - 1, y + i - 1);
      filtered[0] += kFilter[i][j] * val;
      filtered[1] += kFilter[j][i] * val;
    }
  }
  return math::Normalized(filtered);
}

const Grid DistanceComputer::InitializeDistanceGrid(
    const Grid& image, const Array2<Vector2d>& gradients) {
  const size_t h = image.GetHeight();
  const size_t w = image.GetWidth();
  Grid distances(w, h);
  for (size_t y = 0; y < h; ++y) {
    for (size_t x = 0; x < w; ++x) {
      const double v = image.Get(x, y);
      const double dist = v <= 0.0 ? kLargeDistance :
                          v >= 1.0 ? 0.0 :
                          ApproximateDistanceToEdge(v, gradients.Get(x, y));
      distances.Set(x, y, dist);
    }
  }
  return distances;
}

double DistanceComputer::ApproximateDistanceToEdge(
    double value, const Vector2d& gradient) {
  if (gradient[0] == 0.0 || gradient[1] == 0.0) {
    // Approximate the gradient linearly using the middle of the range.
    return 0.5 - value;
  } else {
    // Since the gradients are symmetric with respect to both sign and X/Y
    // transposition, do the work in the first octant (positive gradients, x
    // gradient >= y gradient) for simplicity.
    Vector2d g = math::Normalized(Vector2d(math::Abs(gradient[0]),
                                           math::Abs(gradient[1])));
    if (g[0] < g[1])
      std::swap(g[0], g[1]);
    const float gradient_value = static_cast<float>(0.5 * g[1] / g[0]);
    double dist;
    if (value < gradient_value) {
      // 0 <= value < gradient_value.
      dist = 0.5 * (g[0] + g[1]) - sqrt(2.0 * g[0] * g[1] * value);
    } else if (value < 1.0 - gradient_value) {
      // gradient_value <= value <= 1 - gradient_value.
      dist = (0.5 - value) * g[0];
    } else {
      // 1 - gradient_value < value <= 1.
      dist = -0.5 * (g[0] + g[1]) + sqrt(2.0 * g[0] * g[1] * (1.0 - value));
    }
    return dist;
  }
}

void DistanceComputer::ComputeDistances(Data* data) {
  const int height = static_cast<int>(data->image.GetHeight());
  const int width = static_cast<int>(data->image.GetWidth());

  // Keep processing while distances are being modified.
  do {
    data->any_distance_changed = false;

    // Propagate from top down, starting with the second row.
    for (int y = 1; y < height; ++y) {
      data->cur_pixel[1] = y;

      // Propagate distances to the right.
      for (int x = 0; x < width; ++x) {
        data->cur_pixel[0] = x;
        double dist = data->GetCurDistance();
        if (dist > 0.0) {
          UpdateDistance(data, Vector2i(0, -1), &dist);
          if (x > 0) {
            UpdateDistance(data, Vector2i(-1, 0), &dist);
            UpdateDistance(data, Vector2i(-1, -1), &dist);
          }
          if (x < width - 1) {
            UpdateDistance(data, Vector2i(1, -1), &dist);
          }
        }
      }

      // Propagate distances to the left (skip the rightmost pixel).
      for (int x = width - 2; x >= 0; --x) {
        data->cur_pixel[0] = x;
        double dist = data->GetCurDistance();
        if (dist > 0.0) {
          UpdateDistance(data, Vector2i(1, 0), &dist);
        }
      }
    }

    // Propagate from bottom up, starting with the second row from the bottom.
    for (int y = height - 2; y >= 0; --y) {
      data->cur_pixel[1] = y;

      // Propagate distances to the left.
      for (int x = width - 1; x >= 0; --x) {
        data->cur_pixel[0] = x;
        double dist = data->GetCurDistance();
        if (dist > 0.0) {
          UpdateDistance(data, Vector2i(0, 1), &dist);
          if (x > 0) {
            UpdateDistance(data, Vector2i(-1, 1), &dist);
          }
          if (x < width - 1) {
            UpdateDistance(data, Vector2i(1, 0), &dist);
            UpdateDistance(data, Vector2i(1, 1), &dist);
          }
        }
      }

      // Propagate distances to the right (skip the leftmost pixel).
      for (int x = 1; x < width; ++x) {
        data->cur_pixel[0] = x;
        double dist = data->GetCurDistance();
        if (dist > 0.0) {
          UpdateDistance(data, Vector2i(-1, 0), &dist);
        }
      }
    }
  } while (data->any_distance_changed);

  // Don't return negative distances.
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      data->distances.Set(x, y, std::max(0.0, data->distances.Get(x, y)));
    }
  }
}

void DistanceComputer::UpdateDistance(Data* data, const Vector2i& offset,
                                      double* dist) {
  const Vector2i test_pixel = data->cur_pixel + offset;
  const Vector2i& xy_dist = data->GetDistanceToEdge(test_pixel);
  const Vector2i edge_pixel = test_pixel - xy_dist;
  const Vector2i new_xy_dist = xy_dist - offset;
  const double new_dist = ComputeDistanceToEdge(data, edge_pixel,
                                                Vector2d(new_xy_dist));
  static const double kEpsilon = 1e-3;
  if (new_dist < *dist - kEpsilon) {
    data->SetCurDistance(new_dist);
    data->SetCurDistanceToEdge(new_xy_dist);
    *dist = new_dist;
    data->any_distance_changed = true;
  }
}

double DistanceComputer::ComputeDistanceToEdge(
    Data* data, const Vector2i& pixel, const Vector2d& vec_to_edge_pixel) {
  // Clamp the pixel value to [0,1].
  const double value = math::Clamp(data->image.Get(pixel[0], pixel[1]),
                                   0.0, 1.0);

  // If the pixel value is negative or 0, return kLargeDistance so that
  // processing will continue.
  if (value == 0.0)
    return kLargeDistance;

  // Use the length of the vector to the edge pixel to estimate the real
  // distance to the edge.
  const double length = math::Length(vec_to_edge_pixel);
  const double dist =
      length > 0.0 ?
      // Estimate based on direction to edge (accurate for large vectors).
      ApproximateDistanceToEdge(value, vec_to_edge_pixel) :
      // Estimate based on local gradient only.
      ApproximateDistanceToEdge(value, data->gradients.Get(pixel[0], pixel[1]));

  return length + dist;
}

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Pads a grid on all 4 sides, setting all new elements to 0.
static const Grid PadGrid(const Grid& grid, size_t padding) {
  const size_t width = grid.GetWidth();
  const size_t height = grid.GetHeight();
  Grid padded_grid(width + 2 * padding, height + 2 * padding, 0.0);
  for (size_t y = 0; y < height ; ++y) {
    for (size_t x = 0; x < width; ++x) {
      padded_grid.Set(padding + x, padding + y, grid.Get(x, y));
    }
  }
  return padded_grid;
}

// Returns the inverse of a Grid by subtracting each value from 1.0.
static const Grid InvertGrid(const Grid& grid) {
  const size_t w = grid.GetWidth();
  const size_t h = grid.GetHeight();
  Grid inverted(w, h);
  for (size_t y = 0; y < h; ++y) {
    for (size_t x = 0; x < w; ++x) {
      inverted.Set(x, y, 1.0 - grid.Get(x, y));
    }
  }
  return inverted;
}

// Builds and returns a signed distance field grid for an input grid containing
// antialiased pixel values.
static const Grid BuildSdfGrid(const Grid& grid) {
  const size_t height = grid.GetHeight();
  const size_t width = grid.GetWidth();
  Grid sdf(width, height);
  if (height > 0 && width > 0) {
    // Compute the distances to the background edges (original grid) and the
    // foreground edges (inverse grid). The difference is the signed distance.
    DistanceComputer dc;
    const Grid bg_distances = dc.Compute(grid);
    const Grid fg_distances = dc.Compute(InvertGrid(grid));
    for (size_t y = 0; y < height; ++y) {
      for (size_t x = 0; x < width; ++x) {
        sdf.Set(x, y, bg_distances.Get(x, y) - fg_distances.Get(x, y));
      }
    }
  }
  return sdf;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Public SDF utility functions.
//
//-----------------------------------------------------------------------------

const Grid ComputeSdfGrid(const Grid& image_grid, size_t padding) {
  return BuildSdfGrid(PadGrid(image_grid, padding));
}

}  // namespace text
}  // namespace ion
