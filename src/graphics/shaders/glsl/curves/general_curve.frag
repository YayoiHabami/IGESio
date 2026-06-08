// Fragment Shader for General Curves
//
// THIS CODE REQUIRES THE FOLLOWING VARIABLES TO BE BOUND:
// - General:
//   - mainColor (uniform vec4)
#version 400 core

out vec4 FragColor;
uniform vec4 mainColor;

void main() {
    FragColor = mainColor;

    // 面上に乗る曲線を面より僅かに手前へ出すための深度バイアス.
    // 一定量だと透視投影の深度圧縮 (far/near比) により、中〜遠距離で窓深度の
    // 僅かな差がワールド空間の大きな前方シフトに化け、面の奥にある曲線まで
    // 前面を突き抜けてしまう. そこで局所深度勾配 fwidth(z) に比例させる
    // (ラインに対する glPolygonOffset の傾き項の等価物).
    // 面と同一平面上の曲線は「面の傾き×数ピクセル」分だけ手前に出て確実に勝つ
    // 一方、面の奥にある曲線は自身の僅かな傾き分しかずれないため、シルエットの
    // 細い縁を除いて面に正しく遮蔽される. 深度精度に依存しないのが利点.
    float slope = fwidth(gl_FragCoord.z);
    gl_FragDepth = gl_FragCoord.z - (slope * 2.0 + 1e-6);
}
