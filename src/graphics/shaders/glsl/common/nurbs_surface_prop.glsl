// Common GLSL Code for NURBS Surface Properties Computation
// This section implements the evaluation of a NURBS surface at given
// (u, v) parameters, including the computation of the surface point
// and normal vector (`computeNURBSSurfaceProperties`).
//
// THIS SECTION NEEDS THE FOLLOWING VARIABLES DEFINED:
// - degreeU, degreeV (int)
// - numCtrlPointsU, numCtrlPointsV (int)
// - knotsU[], knotsV[] (float arrays)
// - controlPointsWithWeights[] (vec4 array: x,y,z = control point, w = weight)
// - MAX_DEGREE (const int)
//
// This section provides the following struct:
// - BasisFunctionResultD
//
// This section provides the following functions:
// - findKnotSpan(float, bool, vec2) -> int
// - computeBasisFunctionsD(float, bool, vec2) -> BasisFunctionResultD
// - computeNURBSSurfaceProperties(float, float, out vec3, out vec3)



// Struct to hold the results of basis function calculation
struct BasisFunctionResultD {
    int knotSpan;
    float N[MAX_DEGREE + 1];     // Basis function values
    float ders[MAX_DEGREE + 1];  // 1st derivative values
};

// Finds the knot span index for a given parameter value.
//
// Parameters:
// - t: Parameter value
// - isU: true if in U direction, false if in V direction
// - paramRange: vec2(min, max) valid parameter range
// Returns:
// - knot span index
int findKnotSpan(float t, bool isU, vec2 paramRange) {
    // Clamp t to the valid parameter range
    t = clamp(t, paramRange.x, paramRange.y);

    int n = isU ? numCtrlPointsU - 1 : numCtrlPointsV - 1;
    int p = isU ? degreeU : degreeV;

    // Special cases at the boundaries
    if (t >= (isU ? knotsU[n + 1] : knotsV[n + 1])) {
        return n;
    }

    // Binary search (similar to 'std::upper_bound')
    int low = p;
    int high = n + 1;
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (t < (isU ? knotsU[mid] : knotsV[mid])) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }

    return low - 1;
}

// Computes B-spline basis functions and their first derivatives.
// Based on "The NURBS Book" Algorithm A2.3.
// Parameters:
// - t: Parameter value
// - isU: true if in U direction, false if in V direction
// - paramRange: vec2(min, max) valid parameter range
// Returns:
// - BasisFunctionResultD struct containing knot span, basis function values, and derivatives
BasisFunctionResultD computeBasisFunctionsD(float t, bool isU, vec2 paramRange) {
    BasisFunctionResultD result;
    result.knotSpan = findKnotSpan(t, isU, paramRange);
    int span = result.knotSpan;
    int degree_ = isU ? degreeU : degreeV;

    float left[MAX_DEGREE + 1];
    float right[MAX_DEGREE + 1];
    // ndu[i][j] means i-th basis function value at j-th degree
    float ndu[MAX_DEGREE + 1][MAX_DEGREE + 1];

    ndu[0][0] = 1.0;
    for (int j = 1; j <= degree_; j++) {
        left[j] = t - (isU ? knotsU[span + 1 - j] : knotsV[span + 1 - j]);
        right[j] = (isU ? knotsU[span + j] : knotsV[span + j]) - t;
        float saved = 0.0;
        for (int r = 0; r < j; r++) {
            ndu[j][r] = right[r + 1] + left[j - r];
            float temp = 0.0;
            if (ndu[j][r] != 0.0) {
                temp = ndu[r][j - 1] / ndu[j][r];
            }
            ndu[r][j] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        ndu[j][j] = saved;
    }

    for (int i = 0; i <= degree_; i++) {
        result.N[i] = ndu[i][degree_];
    }

    // Compute 1st derivatives
    for (int i = 0; i <= degree_; i++) {
        // First term of the derivative
        float term1 = 0.0;
        // denominator: (knots[span-i+degree_] - knots[span-i])
        float den1 = (isU ? knotsU[span + i] : knotsV[span + i])
                   - (isU ? knotsU[span - degree_ + i] : knotsV[span - degree_ + i]);
        if (den1 != 0.0) {
            if (i >= 1) {
                term1 = ndu[i - 1][degree_ - 1] / den1;
            }
        }

        // Second term of the derivative
        float term2 = 0.0;
        // denominator: (knots[span-i+degree_+1] - knots[span-i+1])
        float den2 = (isU ? knotsU[span + i + 1] : knotsV[span + i + 1])
                   - (isU ? knotsU[span - degree_ + i + 1] : knotsV[span - degree_ + i + 1]);
        if (den2 != 0.0) {
            if (i <= degree_ - 1) {
                term2 = ndu[i][degree_ - 1] / den2;
            }
        }

        result.ders[i] = float(degree_) * (term1 - term2);
    }

    return result;
}

// Calculates the point and normal vector on a NURBS surface at (u, v).
//
// Parameters:
// - u, v: Parameter values in U and V directions
// - point: Output point on the surface
// - normal: Output normal vector at the point
void computeNURBSSurfaceProperties(float u, float v, out vec3 point, out vec3 normal) {
    // 1. Calculate basis functions and their first derivatives for both u and v.
    BasisFunctionResultD basisU = computeBasisFunctionsD(u, true, paramRangeU);
    BasisFunctionResultD basisV = computeBasisFunctionsD(v, false, paramRangeV);

    // 2. Initialize variables for summation.
    // 'S' accumulates weighted control points for the point on the surface.
    // 'Su' and 'Sv' accumulate derivatives with respect to u and v.
    // 'w', 'wu', 'wv' are their corresponding weight summations.
    vec3 S = vec3(0.0);
    vec3 Su = vec3(0.0);
    vec3 Sv = vec3(0.0);
    float w = 0.0, wu = 0.0, wv = 0.0;

    // 3. Loop through the relevant control points.
    for (int j = 0; j <= degreeV; j++) {
        for (int i = 0; i <= degreeU; i++) {
            // Determine the indices of the control points and weights.
            int ctrlIdxU = basisU.knotSpan - degreeU + i;
            int ctrlIdxV = basisV.knotSpan - degreeV + j;
            int ctrlIdx = ctrlIdxU * numCtrlPointsV + ctrlIdxV;

            vec3 P = controlPointsWithWeights[ctrlIdx].xyz;
            float weight = controlPointsWithWeights[ctrlIdx].w;

            // Get basis function values and their derivatives.
            float N_u  = basisU.N[i];
            float N_v  = basisV.N[j];
            float dN_u = basisU.ders[i];
            float dN_v = basisV.ders[j];

            // Calculate products of basis functions for summation.
            float basisProd   = N_u * N_v;
            float basisProd_u = dN_u * N_v;
            float basisProd_v = N_u * dN_v;

            // Perform the summations.
            S  += basisProd   * weight * P;
            Su += basisProd_u * weight * P;
            Sv += basisProd_v * weight * P;
            w  += basisProd   * weight;
            wu += basisProd_u * weight;
            wv += basisProd_v * weight;
        }
    }

    // 4. Calculate the final surface point and partial derivatives.
    if (w == 0.0) {
        // Handle undefined case.
        point = vec3(0.0);
        normal = vec3(0.0, 1.0, 0.0);
        return;
    }

    // The surface point is the sum of weighted control points divided by the sum of weights.
    point = S / w;

    // Use the rational derivative formula to find the tangent vectors.
    vec3 dSdu = (Su * w - S * wu) / (w * w);
    vec3 dSdv = (Sv * w - S * wv) / (w * w);

    // 5. Compute the normal vector.
    // The normal is the cross product of the tangent vectors.
    normal = cross(dSdu, dSdv);

    if (length(normal) == 0.0) {
        // Handle degenerate case (e.g., a cusp).
        normal = vec3(0.0, 1.0, 0.0);
        return;
    }

    // Return a unit normal vector.
    normal = normalize(normal);
}
