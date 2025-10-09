// Common GLSL Code for NURBS Curve Properties Computation
// This section implements the evaluation of a NURBS curve at given
// (u, v) parameters, including the computation of the curve point
// (`computeNURBSCurvePoint`).
//
// THIS SECTION NEEDS THE FOLLOWING VARIABLES DEFINED:
// - degreeT (int)
// - numCtrlPointsT (int)
// - knotsT (float arrays)
// - controlPointsWithWeightsT[] (vec4 array: x,y,z = control point, w = weight)
// - MAX_DEGREE (const int)
//
// This section provides the following struct:
// - BasisFunctionResult
//
// This section provides the following functions:
// - computeBasisFunctions(float) -> BasisFunctionResult
// - computeNURBSCurvePoint(float) -> vec3



// Struct to hold the results of basis function calculation
struct BasisFunctionResult {
    int knotSpan;
    float N[MAX_DEGREE + 1];     // Basis function values
};

// Computes B-spline basis functions. Based on "The NURBS Book" Algorithm A2.3.
//
// Parameters:
// - t: Parameter value
// Returns:
// - BasisFunctionResult struct containing knot span, basis function values
BasisFunctionResult computeBasisFunctions(float t) {
    BasisFunctionResult result;

    // Finds the knot span index
    t = clamp(t, paramRangeT.x, paramRangeT.y);
    if (t >= knotsT[numCtrlPointsT]) {
        // Special cases at the boundaries
        result.knotSpan = numCtrlPointsT - 1;
    } else {
        // Binary search (similar to 'std::upper_bound')
        int low = degreeT;
        int high = numCtrlPointsT;
        while (low < high) {
            int mid = low + (high - low) / 2;
            if (t < knotsT[mid]) {
                high = mid;
            } else {
                low = mid + 1;
            }
        }
        result.knotSpan = low - 1;
    }

    // Compute basis functions
    float left[MAX_DEGREE + 1];
    float right[MAX_DEGREE + 1];
    float ndu[MAX_DEGREE + 1][MAX_DEGREE + 1];
    ndu[0][0] = 1.0;
    for (int j = 1; j <= degreeT; ++j) {
        left[j] = t - knotsT[result.knotSpan + 1 - j];
        right[j] = knotsT[result.knotSpan + j] - t;
        float saved = 0.0;
        for (int r = 0; r < j; ++r) {
            ndu[j][r] = right[r + 1] + left[j - r];
            float temp = ndu[r][j - 1] / ndu[j][r];
            ndu[r][j] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        ndu[j][j] = saved;
    }
    for (int j = 0; j <= degreeT; ++j) {
        result.N[j] = ndu[j][degreeT];
    }

    return result;
}


/// Calculate the NURBS curve point at parameter t
//
// Parameters:
// - t: Parameter value
//
// Returns:
// - C(t)
vec3 computeNURBSCurvePoint(float t) {
    // Compute basis functions
    BasisFunctionResult basisResult = computeBasisFunctions(t);

    // Calculate the point coordinates according to the NURBS definition
    vec3 numerator = vec3(0.0);
    float denominator = 0.0;

    for (int i = 0; i <= degreeT; ++i) {
        int cp_idx = basisResult.knotSpan - degreeT + i;
        if (cp_idx < 0 || cp_idx >= numCtrlPointsT) continue;

        float temp = basisResult.N[i] * controlPointsWithWeightsT[cp_idx].w;
        numerator += controlPointsWithWeightsT[cp_idx].xyz * temp;
        denominator += temp;
    }

    // Avoid division by zero
    if (abs(denominator) < 1e-6) {
        return controlPointsWithWeightsT[basisResult.knotSpan - degreeT].xyz;
    }

    return numerator / denominator;
}
